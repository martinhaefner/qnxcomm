#ifndef __QNXCOMM_INTERNAL_H
#define __QNXCOMM_INTERNAL_H


#include <linux/sched.h>
#include <linux/list.h>
#include <linux/fs.h>

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


#define QNX_MAX_IOVEC_LEN     5


struct qnx_pollfd
{
   struct list_head hook;
   
   struct file* file;
   int chid;   
};


#endif   // __QNXCOMM_INTERNAL_H
