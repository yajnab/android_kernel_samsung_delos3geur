/*
 * drivers/misc/logger.c
 *
 * A Logging Subsystem
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * Robert Love <rlove@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/time.h>
#include "logger.h"

#include <asm/ioctls.h>

static unsigned long platform_reset_count;

#ifdef CONFIG_APPLY_GA_SOLUTION
// @message
static char klog_buf[256];
#endif

#ifdef CONFIG_APPLY_GA_SOLUTION
// GAF
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>

extern struct GAForensicINFO GAFINFO;
extern u64 get_idle_time_ram(int cpu);
extern u64 get_iowait_time_ram(int cpu);

void dump_one_task_info(struct task_struct *tsk, bool isMain)
{
	char stat_array[3] = { 'R', 'S', 'D'};
	char stat_ch;
	char *pThInf = tsk->stack;

	stat_ch = tsk->state <= TASK_UNINTERRUPTIBLE ? stat_array[tsk->state] : '?';
	printk( "%8d %8d %8d %16lld %c (%d) %3d %08x %c %s\n",
		tsk->pid, (int)(tsk->utime), (int)(tsk->stime), tsk->se.exec_start, stat_ch, (int)(tsk->state),
		*(int*)(pThInf + GAFINFO.thread_info_struct_cpu),
		(int)tsk, isMain?'*':' ', tsk->comm );
	
	if( tsk->state == TASK_RUNNING || tsk->state == TASK_UNINTERRUPTIBLE ) {
		show_stack(tsk, NULL);
	}
}

void dump_all_task_info()
{
	struct task_struct *frst_tsk;
	struct task_struct *curr_tsk;
	struct task_struct *frst_thr;
	struct task_struct *curr_thr;

	printk ( "\n" );
	printk ( " current proc: %d %s\n", current->pid, current->comm );
	printk ( "-----------------------------------------------------------------------------------\n" );
	printk ( "    pid     uTime     sTime              exec(ns)     stat     cpu     task_struct\n" );
	printk ( "-----------------------------------------------------------------------------------\n" );

	//process
	frst_tsk = &init_task;
	curr_tsk = frst_tsk;
	while(curr_tsk != NULL )
	{
		dump_one_task_info( curr_tsk, true);
		//threads
		if( curr_tsk->thread_group.next != NULL)
		{
			frst_thr = container_of( curr_tsk->thread_group.next, struct task_struct, thread_group );
			curr_thr = frst_thr;
			if( frst_thr != curr_tsk)
			{
				while( curr_thr != curr_tsk)
				{
					dump_one_task_info( curr_thr, false);
					curr_thr = container_of( curr_thr->thread_group.next, struct task_struct, thread_group);
					if( curr_thr == curr_tsk)  break;
				}
			}
		}
		curr_tsk = container_of( curr_tsk->tasks.next, struct task_struct, tasks);
		if(curr_tsk == frst_tsk) break;
	}
	printk ( "-----------------------------------------------------------------------------------\n" );
}

#include <linux/kernel_stat.h>

#ifndef arch_irq_stat_cpu
#define arch_irq_stat_cpu(cpu) 0
#endif

#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif

#ifndef arch_idle_time
#define arch_idle_time(cpu) 0
#endif

void dump_cpu_stat()
{
	int i, j;
	unsigned long jif;
	u64 user, nice, system, idle, iowait, irq, softirq, steal;
	u64 guest, guest_nice;
	u64 sum = 0;
	u64 sum_softirq = 0;
	unsigned int per_softirq_sums[NR_SOFTIRQS] = {0};
	struct timespec boottime;
	unsigned int per_irq_sum;
	user = nice = system = idle = iowait =
	irq = softirq = steal = cputime64_zero;
	guest = guest_nice = cputime64_zero;
	getboottime(&boottime);
	jif = boottime.tv_sec;
	
	for_each_possible_cpu(i) {
		user += kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice += kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system += kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle += get_idle_time_ram(i);
		iowait += get_iowait_time_ram(i);
		irq += kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq += kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		//steal += kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
		//guest += kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
		//guest_nice += kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];
		for_each_irq_nr(j) {
			sum += kstat_irqs_cpu(i, j);
		}
		
		sum += arch_irq_stat_cpu(i);
		
		for (j=0; j< NR_SOFTIRQS; j++)
		{
			unsigned int softirq_stat = kstat_softirqs_cpu(j, i);
			per_softirq_sums[j] += softirq_stat;
			sum_softirq += softirq_stat;
		}
	}
	sum += arch_irq_stat();
	
	printk("\n");
	printk(" cpu  user:%llu nice:%llu system:%llu idle:%llu iowait:%llu irq:%llu softirq:%llu %llu %llu %llu\n",
		(unsigned long long)cputime64_to_clock_t(user),
		(unsigned long long)cputime64_to_clock_t(nice),
		(unsigned long long)cputime64_to_clock_t(system),
		(unsigned long long)cputime64_to_clock_t(idle),
		(unsigned long long)cputime64_to_clock_t(iowait),
		(unsigned long long)cputime64_to_clock_t(irq),
		(unsigned long long)cputime64_to_clock_t(softirq),
		(unsigned long long)0, //cputime64_to_clock_t(steal),
		(unsigned long long)0, //cputime64_to_clock_t(guest),
		(unsigned long long)0);//cputime64_to_clock_t(guest_nice));
	printk(" -----------------------------------------------------------------------------------\n" );
	
	for_each_online_cpu(i) {
		/* Copy values here to work around gcc-2.95.3, gcc-2.96 */
		user = kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice = kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system = kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle = get_idle_time_ram(i);
		iowait = get_iowait_time_ram(i);
		irq = kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq = kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		//steal = kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
		//guest = kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
		//guest_nice = kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];
		
		printk(" cpu %d user:%llu nice:%llu system:%llu idle:%llu iowait:%llu irq:%llu softirq:%llu %llu %llu %llu\n",
			i,
			(unsigned long long)cputime64_to_clock_t(user),
			(unsigned long long)cputime64_to_clock_t(nice),
			(unsigned long long)cputime64_to_clock_t(system),
			(unsigned long long)cputime64_to_clock_t(idle),
			(unsigned long long)cputime64_to_clock_t(iowait),
			(unsigned long long)cputime64_to_clock_t(irq),
			(unsigned long long)cputime64_to_clock_t(softirq),
			(unsigned long long)0, //cputime64_to_clock_t(steal),
			(unsigned long long)0, //cputime64_to_clock_t(guest),
			(unsigned long long)0);//cputime64_to_clock_t(guest_nice));
		
	}
	
	printk(" -----------------------------------------------------------------------------------\n" );
	printk("\n");
	printk(" irq : %llu", (unsigned long long)sum);
	printk(" -----------------------------------------------------------------------------------\n" );
	
	/* sum again ? it could be updated? */
	for_each_irq_nr(j) {
		per_irq_sum = 0;
		for_each_possible_cpu(i)
		per_irq_sum += kstat_irqs_cpu(j, i);
		if(per_irq_sum) printk(" irq-%d : %u\n", j, per_irq_sum);
	}
	
	printk(" -----------------------------------------------------------------------------------\n" );
	printk("\n");
	printk(" softirq : %llu", (unsigned long long)sum_softirq);
	printk(" -----------------------------------------------------------------------------------\n" );
	
	for (i = 0; i < NR_SOFTIRQS; i++)
		if(per_softirq_sums[i]) printk(" softirq-%d : %u", i, per_softirq_sums[i]);
			
	printk(" -----------------------------------------------------------------------------------\n" );
	return 0;
}

static struct GAForensicHELP{
	unsigned int real_pc_from_context_sp;
	unsigned int task_struct_of_gaf_proc;
	unsigned int thread_info_of_gaf_proc;
	unsigned int cpu_context_of_gaf_proc;
}GAFHELP;

DEFINE_SEMAPHORE(g_gaf_mutex);

int gaf_proc(void* data)
{
	volatile int stack[2];

	stack[0] = (int)('_fag');
	stack[1] = (int)('corp');

	down_interruptible(&g_gaf_mutex);
	return 1;
}

void gaf_helper(void)
{
	unsigned int *ptr_task_struct;
	unsigned int *ptr_thread_info;
	unsigned int *ptr_cpu_cntx;
	unsigned int ptr_sp, context_sp;
	unsigned int fn_down_interruptible = (unsigned int)down_interruptible;
	unsigned int fn_down = (unsigned int)down;

	down_interruptible(&g_gaf_mutex);
	ptr_task_struct = kthread_create(gaf_proc, NULL, "gaf-proc");
	wake_up_process(ptr_task_struct);
	msleep(100);

	ptr_thread_info = *(unsigned int*)((unsigned int)ptr_task_struct + GAFINFO.task_struct_struct_stack);
	ptr_cpu_cntx = (unsigned int)ptr_thread_info + GAFINFO.thread_info_struct_cpu_context;

	GAFHELP.task_struct_of_gaf_proc = ptr_task_struct;
	GAFHELP.thread_info_of_gaf_proc = ptr_thread_info;
	GAFHELP.cpu_context_of_gaf_proc = ptr_cpu_cntx;

	printk("\n========== kernel thread : gaf-proc ==========\n");
	printk("task_struct at %x\n", ptr_task_struct);
	printk("thread_info at %x\n\n", ptr_thread_info);

	printk("saved_cpu_context at %x\n", ptr_cpu_cntx);
	printk("%08x r4 :%08x r5 :%08x r6 :%08x r7 :%08x\n", ((unsigned int)ptr_cpu_cntx + 0x00), *(unsigned int*)((unsigned int)ptr_cpu_cntx + 0x00), *(unsigned int*)((unsigned int)ptr_cpu_cntx + 0x04), *(unsigned int*)((unsigned int)ptr_cpu_cntx + 0x08), *(unsigned int*)((unsigned int)ptr_cpu_cntx + 0x0c));
	printk("%08x r8 :%08x r9 :%08x r10:%08x r11:%08x\n", ((unsigned int)ptr_cpu_cntx + 0x10), *(unsigned int*)((unsigned int)ptr_cpu_cntx + 0x10), *(unsigned int*)((unsigned int)ptr_cpu_cntx + 0x14), *(unsigned int*)((unsigned int)ptr_cpu_cntx + 0x18), *(unsigned int*)((unsigned int)ptr_cpu_cntx + 0x1c));
	printk("%08x sp :%08x pc :%08x \n\n", ((unsigned int)ptr_cpu_cntx + 0x20), *(unsigned int*)((unsigned int)ptr_cpu_cntx + 0x20), *(unsigned int*)((unsigned int)ptr_cpu_cntx + 0x24));
	ptr_sp = context_sp = *(unsigned int*)((unsigned int)ptr_cpu_cntx + GAFINFO.cpu_context_save_struct_sp);

	printk("searching saved pc which is stopped in down_interruptible() from %08x to %08x\n", ptr_sp, (unsigned int)ptr_thread_info + THREAD_SIZE);
	printk("down_interruptible() is from %08x to %08x\n\n", fn_down_interruptible, fn_down);

	while(ptr_sp < (unsigned int)ptr_thread_info + THREAD_SIZE) {
		//printk("%08x at %08x\n", *(unsigned int*)ptr_sp, ptr_sp);
		if( fn_down_interruptible <= *(unsigned int*)ptr_sp && *(unsigned int*)ptr_sp < fn_down ) {
			printk("pc (%08x) is found at %08x\n", *(unsigned int*)ptr_sp, ptr_sp);
			break;
		}
		ptr_sp += 4;
	}

	if(ptr_sp < (unsigned int)ptr_thread_info + THREAD_SIZE ) {

		GAFHELP.real_pc_from_context_sp = ptr_sp -context_sp;	
		printk("%08x r4 :xxxxxxxx r5 :%08x r6 :%08x r7 :%08x\n", (ptr_sp -0x2c), *(unsigned int*)(ptr_sp -0x28), *(unsigned int*)(ptr_sp -0x24), *(unsigned int*)(ptr_sp -0x20));
		printk("%08x r8 :%08x r9 :%08x r10:%08x r11:%08x\n", (ptr_sp -0x1c), *(unsigned int*)(ptr_sp -0x1c), *(unsigned int*)(ptr_sp -0x18), *(unsigned int*)(ptr_sp -0x14), *(unsigned int*)(ptr_sp -0x10));
		printk("%08x r12:%08x sp :%08x lr :%08x pc :%08x\n", (ptr_sp -0x0c), *(unsigned int*)(ptr_sp -0x0c), *(unsigned int*)(ptr_sp -0x08), *(unsigned int*)(ptr_sp -0x04), *(unsigned int*)(ptr_sp -0x00));
	} else {
		GAFHELP.real_pc_from_context_sp = 0xFFFFFFFF;
		printk("pc is not found\n");
	} 
	printk("===================\n\n");
}
#endif

/*
 * struct logger_log - represents a specific log, such as 'main' or 'radio'
 *
 * This structure lives from module insertion until module removal, so it does
 * not need additional reference counting. The structure is protected by the
 * mutex 'mutex'.
 */
struct logger_log {
	unsigned char		*buffer;/* the ring buffer itself */
	struct miscdevice	misc;	/* misc device representing the log */
	wait_queue_head_t	wq;	/* wait queue for readers */
	struct list_head	readers; /* this log's readers */
	struct mutex		mutex;	/* mutex protecting buffer */
	size_t			w_off;	/* current write head offset */
	size_t			head;	/* new readers start here */
	size_t			size;	/* size of the log */
};

/*
 * struct logger_reader - a logging device open for reading
 *
 * This object lives from open to release, so we don't need additional
 * reference counting. The structure is protected by log->mutex.
 */
struct logger_reader {
	struct logger_log	*log;	/* associated log */
	struct list_head	list;	/* entry in logger_log's list */
	size_t			r_off;	/* current read head offset */
	bool			r_all;	/* reader can read all entries */
	int			r_ver;	/* reader ABI version */
};

/* logger_offset - returns index 'n' into the log via (optimized) modulus */
size_t logger_offset(struct logger_log *log, size_t n)
{
	return n & (log->size-1);
}


/*
 * file_get_log - Given a file structure, return the associated log
 *
 * This isn't aesthetic. We have several goals:
 *
 *	1) Need to quickly obtain the associated log during an I/O operation
 *	2) Readers need to maintain state (logger_reader)
 *	3) Writers need to be very fast (open() should be a near no-op)
 *
 * In the reader case, we can trivially go file->logger_reader->logger_log.
 * For a writer, we don't want to maintain a logger_reader, so we just go
 * file->logger_log. Thus what file->private_data points at depends on whether
 * or not the file was opened for reading. This function hides that dirtiness.
 */
static inline struct logger_log *file_get_log(struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		return reader->log;
	} else
		return file->private_data;
}

/*
 * get_entry_header - returns a pointer to the logger_entry header within
 * 'log' starting at offset 'off'. A temporary logger_entry 'scratch' must
 * be provided. Typically the return value will be a pointer within
 * 'logger->buf'.  However, a pointer to 'scratch' may be returned if
 * the log entry spans the end and beginning of the circular buffer.
 */
static struct logger_entry *get_entry_header(struct logger_log *log,
		size_t off, struct logger_entry *scratch)
{
	size_t len = min(sizeof(struct logger_entry), log->size - off);
	if (len != sizeof(struct logger_entry)) {
		memcpy(((void *) scratch), log->buffer + off, len);
		memcpy(((void *) scratch) + len, log->buffer,
			sizeof(struct logger_entry) - len);
		return scratch;
	}

	return (struct logger_entry *) (log->buffer + off);
}

/*
 * get_entry_msg_len - Grabs the length of the message of the entry
 * starting from from 'off'.
 *
 * An entry length is 2 bytes (16 bits) in host endian order.
 * In the log, the length does not include the size of the log entry structure.
 * This function returns the size including the log entry structure.
 *
 * Caller needs to hold log->mutex.
 */
static __u32 get_entry_msg_len(struct logger_log *log, size_t off)
{
	struct logger_entry scratch;
	struct logger_entry *entry;

	entry = get_entry_header(log, off, &scratch);
	return entry->len;
}

static size_t get_user_hdr_len(int ver)
{
	if (ver < 2)
		return sizeof(struct user_logger_entry_compat);
	else
		return sizeof(struct logger_entry);
}

static ssize_t copy_header_to_user(int ver, struct logger_entry *entry,
					 char __user *buf)
{
	void *hdr;
	size_t hdr_len;
	struct user_logger_entry_compat v1;

	if (ver < 2) {
		v1.len      = entry->len;
		v1.__pad    = 0;
		v1.pid      = entry->pid;
		v1.tid      = entry->tid;
		v1.sec      = entry->sec;
		v1.nsec     = entry->nsec;
		hdr         = &v1;
		hdr_len     = sizeof(struct user_logger_entry_compat);
	} else {
		hdr         = entry;
		hdr_len     = sizeof(struct logger_entry);
	}

	return copy_to_user(buf, hdr, hdr_len);
}

/*
 * do_read_log_to_user - reads exactly 'count' bytes from 'log' into the
 * user-space buffer 'buf'. Returns 'count' on success.
 *
 * Caller must hold log->mutex.
 */
static ssize_t do_read_log_to_user(struct logger_log *log,
				   struct logger_reader *reader,
				   char __user *buf,
				   size_t count)
{
	struct logger_entry scratch;
	struct logger_entry *entry;
	size_t len;
	size_t msg_start;

	/*
	 * First, copy the header to userspace, using the version of
	 * the header requested
	 */
	entry = get_entry_header(log, reader->r_off, &scratch);
	if (copy_header_to_user(reader->r_ver, entry, buf))
		return -EFAULT;

	count -= get_user_hdr_len(reader->r_ver);
	buf += get_user_hdr_len(reader->r_ver);
	msg_start = logger_offset(log,
		reader->r_off + sizeof(struct logger_entry));

	/*
	 * We read from the msg in two disjoint operations. First, we read from
	 * the current msg head offset up to 'count' bytes or to the end of
	 * the log, whichever comes first.
	 */
	len = min(count, log->size - msg_start);
	if (copy_to_user(buf, log->buffer + msg_start, len))
		return -EFAULT;

	/*
	 * Second, we read any remaining bytes, starting back at the head of
	 * the log.
	 */
	if (count != len)
		if (copy_to_user(buf + len, log->buffer, count - len))
			return -EFAULT;

	reader->r_off = logger_offset(log, reader->r_off +
		sizeof(struct logger_entry) + count);

	return count + get_user_hdr_len(reader->r_ver);
}

/*
 * get_next_entry_by_uid - Starting at 'off', returns an offset into
 * 'log->buffer' which contains the first entry readable by 'euid'
 */
static size_t get_next_entry_by_uid(struct logger_log *log,
		size_t off, uid_t euid)
{
	while (off != log->w_off) {
		struct logger_entry *entry;
		struct logger_entry scratch;
		size_t next_len;

		entry = get_entry_header(log, off, &scratch);

		if (entry->euid == euid)
			return off;

		next_len = sizeof(struct logger_entry) + entry->len;
		off = logger_offset(log, off + next_len);
	}

	return off;
}

/*
 * logger_read - our log's read() method
 *
 * Behavior:
 *
 *	- O_NONBLOCK works
 *	- If there are no log entries to read, blocks until log is written to
 *	- Atomically reads exactly one log entry
 *
 * Will set errno to EINVAL if read
 * buffer is insufficient to hold next entry.
 */
static ssize_t logger_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	struct logger_reader *reader = file->private_data;
	struct logger_log *log = reader->log;
	ssize_t ret;
	DEFINE_WAIT(wait);

start:
	while (1) {
		mutex_lock(&log->mutex);

		prepare_to_wait(&log->wq, &wait, TASK_INTERRUPTIBLE);

		ret = (log->w_off == reader->r_off);
		mutex_unlock(&log->mutex);
		if (!ret)
			break;

		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}

		schedule();
	}

	finish_wait(&log->wq, &wait);
	if (ret)
		return ret;

	mutex_lock(&log->mutex);

	if (!reader->r_all)
		reader->r_off = get_next_entry_by_uid(log,
			reader->r_off, current_euid());

	/* is there still something to read or did we race? */
	if (unlikely(log->w_off == reader->r_off)) {
		mutex_unlock(&log->mutex);
		goto start;
	}

	/* get the size of the next entry */
	ret = get_user_hdr_len(reader->r_ver) +
		get_entry_msg_len(log, reader->r_off);
	if (count < ret) {
		ret = -EINVAL;
		goto out;
	}

	/* get exactly one entry from the log */
	ret = do_read_log_to_user(log, reader, buf, ret);

out:
	mutex_unlock(&log->mutex);

	return ret;
}

/*
 * get_next_entry - return the offset of the first valid entry at least 'len'
 * bytes after 'off'.
 *
 * Caller must hold log->mutex.
 */
static size_t get_next_entry(struct logger_log *log, size_t off, size_t len)
{
	size_t count = 0;

	do {
		size_t nr = sizeof(struct logger_entry) +
			get_entry_msg_len(log, off);
		off = logger_offset(log, off + nr);
		count += nr;
	} while (count < len);

	return off;
}

/*
 * is_between - is a < c < b, accounting for wrapping of a, b, and c
 *    positions in the buffer
 *
 * That is, if a<b, check for c between a and b
 * and if a>b, check for c outside (not between) a and b
 *
 * |------- a xxxxxxxx b --------|
 *               c^
 *
 * |xxxxx b --------- a xxxxxxxxx|
 *    c^
 *  or                    c^
 */
static inline int is_between(size_t a, size_t b, size_t c)
{
	if (a < b) {
		/* is c between a and b? */
		if (a < c && c <= b)
			return 1;
	} else {
		/* is c outside of b through a? */
		if (c <= b || a < c)
			return 1;
	}

	return 0;
}

/*
 * fix_up_readers - walk the list of all readers and "fix up" any who were
 * lapped by the writer; also do the same for the default "start head".
 * We do this by "pulling forward" the readers and start head to the first
 * entry after the new write head.
 *
 * The caller needs to hold log->mutex.
 */
static void fix_up_readers(struct logger_log *log, size_t len)
{
	size_t old = log->w_off;
	size_t new = logger_offset(log, old + len);
	struct logger_reader *reader;

	if (is_between(old, new, log->head))
		log->head = get_next_entry(log, log->head, len);

	list_for_each_entry(reader, &log->readers, list)
		if (is_between(old, new, reader->r_off))
			reader->r_off = get_next_entry(log, reader->r_off, len);
}

/*
 * do_write_log - writes 'len' bytes from 'buf' to 'log'
 *
 * The caller needs to hold log->mutex.
 */
static void do_write_log(struct logger_log *log, const void *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	memcpy(log->buffer + log->w_off, buf, len);

	if (count != len)
		memcpy(log->buffer, buf + len, count - len);

	log->w_off = logger_offset(log, log->w_off + count);

}

/*
 * do_write_log_user - writes 'len' bytes from the user-space buffer 'buf' to
 * the log 'log'
 *
 * The caller needs to hold log->mutex.
 *
 * Returns 'count' on success, negative error code on failure.
 */
static ssize_t do_write_log_from_user(struct logger_log *log,
				      const void __user *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	if (len && copy_from_user(log->buffer + log->w_off, buf, len))
		return -EFAULT;

	if (count != len)
		if (copy_from_user(log->buffer, buf + len, count - len))
			/*
			 * Note that by not updating w_off, this abandons the
			 * portion of the new entry that *was* successfully
			 * copied, just above.  This is intentional to avoid
			 * message corruption from missing fragments.
			 */
			return -EFAULT;

#ifdef CONFIG_APPLY_GA_SOLUTION
// @message
	memset(klog_buf,0,255);
	if(strncmp(log->buffer + log->w_off, "!@", 2) == 0) {
		if (count < 255)
			memcpy(klog_buf,log->buffer + log->w_off, count);
		else
			memcpy(klog_buf,log->buffer + log->w_off, 255);

		klog_buf[255]=0;
	}
#endif

	log->w_off = logger_offset(log, log->w_off + count);

	return count;
}

/*
 * logger_aio_write - our write method, implementing support for write(),
 * writev(), and aio_write(). Writes are our fast path, and we try to optimize
 * them above all else.
 */
/* cpu currently holding logbuf_lock */
#ifdef ADD_SYSTEM_TIMEINFO
static volatile unsigned int logger_cpu = UINT_MAX;
#endif
ssize_t logger_aio_write(struct kiocb *iocb, const struct iovec *iov,
			 unsigned long nr_segs, loff_t ppos)
{
	struct logger_log *log = file_get_log(iocb->ki_filp);
	size_t orig = log->w_off;
	struct logger_entry header;
	struct timespec now;
	ssize_t ret = 0;

#ifdef ADD_SYSTEM_TIMEINFO
	char tbuf[50], *tp;
	unsigned tlen;
	unsigned long long t;
	unsigned long nanosec_rem;
#endif	

	now = current_kernel_time();

	header.pid = current->tgid;
	header.tid = current->pid;
	header.sec = now.tv_sec;
	header.nsec = now.tv_nsec;
	header.euid = current_euid();
	header.len = min_t(size_t, iocb->ki_left, LOGGER_ENTRY_MAX_PAYLOAD);
	header.hdr_size = sizeof(struct logger_entry);

#ifdef ADD_SYSTEM_TIMEINFO
	/* Follow the token with the time */
	memset(tbuf, 0, sizeof(tbuf));

	t = cpu_clock(logger_cpu);
	nanosec_rem = do_div(t, 1000000000);
	tlen = sprintf(tbuf, "[%5lu.%06lu] ",
			(unsigned long) t,
			nanosec_rem / 1000);
	header.system_sec = (unsigned long) t;
	header.system_nsec = nanosec_rem / 1000;
#endif

	/* null writes succeed, return zero */
	if (unlikely(!header.len))
		return 0;

	mutex_lock(&log->mutex);

	/*
	 * Fix up any readers, pulling them forward to the first readable
	 * entry after (what will be) the new write offset. We do this now
	 * because if we partially fail, we can end up with clobbered log
	 * entries that encroach on readable buffer.
	 */
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	do_write_log(log, &header, sizeof(struct logger_entry));

	while (nr_segs-- > 0) {
		size_t len;
		ssize_t nr;

		/* figure out how much of this vector we can keep */
		len = min_t(size_t, iov->iov_len, header.len - ret);

		/* write out this segment's payload */
		nr = do_write_log_from_user(log, iov->iov_base, len);
		if (unlikely(nr < 0)) {
			log->w_off = orig;
			mutex_unlock(&log->mutex);
			return nr;
		}

		iov++;
		ret += nr;
	}

	mutex_unlock(&log->mutex);

	/* wake up any blocked readers */
	wake_up_interruptible(&log->wq);

#ifdef CONFIG_APPLY_GA_SOLUTION
// @message
	if(strncmp(klog_buf, "!@", 2) == 0)
	{
		printk("%s\n",klog_buf);
	}
#endif

	return ret;
}

unsigned long get_platform_reset_count(void)
{
	return platform_reset_count;
}

static struct logger_log *get_log_from_minor(int);

/*
 * logger_open - the log's open() file operation
 *
 * Note how near a no-op this is in the write-only case. Keep it that way!
 */
static int logger_open(struct inode *inode, struct file *file)
{
	struct logger_log *log;
	int ret;

	ret = nonseekable_open(inode, file);
	if (ret)
		return ret;

	log = get_log_from_minor(MINOR(inode->i_rdev));
	if (!log)
		return -ENODEV;

	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader;

		reader = kmalloc(sizeof(struct logger_reader), GFP_KERNEL);
		if (!reader)
			return -ENOMEM;

		reader->log = log;
		reader->r_ver = 1;
		reader->r_all = in_egroup_p(inode->i_gid) ||
			capable(CAP_SYSLOG);

		INIT_LIST_HEAD(&reader->list);

		mutex_lock(&log->mutex);
		reader->r_off = log->head;
		list_add_tail(&reader->list, &log->readers);
		mutex_unlock(&log->mutex);

		file->private_data = reader;
	} else
		file->private_data = log;

	return 0;
}

/*
 * logger_release - the log's release file operation
 *
 * Note this is a total no-op in the write-only case. Keep it that way!
 */
static int logger_release(struct inode *ignored, struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		struct logger_log *log = reader->log;

		mutex_lock(&log->mutex);
		list_del(&reader->list);
		mutex_unlock(&log->mutex);

		kfree(reader);
	}

	return 0;
}

/*
 * logger_poll - the log's poll file operation, for poll/select/epoll
 *
 * Note we always return POLLOUT, because you can always write() to the log.
 * Note also that, strictly speaking, a return value of POLLIN does not
 * guarantee that the log is readable without blocking, as there is a small
 * chance that the writer can lap the reader in the interim between poll()
 * returning and the read() request.
 */
static unsigned int logger_poll(struct file *file, poll_table *wait)
{
	struct logger_reader *reader;
	struct logger_log *log;
	unsigned int ret = POLLOUT | POLLWRNORM;

	if (!(file->f_mode & FMODE_READ))
		return ret;

	reader = file->private_data;
	log = reader->log;

	poll_wait(file, &log->wq, wait);

	mutex_lock(&log->mutex);
	if (!reader->r_all)
		reader->r_off = get_next_entry_by_uid(log,
			reader->r_off, current_euid());

	if (log->w_off != reader->r_off)
		ret |= POLLIN | POLLRDNORM;
	mutex_unlock(&log->mutex);

	return ret;
}

static long logger_set_version(struct logger_reader *reader, void __user *arg)
{
	int version;
	if (copy_from_user(&version, arg, sizeof(int)))
		return -EFAULT;

	if ((version < 1) || (version > 2))
		return -EINVAL;

	reader->r_ver = version;
	return 0;
}

static long logger_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct logger_log *log = file_get_log(file);
	struct logger_reader *reader;
	long ret = -EINVAL;
	void __user *argp = (void __user *) arg;

	mutex_lock(&log->mutex);

	switch (cmd) {
	case LOGGER_GET_LOG_BUF_SIZE:
		ret = log->size;
		break;
	case LOGGER_GET_LOG_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		if (log->w_off >= reader->r_off)
			ret = log->w_off - reader->r_off;
		else
			ret = (log->size - reader->r_off) + log->w_off;
		break;
	case LOGGER_GET_NEXT_ENTRY_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;

		if (!reader->r_all)
			reader->r_off = get_next_entry_by_uid(log,
				reader->r_off, current_euid());

		if (log->w_off != reader->r_off)
			ret = get_user_hdr_len(reader->r_ver) +
				get_entry_msg_len(log, reader->r_off);
		else
			ret = 0;
		break;
	case LOGGER_FLUSH_LOG:
		if (!(file->f_mode & FMODE_WRITE)) {
			ret = -EBADF;
			break;
		}
		list_for_each_entry(reader, &log->readers, list)
			reader->r_off = log->w_off;
		log->head = log->w_off;
		ret = 0;
		break;
	case LOGGER_GET_VERSION:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		ret = reader->r_ver;
		break;
	case LOGGER_SET_VERSION:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		ret = logger_set_version(reader, argp);
		break;
	}

	mutex_unlock(&log->mutex);

	return ret;
}

static const struct file_operations logger_fops = {
	.owner = THIS_MODULE,
	.read = logger_read,
	.aio_write = logger_aio_write,
	.poll = logger_poll,
	.unlocked_ioctl = logger_ioctl,
	.compat_ioctl = logger_ioctl,
	.open = logger_open,
	.release = logger_release,
};

/*
 * Defines a log structure with name 'NAME' and a size of 'SIZE' bytes, which
 * must be a power of two, and greater than
 * (LOGGER_ENTRY_MAX_PAYLOAD + sizeof(struct logger_entry)).
 */
#define DEFINE_LOGGER_DEVICE(VAR, NAME, SIZE) \
static unsigned char _buf_ ## VAR[SIZE]; \
static struct logger_log VAR = { \
	.buffer = _buf_ ## VAR, \
	.misc = { \
		.minor = MISC_DYNAMIC_MINOR, \
		.name = NAME, \
		.fops = &logger_fops, \
		.parent = NULL, \
	}, \
	.wq = __WAIT_QUEUE_HEAD_INITIALIZER(VAR .wq), \
	.readers = LIST_HEAD_INIT(VAR .readers), \
	.mutex = __MUTEX_INITIALIZER(VAR .mutex), \
	.w_off = 0, \
	.head = 0, \
	.size = SIZE, \
};
#if defined(CONFIG_MACH_DELOS_OPEN) || defined(CONFIG_MACH_ARUBA_OPEN) || defined(CONFIG_MACH_ARUBASLIM_OPEN) || defined(CONFIG_MACH_ARUBA_CTC) || defined(CONFIG_MACH_DELOS_CTC)
DEFINE_LOGGER_DEVICE(log_main, LOGGER_LOG_MAIN, 2048*1024)
#else
DEFINE_LOGGER_DEVICE(log_main, LOGGER_LOG_MAIN, 512*1024)
#endif
DEFINE_LOGGER_DEVICE(log_events, LOGGER_LOG_EVENTS, 256*1024)
#if defined(CONFIG_MACH_DELOS_OPEN)|| defined(CONFIG_MACH_ARUBA_OPEN) || defined(CONFIG_MACH_ARUBASLIM_OPEN) || defined(CONFIG_MACH_ARUBA_CTC)  || defined(CONFIG_MACH_DELOS_CTC)
DEFINE_LOGGER_DEVICE(log_radio, LOGGER_LOG_RADIO, 2048*1024)
#else
DEFINE_LOGGER_DEVICE(log_radio, LOGGER_LOG_RADIO, 256*1024)
#endif
DEFINE_LOGGER_DEVICE(log_system, LOGGER_LOG_SYSTEM, 256*1024)

static struct logger_log *get_log_from_minor(int minor)
{
	if (log_main.misc.minor == minor)
		return &log_main;
	if (log_events.misc.minor == minor)
		return &log_events;
	if (log_radio.misc.minor == minor)
		return &log_radio;
	if (log_system.misc.minor == minor)
		return &log_system;
	return NULL;
}

static int __init init_log(struct logger_log *log)
{
	int ret;

	ret = misc_register(&log->misc);
	if (unlikely(ret)) {
		printk(KERN_ERR "logger: failed to register misc "
		       "device for log '%s'!\n", log->misc.name);
		return ret;
	}

	printk("logger: created %luK log '%s'\n",
	       (unsigned long) log->size >> 10, log->misc.name);

	return 0;
}

#ifdef CONFIG_APPLY_GA_SOLUTION
/* Mark for GetLog */

struct struct_plat_log_mark  {
	u32 special_mark_1;
	u32 special_mark_2;
	u32 special_mark_3;
	u32 special_mark_4;
	void *p_main;
	void *p_radio;
	void *p_events;
	void *p_system;
};

struct struct_marks_ver_mark {
  u32 special_mark_1;
  u32 special_mark_2;
  u32 special_mark_3;
  u32 special_mark_4;
  u32 log_mark_version;
  u32 framebuffer_mark_version;
  void * this;
  u32 first_size;
  u32 first_start_addr;
  u32 second_size;
  u32 second_start_addr;
  u32 third_size;
  u32 third_start_addr;
};


static struct struct_plat_log_mark plat_log_mark = {
	.special_mark_1 = (('*' << 24) | ('^' << 16) | ('^' << 8) | ('*' << 0)),
	.special_mark_2 = (('I' << 24) | ('n' << 16) | ('f' << 8) | ('o' << 0)),
	.special_mark_3 = (('H' << 24) | ('e' << 16) | ('r' << 8) | ('e' << 0)),
	.special_mark_4 = (('p' << 24) | ('l' << 16) | ('o' << 8) | ('g' << 0)),
	.p_main = 0, 
	.p_radio = 0,
	.p_events = 0,
	.p_system = 0, 
};


static struct struct_marks_ver_mark marks_ver_mark = {
  .special_mark_1 = (('*' << 24) | ('^' << 16) | ('^' << 8) | ('*' << 0)),
  .special_mark_2 = (('I' << 24) | ('n' << 16) | ('f' << 8) | ('o' << 0)),
  .special_mark_3 = (('H' << 24) | ('e' << 16) | ('r' << 8) | ('e' << 0)),
  .special_mark_4 = (('v' << 24) | ('e' << 16) | ('r' << 8) | ('s' << 0)),
  .log_mark_version = 1,
  .framebuffer_mark_version = 1,
  .this=(&marks_ver_mark + 0x200000),
//  .first_size=256*1024*1024,
  .first_size=512*1024*1024,
  .first_start_addr=0x200000,
  .second_size=0,
  .second_start_addr=0,
  .third_size=0,
  .third_start_addr=0
};
#endif

static int __init logger_init(void)
{
	int ret;

#ifdef CONFIG_APPLY_GA_SOLUTION
	/* Mark for GetLog */
	plat_log_mark.p_main   = _buf_log_main+0x200000;
	plat_log_mark.p_radio  = _buf_log_radio+0x200000;
	plat_log_mark.p_events = _buf_log_events+0x200000;
	plat_log_mark.p_system = _buf_log_system+0x200000;	
	marks_ver_mark.log_mark_version = 1; 
#endif

#ifdef CONFIG_APPLY_GA_SOLUTION
// GAF
	gaf_helper();
#endif

	ret = init_log(&log_main);
	if (unlikely(ret))
		goto out;

	ret = init_log(&log_events);
	if (unlikely(ret))
		goto out;

	ret = init_log(&log_radio);
	if (unlikely(ret))
		goto out;

	ret = init_log(&log_system);
	if (unlikely(ret))
		goto out;

out:
	return ret;
}
device_initcall(logger_init);
