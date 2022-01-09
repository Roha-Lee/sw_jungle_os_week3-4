#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif


/* States in a thread's life cycle. */
// thread의 라이프 사이클에서 사용되는 상태들 모든 스레드는 하나의 상태만을 갖는다. 
// thread.c 코드는 thread state diagram과 연관지어서 이해하면 좋을 것 같아서 그림으로 그려보면 좋을것 같다.
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
// 다음으로 running할 쓰레드를 결정하기 위한 우선순위 값.
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 * Q) 프로세스 구조에서도 그렇고 여기서도 그렇고 왜 거꾸로 내려올까? 
 * A) 옛날에는 메모리가 부족하여 stack따로, heap따로 공간을 할당하기 어려워서 
 * 중간 부분을 공유하기 위해 이런식으로 설계했다고 한다. 
 * 
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 * Q) 왜 thread구조체에 정의된 순서와 반대일까? 
 * A) "stack"이므로 0kB부터 채우기 시작하면 위와 같은 그림의 형태로 데이터가 들어간다. 
 * 그러면 자연스럽게 magic 다음부터 Kernel stack공간이므로 magic값의 변동을 통해 stackoverflow감지 가능 
 * -> magic number를 건너 뛴다면? -> stackoverflow잡아낼 수 없게 된다. -> 깃북에서 완벽하게 잡아낼 수 없다는 표현이 이런거인듯? 
 * 
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *		스레드 구조체가 너무 크면 실제로 사용할 공간이 부족해지니까 최소한으로 설계해야 한다. 
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *		kernel stack이 너무 커지게 되면 stack overflow발생하여 스레드 구조체가 망가지게 된다. 
 * 		큰 데이터를 non-static local variable에 저장하고 싶은 경우는 그냥 저장하면 kernel stack공간에 들어가서 
 * 		구조체를 망가뜨릴 수 있으므로 heap공간에 넣기 위해 malloc을 사용하자. 
 * 		palloc_get_page는 뭔지 아직 모르겠음 
 * 
 * 
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */

struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. */
	enum thread_status status;          /* Thread state. */
	// 현재 스레드의 상태를 나타냄
	char name[16];                      /* Name (for debugging purposes). */
	// 스레드에 이름을 붙이고 싶을 때 사용 
	int priority;                       /* Priority. */
	int ori_priority;					/* donation받기 이전의 원본 priority */ 
	int awake_time;						/* Awake time */ 
	// ready queue에 있는 스레드를 running으로 바꾸는 순서 결정하는 값
	
	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* List element. */
	struct list_elem d_elem;			/* donation list element */
	struct list donations;				/* List for donator */

	struct lock *wait_on_lock;			/* 현재 lock 해제를 기다리고 있는 Lock */
	
	/* 
		run queue의 원소로 사용되거나 semaphore wait의 원소로 사용.
		동시에 두가지 기능을 할 수 있는 이유는 두 기능이 Mutually exclusive이기 때문이다. 
		run queue에 들어가려면 ready state이어야 하고 
		semaphore wait list에 들어가려면 block state이다. 
		스레드가 동시에 두가지 state를 가질 수 없으므로 elem을 통해 두가지 작업을 수행해도 문제가 생기지 않는다. 
	*/ 
// #ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
	// [Project 2]
	int exit_status;
	struct file **fd_table;
	struct list child_list;
	struct list_elem child_elem;
	struct semaphore sema_fork;
	struct semaphore sema_wait;
	struct semaphore sema_free;
	struct intr_frame parent_if;
	bool is_waited;
	struct file * executable;
// #endif

#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
// true인 경우 multi-level feedback queue 스케쥴러 사용 
// false인 경우 라운드 로빈 방식 스케쥴러 사용 
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

int64_t get_next_wakeup_ticks(void);
void set_next_wakeup_ticks(int64_t);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

void thread_sleep(int64_t);
void thread_awake(int64_t);

int thread_get_priority (void);
void thread_set_priority (int);
void test_max_priority (void);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);
bool cmp_priority(struct list_elem *, struct list_elem *, void *);
bool cmp_donate_priority(struct list_elem *, struct list_elem*, void* UNUSED);

void donate_priority(void);
void remove_with_lock(struct lock *lock); 
void refresh_priority(void);

#endif /* threads/thread.h */
