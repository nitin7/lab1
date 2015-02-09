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
#define DEBUG 0

/* This flag is added to an OSPRD file's f_flags to indicate that the file
 * is locked. */
#define F_OSPRD_LOCKED	0x80000

/* eprintk() prints messages to the console.
 * (If working on a real Linux machine, change KERN_NOTICE to KERN_ALERT or
 * KERN_EMERG so that you are sure to see the messages.  By default, the
 * kernel does not print all messages to the console.  Levels like KERN_ALERT
 * and KERN_EMERG will make sure that you will see messages.) */
#define eprintk(format, ...) printk(KERN_NOTICE format, ## __VA_ARGS__)

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("CS 111 RAM Disk");
// EXERCISE: Pass your names into the kernel as the module's authors.
MODULE_AUTHOR("Skeletor");

#define OSPRD_MAJOR	222

/* This module parameter controls how big the disk will be.
 * You can specify module parameters when you load the module,
 * as an argument to insmod: "insmod osprd.ko nsectors=4096" */
static int nsectors = 32;
module_param(nsectors, int, 0);


struct pidNode {
	pid_t pid;
	struct pidNode* next;
};

struct pidList {
	struct pidNode* head;
	int size;
};

struct ticketNode {
	unsigned ticket;
	struct ticketNode* next;
};

struct ticketList {
	struct ticketNode* head;
	int size;
};

/* The internal representation of our device. */
typedef struct osprd_info {
	uint8_t *data;                  // The data array. Its size is
	                                // (nsectors * SECTOR_SIZE) bytes.

	osp_spinlock_t mutex;           // Mutex for synchronizing access to
					// this block device

	unsigned ticket_head;		// Currently running ticket for
					// the device lock

	unsigned ticket_tail;		// Next available ticket for
					// the device lock

	wait_queue_head_t blockq;       // Wait queue for tasks blocked on
					// the device lock

	/* HINT: You may want to add additional fields to help
	         in detecting deadlock. */
	//added fields
	struct pidList* pid_read_lock;
	struct pidList* pid_write_lock;
	struct ticketList* exitedTickets;
    //TUAN: current process uses this to see if it holds other locks on
    //other open RAM disk files, and not just the current RAM disk
    //file that it requests the lock.
	int holdOtherLocks;

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


// Function declarations
void pidlist_add(struct pidList** list, pid_t pid);
void pidlist_remove(struct pidList** list, pid_t pid);
int pid_contains(struct pidList* list, pid_t pid);
void tixlist_add(struct ticketList** list, unsigned ticket);
void tixlist_remove(struct ticketList** list, unsigned ticket);
int tixlist_contains(struct ticketList* list, unsigned ticket);
void check_locks(struct file *filp, osprd_info_t *d);
void increment_ticket(osprd_info_t *d);

/*
 * osprd_process_request(d, req)
 *   Called when the user reads or writes a sector.
 *   Should perform the read or write, as appropriate.
 */
 //DONE
static void osprd_process_request(osprd_info_t *d, struct request *req)
{
    
    // Normal file system requests not supported
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

	// Your code here.
	uint8_t* data = d->data + (req->sector) * SECTOR_SIZE;
    long size = req->current_nr_sectors * SECTOR_SIZE;
	
	if (rq_data_dir(req) == WRITE) 
		memcpy((void *) data, (void *) req->buffer, size);
	else if (rq_data_dir(req) == READ)
		memcpy((void *) req->buffer, (void *) data, size);

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
//almost done
static int osprd_close_last(struct inode *inode, struct file *filp)
{
	if (filp) {
		osprd_info_t *d = file2osprd(filp);
		int filp_writable = filp->f_mode & FMODE_WRITE;
		// EXERCISE: If the user closes a ramdisk file that holds
		// a lock, release the lock.  Also wake up blocked processes
		// as appropriate.

		if (d != NULL) {
			osp_spinlock_t *lock = &(d->mutex);
			wait_queue_head_t *blockq = &(d->blockq);
			
			int readlock = pid_contains(d->pid_read_lock, current->pid);
			int writelock = pid_contains(d->pid_write_lock, current->pid);

			osp_spin_lock(lock);
			// Invalid if file hasn't locked ramdisk
			
			if (readlock) 
				pidlist_remove(&d->pid_read_lock, current->pid);
			if (writelock) 
				pidlist_remove(&d->pid_write_lock, current->pid);

			if (!readlock && !writelock) {
				osp_spin_unlock(lock);
				return -EINVAL;
			}
			
			// Clear the lock from filp->f_flags if no processes (not just current process) hold any lock.
			if (readlock == NULL && writelock == NULL) {
				filp->f_flags &= !F_OSPRD_LOCKED;
			}
			
			osp_spin_unlock(lock);
			wake_up_all(blockq);
		}
		else
			return 1;
		
	}

	return 0;
}

void unlockWakeIncrement(osprd_info_t *d) {
	osp_spin_unlock(&(d->mutex));
	wake_up_all(&(d->blockq));
	increment_ticket(d);
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
	osprd_info_t *d = file2osprd(filp);	// device info
	int r = 0;			// return value: initially 0
	unsigned ticket;

	// is file open for writing?
	int filp_writable = (filp->f_mode & FMODE_WRITE) != 0;

	// This line avoids compiler warnings; you may remove it.
	(void) filp_writable, (void) d;

	int readlock = pid_contains(d->pid_read_lock, current->pid);
	int writelock = pid_contains(d->pid_write_lock, current->pid);

	// Set 'r' to the ioctl's return value: 0 on success, negative on error

	if (cmd == OSPRDIOCACQUIRE) {

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

		
		/** Ticket + check error cases **/
		
		// Get ticket
		osp_spin_lock(&(d->mutex));
		ticket = d->ticket_head;
		d->ticket_head++;
		
		// Deadlock - if current proc already has other lock
		if ((writelock && !filp_writable) || (readlock && filp_writable)) {
			unlockWakeIncrement(d);
			return -EDEADLK;
		}
		
		for_each_open_file(current, check_locks, d);
		if (d->holdOtherLocks) {
			d->holdOtherLocks = 0;
			unlockWakeIncrement(d);
			return -EDEADLK;
		}

		
		osp_spin_unlock(&(d->mutex));
		
		/** Blocking **/
		
		// first argument is wait queue
		// second argument is condition to wake
		if (wait_event_interruptible(d->blockq, d->ticket_tail==ticket &&
									 d->pid_write_lock == NULL &&
									 (d->pid_read_lock == NULL || !filp_writable))) {
			
			if (d->ticket_tail != ticket) 
				tixlist_add(&(d->exitedTickets), ticket);
			else   // signal received
				increment_ticket(d);
		
			return -ERESTARTSYS;
		}
		
		/** Obtain lock **/
		
		osp_spin_lock(&(d->mutex));
		filp->f_flags |= F_OSPRD_LOCKED; // claim lock
		
		// keep track of the ID processes that are holding lock
		if (filp_writable)
			pidlist_add(&(d->pid_write_lock), current->pid);
		else
			pidlist_add(&(d->pid_read_lock), current->pid);

		unlockWakeIncrement(d);
		return 0;

	} else if (cmd == OSPRDIOCTRYACQUIRE) {

		// EXERCISE: ATTEMPT to lock the ramdisk.
		//
		// This is just like OSPRDIOCACQUIRE, except it should never
		// block.  If OSPRDIOCACQUIRE would block or return deadlock,
		// OSPRDIOCTRYACQUIRE should return -EBUSY.
		// Otherwise, if we can grant the lock request, return 0.
		
		/** Ticket + check error cases **/
		
		// get ticket
		osp_spin_lock(&(d->mutex));
		ticket = d->ticket_head;
		d->ticket_head++;
		
		// Deadlock - if current proc already has complement lock
		if ((writelock && !filp_writable) || (readlock && filp_writable)) {
			unlockWakeIncrement(d);
			return -EBUSY;
		}
		
		// Deadlock - if current proc holds lock on different file
		for_each_open_file(current, check_locks, d);
		if (d->holdOtherLocks) {
			d->holdOtherLocks = 0;
			unlockWakeIncrement(d);
			return -EBUSY;
		}
		
		osp_spin_unlock(&(d->mutex));
		
		// busy if locks are held
		if (d->pid_write_lock != NULL || (d->pid_read_lock != NULL && filp_writable)) {
			increment_ticket(d);
			wake_up_all(&(d->blockq)); // required??
			return -EBUSY;
		}
		
		// Acquire lock
		osp_spin_lock(&(d->mutex));
		filp->f_flags |= F_OSPRD_LOCKED;
		if (filp_writable)
			pidlist_add(&(d->pid_write_lock), current->pid);
		else
			pidlist_add(&(d->pid_read_lock), current->pid);

		unlockWakeIncrement(d);
		return 0;

	} else if (cmd == OSPRDIOCRELEASE) {

		// EXERCISE: Unlock the ramdisk.
		//
		// If the file hasn't locked the ramdisk, return -EINVAL.
		// Otherwise, clear the lock from filp->f_flags, wake up
		// the wait queue, perform any additional accounting steps
		// you need, and return 0.
		osp_spin_lock(&(d->mutex));
		
		// if file hasn't locked the ramdisk, return -EINVAL.
		if (!pid_contains(d->pid_write_lock, current->pid) && !(pid_contains(d->pid_read_lock, current->pid))) {
			osp_spin_unlock(&(d->mutex));
			return -EINVAL;
		}
		
		if (pid_contains(d->pid_write_lock, current->pid)) {
			pidlist_remove(&d->pid_write_lock, current->pid);
		}
		
		if (pid_contains(d->pid_read_lock, current->pid)) {
			pidlist_remove(&d->pid_read_lock, current->pid);
		}
		
		// clear the lock from filp->f_flags if no processes (not just current process) hold any lock.
		if (d->pid_read_lock == NULL && d->pid_write_lock == NULL) {
			filp->f_flags &= !F_OSPRD_LOCKED;
		}
		
		osp_spin_unlock(&(d->mutex));
		wake_up_all(&(d->blockq));

	} else
		r = -ENOTTY; /* unknown command */
	return r;
}


// Initialize internal fields for an osprd_info_t.

static void osprd_setup(osprd_info_t *d)
{
	/* Initialize the wait queue. */
	init_waitqueue_head(&d->blockq);
	osp_spin_lock_init(&d->mutex);
	d->ticket_head = d->ticket_tail = 0;
	d->pid_read_lock = NULL;
	d->pid_write_lock = NULL;
	d->exitedTickets = NULL;
	d->holdOtherLocks = 0;
}

//our functions start---------------------------------------------
void pidlist_add(struct pidList** list, pid_t pid) {
	
	struct pidNode* newNode;
	//check if need to initialize list
	if (*list == NULL) {
		//kzalloc allocates the kernel memory and zero it before return the pointer.
		//using GFP_ATOMIC to prevent the kernel from block your current memory allocation.
		//Another commonly used flag is GFP_KERNEL which specifies a normal kernel allocation.
		//Kernel memory is limited to about 1GB of physical and virtual memory and is not pageable.
		
		*list = kzalloc(sizeof(struct pidList), GFP_ATOMIC);
		(*list)->head = NULL;
		(*list)->size = 0;
	}
	newNode = kzalloc(sizeof(struct pidNode), GFP_ATOMIC);
	newNode->pid = pid;
	if ((*list)->head == NULL) {
		(*list)->head = newNode;
		newNode->next = NULL;
	}
	else {
		newNode->next = (*list)->head;
		(*list)->head = newNode;
	}
	(*list)->size++;
}

//Note: pid can appear many places in the list. This function removes all
//occurances of pid.
//works even if pid is not in list
void pidlist_remove(struct pidList** list, pid_t pid) {
	
	struct pidNode* cur;
	struct pidNode* toDelete;
	
	if (list == NULL) {
		return;
	}
	
	cur = (*list)->head;
	if (cur == NULL) {
		return;
	}
	
	//if pid matches the head node
	if (cur->pid == pid) {
		(*list)->head = cur->next;
		kfree(cur); //kfree frees kernel memory
		(*list)->size--;
	}
	
	//remove other occurances of pid in the list.
	while (cur->next != NULL) {
		if (cur->next->pid == pid) {
			toDelete = cur->next;
			cur->next = cur->next->next;
			kfree(toDelete);
			(*list)->size--;
			return;
		}
		cur = cur->next;
	}
	if ((*list)->size == 0) {
		//deallocate list so no memory leaks
		kfree(*list);
		*list = NULL;
	}
}

//returns 1 if pid is in the list, 0 otherwise
int pid_contains(struct pidList* list, pid_t pid) {
	
	struct pidNode* cur;
	if (list == NULL) {
		return 0;
	}
	cur = list->head;
	while (cur != NULL) {
		if (cur->pid == pid) {
			return 1;
		}
		cur = cur->next;
	}
	return 0;
}

void tixlist_add(struct ticketList** list, unsigned ticket) {
	struct ticketNode* newNode;
	//check if need to initialize list
	if (*list == NULL) {
		*list = kzalloc(sizeof(struct ticketList), GFP_ATOMIC);
		(*list)->head = NULL;
		(*list)->size = 0;
	}
	newNode = kzalloc(sizeof(struct ticketNode), GFP_ATOMIC);
	newNode->ticket = ticket;
	if ((*list)->head == NULL) {
		(*list)->head = newNode;
		newNode->next = NULL;
	}
	else {
		newNode->next = (*list)->head;
		(*list)->head = newNode;
	}
	(*list)->size++;
}

//works even if ticket is not in list
void tixlist_remove(struct ticketList** list, unsigned ticket) {
	
	struct ticketNode* cur;
	struct ticketNode* toDelete;
	
	if (list == NULL) {
		return;
	}
	
	cur = (*list)->head;
	if (cur == NULL) {
		return;
	}
	if (cur->ticket == ticket) {
		(*list)->head = cur->next;
		kfree(cur);
		(*list)->size--;
	}
	while (cur->next != NULL) {
		if (cur->next->ticket == ticket) {
			toDelete = cur->next;
			cur->next = cur->next->next;
			kfree(toDelete);
			(*list)->size--;
			return;
		}
		cur = cur->next;
	}
	if ((*list)->size == 0) {
		//deallocate list so no memory leaks
		kfree(*list);
		*list = NULL;
	}
}


int tixlist_contains(struct ticketList* list, unsigned ticket) {
	
	struct ticketNode* cur;
	if (list == NULL) {
		return 0;
	}
	cur = list->head;
	while (cur != NULL) {
		if (cur->ticket == ticket) {
			return 1;
		}
		cur = cur->next;
	}
	return 0;
}

void check_locks(struct file *filp, osprd_info_t *d) {
	osprd_info_t *otherDisk;
	if ((otherDisk = file2osprd(filp)) != NULL) {
		if (pid_contains(otherDisk->pid_read_lock, current->pid) || pid_contains(otherDisk->pid_write_lock, current->pid)) {
			d->holdOtherLocks = 1;
			return;
		}
	}
}

void increment_ticket(osprd_info_t *d) {
	while (++(d->ticket_tail)) {//without this, program will be paused
		//if some processes already died and can't handle the ticket.
		//we make sure only pass the alive ticket to alive processes.
		if (!tixlist_contains(d->exitedTickets, d->ticket_tail)) {
			break; //good. The next process is still alive to accept this ticket.
		}
		else {
			//no one hanles this ticket.
			tixlist_remove(&(d->exitedTickets), d->ticket_tail);
		}
	}
}
//our functions end


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