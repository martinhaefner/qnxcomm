#ifndef __QNXCOMM_INTERNAL_H
#define __QNXCOMM_INTERNAL_H


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/proc_fs.h>
#include <linux/version.h>

#include "qnxcomm_driver.h"
#include "compatibility.h"


// XXX this is a hack: my ARM kernel config does not match the installed kernel -> fixit
#ifdef CONFIG_ARM
#   define current_get_pid_nr(cur) (*(&cur->tgid-1))
#   define current_get_tid_nr(cur) (*(&cur->pid-1))
#else
#   define current_get_pid_nr(cur) (cur->tgid)
#   define current_get_tid_nr(cur) (cur->pid)
#endif


#define QNX_STATE_INITIAL     0
#define QNX_STATE_RECEIVING   1
#define QNX_STATE_PENDING     2
#define QNX_STATE_FINISHED    3

#define QNX_MAX_IOVEC_LEN     5


struct qnx_internal_msgsend
{
   struct list_head hook;
   
   int rcvid;
   int status;
   
   pid_t sender_pid;
   pid_t receiver_pid;
   int receiver_chid;
   
   union
   {
      struct qnx_io_msgsend msg;
      struct qnx_io_msgsendpulse pulse; 
   } data;
      
   struct iovec reply;
   struct task_struct* task;
   
   int state;
};


struct qnx_channel
{
   struct list_head hook;
   struct kref refcnt;
      
   int chid;
   
   struct list_head waiting;
   spinlock_t waiting_lock;
   
   wait_queue_head_t waiting_queue;
   atomic_t num_waiting;     ///< wait queue helper flag
};


struct qnx_connection
{
   pid_t pid;   ///< the real pid, not the task id (i.e. the tgid)
   int chid;    ///< the chid the connection is connected to...
};


struct qnx_connection_table_data
{
   size_t capacity;   ///< capacity of conn array
   int max;           ///< maximum fd set 
   
   struct qnx_connection* conn[1];   
};

typedef void(*ct_callback_t)(int coid, struct qnx_connection*, void*);


struct qnx_connection_table
{
   struct qnx_connection_table_data* data;      
   spinlock_t lock;
};


struct qnx_driver_data
{
   struct list_head process_entries;
   spinlock_t process_entries_lock;   
};


struct qnx_pollfd
{
   struct list_head hook;
   
   struct file* file;
   int chid;   
};


struct qnx_process_entry
{
   struct list_head hook;
   struct kref refcnt;
   
   pid_t pid;
   
   struct list_head channels;
   struct qnx_connection_table connections;
   struct list_head pending;
   struct list_head pollfds;
      
   spinlock_t channels_lock;  
   spinlock_t pending_lock;
   spinlock_t pollfds_lock;
   
   struct qnx_driver_data* driver;
};


// -----------------------------------------------------------------------------


extern int qnx_channel_init(struct qnx_channel* chnl);
extern void qnx_channel_release(struct qnx_channel* chnl);
extern int qnx_channel_add_new_message(struct qnx_channel* chnl, struct qnx_internal_msgsend* data);
extern int qnx_channel_remove_message(struct qnx_channel* chnl, int rcvid);


extern void qnx_connection_init(struct qnx_connection* conn, pid_t pid, int chid);


extern void qnx_driver_data_init(struct qnx_driver_data* data);
extern void qnx_driver_data_add_process(struct qnx_driver_data* data, struct qnx_process_entry* entry);

extern struct qnx_process_entry* qnx_driver_data_find_process(struct qnx_driver_data* data, pid_t pid);
extern void qnx_driver_data_remove(struct qnx_driver_data* data, pid_t pid);
extern struct qnx_channel* qnx_driver_data_find_channel(struct qnx_driver_data* data, int pid, int chid);
extern int qnx_driver_data_is_process_available(struct qnx_driver_data* data, pid_t pid);


extern int qnx_internal_msgsend_init(struct qnx_internal_msgsend* data, struct qnx_io_msgsend* io, pid_t pid);
extern int qnx_internal_msgsend_init_noreply(struct qnx_internal_msgsend** out_data, struct qnx_io_msgsend* io, pid_t pid);
extern int qnx_internal_msgsend_initv(struct qnx_internal_msgsend* data, struct qnx_io_msgsendv* _iov, pid_t pid);
extern int qnx_internal_msgsend_init_noreplyv(struct qnx_internal_msgsend** data, struct qnx_io_msgsendv* _iov, pid_t pid);
extern int qnx_internal_msgsend_init_pulse(struct qnx_internal_msgsend* data, struct qnx_io_msgsendpulse* io, pid_t pid);

extern void qnx_internal_msgsend_cleanup_and_free(struct qnx_internal_msgsend* send_data);
extern void qnx_internal_msgsend_destroy(struct qnx_internal_msgsend* send_data);
extern void qnx_internal_msgsend_destroyv(struct qnx_internal_msgsend* data);

extern void qnx_process_entry_init(struct qnx_process_entry* entry, struct qnx_driver_data* driver);
extern void qnx_process_entry_release(struct qnx_process_entry* entry);

extern int qnx_process_entry_add_channel(struct qnx_process_entry* entry);
extern int qnx_process_entry_remove_channel(struct qnx_process_entry* entry, int chid);
extern struct qnx_channel* qnx_process_entry_find_channel(struct qnx_process_entry* entry, int chid);
extern int qnx_process_entry_is_channel_available(struct qnx_process_entry* entry, int chid);

extern int qnx_process_entry_add_pollfd(struct qnx_process_entry* entry, struct file* f, int chid);
extern int qnx_process_entry_remove_pollfd(struct qnx_process_entry* entry, struct file* f);
extern struct qnx_channel* qnx_process_entry_find_channel_from_file(struct qnx_process_entry* entry, struct file* f);

extern int qnx_process_entry_add_connection(struct qnx_process_entry* entry, struct qnx_io_attach* att_data);
extern int qnx_process_entry_remove_connection(struct qnx_process_entry* entry, int coid);
extern struct qnx_connection qnx_process_entry_find_connection(struct qnx_process_entry* entry, int coid);

extern void qnx_process_entry_add_pending(struct qnx_process_entry* entry, struct qnx_internal_msgsend* data);
extern struct qnx_internal_msgsend* qnx_process_entry_release_pending(struct qnx_process_entry* entry, int rcvid);

extern int qnx_proc_init(struct qnx_driver_data* data);
extern void qnx_proc_destroy(struct qnx_driver_data* data);


extern int qnx_connection_table_init(struct qnx_connection_table* table);
extern void qnx_connection_table_destroy(struct qnx_connection_table* table);
extern int qnx_connection_table_add(struct qnx_connection_table* table, struct qnx_connection* conn);
extern int qnx_connection_table_remove(struct qnx_connection_table* table, int coid);
extern struct qnx_connection qnx_connection_table_retrieve(struct qnx_connection_table* table, int coid);

/// these functions must only be called within a rcu_read_lock critical section.
extern int qnx_connection_table_is_empty(struct qnx_connection_table* table);
extern int qnx_connection_table_for_each(struct qnx_connection_table* table, ct_callback_t func, void* arg);


#endif   // __QNXCOMM_INTERNAL_H
