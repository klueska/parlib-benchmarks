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

#include <stdbool.h>
#include <sys/param.h> /* MIN/MAX */
#include <parlib/arch.h>
#include <parlib/timing.h>

#define TRUE true
#define FALSE false

#ifndef PREEMPT_PERIOD
#define PREEMPT_PERIOD 10000
#endif

#ifdef USE_PTHREAD
	void calibrate_all_tscs();
	#include <pthread.h>
	#define test_prep() \
		calibrate_all_tscs();
    #define pthread_id() pthread_self()
	#define vcore_id() (-1)
	#define udelay usleep
#elif USE_UPTHREAD | USE_UPTHREAD_JUGGLE
	#if USE_UPTHREAD
		#include <upthread/upthread.h>
		#define test_prep()
	#elif USE_UPTHREAD_JUGGLE
		#include <upthread-juggle/upthread.h>
		#define test_prep() \
			upthread_set_sched_period(preempt_period);
		#define printf(...) \
		{ \
			upthread_disable_interrupts(); \
			printf(__VA_ARGS__); \
			upthread_enable_interrupts(); \
		}
	#endif
	#define pthread_yield() upthread_yield
	#define pthread_t upthread_t
	#define pthread_create upthread_create
	#define pthread_join upthread_join
	#define calibrate_all_tscs() 
    #define pthread_id() (upthread_self()->id)
#endif
