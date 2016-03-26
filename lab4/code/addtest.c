
//  Created by cnyao on 3/5/16.
//  Copyright © 2016 cnyao. All rights reserved.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>

#define ITER 'i'
#define THREAD 't'
#define YIELD 'y'
#define SYNC 's'
#define BILLION 1000000000L

#define handle_error_en(en, msg) \
        do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

typedef void (*add_func)(long long *pointer, long long value);

typedef struct thread_info {
    long long *counter;
    int iter;
    add_func add_spec;
} thread_info_t;

int opt_yield;
pthread_mutex_t test_mutex;
volatile static int test_lock;

static void 
add(long long *pointer, long long value) {
    long long sum = *pointer + value;
    if (opt_yield)
        pthread_yield();
    *pointer = sum;
}

static void 
add1(long long *pointer, long long value) {
    pthread_mutex_lock(&test_mutex);
    long long sum = *pointer + value;
    if (opt_yield)
        pthread_yield();
    *pointer = sum;
    pthread_mutex_unlock(&test_mutex);
}

static void 
add2(long long *pointer, long long value) {
    while(__sync_lock_test_and_set(&test_lock,1));
    long long sum = *pointer + value;
    if (opt_yield)
        pthread_yield();
    *pointer = sum;
    __sync_lock_release(&test_lock);
}

static void 
add3(long long *pointer, long long value) {
    long long sum;
    long long orig;
    do {
        orig = *pointer;
        if (opt_yield)
            pthread_yield();
        sum = orig + value;
    } while(__sync_val_compare_and_swap(pointer, orig, sum) != orig);
}

static void *
thread_start(void *arg) {
    thread_info_t *tinfo = arg;
    add_func add_spec = tinfo->add_spec;
    int i;
    for(i = 0; i < tinfo->iter; i++) {
        add_spec(tinfo->counter, 1);
    }

    for(i = 0; i < tinfo->iter; i++) {
        add_spec(tinfo->counter, -1);
    }

    return NULL;
}


int main (int argc, char **argv)
{
    int nthreads = 0;
    int niters = 0;
    int tnum;
    int c;
    long long counter = 0;
    opt_yield = 0;
    test_lock = 0;
    pthread_mutex_init(&test_mutex, NULL);
    pthread_t *threads;
    char sync_option = '0';
    while (1)
    {
        static struct option long_options[] =
        {
            /* These options set a flag. */
            {"iter",required_argument, 0,'i'},
            {"threads",required_argument, 0,'t'},
            {"yield", required_argument, 0,'y'},
            {"sync", required_argument, 0, 's'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;
        
        c = getopt_long (argc, argv, "",
                         long_options, &option_index);
    
        /* Detect the end of the options. */
        if (c == -1)
            break;
        switch (c)
        {
            case 0:
                break;
            case ITER: {
                niters = atoi(optarg);
                break;
            }
            case THREAD:
                nthreads = atoi(optarg);
                break;
            case YIELD:
                opt_yield = atoi(optarg);
                break;
            case SYNC:
                sync_option = optarg[0];
                break;
            case '?':
                 /* getopt_long already printed an error message. */
                break;
            default:
                abort ();
        }
    }
    threads = (pthread_t*)malloc(sizeof(pthread_t) * nthreads);
    thread_info_t arg;
    arg.counter = &counter;
    arg.iter = niters;
    if(sync_option == 'm') {
        arg.add_spec = add1;
    } else if(sync_option == 's') {
        arg.add_spec = add2;
    } else if(sync_option == 'c') {
        arg.add_spec = add3;
    } else {
        arg.add_spec = add;
    }
    struct timespec begin, end;
    clock_gettime(CLOCK_MONOTONIC, &begin);
    for(tnum = 0; tnum < nthreads; tnum++) {
        int ret = pthread_create(&threads[tnum], NULL, thread_start, &arg);
        if(ret != 0) {
            handle_error_en(ret, "pthread_create");
        }
    }

    for(tnum = 0; tnum < nthreads; tnum++) {
        int ret = pthread_join(threads[tnum], NULL);
        if (ret != 0) handle_error_en(ret, "pthread_join");
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    long ttime = BILLION * (end.tv_sec - begin.tv_sec) + end.tv_nsec - begin.tv_nsec;
    long nop = nthreads * niters * 2;
    long perop = ttime / nop;
    printf("%d threads X %d iterations X (add + substract) = %li operations\n", nthreads, niters, nop);
    if(counter != 0) {
        fprintf(stderr,"ERROR: final count = %lli\n", counter);
    }
    printf("elapsed time: %li ns\n", ttime);
    printf("per operation: %li ns\n", perop);
    if(counter != 0) exit (-1);
    exit(0);
}


