#include "qnxcomm_internal.h"


void qnx_connection_init(struct qnx_connection* conn, pid_t pid, int chid)
{
   // target channel information
   conn->pid = pid;
   conn->chid = chid;
}
