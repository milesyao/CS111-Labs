
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
#include <string.h>
#include "SortedList.h"

#define ITER 'i'
#define THREAD 't'
#define YIELD 'y'
#define SYNC 's'
#define LISTS 'l'
#define BILLION 1000000000L

#define handle_error_en(en, msg) \
        do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

typedef struct thread_info {
    pthread_t thread_id;
    char** elemKeys;
} thread_info_t;

typedef struct list_info {
    SortedList_t *list;
    int list_id;
    pthread_mutex_t test_mutex;
    volatile int test_lock;
} list_info_t;

typedef void (*func_insert)(list_info_t list_info, SortedListElement_t *element);
typedef int (*func_delete)(list_info_t list_info, const char *key);
typedef int (*func_length)(list_info_t* list_collection);

int opt_yield;
int niters;
char sync_option;
// pthread_mutex_t test_mutex;
// volatile static int test_lock;
func_insert f_insert;
func_delete f_delete;
// func_lookup f_lookup;
func_length f_length;
// SortedList_t *list_test;
list_info_t* list_collection;
int nlists;
//remedy for struct failure
pthread_mutex_t* mutexes;
int* volatile test_locks;

char *randstring(size_t length);
unsigned long hash(const char *str);

void SortedList_insert0(list_info_t list_info, SortedListElement_t *element);
void SortedList_insert1(list_info_t list_info, SortedListElement_t *element);
void SortedList_insert2(list_info_t list_info, SortedListElement_t *element);
int SortedList_delete0(list_info_t list_info, const char *key);
int SortedList_delete1(list_info_t list_info, const char *key);
int SortedList_delete2(list_info_t list_info, const char *key);
int SortedList_length0(list_info_t* list_collection);
int SortedList_length1(list_info_t* list_collection);
int SortedList_length2(list_info_t* list_collection);

static void *thread_start(void *arg);


int main (int argc, char **argv)
{
    int nthreads = 0;
    int tnum;
    int c;
    int randstring_len = 10;
    nlists = 1;
    // list_test = malloc(sizeof(SortedList_t));
    //list and synchornization objects initialization
    opt_yield = 0;
    // test_lock = 0;
    niters = 0;
    // pthread_mutex_init(&test_mutex, NULL);
    thread_info_t *threads;
    sync_option = '0';
    f_insert = SortedList_insert0;
    f_delete = SortedList_delete0;
    // f_lookup = SortedList_lookup;
    f_length = SortedList_length0;
    while (1)
    {
        static struct option long_options[] =
        {
            /* These options set a flag. */
            {"iter",required_argument, 0,'i'},
            {"threads",required_argument, 0,'t'},
            {"yield", required_argument, 0,'y'},
            {"sync", required_argument, 0, 's'},
            {"lists", required_argument, 0, 'l'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;
        
        c = getopt_long(argc, argv, "",
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
                if(strcmp(optarg, "i") == 0) {
                    opt_yield = 0x01;
                } else if(strcmp(optarg, "d") == 0) {
                    opt_yield = 0x02;
                } else if(strcmp(optarg, "is") == 0) {
                    opt_yield = 0x05;
                } else if(strcmp(optarg, "ds") == 0) {
                    opt_yield = 0x06;
                }
                break;
            case SYNC:
                sync_option = optarg[0];
                break;
            case LISTS:
                nlists = atoi(optarg);
                break;
            case '?':
                 /* getopt_long already printed an error message. */
                break;
            default:
                abort ();
        }
    }

    // printf("here: niters: %d; nthreads: %d; opt_yield: %d; sync_option: %c; nlists: %d\n", niters, nthreads, opt_yield ,sync_option, nlists);
    //list and synchronizaiton object initialization
    list_collection = (list_info_t*) malloc(sizeof(list_info_t) * nlists);

    mutexes = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t) * nlists);
    test_locks = (int*)malloc(sizeof(int) * nlists);

    int lnum;
    for(lnum = 0; lnum < nlists; lnum++) {
        list_collection[lnum].list = (SortedList_t*)malloc(sizeof(SortedList_t));
        list_collection[lnum].list->prev = NULL;
        list_collection[lnum].list->next = NULL;
        list_collection[lnum].list->key = NULL;
        pthread_mutex_init(&list_collection[lnum].test_mutex, NULL);
        pthread_mutex_init(&mutexes[lnum], NULL);
        list_collection[lnum].test_lock = 0;
        test_locks[lnum] = 0;
        list_collection[lnum].list_id = lnum;

    }

    if(sync_option == 'm') {
        f_insert = SortedList_insert1;
        f_delete = SortedList_delete1;
        // f_lookup = SortedList_lookup1;
        f_length = SortedList_length1;
    } else if(sync_option == 's') {
        f_insert = SortedList_insert2;
        f_delete = SortedList_delete2;
        // f_lookup = SortedList_lookup2;
        f_length = SortedList_length2;
    }
    threads = (thread_info_t*)malloc(sizeof(thread_info_t) * nthreads);
    int i;
    for(tnum = 0; tnum < nthreads; tnum++) {
        threads[tnum].elemKeys = (char**)malloc(sizeof(char*) * niters);
        for(i = 0; i < niters; i++) {
            threads[tnum].elemKeys[i] = randstring(randstring_len);
        }
    }
    // struct timespec begin;
    struct timespec begin, end;
    clock_gettime(CLOCK_MONOTONIC, &begin);
    // struct timespec end;
    for(tnum = 0; tnum < nthreads; tnum++) {
        int ret = pthread_create(&threads[tnum].thread_id, NULL, thread_start, &threads[tnum]);
        if(ret != 0) {
            handle_error_en(ret, "pthread_create");
        }
    }

    for(tnum = 0; tnum < nthreads; tnum++) {
        int ret = pthread_join(threads[tnum].thread_id, NULL);
        if (ret != 0) handle_error_en(ret, "pthread_join");
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    //check length
    int final_len = SortedList_length0(list_collection);
    if(final_len != 0) {
        if(final_len < 0) fprintf(stderr,"ERROR: data structure inconsistent!\n");
        else fprintf(stderr,"ERROR: final length %d\n", final_len);
    }

    long ttime = BILLION * (end.tv_sec - begin.tv_sec) + end.tv_nsec - begin.tv_nsec;
    long long nop = nthreads * niters * 2 * niters / 2;
    long perop = ttime / nop;
    printf("%d threads X %d iterations X (ins + lookup/del) X (%d/2 avg len) = %lli operations\n", nthreads, niters, niters, nop);
    printf("elapsed time: %li ns\n", ttime);
    printf("per operation: %li ns\n", perop);
    if(final_len != 0) exit (-1);
    exit(0);
}

/* http://codereview.stackexchange.com/questions/29198/random-string-generator-in-c
 * Random string(key) generator
 */
char *randstring(size_t length) {
    
    static char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.-#'?!";
    char *randomString = NULL;
    int n;
    if (length) {
        randomString = malloc(sizeof(char) * (length +1));
        
        if (randomString) {
            for (n = 0;n < length;n++) {
                int key = rand() % (int)(sizeof(charset) -1);
                randomString[n] = charset[key];
            }
            
            randomString[length] = '\0';
        }
    }
    
    return randomString;
}

/* http://stackoverflow.com/questions/7666509/hash-function-for-string
 * Hash function for string
 */
unsigned long
hash(const char *str)
{
    // printf("wocao\n");
    unsigned long hash = 5381;
    int c;
    while (*str != '\0') {
        // printf("nima\n");
        c = *str;
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        str++;
    }
    // printf("hehe\n");
    return hash;
}

void SortedList_insert0(list_info_t list_info, SortedListElement_t *element) {
    SortedList_insert(list_info.list, element);
}

void SortedList_insert1(list_info_t list_info, SortedListElement_t *element) {
    // pthread_mutex_lock(&list_info.test_mutex);
    pthread_mutex_lock(&mutexes[list_info.list_id]);
    SortedList_insert(list_info.list, element);
    // pthread_mutex_unlock(&list_info.test_mutex);
    pthread_mutex_unlock(&mutexes[list_info.list_id]);
}

void SortedList_insert2(list_info_t list_info, SortedListElement_t *element) {
    // while(__sync_lock_test_and_set(&list_info.test_lock,1));
    while(__sync_lock_test_and_set(&test_locks[list_info.list_id],1));
    SortedList_insert(list_info.list, element);
    // __sync_lock_release(&list_info.test_lock);
    __sync_lock_release(&test_locks[list_info.list_id]);
}

int SortedList_delete0(list_info_t list_info, const char *key) {
    SortedListElement_t *found = SortedList_lookup(list_info.list, key);
    int retval = SortedList_delete(found);   
    return retval;
}

int SortedList_delete1(list_info_t list_info, const char *key) {
    // pthread_mutex_lock(&list_info.test_mutex);
    pthread_mutex_lock(&mutexes[list_info.list_id]);
    SortedListElement_t *found = SortedList_lookup(list_info.list, key);
    int retval = SortedList_delete(found);
    // pthread_mutex_unlock(&list_info.test_mutex);  
    pthread_mutex_unlock(&mutexes[list_info.list_id]);
    return retval;
}

int SortedList_delete2(list_info_t list_info, const char *key) {
    // while(__sync_lock_test_and_set(&list_info.test_lock,1));
    while(__sync_lock_test_and_set(&test_locks[list_info.list_id],1));
    SortedListElement_t *found = SortedList_lookup(list_info.list, key);
    int retval = SortedList_delete(found);
    // __sync_lock_release(&list_info.test_lock); 
    __sync_lock_release(&test_locks[list_info.list_id]);
    return retval;
}

int SortedList_length0(list_info_t* list_collection) {
    int res = 0;
    int sub;
    int i;
    for(i = 0; i < nlists; i++) {
        sub = SortedList_length(list_collection[i].list);
        if(sub == -1) return -1;
        res += sub;
    }
    return res;
}

int SortedList_length1(list_info_t* list_collection) {
    int res = 0;
    int sub;
    int i;
    for(i = 0; i < nlists; i++) {
        // printf("hehe\n");
        // pthread_mutex_lock(&list_collection[i].test_mutex);
        pthread_mutex_lock(&mutexes[i]);
        sub = SortedList_length(list_collection[i].list);
        pthread_mutex_unlock(&mutexes[i]);
        if(sub == -1) return -1;
        res += sub;
    }
    return res;
}

int SortedList_length2(list_info_t* list_collection) {
    int res = 0;
    int sub;
    int i;
    for(i = 0; i < nlists; i++) {
        while(__sync_lock_test_and_set(&test_locks[i],1));
        sub = SortedList_length(list_collection[i].list);
        __sync_lock_release(&test_locks[i]);
        if(sub == -1) return -1;
        res += sub;
    }
    return res;
}

static void *
thread_start(void *arg) {
    thread_info_t *tinfo = arg;
    int i;
    int clist;
    // printf("******1*******\n");
    for(i = 0; i < niters; i++) {
        // printf("******2*******\n");
        SortedListElement_t *newelement = malloc(sizeof(SortedList_t));
        newelement->next = NULL;
        newelement->prev = NULL;
        newelement->key = tinfo->elemKeys[i];
        clist = (int)(hash(newelement->key) % (unsigned long)nlists);
        // printf("******3*******\n");
        f_insert(list_collection[clist], newelement);
        // printf("******4*******\n");
    }
    // printf("******2*******\n");
    f_length(list_collection);
    for(i = 0; i < niters; i++) {
        clist = (int)(hash(tinfo->elemKeys[i]) % (unsigned long)nlists);
        // printf("******begin delete*******\n");
        f_delete(list_collection[clist], tinfo->elemKeys[i]);
        // printf("******after delete*******\n");
    }
    // printf("******3*******\n");
    return NULL;
}
