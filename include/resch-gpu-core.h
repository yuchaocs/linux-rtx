#ifndef __RESCH_GPU_CORE_H__
#define __RESCH_CORE_H__
#include <resch-config.h>
#include <linux/types.h>
#include <resch-gpu-lock.h>
#include <linux/wait.h>

#define MAX 25

#define SCHED_YILED() yield()

#define RESCH_G_PRINT(fmt,arg...) printk(KERN_INFO "[RESCH-G]:" fmt, ##arg)
#define GDEV_DEVICE_MAX_COUNT 2
#define GDEV_CONTEXT_MAX_COUNT 32

/**
  36  * Queueing methods:
  37  * SDQ: Single Device Queue
  38  * MRQ: Multiple Resource Queues
  39  */
#define GDEV_SCHED_SDQ /*GDEV_SCHED_MRQ */

/**
  43  * priority levels.
  44  */
#define GDEV_PRIO_MAX 40
#define GDEV_PRIO_MIN 0
#define GDEV_PRIO_DEFAULT 20

/**
  50  * virtual device period/threshold.
  51  */
#define GDEV_PERIOD_DEFAULT 30000 /* microseconds */
#define GDEV_CREDIT_INACTIVE_THRESHOLD GDEV_PERIOD_DEFAULT
#define GDEV_UPDATE_INTERVAL 1000000

/**
  57  * scheduling properties.
  58  */
#define GDEV_INSTANCES_LIMIT 32


#define GDEV_IOCTL_CTX_CREATE 201
#define GDEV_IOCTL_LAUNCH 202
#define GDEV_IOCTL_SYNC 203
#define GDEV_IOCTL_CLOSE 204

/* list function  */

struct gdev_list {
    struct gdev_list *next;
    struct gdev_list *prev;
    void *container;
};

static inline void gdev_list_init(struct gdev_list *entry, void *container)
{
    entry->next = entry->prev = entry; /* used to be "= NULL" */
    entry->container = container;
}

static inline void gdev_list_add_next(struct gdev_list *entry, struct gdev_list *pos)
{
    struct gdev_list *next = pos->next;

    entry->next = next;
    next->prev = entry;
    entry->prev = pos;
    pos->next = entry;
}

static inline void gdev_list_add_prev(struct gdev_list *entry, struct gdev_list *pos)
{
    struct gdev_list *prev = pos->prev;

    entry->prev = prev;
    prev->next = entry;
    entry->next = pos;
    pos->prev = entry;
}

static inline void gdev_list_add(struct gdev_list *entry, struct gdev_list *head)
{
    return gdev_list_add_next(entry, head);
}

static inline void gdev_list_add_tail(struct gdev_list *entry, struct gdev_list *head)
{
    return gdev_list_add_prev(entry, head);
}

static inline void gdev_list_del(struct gdev_list *entry)
{
    struct gdev_list *next = entry->next;
    struct gdev_list *prev = entry->prev;

    /* if prev is null, @entry points to the head, hence something wrong. */
    prev->next = next;
    next->prev = prev;
    entry->next = entry->prev = entry;
}

static inline int gdev_list_empty(struct gdev_list *entry)
{
    return (entry->next == entry->prev) && (entry->next == entry);
}

static inline struct gdev_list *gdev_list_head(struct gdev_list *head)
{
    /* head->next is the actual head of the list. */
    return (head && !gdev_list_empty(head)) ? head->next : NULL;
}

static inline void *gdev_list_container(struct gdev_list *entry)
{
    return entry ? entry->container : NULL;
}

#define gdev_list_for_each(p, list, entry_name)				\
    for (p = gdev_list_container(gdev_list_head(list));		\
	    p != NULL;											\
	    p = gdev_list_container((p)->entry_name.next))



//time function
//
#ifdef __KERNEL__
#define gettimeofday(x, y) do_gettimeofday(x)
#include <linux/time.h>
#include <linux/types.h>
#else
#include <sys/time.h>
#include <stdint.h>
#endif

#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif
#ifndef NULL
#define NULL 0
#endif

#define USEC_1SEC	1000000
#define USEC_1MSEC	1000
#define MSEC_1SEC	1000

/* compatible with struct timeval. */
struct gdev_time {
    uint64_t sec;
    uint64_t usec;
    int neg;
};

/* ret = current time */
static inline void gdev_time_stamp(struct gdev_time *ret)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ret->sec = tv.tv_sec;
    ret->usec = tv.tv_usec;
    ret->neg = 0;
}

/* generate struct gdev_time from seconds. */
static inline void gdev_time_sec(struct gdev_time *ret, unsigned long sec)
{
    ret->sec = sec;
    ret->usec = 0;
}

/* generate struct gdev_time from milliseconds. */
static inline void gdev_time_ms(struct gdev_time *ret, unsigned long ms)
{
    unsigned long carry = ms / MSEC_1SEC;
    ret->sec = carry;
    ret->usec = (ms - carry * MSEC_1SEC) * USEC_1MSEC;
    ret->neg = 0;
}

/* generate struct gdev_time from microseconds. */
static inline void gdev_time_us(struct gdev_time *ret, unsigned long us)
{
    ret->sec = 0;
    ret->usec = us;
    ret->neg = 0;
}

/* transform from struct gdev_time to seconds. */
static inline unsigned long gdev_time_to_sec(struct gdev_time *p)
{
    return (p->sec * USEC_1SEC + p->usec) / USEC_1SEC;
}

/* transform from struct gdev_time to milliseconds. */
static inline unsigned long gdev_time_to_ms(struct gdev_time *p)
{
    return (p->sec * USEC_1SEC + p->usec) / USEC_1MSEC;
}

/* transform from struct gdev_time to microseconds. */
static inline unsigned long gdev_time_to_us(struct gdev_time *p)
{
    return (p->sec * USEC_1SEC + p->usec);
}

/* clear the timeval values. */
static inline void gdev_time_clear(struct gdev_time *t)
{
    t->sec = t->usec = t->neg = 0;
}


/* x == y */
static inline int gdev_time_eq(struct gdev_time *x, struct gdev_time *y)
{
    return (x->sec == y->sec) && (x->usec == y->usec);
}

/* p == 0 */
static inline int gdev_time_eqz(struct gdev_time *p)
{
    return (p->sec == 0) && (p->usec == 0);
}

/* x > y */
static inline int gdev_time_gt(struct gdev_time *x, struct gdev_time *y)
{
    if (!x->neg && y->neg)
	return true;
    else if (x->neg && !y->neg)
	return false;
    else if (x->neg && y->neg)
	return (x->sec == y->sec) ? (x->usec < y->usec) : (x->sec < y->sec);
    else
	return (x->sec == y->sec) ? (x->usec > y->usec) : (x->sec > y->sec);
}

/* p > 0 */
static inline int gdev_time_gtz(struct gdev_time *p)
{
    return (!p->neg) && ((p->sec > 0) || (p->usec > 0));
}

/* x >= y */
static inline int gdev_time_ge(struct gdev_time *x, struct gdev_time *y)
{
    if (gdev_time_eq(x, y))
	return true;
    else
	return gdev_time_gt(x, y);
}

/* p >= 0 */
static inline int gdev_time_gez(struct gdev_time *p)
{
    return gdev_time_gtz(p) || gdev_time_eqz(p);
}

/* x < y */
static inline int gdev_time_lt(struct gdev_time *x, struct gdev_time *y)
{
    if (!x->neg && y->neg)
	return false;
    else if (x->neg && !y->neg)
	return true;
    else if (x->neg && y->neg)
	return (x->sec == y->sec) ? (x->usec > y->usec) : (x->sec > y->sec);
    else
	return (x->sec == y->sec) ? (x->usec < y->usec) : (x->sec < y->sec);
}

/* p < 0 */
static inline int gdev_time_ltz(struct gdev_time *p)
{
    return p->neg;
}

/* x <= y */
static inline int gdev_time_le(struct gdev_time *x, struct gdev_time *y)
{
    if (gdev_time_eq(x, y))
	return true;
    else
	return gdev_time_lt(x, y);
}

/* p <= 0 */
static inline int gdev_time_lez(struct gdev_time *p)
{
    return gdev_time_ltz(p) || gdev_time_eqz(p);
}

/* ret = x + y (x and y must be positive) */
static inline void __gdev_time_add_pos(struct gdev_time *ret, struct gdev_time *x, struct gdev_time *y)
{
    ret->sec = x->sec + y->sec;
    ret->usec = x->usec + y->usec;
    if (ret->usec >= USEC_1SEC) {
	ret->sec++;
	ret->usec -= USEC_1SEC;
    }
}

/* ret = x - y (x and y must be positive) */
static inline void __gdev_time_sub_pos(struct gdev_time *ret, struct gdev_time *x, struct gdev_time *y)
{
    if (gdev_time_lt(x, y)) {
	struct gdev_time *tmp = x;
	x = y;
	y = tmp;
	ret->neg = 1;
    }
    else
	ret->neg = 0;
    ret->sec = x->sec - y->sec;
    ret->usec = x->usec - y->usec;
    if (ret->usec < 0) {
	ret->sec--;
	ret->usec += USEC_1SEC;
    }
}

/* ret = x + y. */
static inline void gdev_time_add(struct gdev_time *ret, struct gdev_time *x, struct gdev_time *y)
{
    if (ret != x && ret != y)
	gdev_time_clear(ret);

    if (!x->neg && y->neg) { /* x - y */
	y->neg = 0;
	__gdev_time_sub_pos(ret, x, y);
	y->neg = 1;
    }
    else if (x->neg && !y->neg) { /* y - x */
	x->neg = 0;
	__gdev_time_sub_pos(ret, y, x);
	x->neg = 1;
    }
    else if (x->neg && y->neg) { /* - (x + y) */
	x->neg = y-> neg = 0;
	__gdev_time_add_pos(ret, x, y);
	ret->neg = 1;
	x->neg = y-> neg = 1;
    }
    else { /* x + y */
	__gdev_time_add_pos(ret, x, y);
    }
}

/* ret = x - y. */
static inline void gdev_time_sub(struct gdev_time *ret, struct gdev_time *x, struct gdev_time *y)
{
    if (ret != x && ret != y)
	gdev_time_clear(ret);

    if (!x->neg && y->neg) { /* x + y */
	y->neg = 0;
	__gdev_time_add_pos(ret, x, y);
	y->neg = 1;
    }
    else if (x->neg && !y->neg) { /* - (x + y) */
	x->neg = 0;
	__gdev_time_add_pos(ret, y, x);
	ret->neg = 1;
	x->neg = 1;
    }
    else if (x->neg && y->neg) { /* y - x */
	x->neg = y-> neg = 0;
	__gdev_time_sub_pos(ret, y, x);
	x->neg = y-> neg = 1;
    }
    else { /* x - y */
	__gdev_time_sub_pos(ret, x, y);
    }
}

/* ret = x * I. */
static inline void gdev_time_mul(struct gdev_time *ret, struct gdev_time *x, int I)
{
    if (ret != x)
	gdev_time_clear(ret);

    ret->sec = x->sec * I;
    ret->usec = x->usec * I;
    if (ret->usec >= USEC_1SEC) {
	unsigned long carry = ret->usec / USEC_1SEC;
	ret->sec += carry;
	ret->usec -= carry * USEC_1SEC;
    }
}

/* ret = x / I. */
static inline void gdev_time_div(struct gdev_time *ret, struct gdev_time *x, int I)
{
    if (ret != x)
	gdev_time_clear(ret);

    ret->sec = x->sec / I;
    ret->usec = x->usec / I;
}

//////

#include <linux/mutex.h>


struct gdev_device {
    int id; /* device ID */
    int users; /* the number of threads/processes using the device */
    int accessed; /* indicate if any process is on the device */
    int blocked; /* incidate if a process is allowed to access the device */
    uint32_t chipset;
    uint64_t mem_size;
    uint64_t mem_used;
    uint64_t dma_mem_size;
    uint64_t dma_mem_used;
    uint32_t com_bw; /* available compute bandwidth */
    uint32_t mem_bw; /* available memory bandwidth */
    uint32_t mem_sh; /* available memory space share */
    uint32_t period; /* minimum inter-arrival time (us) of replenishment. */
    uint32_t com_bw_used; /* used compute bandwidth */
    uint32_t mem_bw_used; /* used memory bandwidth */
    uint32_t com_time; /* cumulative computation time. */
    uint32_t mem_time; /* cumulative memory transfer time. */
    struct gdev_time credit_com; /* credit of compute execution */
    struct gdev_time credit_mem; /* credit of memory transfer */
    void *priv; /* private device object */
    void *compute; /* private set of compute functions */
    void *sched_com_thread; /* compute scheduler thread */
    void *sched_mem_thread; /* memory scheduler thread */
    void *credit_com_thread; /* compute credit thread */
    void *credit_mem_thread; /* memory credit thread */
    void *current_com; /* current compute execution entity */
    void *current_mem; /* current memory transfer entity */
    struct gdev_device *parent; /* only for virtual devices */
    struct gdev_list list_entry_com; /* entry to active compute list */
    struct gdev_list list_entry_mem; /* entry to active memory list */
    struct gdev_list sched_com_list; /* wait list for compute scheduling */
    struct gdev_list sched_mem_list; /* wait list for memory scheduling */
    struct gdev_list vas_list; /* list of VASes allocated to this device */
    struct gdev_list shm_list; /* list of shm users allocated to this device */
    gdev_lock_t sched_com_lock;
    gdev_lock_t sched_mem_lock;
    gdev_lock_t vas_lock;
    gdev_lock_t global_lock;
    gdev_mutex_t shm_mutex;
    //gdev_mem_t *swap; /* reserved swap memory space */
};


struct gdev_vas{
    int vid;
    void *handle;
    void *pvas;
};

struct gdev_ctx {
	int cid; /* context ID. */
	void *pctx;
	struct gdev_vas *vas;
};
struct gdev_mem;

/**
 * Gdev types: they are not exposed to end users.
 */
typedef struct gdev_vas gdev_vas_t;
typedef struct gdev_ctx gdev_ctx_t;
typedef struct gdev_mem gdev_mem_t;

struct gdev_sched_entity {
    struct gdev_device *gdev; /* associated Gdev (virtual) device */
    void *task; /* private task structure */
    gdev_ctx_t *ctx; /* holder context */
    int prio; /* general priority */
    int rt_prio; /* real-time priority */
    struct gdev_list list_entry_com; /* entry to compute scheduler list */
    struct gdev_list list_entry_mem; /* entry to memory scheduler list */
    struct gdev_time last_tick_com; /* last tick of compute execution */
    struct gdev_time last_tick_mem; /* last tick of memory transfer */
    int launch_instances;
    int memcpy_instances;
    /* for sched_deadline  */
    unsigned long long dl_runtime; /*   */
    unsigned long long dl_deadline;
    struct task_struct * current_task;
    wait_queue_head_t *wqueue;
    int wait_cond;
    int64_t wait_time;

};


/**
 * Gdev handle struct: not visible to outside.
 */
struct gdev_handle {
	struct gdev_device *gdev; /* gdev handle object. */
	struct gdev_sched_entity *se; /* scheduling entity. */
	gdev_vas_t *vas; /* virtual address space object. */
	gdev_ctx_t *ctx; /* device context object. */
	gdev_mem_t **dma_mem; /* host-side DMA memory object (bounce buffer). */
	uint32_t chunk_size; /* configurable memcpy chunk size. */
	int pipeline_count; /* configurable memcpy pipeline count. */
	int dev_id; /* device ID. */
	int fd_resch;
};


struct gdev_vsched_policy {
    void (*schedule_compute)(struct gdev_sched_entity *se);
    struct gdev_device *(*select_next_compute)(struct gdev_device *gdev);
    void (*replenish_compute)(struct gdev_device *gdev);
    void (*schedule_memory)(struct gdev_sched_entity *se);
    struct gdev_device *(*select_next_memory)(struct gdev_device *gdev);
    void (*replenish_memory)(struct gdev_device *gdev);
};

void gdev_schedule_compute(struct gdev_sched_entity *se);
void gdev_select_next_compute(struct gdev_device *gdev);
void gdev_schedule_memory(struct gdev_sched_entity *se);
void gdev_select_next_memory(struct gdev_device *gdev);
void gdev_replenish_credit_compute(struct gdev_device *gdev);
void gdev_replenish_credit_memory(struct gdev_device *gdev);

void gsched_init(void);
void gsched_exit(void);
void* gdev_current_com_get(struct gdev_device *gdev);
void gdev_current_com_set(struct gdev_device *gdev, void *com);
void gdev_lock_init(gdev_lock_t *__p);
void gdev_lock(gdev_lock_t *__p);
void gdev_unlock(gdev_lock_t *__p);
void gdev_lock_save(gdev_lock_t *__p, unsigned long *__pflags);
void gdev_unlock_restore(gdev_lock_t *__p, unsigned long *__pflags);
void gdev_lock_nested(gdev_lock_t *__p);
void gdev_unlock_nested(gdev_lock_t *__p);
void gdev_mutex_init(struct gdev_mutex *__p);
void gdev_mutex_lock(struct gdev_mutex *__p);
void gdev_mutex_unlock(struct gdev_mutex *__p);
struct gdev_device* gdev_phys_get(struct gdev_device *gdev);

extern int gsched_ctxcreate(unsigned long __arg);
extern int gsched_launch(unsigned long __arg);
extern int gsched_sync(unsigned long __arg);


extern struct gdev_device phys[GDEV_DEVICE_MAX_COUNT];
extern struct gdev_device gdev_vds[GDEV_DEVICE_MAX_COUNT];
extern struct gdev_sched_entity *sched_entity_ptr[GDEV_CONTEXT_MAX_COUNT];
extern int gdev_vcount;
extern int gdev_count;

extern gdev_lock_t global_sched_lock;

extern struct gdev_vsched_policy *gdev_vsched;

#endif