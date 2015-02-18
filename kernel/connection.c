#include "qnxcomm_internal.h"


void qnx_connection_init(struct qnx_connection* conn, pid_t pid, int chid)
{
   // FIXME implement this somehow!
//   if (index & _NTO_SIDE_CHANNEL)   
   //conn->coid |= _NTO_SIDE_CHANNEL;   
   
   // target channel information
   conn->pid = pid;
   conn->chid = chid;
}
