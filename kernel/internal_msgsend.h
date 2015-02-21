#ifndef __QNX_INTERNAL_MSGSEND_H
#define __QNX_INTERNAL_MSGSEND_H


#define QNX_STATE_INITIAL     0
#define QNX_STATE_RECEIVING   1
#define QNX_STATE_PENDING     2
#define QNX_STATE_FINISHED    3


#include <linux/types.h>
#include <linux/sched.h>

#include "qnxcomm_driver.h"


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


// ---------------------------------------------------------------------


/// constructors
int qnx_internal_msgsend_init(struct qnx_internal_msgsend* data, struct qnx_io_msgsend* io, pid_t pid);

int qnx_internal_msgsend_init_noreply(struct qnx_internal_msgsend** out_data, struct qnx_io_msgsend* io, pid_t pid);

int qnx_internal_msgsend_initv(struct qnx_internal_msgsend* data, struct qnx_io_msgsendv* _iov, pid_t pid);

int qnx_internal_msgsend_init_noreplyv(struct qnx_internal_msgsend** data, struct qnx_io_msgsendv* _iov, pid_t pid);

int qnx_internal_msgsend_init_pulse(struct qnx_internal_msgsend* data, struct qnx_io_msgsendpulse* io, pid_t pid);


/// destructors
void qnx_internal_msgsend_cleanup_and_free(struct qnx_internal_msgsend* send_data);

void qnx_internal_msgsend_destroy(struct qnx_internal_msgsend* send_data);

void qnx_internal_msgsend_destroyv(struct qnx_internal_msgsend* data);


#endif   // __QNX_INTERNAL_MSGSEND_H
