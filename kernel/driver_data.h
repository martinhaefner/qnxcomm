#ifndef __QNXCOMM_DRIVER_DATA_H
#define __QNXCOMM_DRIVER_DATA_H


#include <linux/list.h>
#include <linux/spinlock.h>

#include "process_entry.h"


// forward decls
struct qnx_process_entry;
struct qnx_channel;


struct qnx_driver_data
{
   struct list_head process_entries;
   spinlock_t process_entries_lock;   
};


// ---------------------------------------------------------------------


extern void qnx_driver_data_init(struct qnx_driver_data* data);

extern void qnx_driver_data_add_process(struct qnx_driver_data* data, struct qnx_process_entry* entry);

extern void qnx_driver_data_remove(struct qnx_driver_data* data, pid_t pid);

extern struct qnx_process_entry* qnx_driver_data_find_process(struct qnx_driver_data* data, pid_t pid);

extern struct qnx_channel* qnx_driver_data_find_channel(struct qnx_driver_data* data, int pid, int chid);

extern int qnx_driver_data_is_process_available(struct qnx_driver_data* data, pid_t pid);


#endif   // __QNXCOMM_DRIVER_DATA_H
