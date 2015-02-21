#ifndef __QNXCOMM_CONNECTION_H
#define __QNXCOMM_CONNECTION_H


#include <linux/types.h>


struct qnx_connection
{
   pid_t pid;   ///< the real pid, not the task id (i.e. the tgid)
   int chid;    ///< the chid the connection is connected to...
};


// ---------------------------------------------------------------------


static inline
void qnx_connection_init(struct qnx_connection* conn, pid_t pid, int chid)
{
   // target channel information
   conn->pid = pid;
   conn->chid = chid;
}


#endif   // __QNXCOMM_CONNECTION_H
