#include "qnxcomm_internal.h"


static 
atomic_t gbl_next_connection_id = ATOMIC_INIT(0);     


static
int get_new_connection_id(void)
{
   return atomic_inc_return(&gbl_next_connection_id);
}


int qnx_connection_init(struct qnx_connection* conn, pid_t pid, int chid, int index)
{
   conn->coid = get_new_connection_id();
   
   if (index & _NTO_SIDE_CHANNEL)   
      conn->coid |= _NTO_SIDE_CHANNEL;   
   
   // target channel information
   conn->pid = pid;
   conn->chid = chid;
   
   return conn->coid;
}
