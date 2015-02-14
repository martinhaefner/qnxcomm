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
   
   void* inbuf = 0;
   void* outbuf = 0;
   
   inbuf = kmalloc(inlen, GFP_USER);   
   if (unlikely(!inbuf))
      goto out;
   
   if (outlen > 0)
   {
      outbuf = kmalloc(outlen, GFP_USER);   
      if (unlikely(!outbuf))
         goto out_free_inbuf;
   }
   
   data->task = current; 
            
   if (unlikely(memcpy_fromiovec(inbuf, _iov->in, inlen)))
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


int qnx_internal_msgsend_init_noreplyv(struct qnx_internal_msgsend** _data, struct qnx_io_msgsendv* _iov, pid_t pid)
{   
   int rc = -ENOMEM;
   
   struct qnx_internal_msgsend* data = *_data;
   
   size_t inlen = iov_length(_iov->in, _iov->in_len);   
   size_t outlen = iov_length(_iov->out, _iov->out_len);
   
   void* allocated = 0;
   void* inbuf = 0;
   void* outbuf = 0;
   
   if (data)
   {
      inbuf = kmalloc(inlen, GFP_USER);   
      if (unlikely(!inbuf))
         goto out;
         
      allocated = inbuf;
      
      if (outlen > 0)
      {
         outbuf = kmalloc(outlen, GFP_USER);   
         if (unlikely(!outbuf))
            goto out_free_inbuf;
      }
      
      data->task = current; 
   }
   else
   {
      data = (struct qnx_internal_msgsend*)kmalloc(sizeof(struct qnx_internal_msgsend) + inlen, GFP_USER);
      if (unlikely(!data))
         goto out;
         
      allocated = data;
      
      inbuf = data + 1;
      
      data->task = 0;   // no reply 
      *_data = data;
   }
            
   if (unlikely(memcpy_fromiovec(inbuf, _iov->in, inlen)))
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
   
   data->state = QNX_STATE_INITIAL;
   
   rc = 0;
   goto out;

out_free_all:
   kfree(outbuf);
   rc = -EFAULT;

out_free_inbuf:
   kfree(allocated);
   
out:
   return rc;
}


int qnx_internal_msgsend_init(struct qnx_internal_msgsend* data, struct qnx_io_msgsend* io, pid_t pid)
{
   void* buf;
   
   memset(data, 0, sizeof(struct qnx_internal_msgsend));
     
   if (unlikely(copy_from_user(&data->data.msg, io, sizeof(struct qnx_io_msgsend))))
      return -EFAULT;
      
   buf = kmalloc(data->data.msg.in.iov_len, GFP_USER);
   if (unlikely(!buf))
      return -ENOMEM;
         
   if (unlikely(copy_from_user(buf, data->data.msg.in.iov_base, data->data.msg.in.iov_len)))
   {
      kfree(buf);
      return -EFAULT;
   }
   
   data->rcvid = get_new_rcvid();   
   data->sender_pid = pid;
   data->task = current;   
   data->state = QNX_STATE_INITIAL;
   data->data.msg.in.iov_base = buf;
   
   return 0;
}


int qnx_internal_msgsend_init_noreply(struct qnx_internal_msgsend** out_data, struct qnx_io_msgsend* io, pid_t pid)
{
   struct qnx_internal_msgsend* data;
   struct qnx_io_msgsend tmp;
   void* buf;
   
   if (unlikely(copy_from_user(&tmp, io, sizeof(struct qnx_io_msgsend))))
      return -EFAULT;
      
   data = (struct qnx_internal_msgsend*)kmalloc(sizeof(struct qnx_internal_msgsend) + tmp.in.iov_len, GFP_USER);
   if (unlikely(!data))
      return -ENOMEM;

   memset(data, 0, sizeof(struct qnx_internal_msgsend));
   memcpy(&data->data, &tmp, sizeof(tmp));
   buf = data + 1;
   
   if (unlikely(copy_from_user(buf, data->data.msg.in.iov_base, data->data.msg.in.iov_len)))
   {
      kfree(data);
      return -EFAULT;
   }
   
   data->rcvid = get_new_rcvid();   
   data->sender_pid = pid;
   data->state = QNX_STATE_INITIAL;
   data->data.msg.in.iov_base = buf;
   // data->task will stay zero here!

   *out_data = data;
   
   return 0;
}


int qnx_internal_msgsend_init_pulse(struct qnx_internal_msgsend* data, struct qnx_io_msgsendpulse* io, pid_t pid)
{
   if (unlikely(copy_from_user(&data->data, io, sizeof(struct qnx_io_msgsendpulse))))
      return -EFAULT;

   data->rcvid = 0;     // is a pulse
   data->status = 0;
   data->sender_pid = pid;
   data->receiver_pid = 0;
   data->task = 0;      // pulses don't have replies
   data->state = QNX_STATE_INITIAL;

   memset(&data->reply, 0, sizeof(data->reply));
   
   return 0;
}


void qnx_internal_msgsend_destroy(struct qnx_internal_msgsend* data)
{      
   // only free if not directly attached data
   if (data->task)
   {
      kfree(data->data.msg.in.iov_base);
      kfree(data->reply.iov_base);
   }
}


void qnx_internal_msgsend_destroyv(struct qnx_internal_msgsend* data)
{      
   if (data->task != 0)
   {
      kfree(data->data.msg.in.iov_base);
      kfree(data->data.msg.out.iov_base);
   
      kfree(data->reply.iov_base);
   }
}


void qnx_internal_msgsend_cleanup_and_free(struct qnx_internal_msgsend* send_data)
{   
   // anybody waiting for response?
   if (send_data->task == 0)
   {            
      // pulse or no-reply message...
      kfree(send_data);
   }
   else
   {      
      // normal message
      send_data->status = -ESRCH;
      wake_up_process(send_data->task);
   }       
}
