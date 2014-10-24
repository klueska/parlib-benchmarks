/* Copyright (c) 2010-14 The Regents of the University of California
 * Kevin Klues <klueska@cs.berkeley.edu>
 * See LICENSE for details. */

#define _GNU_SOURCE /* for pth_yield on linux */

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "../libconfig.h"

/* Global vars */
int nr_vcores = 0;
int nr_threads_per_core = 1;
int test_duration = 10; /* In seconds. */
int human_dump = 1;

int nr_threads = 0;
uint64_t start_time = 0;
uint64_t stop_time = 0;
atomic_t stop_counting = ATOMIC_INITIALIZER(0);

/* Threads and their per thread data. We will allocate exactly one thread per
 * vcore for this experiment. */
struct pt_data {
	pthread_t thandle;
	volatile uint64_t yield_count;
} __attribute__((aligned(ARCH_CL_SIZE)));
struct pt_data *pt_data;
uint64_t *yield_count_copy;
#define thandle(i) (pt_data[i].thandle)
#define yield_count(i) (pt_data[i].yield_count)

/* Simple Barriers */
volatile int barrier = 0;
bool ready = FALSE;

void *yield_thread(void* arg)
{	
	uint64_t end_time;
	int vcoreid = (int)(long)arg;

	/* Wait til all threads are up and running */
	atomic_add(&barrier, 1);
	while (barrier != nr_threads)
		pthread_yield();

	/* Do the loop... */
	for (uint64_t i=0 ;; i++) {
		pthread_yield();
		yield_count(vcoreid)++;

		/* Check the loop counter and see if it's time to end the program.
		 * This is a little racy in terms of getting an exact count for the
		 * yields, but it's all in the noise for the benchmark. */
		end_time = read_tsc();
		if (end_time > stop_time) {
			while(!atomic_cas(&stop_counting, 0, 1));
			/* This break is for the for loop not the while above. Only
			 * one thread should ever set this variable and break out of
			 * the loop.  the rest should just sit here forever spinning
			 * and do no more yielding. */
			break;
		}
	}

	/* Aggregate results if we are the one to break the loop above... */

	/* Qick copy out all the yield counts as fast as possible */
	for (int i=0; i<nr_threads; i++)
		yield_count_copy[i] = yield_count(i);	

	/* Now aggregate them */
	uint64_t total_ctx_switches = 0;
	for (int i=0; i<nr_threads; i++)
		total_ctx_switches += yield_count_copy[i];

	/* Dump the results */
	if (human_dump) {
		uint64_t usec_diff = tsc2usec(end_time - start_time);
		printf("Done: %d vcores, %d threads\n", nr_vcores, nr_threads);
		printf("Nr context switches: %ld\n", total_ctx_switches);
		printf("Time to run: %ld usec\n", usec_diff);
		printf("Context switches / sec: %lld\n\n",
		       (1000000LL*total_ctx_switches / usec_diff));
	} else {
		printf("%d:%d:%lu:%lu:%lu:", nr_vcores, nr_threads,
		                             get_tsc_freq(), start_time, end_time);
		for (int i=0; i<nr_threads; i++) {
			printf("%ld", yield_count_copy[i]);
			if (i != nr_threads-1)
				printf(":");
			else
				printf("\n");
		}
	}
	exit(0);

	return (void*)(long)(pthread_id());
}

int main(int argc, char** argv) 
{
	/* Parse the args */
	if (argc > 1)
		test_duration = strtol(argv[1], 0, 10);
	if (argc > 2)
		nr_threads_per_core = strtol(argv[2], 0, 10);
	if (argc > 3)
		nr_vcores = strtol(argv[3], 0, 10);
	if (argc > 4)
		human_dump = strtol(argv[4], 0, 10);

	/* Adjust num vcores so we have alot of threads in our queue */
	nr_threads = nr_threads_per_core * nr_vcores;

	/* DUmp some info */
	if (human_dump)
		printf("Making %d threads for a %d second test, on %d vcore(s)\n",
		       nr_threads, test_duration, nr_vcores);

	/* Allocate our threads and per thread data. */
	pt_data = parlib_aligned_alloc(ARCH_CL_SIZE,
	              sizeof(struct pt_data) * nr_threads);
	yield_count_copy = malloc(sizeof(uint64_t) * nr_threads);
	for (int i=0; i<nr_threads; i++) {
		yield_count(i) = 0;
		yield_count_copy[i] = 0;
	}

#ifndef USE_PTHREAD
	if (nr_vcores) {
		upthread_can_vcore_request(FALSE);
		upthread_can_vcore_steal(FALSE);
		upthread_short_circuit_yield(FALSE);
		upthread_set_num_vcores(nr_vcores, 1);
		vcore_request(nr_vcores - 1);
	}
#endif

	/* Create all threads other than 0 */
	for (int i = 1; i < nr_threads; i++)
		pthread_create(&thandle(i), NULL, &yield_thread, (void*)(long)i);

	/* Wait til all but one thread are up and running */
	while (barrier != (nr_threads - 1))
		pthread_yield();

	/* Set some times. Do this in this order for better accuracy. */
	uint64_t test_ticks = (uint64_t)test_duration * get_tsc_freq();
	start_time = read_tsc();
	stop_time = test_ticks + start_time;

	/* Become thread 0 */
	yield_thread(0);
}

