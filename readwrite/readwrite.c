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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <parlib/timing.h>
#include <parlib/arch.h>
#include <parlib/atomic.h>
#include "../libconfig.h"

void pin_to_core(int core)
{
	cpu_set_t c;                                                                           
	CPU_ZERO(&c);
	CPU_SET(core, &c);
	sched_setaffinity(0, sizeof(cpu_set_t), &c);
	sched_yield();                                                                         
}

void multi_core_tests(int nthreads, int duration, bool human)
{
	struct tdata {
		uint64_t tsc_freq;
		uint64_t beg_time;
		uint64_t end_time;
		atomic_t start;
		bool stop;
		bool done;
		uint64_t count;
	} __attribute__((aligned(ARCH_CL_SIZE)));

	static int barrier;
	static void (*run_loop)(int, int, int);
	static char *run_prefix;
	static pthread_t thread;
	static struct tdata *tdata;
	static bool test_done;
	tdata = aligned_alloc(PGSIZE, sizeof(struct tdata) * nthreads);

	void dump_results(char *prefix, bool human)
	{
		if (human) {
			uint64_t tcount = 0;
			uint64_t max_end_time = 0;
			uint64_t min_beg_time = LONG_MAX;
			printf("Multicore %s test:\n", prefix);
			for (int i=0; i<nthreads; i++) {
				printf("  Thread %2d: ", i);
				printf("    ops/s: %ld",
				         tdata[i].count/tsc2msec(tdata[i].end_time 
						                         - tdata[i].beg_time) * 1000);
				printf("    latency: %ldns\n",
				         tsc2nsec(tdata[i].end_time - tdata[i].beg_time)
						 / tdata[i].count);
				tcount += tdata[i].count;
				max_end_time = MAX(max_end_time, tdata[i].end_time);
				min_beg_time = MIN(min_beg_time, tdata[i].beg_time);
			}
			printf("  Total  : ");
			printf("    ops/s: %ld",
			         tcount/tsc2msec(max_end_time - min_beg_time) * 1000);
			printf("   latency: %ldns\n",
			         tsc2nsec(max_end_time - min_beg_time)/tcount);
		} else {
			for (int i=0; i<nthreads; i++)
				printf("%d:%ld:%ld:%ld:%ld\n", i, tdata[i].tsc_freq,
				       tdata[i].beg_time, tdata[i].end_time, tdata[i].count);
		}
	}

	void alarm_handler(int sig)
	{
		tdata[0].stop = true;
	}

	void yieldloop(int id, int fdr, int fdw) {
		/* Pull these out to optimize the loop. */
		volatile bool *stop = &tdata[id].stop;
		uint64_t *count = &tdata[id].count;
		char buf[1000];

		/* Do the loop */
		tdata[id].beg_time = read_tsc();
		while (!*stop) {
			if (read(fdr, buf, 1000) == -1)
				perror("read");
			if (lseek(fdr, 0, SEEK_SET) == -1)
				perror("lseek");
			if (write(fdw, buf, 1000) == -1)
				perror("write");
			(*count)++;
		}
		tdata[id].end_time = read_tsc();
		tdata[id].done = true;
	}

	void *thread_handler(void *arg)
	{
		int id = (int)(long)arg;
		int fdr = open("readwrite.c", O_RDONLY);
		int fdw = open("/dev/null", O_WRONLY);

		/* When id == 0, we are coming in from the main thread, so skip all
		 * this work that we will have already done. */
		if (id != 0) {
			/* Get this core's tsc_frequency. */
			tdata[id].tsc_freq = get_tsc_freq();
			/* Up the barrier. */
			__sync_fetch_and_add(&barrier, 1);
			/* Checkin and barrier waiting for all cpus to come up. */
			while (barrier < nthreads)
				pthread_yield();
		} else {
			/* Set the alarm */
			alarm(duration);
			/* Release the threads */
			barrier++;
		}

		/* Run the loop. */
		run_loop(id, fdr, fdw);
	}

	void run_test(char *prefix, int duration, void (*test_func)(int, int, int))
	{
		/* Set up the globals for the test */
		run_prefix = prefix;
		run_loop = test_func;
		barrier = 0;
		test_done = false;
		for (int i=0; i<nthreads; i++) {
			tdata[i].tsc_freq = 0;
			tdata[i].beg_time = 0;
			tdata[i].end_time = 0;
			tdata[i].start = ATOMIC_INITIALIZER(0);
			tdata[i].stop = 0;
			tdata[i].done = 0;
			tdata[i].count = 0;
		}

#ifndef USE_PTHREAD
		upthread_can_vcore_request(TRUE);
		upthread_can_vcore_steal(TRUE);
		upthread_short_circuit_yield(TRUE);
#endif

		/* Set myself up similar to other threads. */
		pin_to_core(0);
		atomic_set(&tdata[0].start, 1);
		tdata[0].tsc_freq = get_tsc_freq();

		/* Spawn off 1 thread per core except for core 0. */
		for (int i = 1; i < nthreads; i++)
			pthread_create(&thread, NULL, thread_handler, (void*)(long)i);
		
		/* Barrier waiting for all other threads to come up. */
		while (barrier < (nthreads - 1))
			pthread_yield();

		/* Become thread 0 */
		thread_handler(0);

		/* When we get here, the alarm went off and we need to conclude the
		 * test. Stop the other threads. */
		for (int i=1; i<nthreads; i++)
			tdata[i].stop = true;
		for (int i=1; i<nthreads; i++)
			while (!tdata[i].done)
				cpu_relax();
		dump_results(run_prefix, human);
	}

	/* Set up the alarm */
	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_handler = &alarm_handler;
	act.sa_flags = 0;
	sigaction(SIGALRM, &act, NULL);

	/* Run the tests */
	run_test("CTXSWITCH", duration, yieldloop);
}

void print_header(char *name, int nthreads, int duration, bool human)
{
	if (human)
		printf("%s tests: nthreads: %d, duration: %ds\n",
		       name, nthreads, duration);
	else
		printf("%s:%d:%d\n", name, nthreads, duration);
}

int main (int argc, char **argv)
{
	int nthreads = 12;
	int duration = 10;
	bool human = true;

	if (argc > 1)
		nthreads = strtol(argv[1], 0, 10);
	if (argc > 2)
		duration = strtol(argv[2], 0, 10);
	if (argc > 3)
		human = strtol(argv[3], 0, 10);

	print_header("ctxswitch", nthreads, duration, human);
	multi_core_tests(nthreads, duration, human);
}


