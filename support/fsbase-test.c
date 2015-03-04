/* Copyright (c) 2014 The Regents of the University of California
 * Kevin Klues <klueska@cs.berkeley.edu>
 * See LICENSE for details. */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <asm/prctl.h>
#include <sys/prctl.h>
#include <signal.h>
#include <stdlib.h>
#include <sched.h>
#include <limits.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <parlib/timing.h>
#include <parlib/arch.h>
#include <parlib/atomic.h>

static __thread void *fakefs = 0;
static void (*wrfsbase)(void *tls_addr);
static void *(*rdfsbase)(void);

static inline void *inst_rdfsbase(void)
{
	void *fs;
	asm volatile(".byte 0xf3,0x48,0x0f,0xae,0xc0 # rdfsbaseq %%rax"
	             : "=a" (fs)
	             :: "memory");
	return fs;
}

static inline void inst_wrfsbase(void *fs)
{
	asm volatile(".byte 0xf3,0x48,0x0f,0xae,0xd0 # wrfsbaseq %%rax"
	             :: "a" (fs)
	             : "memory");
}

static inline void *sys_rdfsbase(void)
{
	int arch_prctl(int code, unsigned long *addr);
	unsigned long fs;
	arch_prctl(ARCH_GET_FS, &fs);
	return (void*)fs;
}

static inline void sys_wrfsbase(void *fs)
{
	int arch_prctl(int code, unsigned long *addr);
	arch_prctl(ARCH_SET_FS, (unsigned long*)fs);
}

static inline void *null_rdfsbase(void)
{
	return fakefs;
}

static inline void null_wrfsbase(void *fs)
{
	fakefs = fs;
}

void pin_to_core(int core)
{
	cpu_set_t c;                                                                           
	CPU_ZERO(&c);
	CPU_SET(core, &c);
	sched_setaffinity(0, sizeof(cpu_set_t), &c);
	sched_yield();                                                                         
}

void single_core_tests(int time, bool human, bool rdwr[2])
{
	void *tls_addr;
	uint64_t beg, end;
	uint64_t count;
	volatile bool stop;
	uint64_t tsc_freq = get_tsc_freq();

	/* Set up an alarm */
	void alarm_handler(int sig)
	{
		end = read_tsc();
		stop = true;
	}
	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_handler = &alarm_handler;
	act.sa_flags = 0;
	sigaction(SIGALRM, &act, NULL);

	void dump_results(char *prefix, bool human)
	{
		if (human) {
			printf("Single core %s fsbase:\n", prefix);
			printf("  Core %2d: ", 0);
			printf("    ops/s: %ld", count/tsc2msec(end - beg) * 1000);
			printf("    latency: %ldns, %ld cycles\n", tsc2nsec(end - beg)/count, (end - beg)/count);
		} else {
			printf("SC:%s:%ld:%ld:%ld:%ld\n", prefix, tsc_freq, beg, end, count);
		}
	}

	pin_to_core(0);

	// Read fs base 
	if (rdwr[0]) {
		count = 0;
		stop = false;
		alarm(time);
		beg = read_tsc();
		while (!stop) {
			tls_addr = rdfsbase();
			count++;
		}
		dump_results("RD", human);
	}

	// Write fs base 
	if (rdwr[1]) {
		count = 0;
		stop = false;
		alarm(time);
		beg = read_tsc();
		while (!stop) {
			wrfsbase(tls_addr);
			count++;
		}
		dump_results("WR", human);
	}
}

void multi_core_tests(int ncpus, int time, bool human, bool rdwr[2])
{
	struct tdata {
		void *tls_addr;
		uint64_t count;
		bool stop;
		bool done;
		uint64_t beg_time;
		uint64_t end_time;
	} __attribute__((aligned(ARCH_CL_SIZE)));

	static __thread int __tid;
	static int barrier;
	static void (*run_loop)(int);
	static char *run_prefix;
	static pthread_t thread;
	static uint64_t tsc_freq;
	static struct tdata *tdata;
	static bool test_done;
	tdata = aligned_alloc(ARCH_CL_SIZE, sizeof(struct tdata) * ncpus);
	tsc_freq = get_tsc_freq();

	void dump_results(char *prefix, bool human)
	{
		if (human) {
			printf("Multicore %s fsbase:\n", prefix);
			for (int i=0; i<ncpus; i++) {
				printf("  Core %2d: ", i);
				printf("    ops/s: %ld",
				         tdata[i].count/tsc2msec(tdata[i].end_time 
						                         - tdata[i].beg_time) * 1000);
				printf("    latency: %ldns\n",
				         tsc2nsec(tdata[i].end_time - tdata[i].beg_time)
						 / tdata[i].count);
			}
		} else {
			for (int i=0; i<ncpus; i++)
				printf("MC:%s:%d:%ld:%ld:%ld:%ld\n", prefix, i, tsc_freq,
				       tdata[i].beg_time, tdata[i].end_time, tdata[i].count);
		}
	}

	void alarm_handler(int sig)
	{
		/* Stop my thread right away (for consistency with sc test) */
		tdata[__tid].end_time = read_tsc();
		tdata[__tid].stop = true;

		/* Stop the other threads. */
		for (int i=0; i<ncpus; i++) {
			tdata[i].stop = true;
			if (i != __tid) { 
				while (!tdata[i].done)
					cpu_relax();
			}
		}
		dump_results(run_prefix, human);
		test_done = true;
	}

	void rdloop(int id) {
		/* Pull these out to optimize the loop. */
		volatile bool *stop = &tdata[id].stop;
		uint64_t *count = &tdata[id].count;
		void **tls_addr = tdata[id].tls_addr;

		tdata[id].beg_time = read_tsc();
		while (!*stop) {
			*tls_addr = rdfsbase();
			(*count)++;
		}
		tdata[id].end_time = read_tsc();
		tdata[id].done = true;
	}

	void wrloop(int id) {
		/* Pull these out to optimize the loop. */
		volatile bool *stop = &tdata[id].stop;
		uint64_t *count = &tdata[id].count;
		void **tls_addr = tdata[id].tls_addr;

		tdata[id].beg_time = read_tsc();
		while (!*stop) {
			wrfsbase(*tls_addr);
			(*count)++;
		}
		tdata[id].end_time = read_tsc();
		tdata[id].done = true;
	}

	void *thread_handler(void *arg)
	{
		int id = (int)(long)arg;
		__tid = id;

		/* Pin to our core */
		if (id != 0) {
			tdata[id].tls_addr = inst_rdfsbase();
			pin_to_core(id);
			__sync_fetch_and_add(&barrier, 1);
		}

		/* Checkin and barrier waiting for all threads to come up. */
		while (barrier < ncpus)
			cmb();

		/* Run the loop. */
		run_loop(id);
	}

	void run_test(char *prefix, int time, void (*test_func)(int))
	{
		/* Set up the globals for the test */
		run_prefix = prefix;
		run_loop = test_func;
		barrier = 0;
		test_done = false;
		for (int i=0; i<ncpus; i++) {
			tdata[i].tls_addr = 0;
			tdata[i].count = 0;
			tdata[i].stop = 0;
			tdata[i].done = 0;
			tdata[i].beg_time = 0;
			tdata[i].end_time= 0;
		}
		/* Spawn off 1 thread per core except for core 0. */
		for (int i=1; i<ncpus; i++)
			pthread_create(&thread, NULL, thread_handler, (void*)(long)i);
		
		/* Pin myself to core 0. */
		pin_to_core(0);

		/* Grab my tls_desc */
		tdata[0].tls_addr = inst_rdfsbase();

		/* Barrier waiting for all other threads to come up. */
		while (barrier < (ncpus - 1))
			pthread_yield();

		/* Set the alarm */
		alarm(time);

		/* Release the threads */
		barrier++;

		/* Become thread 0 */
		thread_handler(0);

		while(!test_done)
			cpu_relax();
	}

	/* Set up the alarm */
	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_handler = &alarm_handler;
	act.sa_flags = 0;
	sigaction(SIGALRM, &act, NULL);

	/* Run the tests */
	if (rdwr[0])
		run_test("RD", time, rdloop);
	if (rdwr[1])
		run_test("WR", time, wrloop);
}

void print_header(char *name, int ncpus, int time, bool human)
{
	if (human)
		printf("%s tests: ncpus: %d, duration: %ds\n", name, ncpus, time);
	else
		printf("%s:%d:%d\n", name, ncpus, time);
}

int main (int argc, char **argv)
{
	int ncpus = 12;
	int time = 10;
	bool human = true;
	bool tests[3] = {[0 ... 2] = true};
	bool scmc[2] = {[0 ... 1] = true};
	bool rdwr[2] = {[0 ... 1] = true};

	if (argc > 1)
		ncpus = strtol(argv[1], 0, 10);										  
	if (argc > 2)
		time = strtol(argv[2], 0, 10);									
	if (argc > 3) 
		human = strtol(argv[3], 0, 10);											  
	if (argc > 4) {
		tests[0] = argv[4][0] - '0';
		tests[1] = argv[4][1] - '0';
		tests[2] = argv[4][2] - '0';
	}
	if (argc > 5) {
		scmc[0] = argv[5][0] - '0';
		scmc[1] = argv[5][1] - '0';
	}
	if (argc > 6) {
		rdwr[0] = argv[6][0] - '0';
		rdwr[1] = argv[6][1] - '0';
	}

	if (tests[0]) {
		print_header("baseline", ncpus, time, human);
		rdfsbase = null_rdfsbase;
		wrfsbase = null_wrfsbase;
		if (scmc[0])
			single_core_tests(time, human, rdwr);
		if (scmc[1])
			multi_core_tests(ncpus, time, human, rdwr);
	}

	if (tests[1]) {
		print_header("rd/wrfsbase", ncpus, time, human);
		rdfsbase = inst_rdfsbase;
		wrfsbase = inst_wrfsbase;
		if (scmc[0])
			single_core_tests(time, human, rdwr);
		if (scmc[1])
			multi_core_tests(ncpus, time, human, rdwr);
	}

	if (tests[2]) {
		print_header("arch_prctl", ncpus, time, human);
		rdfsbase = sys_rdfsbase;
		wrfsbase = sys_wrfsbase;
		if (scmc[0])
			single_core_tests(time, human, rdwr);
		if (scmc[1])
			multi_core_tests(ncpus, time, human, rdwr);
	}
}

