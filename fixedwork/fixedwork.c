/*
 * Copyright (c) 2014 The Regents of the University of California
 * Kevin Klues <klueska@cs.berkeley.edu>
 *
 * This file is part of Parlib.
 * 
 * Parlib is free software: you can redistribute it and/or modify
 * it under the terms of the Lesser GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Parlib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * Lesser GNU General Public License for more details.
 * 
 * See COPYING.LESSER for details on the GNU Lesser General Public License.
 * See COPYING for details on the GNU General Public License.
 */

#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <parlib/vcore.h>
#include <parlib/timing.h>
#include <parlib/arch.h>

#ifndef PREEMPT_PERIOD
#define PREEMPT_PERIOD 10000
#endif

#ifdef USE_PTHREAD
	#include <pthread.h>
	#define pthread_yield()
	#define test_prep() \
		calibrate_all_tscs();
	void calibrate_all_tscs();
#elif USE_UPTHREAD | USE_UPTHREAD_JUGGLE
	#if USE_UPTHREAD
		#include <upthread/upthread.h>
		#define pthread_yield upthread_yield
		#define test_prep()
	#elif USE_UPTHREAD_JUGGLE
		#include <upthread-juggle/upthread.h>
		#define pthread_yield()
		#define test_prep() \
			upthread_set_sched_period(preempt_period);
		#define printf(...) \
		{ \
			upthread_disable_interrupts(); \
			printf(__VA_ARGS__); \
			upthread_enable_interrupts(); \
		}
	#endif
	#define pthread_t upthread_t
	#define pthread_create upthread_create
	#define pthread_join upthread_join
	#define calibrate_all_tscs() 
#endif

/* Modifiable via command line */
int nr_threads = 1;
int nr_loops = 1;
int fake_work = 0;
int human_dump = 1;
int preempt_period = PREEMPT_PERIOD;

struct stats {
	uint64_t create_time;
	uint64_t start_time;
	uint64_t end_time;
	uint64_t join_time;
};

/* Variables for the thread handles and gathering statistics */
pthread_t *thandles = NULL;
struct stats *tstats = NULL;

static void parse_args(int argc, char **argv)
{
	if (argc > 1)
		nr_threads = strtol(argv[1], 0, 10);
	if (argc > 2)
		nr_loops = strtol(argv[2], 0, 10);
	if (argc > 3)
		fake_work = strtol(argv[3], 0, 10);
	if (argc > 4)
		human_dump = strtol(argv[4], 0, 10);
	if (argc > 5)
		preempt_period = strtol(argv[5], 0, 10);
}

static void dump_stats(int i, struct stats *stats, int human)
{
	if (!human) {
		printf("%d:%ld:%ld:%ld:%ld\n", i, stats->create_time,
		       stats->start_time, stats->end_time, stats->join_time);
	} else {
		uint64_t start_time = tsc2msec(stats->start_time - stats->create_time);
		uint64_t end_time = tsc2msec(stats->end_time - stats->create_time);
		uint64_t compute_time = tsc2msec(stats->end_time - stats->start_time);
		uint64_t run_time = tsc2msec(stats->join_time - stats->create_time);
		printf("id:           %d\n", i);
		printf("start_time:   %ldms\n", start_time);
		printf("end_time:     %ldms\n", end_time);
		printf("compute_time: %ldms\n", compute_time);
		printf("run_time:     %ldms\n", run_time);
		printf("\n");
	}
}

static void *__thread_wrapper(void *arg)
{
	int id = (int)(long)arg;

	/* Let the games begin! */
	tstats[id].start_time = read_tsc();
	for (int i = 0; i < nr_loops; i++) {
		for (int j=0; j<fake_work; j++)
			cmb();
		#ifdef WITH_YIELD
		pthread_yield();
		#endif
	}
	tstats[id].end_time = read_tsc();
}

int main(int argc, char **argv)
{
	parse_args(argc, argv);
	thandles = calloc(sizeof(pthread_t), nr_threads);
	tstats = calloc(sizeof(struct stats), nr_threads);

	/* Do any library specific test prep */
	test_prep();

	/* Let the games begin! */
	uint64_t prog_start = read_tsc();
	for (int i=0; i<nr_threads; i++) {
		tstats[i].create_time = read_tsc();
		pthread_create(&thandles[i], NULL, __thread_wrapper, (void*)(long)i);
	}
	for (int i=0; i<nr_threads; i++) {
		pthread_join(thandles[i], NULL);
		tstats[i].join_time = read_tsc();
	}
	uint64_t prog_end = read_tsc();

	/* Dump the results */
	if (!human_dump)
		printf("%ld:%ld:%ld\n", get_tsc_freq(), prog_start, prog_end);
	for (int i=0; i<nr_threads; i++) {
		dump_stats(i, &tstats[i], human_dump);
	}
	if (human_dump)
		printf("Program run: %ldms\n", tsc2msec(prog_end - prog_start));
	return 0;
}

