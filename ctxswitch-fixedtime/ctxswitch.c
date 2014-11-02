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
#include "../libconfig.h"

void pin_to_core(int core)
{
	cpu_set_t c;                                                                           
	CPU_ZERO(&c);
	CPU_SET(core, &c);
	sched_setaffinity(0, sizeof(cpu_set_t), &c);
	sched_yield();                                                                         
}

void multi_core_tests(int ncpus, int tpc, int time, bool human)
{
	struct tdata {
		uint64_t count;
		bool stop;
		bool done;
		uint64_t beg_time;
		uint64_t end_time;
		uint64_t tsc_freq;
	} __attribute__((aligned(ARCH_CL_SIZE)));

	static int barrier;
	static void (*run_loop)(int);
	static char *run_prefix;
	static pthread_t thread;
	static struct tdata *tdata;
	static bool test_done;
	tdata = aligned_alloc(PGSIZE, sizeof(struct tdata) * ncpus);

	void dump_results(char *prefix, bool human)
	{
		if (human) {
			uint64_t tcount = 0;
			uint64_t max_end_time = 0;
			uint64_t min_beg_time = LONG_MAX;
			printf("Multicore %s test:\n", prefix);
			for (int i=0; i<ncpus; i++) {
				printf("  Core %2d: ", i);
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
			for (int i=0; i<ncpus; i++)
				printf("%d:%ld:%ld:%ld:%ld\n", i, tdata[i].tsc_freq,
				       tdata[i].beg_time, tdata[i].end_time, tdata[i].count);
		}
	}

	void alarm_handler(int sig)
	{
		tdata[0].stop = true;
	}

	void yieldloop(int id) {
		/* Pull these out to optimize the loop. */
		volatile bool *stop = &tdata[id].stop;
		uint64_t *count = &tdata[id].count;

		/* If we are the first oe in on this core, set the beg time. */
		if (!tdata[id].beg_time) {
			tdata[id].beg_time = read_tsc();
		}

		/* Do the loop */
		while (!*stop) {
			pthread_yield();
			(*count)++;
		}

		/* If we are the first one out on this core, set the end time. */
		if (!tdata[id].end_time) {
			tdata[id].end_time = read_tsc();
			tdata[id].done = true;
		}
	}

	void *thread_handler(void *arg)
	{
		int id = (int)(long)arg;

		/* Pin ourselves to our core */
		pin_to_core(id);

		/* If we aren't one of the threads on core 0. */
		if (id != 0) {
			/* Get this core's tsc_frequency if not already set. */
			if (!tdata[id].tsc_freq)
				tdata[id].tsc_freq = get_tsc_freq();
			/* Up the barrier. */
			__sync_fetch_and_add(&barrier, 1);
		}

		/* Checkin and barrier waiting for all cpus to come up. */
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
			tdata[i].count = 0;
			tdata[i].stop = 0;
			tdata[i].done = 0;
			tdata[i].beg_time = 0;
			tdata[i].end_time= 0;
			tdata[i].tsc_freq = 0;
		}

#ifndef USE_PTHREAD
		upthread_can_vcore_request(FALSE);
		upthread_can_vcore_steal(FALSE);
		upthread_short_circuit_yield(FALSE);
		upthread_set_num_vcores(ncpus);
#endif

		/* Spawn off 1 thread per core except for core 0. */
		int i = 0, j = 1;
		for (i; i< tpc; i++) {
			for (j; j < ncpus; j++)
				pthread_create(&thread, NULL, thread_handler, (void*)(long)j);
			j=0;
		}
		
		/* Wait until now to do our vcore request */
		vcore_request(ncpus - 1);

		/* Pin myself to core 0. */
		pin_to_core(0);

		/* Get core 0's tsc frequency. */
		tdata[0].tsc_freq = get_tsc_freq();

		/* Barrier waiting for all other threads to come up. */
		while (barrier < (ncpus - 1))
			cmb();

		/* Set the alarm */
		alarm(time);

		/* Release the threads */
		barrier++;

		/* Become thread 0 */
		thread_handler(0);

		/* When we get here, the alarm went off and we need to conclude the
		 * test. Stop the other threads. */
		for (int i=1; i<ncpus; i++)
			tdata[i].stop = true;
		for (int i=1; i<ncpus; i++)
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
	run_test("CTXSWITCH", time, yieldloop);
}

void print_header(char *name, int ncpus, int tpc, int time, bool human)
{
	if (human)
		printf("%s tests: ncpus: %d, tpc: %d, duration: %ds\n",
		       name, ncpus, tpc, time);
	else
		printf("%s:%d:%d:%d\n", name, ncpus, tpc, time);
}

int main (int argc, char **argv)
{
	int ncpus = 12;
	int tpc = 1;
	int time = 10;
	bool human = true;

	if (argc > 1)
		ncpus = strtol(argv[1], 0, 10);
	if (argc > 2)
		tpc = strtol(argv[2], 0, 10);
	if (argc > 3)
		time = strtol(argv[3], 0, 10);
	if (argc > 4)
		human = strtol(argv[4], 0, 10);

	print_header("ctxswitch", ncpus, tpc, time, human);
	multi_core_tests(ncpus, tpc, time, human);
}


