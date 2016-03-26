#include <linux/version.h>
#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>  /* printk() */
#include <linux/errno.h>   /* error codes */
#include <linux/types.h>   /* size_t */
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/wait.h>
#include <linux/file.h>

#include "spinlock.h"
#include "osprd.h"

/* The size of an OSPRD sector. */
#define SECTOR_SIZE	512

/* This flag is added to an OSPRD file's f_flags to indicate that the file
 * is locked. */
#define F_OSPRD_LOCKED	0x80000

/* eprintk() prints messages to the console.
 * (If working on a real Linux machine, change KERN_NOTICE to KERN_ALERT or
 * KERN_EMERG so that you are sure to see the messages.  By default, the
 * kernel does not print all messages to the console.  Levels like KERN_ALERT
 * and KERN_EMERG will make sure that you will see messages.) */
// #define eprintk(format, ...) printk(KERN_NOTICE format, ## __VA_ARGS__)
 #define eprintk(format, ...) printk(KERN_ALERT format, ## __VA_ARGS__)

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("CS 111 RAM Disk");
// EXERCISE: Pass your names into the kernel as the module's authors.
MODULE_AUTHOR("Chunnan Yao");

#define OSPRD_MAJOR	222

/* This module parameter controls how big the disk will be.
 * You can specify module parameters when you load the module,
 * as an argument to insmod: "insmod osprd.ko nsectors=4096" */
static int nsectors = 32;
module_param(nsectors, int, 0);

/* list data structure used for read_locking_pids, write_locking_pids, invalid_tickets*/
typedef struct pid_node {
	pid_t pid;
	struct pid_node *next;
} pid_node_t;

typedef struct pid_list {
	pid_node_t *head;
	int size;
} pid_list_t;

typedef struct ticket_node {
	unsigned num;
	struct ticket_node *next;
} ticket_node_t;

typedef struct ticket_list {
	ticket_node_t *head;
	int size;
} ticket_list_t;

/* The internal representation of our device. */
typedef struct osprd_info {
	uint8_t *data;                  // The data array. Its size is
	                                // (nsectors * SECTOR_SIZE) bytes.

	osp_spinlock_t mutex;           // Mutex for synchronizing access to
					// this block device

	unsigned ticket_head;		// Currently running ticket for
					// the device lock
	int head_write; // the lock type of ticket_head. -1: no ticket holder 0: read ticket 1: write ticket
					// used for advanced deadlock detection

	unsigned ticket_tail;		// Next available ticket for
					// the device lock

	wait_queue_head_t blockq;       // Wait queue for tasks blocked on
					// the device lock

	pid_list_t read_locking_pids;
	pid_list_t write_locking_pids;
	ticket_list_t invalid_tickets;

	/* HINT: You may want to add additional fields to help
	         in detecting deadlock. */

	// The following elements are used internally; you don't need
	// to understand them.
	struct request_queue *queue;    // The device request queue.
	spinlock_t qlock;		// Used internally for mutual
	                                //   exclusion in the 'queue'.
	struct gendisk *gd;             // The generic disk.
} osprd_info_t;

#define NOSPRD 4
static osprd_info_t osprds[NOSPRD];


// Declare useful helper functions

/* check_dead_lock(writable, d)
 * Give a device and file status, judge whether there is potential dead lock.
 * Judging algorithm: 
 * If it requires a write lock, check if current pid holds other locks.
 * If yes, then there will be deadlock. 
 * If it requires a read lock, check if currnet pid holds a read lock already. 
 * If yes, then check if the last assigned ticket_head is a write ticket
 * If also yes, then there will be deadlock.
 */
int check_dead_lock(int writable, osprd_info_t *d);

/* return_valid_ticket
 * return next valid ticket, jumping over invalid tickets
 */
unsigned return_valid_ticket(ticket_list_t *ticket_list, unsigned next_tail);

/* add_to_ticket_list
 * insert invalid tickets to invalid ticket list. ticket_list is kept sorted for 
 * convenience of return_valid_ticket()funciton
 */
void add_to_ticket_list(ticket_list_t *ticket_list, unsigned ticket_num);

/* add_to_pid_list
 * insert a node to pid list as its head.
 */
void add_to_pid_list(pid_list_t *pid_list, pid_t pid);

/* remove_pid_list
 * remove the first indicated pid node from pid list. Do this by scanning the list.
 */
int remove_pid_list(pid_list_t *pid_list, pid_t pid);

/*
 * file2osprd(filp)
 *   Given an open file, check whether that file corresponds to an OSP ramdisk.
 *   If so, return a pointer to the ramdisk's osprd_info_t.
 *   If not, return NULL.
 */
static osprd_info_t *file2osprd(struct file *filp);

/*
 * for_each_open_file(task, callback, user_data)
 *   Given a task, call the function 'callback' once for each of 'task's open
 *   files.  'callback' is called as 'callback(filp, user_data)'; 'filp' is
 *   the open file, and 'user_data' is copied from for_each_open_file's third
 *   argument.
 */
static void for_each_open_file(struct task_struct *task,
			       void (*callback)(struct file *filp,
						osprd_info_t *user_data),
			       osprd_info_t *user_data);


/*
 * osprd_process_request(d, req)
 *   Called when the user reads or writes a sector.
 *   Should perform the read or write, as appropriate.
 */
static void osprd_process_request(osprd_info_t *d, struct request *req)
{
	
//	eprintk("Should process request...\n");
	int dataSize, dataOffset;
	if (!blk_fs_request(req)) {
		end_request(req, 0);
		return;
	}

	// EXERCISE: Perform the read or write request by copying data between
	// our data array and the request's buffer.
	// Hint: The 'struct request' argument tells you what kind of request
	// this is, and which sectors are being read or written.
	// Read about 'struct request' in <linux/blkdev.h>.
	// Consider the 'req->sector', 'req->current_nr_sectors', and
	// 'req->buffer' members, and the rq_data_dir() function.
	
	dataSize = req->current_nr_sectors * SECTOR_SIZE;
	dataOffset = req->sector * SECTOR_SIZE;
	if (req->sector + req->current_nr_sectors > nsectors) {
		eprintk("Sector invalid\n");
		end_request(req,0);
	}
	if(rq_data_dir(req) == READ) {
		memcpy(req->buffer, d->data + dataOffset, dataSize);
	}
	else if (rq_data_dir(req) == WRITE) {
		memcpy(d->data+dataOffset, req->buffer, dataSize);
	}
        else {
		end_request(req,0);
	}
	// Your code here.

	end_request(req, 1);
}


// This function is called when a /dev/osprdX file is opened.
// You aren't likely to need to change this.
static int osprd_open(struct inode *inode, struct file *filp)
{
	// Always set the O_SYNC flag. That way, we will get writes immediately
	// instead of waiting for them to get through write-back caches.
	filp->f_flags |= O_SYNC;
	return 0;
}


// This function is called when a /dev/osprdX file is finally closed.
// (If the file descriptor was dup2ed, this function is called only when the
// last copy is closed.)
static int osprd_close_last(struct inode *inode, struct file *filp)
{
	if (filp) {
		osprd_info_t *d = file2osprd(filp);
		int filp_writable = filp->f_mode & FMODE_WRITE;

		// EXERCISE: If the user closes a ramdisk file that holds
		// a lock, release the lock.  Also wake up blocked processes
		// as appropriate.
		// eprintk("***************TRYING TO RELEASE*****************\n");

		// Your code here.
		osp_spin_lock(&(d->mutex));
		if(!d) return -1;
		if((filp->f_flags & F_OSPRD_LOCKED) == 0) {
			osp_spin_unlock(&(d->mutex));
			return 0;
		}
		if(filp_writable) {
			// eprintk("***************GOING TO REMOVE*****************\n");
			// osp_spin_lock(&(d->mutex));
			remove_pid_list(&(d->write_locking_pids), current->pid);
			// eprintk("***************WRITE LOCK LENGTH: %d*****************\n", d->write_locking_pids.size);
		} else {
			// osp_spin_lock(&(d->mutex));
			remove_pid_list(&(d->read_locking_pids), current->pid);			
		}
		
		if(d->write_locking_pids.size == 0 && d->read_locking_pids.size == 0) {
			filp->f_flags &= !F_OSPRD_LOCKED;
		}	
		osp_spin_unlock(&(d->mutex));

		wake_up_all(&(d->blockq));

		return 0;
	}

	return 0;
}



/*
 * osprd_lock
 */

/*
 * osprd_ioctl(inode, filp, cmd, arg)
 *   Called to perform an ioctl on the named file.
 */
int osprd_ioctl(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
{

	int r, filp_writable;			// return value: initially 0
	osprd_info_t *d;
	r=0;
	d = file2osprd(filp);	// device info
	if(!d) return -1;
	
	// is file open for writing?
	filp_writable = (filp->f_mode & FMODE_WRITE) != 0;

	// This line avoids compiler warnings; you may remove it.
	(void) filp_writable, (void) d;

	// Set 'r' to the ioctl's return value: 0 on success, negative on error

	if (cmd == OSPRDIOCACQUIRE) {
		unsigned my_ticket;
		//TO DO: deadlock detection: first only check self lock 
		//check_dead_lock() to be implemented
		if(check_dead_lock(filp_writable, d)) return -EDEADLK;
		osp_spin_lock(&(d->mutex));
		my_ticket = d->ticket_head;
		d->head_write = filp_writable;
		d->ticket_head++;
		osp_spin_unlock(&(d->mutex));

		if(filp_writable) { //write-lock
								//d->blockq: processes waiting on device
			if(wait_event_interruptible(d->blockq, d->ticket_tail == my_ticket
				&& d->write_locking_pids.size == 0 
				&& d->read_locking_pids.size == 0)) {
				//orthogonal to w6hat d->ticket_tail is
				osp_spin_lock(&(d->mutex));
				if(d->ticket_tail == my_ticket) {
					d->ticket_tail = return_valid_ticket(&(d->invalid_tickets), d->ticket_tail+1);
					wake_up_all(&(d->blockq));
				} else {
					add_to_ticket_list(&(d->invalid_tickets), my_ticket);
				}
				osp_spin_unlock(&(d->mutex));
				// wake_up_all(&(d->blockq));
				return -ERESTARTSYS;
			} 
					//wait_event_interruptible() returns 0
			osp_spin_lock(&(d->mutex));
			filp->f_flags |= F_OSPRD_LOCKED;
			add_to_pid_list(&(d->write_locking_pids), current->pid);
			d->ticket_tail = return_valid_ticket(&(d->invalid_tickets), d->ticket_tail+1);
			if(d->ticket_tail == d->ticket_head) d->head_write = -1;
		} else { //read_lock

			if(wait_event_interruptible(d->blockq, d->ticket_tail == my_ticket
				&& d->write_locking_pids.size == 0)) {
				// eprintk("***************SIGNAL RECEIVED*****************\n");
				osp_spin_lock(&(d->mutex));
				if(d->ticket_tail == my_ticket) {
					// eprintk("***************TAIL BEFORE IS: %d*****************\n", d->ticket_tail);
					d->ticket_tail = return_valid_ticket(&(d->invalid_tickets), d->ticket_tail+1);
					// eprintk("***************TAIL AFTER IS: %d*****************\n", d->ticket_tail);
					wake_up_all(&(d->blockq));
				} else {
					add_to_ticket_list(&(d->invalid_tickets), my_ticket);
				}
				osp_spin_unlock(&(d->mutex));
				// wake_up_all(&(d->blockq));
				return -ERESTARTSYS;
			}
			// eprintk("***************GOING TO GET READ LOCK*****************\n");
			osp_spin_lock(&(d->mutex));
			filp->f_flags |= F_OSPRD_LOCKED;
			add_to_pid_list(&(d->read_locking_pids), current->pid);
			d->ticket_tail = return_valid_ticket(&(d->invalid_tickets), d->ticket_tail+1);
			if(d->ticket_tail == d->ticket_head) d->head_write = -1;
		}
		osp_spin_unlock(&(d->mutex));
		wake_up_all(&(d->blockq));
		return 0;

		// EXERCISE: Lock the ramdisk.
		//
		// If *filp is open for writing (filp_writable), then attempt
		// to write-lock the ramdisk; otherwise attempt to read-lock
		// the ramdisk.
		//
                // This lock request must block using 'd->blockq' until:
		// 1) no other process holds a write lock;
		// 2) either the request is for a read lock, or no other process
		//    holds a read lock; and
		// 3) lock requests should be serviced in order, so no process
		//    that blocked earlier is still blocked waiting for the
		//    lock.
		//
		// If a process acquires a lock, mark this fact by setting
		// 'filp->f_flags |= F_OSPRD_LOCKED'.  You also need to
		// keep track of how many read and write locks are held:
		// change the 'osprd_info_t' structure to do this.
		//
		// Also wake up processes waiting on 'd->blockq' as needed.
		//
		// If the lock request would cause a deadlock, return -EDEADLK.
		// If the lock request blocks and is awoken by a signal, then
		// return -ERESTARTSYS.
		// Otherwise, if we can grant the lock request, return 0.

		// 'd->ticket_head' and 'd->ticket_tail' should help you
		// service lock requests in order.  These implement a ticket
		// order: 'ticket_tail' is the next ticket, and 'ticket_head'
		// is the ticket currently being served.  You should set a local
		// variable to 'd->ticket_head' and increment 'd->ticket_head'.
		// Then, block at least until 'd->ticket_tail == local_ticket'.
		// (Some of these operations are in a critical section and must
		// be protected by a spinlock; which ones?)

		// Your code here (instead of the next two lines).
		// eprintk("Attempting to acquire\n");
		// r = -ENOTTY;

	} else if (cmd == OSPRDIOCTRYACQUIRE) {
		// eprintk("hehe");
		if(check_dead_lock(filp_writable, d)) return -EBUSY;
		if(filp_writable) {
			osp_spin_lock(&(d->mutex));
			if(!(d->ticket_tail == d->ticket_head
				&& d->write_locking_pids.size == 0 
				&& d->read_locking_pids.size == 0)) {
				osp_spin_unlock(&(d->mutex));
				return -EBUSY;
			}
			filp->f_flags |= F_OSPRD_LOCKED;
			d->ticket_head++;
			add_to_pid_list(&(d->write_locking_pids), current->pid);
			d->ticket_tail = return_valid_ticket(&(d->invalid_tickets), d->ticket_tail+1);
			if(d->ticket_tail == d->ticket_head) d->head_write = -1;
		} else {
			osp_spin_lock(&(d->mutex));	
			if(!(d->ticket_tail == d->ticket_head
				&& d->write_locking_pids.size == 0)) {
				osp_spin_unlock(&(d->mutex));
				return -EBUSY;
			}		
			filp->f_flags |= F_OSPRD_LOCKED;
			d->ticket_head++;
			add_to_pid_list(&(d->read_locking_pids), current->pid);
			d->ticket_tail = return_valid_ticket(&(d->invalid_tickets), d->ticket_tail+1);		
			if(d->ticket_tail == d->ticket_head) d->head_write = -1;
		}
		osp_spin_unlock(&(d->mutex));
		wake_up_all(&(d->blockq));
		return 0;



		// EXERCISE: ATTEMPT to lock the ramdisk.
		//
		// This is just like OSPRDIOCACQUIRE, except it should never
		// block.  If OSPRDIOCACQUIRE would block or return deadlock,
		// OSPRDIOCTRYACQUIRE should return -EBUSY.
		// Otherwise, if we can grant the lock request, return 0.

		// Your code here (instead of the next two lines).
		// eprintk("Attempting to try acquire\n");
		// r = -ENOTTY;

	} else if (cmd == OSPRDIOCRELEASE) {
		// eprintk("***************TRYING TO RELEASE*****************\n");
		if((filp->f_flags & F_OSPRD_LOCKED) == 0) return -EINVAL;
		//write list has at most 1 element
		//read list can have multiple elements but they don't need to block each other

		if(filp_writable) {
			// eprintk("***************RELEASING WRITE LOCK*****************\n");
			osp_spin_lock(&(d->mutex));
			remove_pid_list(&(d->write_locking_pids), current->pid);
		} else {
			osp_spin_lock(&(d->mutex));
			remove_pid_list(&(d->read_locking_pids), current->pid);			
		}
		
		if(d->write_locking_pids.size == 0 && d->read_locking_pids.size == 0) {
			filp->f_flags &= !F_OSPRD_LOCKED;
		}	
		osp_spin_unlock(&(d->mutex));
		wake_up_all(&(d->blockq));

		return 0;


		// EXERCISE: Unlock the ramdisk.
		//       (process)
		// If the file hasn't locked the ramdisk, return -EINVAL.
		// Otherwise, clear the lock from filp->f_flags, wake up
		// the wait queue, perform any additional accounting steps
		// you need, and return 0.

		// Your code here (instead of the next line).
		// r = -ENOTTY;

	} else
		r = -ENOTTY; /* unknown command */
	return r;
}



int check_dead_lock(int writable, osprd_info_t *d) {
	pid_node_t *pointer;
	if(writable) {
		osp_spin_lock(&(d->mutex));
		pointer = d->write_locking_pids.head;
		while(pointer) {
			if(pointer->pid == current->pid) {
				osp_spin_unlock(&(d->mutex));	
				return 1;
			}
			pointer = pointer->next;
		}

		pointer = d->read_locking_pids.head;
		while(pointer) {
			if(pointer->pid == current->pid) {
				osp_spin_unlock(&(d->mutex));	
				return 1;
			}
			pointer = pointer->next;
		}
	} else {
		osp_spin_lock(&(d->mutex));
		pointer = d->write_locking_pids.head;
		while(pointer) {
			if(pointer->pid == current->pid) {
				osp_spin_unlock(&(d->mutex));	
				return 1;
			}
			pointer = pointer->next;
		}

		pointer = d->read_locking_pids.head;
		while(pointer) {
			if(pointer->pid == current->pid && d->head_write == 1) {
				osp_spin_unlock(&(d->mutex));	
				return 1;
			}
			pointer = pointer->next;
		}

	}
	osp_spin_unlock(&(d->mutex));	
	return 0;
}

//my understanding: the process which holds an effective ticket must be blocked
unsigned return_valid_ticket(ticket_list_t *ticket_list, unsigned next_tail) {
	ticket_node_t *tofree;
	// if(ticket_list->head == NULL) return next_tail;
	while(ticket_list->head != NULL && ticket_list->head->num == next_tail) {
		ticket_list->size--;
		tofree = ticket_list->head;
		ticket_list->head = ticket_list->head->next;
		kfree(tofree);
		next_tail++;
	}
	return next_tail;
}
//keep ticket list ordered
void add_to_ticket_list(ticket_list_t *ticket_list, unsigned ticket_num) {
	ticket_node_t *dummy, *ptr;
	dummy = (ticket_node_t*)kmalloc(sizeof(ticket_node_t), GFP_ATOMIC);
	dummy->next = ticket_list->head;
	dummy->num = 0;
	
	ptr = dummy;
	while(ptr->next) {
		if(ticket_num > ptr->num && ticket_num < ptr->next->num) {
			ticket_node_t *further = ptr->next;
			ptr->next = (ticket_node_t*)kmalloc(sizeof(ticket_node_t), GFP_ATOMIC);
			ptr->next->num = ticket_num;
			ptr->next->next = further;
			break;
		} else {
			ptr = ptr->next;
		}
	}
	if(ptr->next == NULL) {
		ptr->next = (ticket_node_t*)kmalloc(sizeof(ticket_node_t), GFP_ATOMIC);
		ptr->next->num = ticket_num;
		ptr->next->next = NULL;
	}
	ticket_list->head = dummy->next;
	ticket_list->size++;
	kfree(dummy);
	return;
}

void add_to_pid_list(pid_list_t *pid_list, pid_t pid) {
	pid_node_t* new_node = (pid_node_t*)kmalloc(sizeof(pid_node_t), GFP_ATOMIC);
	new_node->next = pid_list->head;
	new_node->pid = pid;
	pid_list->head = new_node;
	pid_list->size++;
	return;
}

int remove_pid_list(pid_list_t *pid_list, pid_t pid) {
	pid_node_t* tofree;
	if(pid_list->head==NULL) return -1;
	if(pid_list->head->pid == pid) {
		tofree = pid_list->head;
		pid_list->head = pid_list->head->next;
		kfree(tofree);
	} else {
		pid_node_t* ptr = pid_list->head;
		while(ptr->next) {
			if(ptr->next->pid == pid) {
				tofree = ptr->next;
				ptr->next = ptr->next->next;
				kfree(tofree);
				pid_list->size--;
				return 0;
			} else {
				ptr = ptr->next;
			}
		}
		return -1;
	}
	pid_list->size--;
	return 0;
}


// Initialize internal fields for an osprd_info_t.

static void osprd_setup(osprd_info_t *d)
{
	/* Initialize the wait queue. */
	init_waitqueue_head(&d->blockq);
	osp_spin_lock_init(&d->mutex);
	d->ticket_head = d->ticket_tail = 0;
	d->head_write = 0;
	d->read_locking_pids.size = 0;
	d->write_locking_pids.size = 0;
	d->invalid_tickets.size = 0;
	d->write_locking_pids.head = NULL;
	d->read_locking_pids.head = NULL;
	d->invalid_tickets.head = NULL;
	/* Add code here if you add fields to osprd_info_t. */
}


/*****************************************************************************/
/*         THERE IS NO NEED TO UNDERSTAND ANY CODE BELOW THIS LINE!          */
/*                                                                           */
/*****************************************************************************/

// Process a list of requests for a osprd_info_t.
// Calls osprd_process_request for each element of the queue.

static void osprd_process_request_queue(request_queue_t *q)
{
	osprd_info_t *d = (osprd_info_t *) q->queuedata;
	struct request *req;

	while ((req = elv_next_request(q)) != NULL)
		osprd_process_request(d, req);
}


// Some particularly horrible stuff to get around some Linux issues:
// the Linux block device interface doesn't let a block device find out
// which file has been closed.  We need this information.

static struct file_operations osprd_blk_fops;
static int (*blkdev_release)(struct inode *, struct file *);

static int _osprd_release(struct inode *inode, struct file *filp)
{
	if (file2osprd(filp))
		osprd_close_last(inode, filp);
	return (*blkdev_release)(inode, filp);
}

static int _osprd_open(struct inode *inode, struct file *filp)
{
	if (!osprd_blk_fops.open) {
		memcpy(&osprd_blk_fops, filp->f_op, sizeof(osprd_blk_fops));
		blkdev_release = osprd_blk_fops.release;
		osprd_blk_fops.release = _osprd_release;
	}
	filp->f_op = &osprd_blk_fops;
	return osprd_open(inode, filp);
}


// The device operations structure.

static struct block_device_operations osprd_ops = {
	.owner = THIS_MODULE,
	.open = _osprd_open,
	// .release = osprd_release, // we must call our own release
	.ioctl = osprd_ioctl
};


// Given an open file, check whether that file corresponds to an OSP ramdisk.
// If so, return a pointer to the ramdisk's osprd_info_t.
// If not, return NULL.

static osprd_info_t *file2osprd(struct file *filp)
{
	if (filp) {
		struct inode *ino = filp->f_dentry->d_inode;
		if (ino->i_bdev
		    && ino->i_bdev->bd_disk
		    && ino->i_bdev->bd_disk->major == OSPRD_MAJOR
		    && ino->i_bdev->bd_disk->fops == &osprd_ops)
			return (osprd_info_t *) ino->i_bdev->bd_disk->private_data;
	}
	return NULL;
}


// Call the function 'callback' with data 'user_data' for each of 'task's
// open files.

static void for_each_open_file(struct task_struct *task,
		  void (*callback)(struct file *filp, osprd_info_t *user_data),
		  osprd_info_t *user_data)
{
	int fd;
	task_lock(task);
	spin_lock(&task->files->file_lock);
	{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 13)
		struct files_struct *f = task->files;
#else
		struct fdtable *f = task->files->fdt;
#endif
		for (fd = 0; fd < f->max_fds; fd++)
			if (f->fd[fd])
				(*callback)(f->fd[fd], user_data);
	}
	spin_unlock(&task->files->file_lock);
	task_unlock(task);
}


// Destroy a osprd_info_t.

static void cleanup_device(osprd_info_t *d)
{
	wake_up_all(&d->blockq);
	if (d->gd) {
		del_gendisk(d->gd);
		put_disk(d->gd);
	}
	if (d->queue)
		blk_cleanup_queue(d->queue);
	if (d->data)
		vfree(d->data);
}


// Initialize a osprd_info_t.

static int setup_device(osprd_info_t *d, int which)
{
	memset(d, 0, sizeof(osprd_info_t));

	/* Get memory to store the actual block data. */
	if (!(d->data = vmalloc(nsectors * SECTOR_SIZE)))
		return -1;
	memset(d->data, 0, nsectors * SECTOR_SIZE);

	/* Set up the I/O queue. */
	spin_lock_init(&d->qlock);
	if (!(d->queue = blk_init_queue(osprd_process_request_queue, &d->qlock)))
		return -1;
	blk_queue_hardsect_size(d->queue, SECTOR_SIZE);
	d->queue->queuedata = d;

	/* The gendisk structure. */
	if (!(d->gd = alloc_disk(1)))
		return -1;
	d->gd->major = OSPRD_MAJOR;
	d->gd->first_minor = which;
	d->gd->fops = &osprd_ops;
	d->gd->queue = d->queue;
	d->gd->private_data = d;
	snprintf(d->gd->disk_name, 32, "osprd%c", which + 'a');
	set_capacity(d->gd, nsectors);
	add_disk(d->gd);

	/* Call the setup function. */
	osprd_setup(d);

	return 0;
}

static void osprd_exit(void);


// The kernel calls this function when the module is loaded.
// It initializes the 4 osprd block devices.

static int __init osprd_init(void)
{
	int i, r;

	// shut up the compiler
	(void) for_each_open_file;
#ifndef osp_spin_lock
	(void) osp_spin_lock;
	(void) osp_spin_unlock;
#endif

	/* Register the block device name. */
	if (register_blkdev(OSPRD_MAJOR, "osprd") < 0) {
		printk(KERN_WARNING "osprd: unable to get major number\n");
		return -EBUSY;
	}

	/* Initialize the device structures. */
	for (i = r = 0; i < NOSPRD; i++)
		if (setup_device(&osprds[i], i) < 0)
			r = -EINVAL;

	if (r < 0) {
		printk(KERN_EMERG "osprd: can't set up device structures\n");
		osprd_exit();
		return -EBUSY;
	} else
		return 0;
}


// The kernel calls this function to unload the osprd module.
// It destroys the osprd devices.

static void osprd_exit(void)
{
	int i;
	for (i = 0; i < NOSPRD; i++)
		cleanup_device(&osprds[i]);
	unregister_blkdev(OSPRD_MAJOR, "osprd");
}


// Tell Linux to call those functions at init and exit time.
module_init(osprd_init);
module_exit(osprd_exit);
