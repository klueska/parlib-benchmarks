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

#define _GNU_SOURCE
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <parlib/vcore.h>
#include <parlib/timing.h>
#include <parlib/arch.h>
#include "../libconfig.h"

/* Modifiable via command line */
int nr_threads = 1;
int nr_loops = 1;
int fake_work = 0;
int human_dump = 1;
int preempt_period = PREEMPT_PERIOD;
static int barrier = 0;

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

static void dump_stats(int i, struct stats *stats, uint64_t prog_start,
                       uint64_t prog_end, int human)
{
	if (!human) {
		printf("%d:%ld:%ld:%ld:%ld\n", i, stats->create_time,
		       stats->start_time, stats->end_time, stats->join_time);
	} else {
		uint64_t start_time = tsc2msec(stats->start_time - stats->create_time);
		uint64_t end_time = tsc2msec(stats->end_time - stats->create_time);
		uint64_t compute_time = tsc2msec(stats->end_time - stats->start_time);
		uint64_t run_time = tsc2msec(stats->join_time - stats->create_time);
		uint64_t completion_time = tsc2msec(stats->end_time - prog_start);
		printf("id:              %d\n", i);
		printf("start_time:      %ldms\n", start_time);
		printf("end_time:        %ldms\n", end_time);
		printf("compute_time:    %ldms\n", compute_time);
		printf("run_time:        %ldms\n", run_time);
		printf("completion_time: %ldms\n", completion_time);
		printf("\n");
	}
}

static void *__thread_wrapper(void *arg)
{
	int id = (int)(long)arg;

	/* Up the barrier. */
	__sync_fetch_and_add(&barrier, 1);

	/* Checkin and barrier waiting for all cpus to come up. */
	while (barrier < nr_threads)
		pthread_yield();

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
	for (int i=1; i<nr_threads; i++) {
		tstats[i].create_time = read_tsc();
		pthread_create(&thandles[i], NULL, __thread_wrapper, (void*)(long)i);
	}

	/* Wait to start the measurement. */
	while (barrier < (nr_threads - 1))
		pthread_yield();
	uint64_t prog_start = read_tsc();

	/* Become thread 0 */
	tstats[0].create_time = read_tsc();
	__thread_wrapper(0);
	tstats[0].join_time = read_tsc();

	/* Join on all the remaining threads */
	for (int i=1; i<nr_threads; i++) {
		pthread_join(thandles[i], NULL);
		tstats[i].join_time = read_tsc();
	}
	uint64_t prog_end = read_tsc();

	/* Dump the results */
	if (!human_dump)
		printf("%ld:%ld:%ld\n", get_tsc_freq(), prog_start, prog_end);
	for (int i=0; i<nr_threads; i++) {
		dump_stats(i, &tstats[i], prog_start, prog_end, human_dump);
	}
	if (human_dump)
		printf("Program run: %ldms\n", tsc2msec(prog_end - prog_start));
	return 0;
}

