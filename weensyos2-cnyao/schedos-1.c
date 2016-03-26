#include "schedos-app.h"
#include "x86sync.h"

/*****************************************************************************
 * schedos-1
 *
 *   This tiny application prints red "1"s to the console.
 *   It yields the CPU to the kernel after each "1" using the sys_yield()
 *   system call.  This lets the kernel (schedos-kern.c) pick another
 *   application to run, if it wants.
 *
 *   The other schedos-* processes simply #include this file after defining
 *   PRINTCHAR appropriately.
 *
 *****************************************************************************/

#ifndef PRINTCHAR
#define PRINTCHAR	('1' | 0x0C00)
#define PRIO 4
#define LOTTERY 10
#endif


void lock(uint16_t* l) {
	while (atomic_swap((uint32_t*)l, 1))
		continue;
}

void unlock(uint16_t* l) {
	*l = 0;
}

// UNCOMMENT THE NEXT LINE TO USE EXERCISE 8 CODE INSTEAD OF EXERCISE 6
#define __EXERCISE_8__
// Use the following structure to choose between them:
#ifdef __EXERCISE_8__
void
start(void)
{
	int i;
	// sys_set_priority(PRIO);
	sys_set_priority(PRIO);
	sys_set_nlottery(LOTTERY);
	
	for (i = 0; i < RUNCOUNT; i++) {
		sys_print(PRINTCHAR);
		sys_yield();
	}
	// while (1)
	// 	sys_yield();
	sys_exit(0);
}
// (exercise 6 code)
#else
void
start(void)
{
	int i;
	sys_set_priority(PRIO);
	sys_set_nlottery(LOTTERY);

    uint16_t *l = (uint16_t*) 0x198004;
	for (i = 0; i < RUNCOUNT; i++) {
		// Write characters to the console, yielding after each one.
		lock(l);
		*cursorpos++ = PRINTCHAR;
		unlock(l);
		sys_yield();
	}

	// Yield forever.
	// while (1)
	// 	sys_yield();
	sys_exit(0);
}
// (exercise 8 code)
#endif



