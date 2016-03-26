//  Created by cnyao on 3/6/16.
//  Copyright © 2016 cnyao. All rights reserved.
// I assume tail->next=NULL head->prev=NULL. 
#define _GNU_SOURCE
#include "SortedList.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
	if(element->key == NULL || list == NULL) return;
	SortedListElement_t *p = list;
	SortedListElement_t *n = list->next;
	while(n!=NULL) {
		if(n==NULL || strcmp(element->key, n->key) <= 0) {
			break;
		}
		p = n;
		// if (opt_yield & INSERT_YIELD) pthread_yield();
		if(n!= NULL) n = n->next;
	}
	element->prev = p;
	// if (opt_yield & INSERT_YIELD) pthread_yield();
	element->next = n;
	if (opt_yield & INSERT_YIELD) pthread_yield();
	if(p!= NULL) p->next = element;
	// if (opt_yield & INSERT_YIELD) pthread_yield();
	if(n!=NULL) n->prev = element;
	return;
}

int SortedList_delete(SortedListElement_t *element) {
	if(element == NULL || element->key == NULL) return -1;
	SortedListElement_t *p = NULL; 
	if(element != NULL) p = element->prev;
	SortedListElement_t *n = NULL; 
	if(element != NULL) n = element->next;
	// if (opt_yield & DELETE_YIELD) pthread_yield();
	if(n == NULL && p->next != element) return 1;
	if(n != NULL && (n->prev != element || p->next != element)) return 1;
	if(p!= NULL) p->next = n;
	if (opt_yield & DELETE_YIELD) pthread_yield();
	if(n != NULL) n->prev = p;
	if(element != NULL) free(element);
	return 0;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
	SortedListElement_t *ptr = list->next;
	while(ptr != NULL) {
		if(ptr == NULL || strcmp(ptr->key, key) == 0) {
			break;
		}
		if(ptr != NULL) ptr = ptr->next;
		if (opt_yield & SEARCH_YIELD) pthread_yield();
	}
	return ptr;
}

int SortedList_length(SortedList_t *list) {
	if(list == NULL) return 1;
	SortedListElement_t *ptr = list->next;
	SortedListElement_t *next_ptr = NULL;
	SortedListElement_t *prev_ptr = NULL;
	int count = 0;
	while(ptr != NULL) {
		// printf("ooooooo\n");
		if (opt_yield & SEARCH_YIELD) pthread_yield();
		count++;
		if(ptr != NULL) next_ptr = ptr->next;
		if(ptr != NULL) prev_ptr = ptr->prev;
		if(next_ptr	== NULL && prev_ptr->next != ptr) return -1;
		if(next_ptr != NULL && (next_ptr->prev != ptr || prev_ptr->next != ptr)) return -1;
		if(ptr != NULL) ptr = ptr->next;
	}
	return count;
}