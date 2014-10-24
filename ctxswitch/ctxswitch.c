/* Copyright (c) 2010-14 The Regents of the University of California
 * Barret Rhoden <brho@cs.berkeley.edu>
 * See LICENSE for details.
 *
 * Basic test for pthreading.  Spawns a bunch of threads that yield.
 *
 * To build on linux, cd into tests and run:
 * $ gcc -O2 -std=gnu99 -fno-stack-protector -g pthread_test.c -lpthread
 *
 * Make sure you run it with taskset to fix the number of vcores/cpus. */

#define _GNU_SOURCE /* for pth_yield on linux */

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "../libconfig.h"

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#define printf_safe(...) {}
//#define printf_safe(...) \
	pthread_mutex_lock(&lock); \
	printf(__VA_ARGS__); \
	pthread_mutex_unlock(&lock);

#define MAX_NR_TEST_THREADS 100000
int nr_yield_threads = 100;
long nr_yield_loops = 100;
int nr_vcores = 0;
int human_dump = 1;

pthread_t my_threads[MAX_NR_TEST_THREADS];
void *my_retvals[MAX_NR_TEST_THREADS];

volatile int barrier = 0;
bool ready = FALSE;

void *yield_thread(void* arg)
{	
	/* Wait til all threads are up and running */
	atomic_add(&barrier, 1);
	while (barrier != nr_yield_threads)
		pthread_yield();
	/* Wait til the main thread releases us */
	while (!ready)
		pthread_yield();
	/* Do the loop... */
	for (uint64_t i = 0; i < nr_yield_loops; i++) {
		pthread_yield();
	}
	return (void*)(long)(pthread_id());
}

int main(int argc, char** argv) 
{
	uint64_t start = 0;
	uint64_t end = 0;
	uint64_t usec_diff;
	uint64_t nr_ctx_switches;

	/* Parse the args */
	if (argc > 1)
		nr_yield_threads = strtol(argv[1], 0, 10);
	if (argc > 2)
		nr_yield_loops = strtol(argv[2], 0, 10);
	if (argc > 3)
		nr_vcores = strtol(argv[3], 0, 10);
	if (argc > 4)
		human_dump = strtol(argv[4], 0, 10);
	nr_yield_threads = MIN(nr_yield_threads, MAX_NR_TEST_THREADS);
	if (human_dump)
		printf("Making %d threads of %ld loops each, on %d vcore(s)\n",
		       nr_yield_threads, nr_yield_loops, nr_vcores);

	/* OS dependent prep work */
#ifndef USE_PTHREAD
	if (nr_vcores) {
		upthread_can_vcore_request(FALSE);
		upthread_can_vcore_steal(TRUE);
		upthread_short_circuit_yield(TRUE);
		upthread_set_num_vcores(nr_vcores, 1);
	}
#endif
	/* Create the threads */
	for (int i = 0; i < nr_yield_threads; i++) {
		printf_safe("[A] About to create thread %d\n", i);
		if (pthread_create(&my_threads[i], NULL, &yield_thread, NULL))
			perror("pth_create failed");
	}
	/* More OS dependent prep work */
#ifndef USE_PTHREAD
	vcore_request(nr_vcores - 1);
#endif

	/* Wait til all threads are up and running */
	while (barrier != nr_yield_threads)
		pthread_yield();
	/* Start the clock */
	start = read_tsc();
	/* Release the threads */
	ready = TRUE;
	/* Join on all the threads */
	for (int i = 0; i < nr_yield_threads; i++) {
		printf_safe("[A] About to join on thread %d(%p)\n", i, my_threads[i]);
		pthread_join(my_threads[i], &my_retvals[i]);
		printf_safe("[A] Successfully joined on thread %d (retval: %p)\n", i,
		            my_retvals[i]);
	}
	/* Stop the clock */
	end = read_tsc();

	/* Dump the results */
	nr_ctx_switches = nr_yield_threads * nr_yield_loops;
	usec_diff = tsc2usec(end - start);
	if (human_dump) {
		printf("Done: %d uthreads, %ld loops, %d vcores\n",
		       nr_yield_threads, nr_yield_loops, nr_vcores);
		printf("Nr context switches: %ld\n", nr_ctx_switches);
		printf("Time to run: %ld usec\n", usec_diff);
		if (nr_vcores == 1)
			printf("Context switch latency: %lld nsec\n",
			       (1000LL*usec_diff / nr_ctx_switches));
		printf("Context switches / sec: %lld\n\n",
		       (1000000LL*nr_ctx_switches / usec_diff));
	} else {
		printf("%d:%lu:%lu:%lu\n", nr_vcores, get_tsc_freq(), start, end);
	}
}
