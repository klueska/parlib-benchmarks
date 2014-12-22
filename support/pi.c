/*
 *      pi.c
 *
 *      Computation of pi by a "montecarlo" method:
 *      Every thread computes a number of points in the unit square
 *      and returns the number of hits in the quarter circle
 *      pi is  #hits/#total * 4
 *      
 *      usage:
 *      pi 
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../libconfig.h"

#define NUM_THREADS 10000         /* number of worker threads */
#define NUM_POINTS  10000L        /* number of points */

long mypoints;                     /* number of points per thread */

void 
main(int argc, char *argv[])
{

   int        i;
   int        nthreads;                      /* number of threads */
   long       npoints;                       /* total number of points */
   pthread_t  thread_id[NUM_THREADS];        /* ids of all threads */
   long       totalhits = 0;                 /* total number of hits */
   double     pi;                            /* result */
   void *     myhits_p;
   void *     calc(void *);                  /* calculation */

   nthreads = NUM_THREADS;
   npoints = NUM_POINTS;
   mypoints = (long) ceil(npoints/nthreads);
   npoints = nthreads * mypoints;              /* correct total */
   
   /* start worker threads */
   for (i=0; i<nthreads; i++) {
     pthread_create(&thread_id[i], NULL, calc, NULL);
   }   

   /*  wait for worker threads and combine partial results */
   for (i=0; i<nthreads; i++) {
     pthread_join(thread_id[i], &myhits_p);
     totalhits +=  * (long *)myhits_p;
   }   
   
   /* compute final result */
   pi = totalhits/(double)npoints*4;
   printf("\nPI = %lf\n", pi);
}

void * calc(void *args)
{
   /* 
    * compute total random points in the unit square
    * and return the number of hits in the sector (x*x + y*y < 1)
    *
    * uses global mypoints
    */

   double  x, y;                     /* random coordinates */
   long *  hits_p;                   /* pointer to number of hits */
   int     seed;                     /* seed for random generator */
   long    i;
   
   /* get memory for return value */
   hits_p = (long *) calloc(1, sizeof(long));        /* initialized  to 0 */

   /* initialize random generator (otherwise all return the same result!) */
   seed = (int) pthread_id();
   for(i=0; i<mypoints; i++) {
      x = ((double) rand_r(&seed))/RAND_MAX;
      y = ((double) rand_r(&seed))/RAND_MAX;
      pthread_yield();
      
      if ( x*x + y*y <= 1.0 ) {
         (*hits_p)++;
      }
   }

   return(hits_p); 
}
