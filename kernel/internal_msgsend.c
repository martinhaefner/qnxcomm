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


// TODO move to separate header
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,0,0)
static 
int memcpy_fromiovec(unsigned char *kdata, struct iovec *iov, int len)
{
   while (len > 0) 
   {
      if (iov->iov_len) 
      {
         int copy = min_t(unsigned int, len, iov->iov_len);
         if (copy_from_user(kdata, iov->iov_base, copy))
            return -EFAULT;
            
         len -= copy;
         kdata += copy;
         iov->iov_base += copy;
         iov->iov_len -= copy;
      }
      
      iov++;
   }

   return 0;
 }
#endif


// -----------------------------------------------------------------------------


int qnx_internal_msgsend_initv(struct qnx_internal_msgsend* data, struct io_msgsend* io, struct io_msgsendv* _iov, pid_t pid)
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
      
   io->coid = _iov->coid;      
   io->timeout_ms = _iov->timeout_ms;
   io->in.iov_base = inbuf;
   io->in.iov_len = inlen;
   
   io->out.iov_base = outbuf;
   io->out.iov_len = outlen;
   
   data->rcvid = get_new_rcvid();   
   data->status = 0;   
   data->sender_pid = pid;
   
   data->data = io;
   data->pulse = 0;
   
   memset(&data->reply, 0, sizeof(data->reply));
   data->task = 0;
   
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


int qnx_internal_msgsend_init(struct qnx_internal_msgsend* data, struct io_msgsend* io, pid_t pid)
{        
   int rc = 0;
   
   void* buf = kmalloc(io->in.iov_len, GFP_USER);
   if (buf)
   {   
      if (!copy_from_user(buf, io->in.iov_base, io->in.iov_len))
      {   
         io->in.iov_base = buf;      
         
         data->rcvid = get_new_rcvid();   
         data->status = 0;   
         data->sender_pid = pid;
         
         data->data = io;
         data->pulse = 0;
            
         memset(&data->reply, 0, sizeof(data->reply));
         data->task = 0;
      }
      else
      {
         kfree(buf);
         rc = -EFAULT;
      }
   }
   else
      rc = -ENOMEM;
      
   return rc;
}


void qnx_internal_msgsend_init_pulse(struct qnx_internal_msgsend* data, struct io_msgsendpulse* io, pid_t pid)
{
   data->rcvid = 0;   
   data->status = 0;
   data->sender_pid = pid;
   
   data->data = 0;
   data->pulse = io;
   
   memset(&data->reply, 0, sizeof(data->reply));
   data->task = 0;
}


void qnx_internal_msgsend_destroy(struct qnx_internal_msgsend* data)
{      
   kfree(data->data->in.iov_base);
   kfree(data->reply.iov_base);
}


void qnx_internal_msgsend_destroyv(struct qnx_internal_msgsend* data)
{      
   kfree(data->data->in.iov_base);
   kfree(data->data->out.iov_base);
   
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
