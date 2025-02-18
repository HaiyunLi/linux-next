/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_TASK_STACK_H
#define _LINUX_SCHED_TASK_STACK_H

/*
 * task->stack (kernel stack) handling interfaces:
 */

#include <linux/sched.h>
#include <linux/magic.h>
#include <linux/refcount.h>

#ifdef CONFIG_THREAD_INFO_IN_TASK

/*
 * When accessing the stack of a non-current task that might exit, use
 * try_get_task_stack() instead.  task_stack_page will return a pointer
 * that could get freed out from under you.
 */
static __always_inline void *task_stack_page(const struct task_struct *task)
{
	return task->stack;
}

#define setup_thread_stack(new,old)	do { } while(0)

static __always_inline unsigned long *end_of_stack(const struct task_struct *task)
{
#ifdef CONFIG_STACK_GROWSUP
	return (unsigned long *)((unsigned long)task->stack + THREAD_SIZE) - 1;
#else
	return task->stack;
#endif
}

#elif !defined(__HAVE_THREAD_FUNCTIONS)

#define task_stack_page(task)	((void *)(task)->stack)

static inline void setup_thread_stack(struct task_struct *p, struct task_struct *org)
{
	*task_thread_info(p) = *task_thread_info(org);
	task_thread_info(p)->task = p;
}

/*
 * Return the address of the last usable long on the stack.
 *
 * When the stack grows down, this is just above the thread
 * info struct. Going any lower will corrupt the threadinfo.
 *
 * When the stack grows up, this is the highest address.
 * Beyond that position, we corrupt data on the next page.
 */
static inline unsigned long *end_of_stack(struct task_struct *p)
{
#ifdef CONFIG_STACK_GROWSUP
	return (unsigned long *)((unsigned long)task_thread_info(p) + THREAD_SIZE) - 1;
#else
	return (unsigned long *)(task_thread_info(p) + 1);
#endif
}

#endif

#ifdef CONFIG_THREAD_INFO_IN_TASK
static inline void *try_get_task_stack(struct task_struct *tsk)
{
	return refcount_inc_not_zero(&tsk->stack_refcount) ?
		task_stack_page(tsk) : NULL;
}

extern void put_task_stack(struct task_struct *tsk);
#else
static inline void *try_get_task_stack(struct task_struct *tsk)
{
	return task_stack_page(tsk);
}

static inline void put_task_stack(struct task_struct *tsk) {}
#endif

void exit_task_stack_account(struct task_struct *tsk);

#define task_stack_end_corrupted(task) \
		(*(end_of_stack(task)) != STACK_END_MAGIC)

static inline int object_is_on_stack(const void *obj)
{
	void *stack = task_stack_page(current);

	return (obj >= stack) && (obj < (stack + THREAD_SIZE));
}

extern void thread_stack_cache_init(void);

#ifdef CONFIG_DEBUG_STACK_USAGE
#ifdef CONFIG_VM_EVENT_COUNTERS
#include <linux/vm_event_item.h>

/* Count the maximum pages reached in kernel stacks */
static inline void kstack_histogram(unsigned long used_stack)
{
	if (used_stack <= 1024)
		this_cpu_inc(vm_event_states.event[KSTACK_1K]);
#if THREAD_SIZE > 1024
	else if (used_stack <= 2048)
		this_cpu_inc(vm_event_states.event[KSTACK_2K]);
#endif
#if THREAD_SIZE > 2048
	else if (used_stack <= 4096)
		this_cpu_inc(vm_event_states.event[KSTACK_4K]);
#endif
#if THREAD_SIZE > 4096
	else if (used_stack <= 8192)
		this_cpu_inc(vm_event_states.event[KSTACK_8K]);
#endif
#if THREAD_SIZE > 8192
	else if (used_stack <= 16384)
		this_cpu_inc(vm_event_states.event[KSTACK_16K]);
#endif
#if THREAD_SIZE > 16384
	else if (used_stack <= 32768)
		this_cpu_inc(vm_event_states.event[KSTACK_32K]);
#endif
#if THREAD_SIZE > 32768
	else if (used_stack <= 65536)
		this_cpu_inc(vm_event_states.event[KSTACK_64K]);
#endif
#if THREAD_SIZE > 65536
	else
		this_cpu_inc(vm_event_states.event[KSTACK_REST]);
#endif
}
#else /* !CONFIG_VM_EVENT_COUNTERS */
static inline void kstack_histogram(unsigned long used_stack) {}
#endif /* CONFIG_VM_EVENT_COUNTERS */

static inline unsigned long stack_not_used(struct task_struct *p)
{
	unsigned long *n = end_of_stack(p);
	unsigned long unused_stack;

	do { 	/* Skip over canary */
# ifdef CONFIG_STACK_GROWSUP
		n--;
# else
		n++;
# endif
	} while (!*n);

# ifdef CONFIG_STACK_GROWSUP
	unused_stack = (unsigned long)end_of_stack(p) - (unsigned long)n;
# else
	unused_stack = (unsigned long)n - (unsigned long)end_of_stack(p);
# endif
	kstack_histogram(THREAD_SIZE - unused_stack);

	return unused_stack;
}
#endif
extern void set_task_stack_end_magic(struct task_struct *tsk);

#ifndef __HAVE_ARCH_KSTACK_END
static inline int kstack_end(void *addr)
{
	/* Reliable end of stack detection:
	 * Some APM bios versions misalign the stack
	 */
	return !(((unsigned long)addr+sizeof(void*)-1) & (THREAD_SIZE-sizeof(void*)));
}
#endif

#endif /* _LINUX_SCHED_TASK_STACK_H */
