#ifndef __QNXCOMM_CHANNEL_H
#define __QNXCOMM_CHANNEL_H


#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/kref.h>
#include <linux/wait.h>


// forward decls
struct qnx_internal_msgsend;


struct qnx_channel
{
   struct list_head hook;
   struct kref refcnt;
      
   int chid;
   
   struct list_head waiting;
   spinlock_t waiting_lock;
   
   wait_queue_head_t waiting_queue;
   atomic_t num_waiting;     ///< wait queue helper flag
   int num_waiting_noreply;
};


// ---------------------------------------------------------------------


/// construction/destruction
int qnx_channel_init(struct qnx_channel* chnl);

void qnx_channel_release(struct qnx_channel* chnl);


/// messages management
int qnx_channel_add_new_message(struct qnx_channel* chnl, struct qnx_internal_msgsend* data);

int qnx_channel_remove_message(struct qnx_channel* chnl, int rcvid);


#endif   // __QNXCOMM_CHANNEL_H

