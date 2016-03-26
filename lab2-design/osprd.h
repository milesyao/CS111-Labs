#ifndef OSPRD_H
#define OSPRD_H

// ioctl constants
#define OSPRDIOCACQUIRE		42
#define OSPRDIOCTRYACQUIRE	43
#define OSPRDIOCRELEASE		44
#define OSPRDWATCHER        45

typedef struct msg {
	char w_type;
	char *u_offset;
	char *u_end;
} msg_t;

#endif
