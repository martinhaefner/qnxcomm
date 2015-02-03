#include "qnxcomm_internal.h"

#include <linux/uio.h>
#include <asm/uaccess.h>


static 
atomic_t gbl_next_rcvid = ATOMIC_INIT(0);      


static
int get_new_rcvid(void)
{
   return atomic_inc_return(&gbl_next_rcvid);
}


int qnx_internal_msgsend_initv(struct qnx_internal_msgsend* data, struct qnx_io_msgsendv* _iov, pid_t pid)
{   
   int rc = -ENOMEM;
   
   size_t inlen = iov_length(_iov->in, _iov->in_len);   
   size_t outlen = iov_length(_iov->out, _iov->out_len);
   
   void* inbuf;
   void* outbuf;
   
   inbuf = kmalloc(inlen, GFP_USER);   
   if (!inbuf)
      goto out;
      
   outbuf = kmalloc(outlen, GFP_USER);   
   if (!outbuf)
      goto out_free_inbuf;
         
   if (memcpy_fromiovec(inbuf, _iov->in, inlen))
      goto out_free_all;
      
   data->rcvid = get_new_rcvid();   
   data->status = 0;   
   data->sender_pid = pid;
   data->receiver_pid = 0;
   
   data->data.msg.coid = _iov->coid;      
   data->data.msg.timeout_ms = _iov->timeout_ms;
   
   data->data.msg.in.iov_base = inbuf;
   data->data.msg.in.iov_len = inlen;
   
   data->data.msg.out.iov_base = outbuf;
   data->data.msg.out.iov_len = outlen;
   
   memset(&data->reply, 0, sizeof(data->reply));
   
   data->task = 0;   
   data->state = QNX_STATE_INITIAL;
   
   rc = 0;
   goto out;

out_free_all:
   kfree(outbuf);
   rc = -EFAULT;

out_free_inbuf:
   kfree(inbuf);
   
out:
   return rc;
}


int qnx_internal_msgsend_init(struct qnx_internal_msgsend* data, struct qnx_io_msgsend* io, pid_t pid)
{        
   void* buf = 0;
   
   if (copy_from_user(&data->data.msg, io, sizeof(struct qnx_io_msgsend)))
      return -EFAULT;
      
   buf = kmalloc(data->data.msg.in.iov_len, GFP_USER);
   if (!buf)
      return -ENOMEM;
   
   if (copy_from_user(buf, data->data.msg.in.iov_base, data->data.msg.in.iov_len))
   {
      kfree(buf);
      return -EFAULT;
   }
      
   data->rcvid = get_new_rcvid();   
   data->status = 0;   
   data->sender_pid = pid;
   data->receiver_pid = 0;
   
   data->data.msg.in.iov_base = buf;           
   memset(&data->reply, 0, sizeof(data->reply));
   
   data->task = current;   
   data->state = QNX_STATE_INITIAL;
   
   return 0;
}


int qnx_internal_msgsend_init_pulse(struct qnx_internal_msgsend* data, struct qnx_io_msgsendpulse* io, pid_t pid)
{
   if (copy_from_user(&data->data, io, sizeof(struct qnx_io_msgsendpulse)))
      return -EFAULT;

   data->rcvid = 0;     // is a pulse
   data->status = 0;
   data->sender_pid = pid;
   data->receiver_pid = 0;
   data->task = 0;
   data->state = QNX_STATE_INITIAL;

   memset(&data->reply, 0, sizeof(data->reply));
   
   return 0;
}


void qnx_internal_msgsend_destroy(struct qnx_internal_msgsend* data)
{      
   kfree(data->data.msg.in.iov_base);
   kfree(data->reply.iov_base);
}


void qnx_internal_msgsend_destroyv(struct qnx_internal_msgsend* data)
{      
   kfree(data->data.msg.in.iov_base);
   kfree(data->data.msg.out.iov_base);
   
   kfree(data->reply.iov_base);
}


void qnx_internal_msgsend_cleanup_and_free(struct qnx_internal_msgsend* send_data)
{   
   if (send_data->rcvid == 0)
   {            
      kfree(send_data);
   }
   else
   {      
      send_data->status = -ESRCH;
      wake_up_process(send_data->task);
   }       
}
