#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/fdtable.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>

#include "internal_msgsend.h"
#include "qnxcomm_internal.h"
#include "channel.h"
#include "driver_data.h"
#include "proc.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Martin Haefner");
MODULE_DESCRIPTION("QNX like message passing for the Linux kernel");


#define QNX_PROC_ENTRY(f) ((struct qnx_process_entry*)f->private_data)

#define QNX_CONN_IS_VALID(conn) (conn.chid > 0)

#define QNX_FREE_IF_NOT(ptr, stack_buf) \
   if (ptr != stack_buf && ptr != 0)    \
      kfree(ptr);


static struct qnx_driver_data driver_data;
static dev_t dev_number;
static struct cdev* instance;
static struct class* the_class;
static struct device* dev;

int qnx_max_connections_per_process = 256;    ///< like max number of open files
int qnx_max_channels_per_process = 64;        ///< same for channels

uint qnx_max_noreply_msg_size = 4096;         ///< max message size for noreply messages
uint qnx_max_noreply_msg_num = 128;           ///< number of enqueued noreply messages per channel


int set_max_connetions(const char *val, const struct kernel_param *kp)
{
   int max;
   
   if (kstrtoint(val, 10, &max) == 0)
   {   
      // don't allow shrinking
      if (max > *(int*)kp->arg)
      {
         *(int*)kp->arg = max;
         return 0;
      }
   }
      
   return -EINVAL;
}


static 
struct kernel_param_ops ops = {
   .set = &set_max_connetions,
   .get = &param_get_int
};


// module parameters exported to sysfs
module_param_cb(max_connections, &ops, &qnx_max_connections_per_process, 0644);
module_param_cb(max_channels, &ops, &qnx_max_channels_per_process, 0644);
module_param_named(noreply_max_size, qnx_max_noreply_msg_size, uint, 0644);
module_param_named(noreply_per_channel, qnx_max_noreply_msg_num, uint, 0644);


// ---------------------------------------------------------------------


static
int handle_msgsend_internal_block(struct qnx_channel* chnl, struct qnx_internal_msgsend* send_data)
{  
   int rc = 0;
   
   // set the state before it's getting scheduled out
   set_current_state(TASK_INTERRUPTIBLE);
   
   qnx_channel_add_new_message(chnl, send_data); 
   
   pr_debug("MsgSend(v) with timeout=%d ms\n", send_data->data.msg.timeout_ms); 
   
   // now wait for MsgReply...   
   if (send_data->data.msg.timeout_ms > 0)
   {
      if (unlikely(msleep_interruptible(send_data->data.msg.timeout_ms) == 0))
      {
         printk("Timeout\n");
         rc = -ETIMEDOUT;
         goto interrupted;
      }      
   }
   else
      schedule();
   
   // break if we got a signal
   if (unlikely(signal_pending(current)))
   {
      printk("signal\n");
      rc = -ERESTARTSYS;
   }
   else
   {
      rc = send_data->status;
      goto out;
   }
   
interrupted:

printk("interrupted\n");
   if (!qnx_channel_remove_message(chnl, send_data->rcvid))
   { 
      struct qnx_process_entry* entry;
      // object is out of the queue. it could be in the following states
      //
      // RECEIVING: the other side is currently running MsgReceive, at the end,
      //            the object is in the pending state. We have to wait
      //            for the object to get into RECEIVING state (1)
      // PENDING: during this state, this thread may take the object 
      //          out of the pending list. If the object is already in
      //          the pending state we can either grab the object (2)
      //          or wait for finish (3)
      // FINISHED: MsgReply is called on the object, so we are free to continue (4)
          
      while(ACCESS_ONCE(send_data->state) == QNX_STATE_RECEIVING);   // (1) busy loop
      
      // object is already in processing
      entry = qnx_driver_data_find_process(&driver_data, send_data->receiver_pid);
      
      if (entry)
      {              
         if (qnx_process_entry_release_pending(entry, send_data->rcvid) == 0)  
         {       
            rc = send_data->status;       // (2) it's obviously done...      
         }
         else   
         {              
            while(ACCESS_ONCE(send_data->state) == QNX_STATE_FINISHED);    // (3) busy loop
            // finished (4)
         }
         
         qnx_process_entry_release(entry);
      }
   }
   
out:   
   
   qnx_channel_release(chnl);
   
   return rc;
}


static
int handle_msgsendpulse(struct qnx_process_entry* entry, long data)
{
   int rc;
   
   struct qnx_connection conn;
   struct qnx_channel* chnl = 0;
         
   // must allocate data (or reuse some other object)...         
   struct qnx_internal_msgsend* snddata = (struct qnx_internal_msgsend*)kmalloc(sizeof(struct qnx_internal_msgsend), GFP_USER);
   if (unlikely(!snddata))          
   {
      rc = -ENOMEM;                    
      goto out;
   }
         
   rc = qnx_internal_msgsend_init_pulse(snddata, (struct qnx_io_msgsendpulse*)data, entry->pid);
   if (unlikely(rc))
      goto out_free;
         
   pr_debug("MsgSendPulse coid=%d\n", snddata->data.pulse.coid);
   conn = qnx_process_entry_find_connection(entry, snddata->data.pulse.coid);         
         
   if (unlikely(!QNX_CONN_IS_VALID(conn)))
   {
      rc = -EBADF;
      goto out_free;
   }
   
   chnl = qnx_driver_data_find_channel(entry->driver, conn.pid, conn.chid);
   if (unlikely(!chnl))
   {
      rc = -EBADF;
      goto out_free;
   }         
        
   qnx_channel_add_new_message(chnl, snddata);   
   qnx_channel_release(chnl);   
   
   rc = 0;
   goto out;
            
out_free:

   kfree(snddata);
            
out:            

   return rc;
}


static
int handle_msgreceive(struct qnx_process_entry* entry, long data)
{
   int rc;
   struct qnx_io_receive recv_data = { 0 };
   struct qnx_channel* chnl;
   struct qnx_internal_msgsend* send_data;
   struct list_head* ptr;
   size_t bytes_to_copy;      
      
   if (unlikely(copy_from_user(&recv_data, (void*)data, sizeof(struct qnx_io_receive))))
   {   
      rc = -EFAULT;
      goto out;
   }        
   
   chnl = qnx_process_entry_find_channel(entry, recv_data.chid);
   if (unlikely(!chnl))
   {
      rc = -EBADF;
      goto out;
   }
   
   //printk("num waiting: %d\n", atomic_read(&chnl->num_waiting));
   
   rc = wait_event_interruptible_timeout(chnl->waiting_queue, 
        atomic_read(&chnl->num_waiting) > 0, 
        msecs_to_jiffies(recv_data.timeout_ms));
   
   if (unlikely(rc < 0))
   {
      printk("rc<0\n");
      rc = -ERESTARTSYS;
      goto out_channel_release;
   }
   
   spin_lock(&chnl->waiting_lock);
   
   //printk("now num waiting: %d\n", atomic_read(&chnl->num_waiting));
   
   // empty?! maybe spurious wakeup here?!
   if (list_empty(&chnl->waiting))
   {
      pr_debug("Empty...\n");
      spin_unlock(&chnl->waiting_lock);      
      
      rc = -ETIMEDOUT;
      goto out_channel_release;
   }
   
   ptr = chnl->waiting.next;
   atomic_dec(&chnl->num_waiting);   
   
   send_data = list_entry(ptr, struct qnx_internal_msgsend, hook);   
   
   // handle noreply message correctly
   if (unlikely(send_data->rcvid > 0 && send_data->task == 0))
      --chnl->num_waiting_noreply;
   
   list_del(ptr);
   
   send_data->state = QNX_STATE_RECEIVING;
   
   spin_unlock(&chnl->waiting_lock);
   
   // assign meta information
   memset(&recv_data.info, 0, sizeof(struct _msg_info));   
   
   recv_data.info.pid = send_data->sender_pid;               
   recv_data.info.chid = chnl->chid;   
   
   // pulse or message?
   if (send_data->rcvid == 0)
   {      
      pr_debug("handling pulse\n");
      
      recv_data.info.scoid = send_data->data.pulse.coid;            
      recv_data.info.coid = send_data->data.pulse.coid;      
            
      recv_data.info.msglen = 2 * sizeof(int);
      recv_data.info.srcmsglen = 2 * sizeof(int);
      recv_data.info.dstmsglen = 0;
      
      if (recv_data.out.iov_len >= sizeof(struct _pulse))
      {      
         struct _pulse* pulse = (struct _pulse*)recv_data.out.iov_base;         
         
         int8_t code = send_data->data.pulse.code;         
         int value = send_data->data.pulse.value;
         int32_t scoid = send_data->data.pulse.coid;      
         
         if (put_user(code, &pulse->code) 
            || put_user(scoid, &pulse->scoid) 
            || copy_to_user(&pulse->value, &value, sizeof(int)))
         {
            rc = -EFAULT;
         }
         else
            rc = 0;
      }      
      
      kfree(send_data);
      send_data = 0;
   }
   else
   {
      pr_debug("handling message\n");
      
      recv_data.info.scoid = send_data->data.msg.coid;      
      recv_data.info.coid = send_data->data.msg.coid;      
      
      recv_data.info.msglen = send_data->data.msg.in.iov_len;      
      recv_data.info.srcmsglen = send_data->data.msg.in.iov_len;      
      recv_data.info.dstmsglen = send_data->data.msg.out.iov_len;
      
      // copy data
      bytes_to_copy = min(send_data->data.msg.in.iov_len, recv_data.out.iov_len);
      
      if (unlikely(copy_to_user(recv_data.out.iov_base, send_data->data.msg.in.iov_base, bytes_to_copy)))
      {
         rc = -EFAULT;
      }
      else
         rc = send_data->rcvid;
         
      if (!send_data->task)
      {
         recv_data.info.flags |= QNX_FLAG_NOREPLY;
   
         // clean-up
         qnx_internal_msgsend_destroy(send_data);
         kfree(send_data);
         send_data = 0;
      }
   } 
   
   if (rc >= 0 && copy_to_user((void*)data, &recv_data, sizeof(struct qnx_io_receive)))
      rc = -EFAULT;
      
   if (send_data)
   {
      if (likely(rc > 0))
      {
         qnx_process_entry_add_pending(entry, send_data);
      }
      else 
      {
         send_data->reply.iov_base = 0;
         send_data->reply.iov_len = 0;
         
         send_data->status = rc;
         send_data->state = QNX_STATE_FINISHED;            

         // wake up the waiting process
         printk("wakeup error %p\n", send_data->task);
         wake_up_process(send_data->task);            
      }
   }
              
   pr_debug("MsgReceive finished rcvid=%d\n", rc);  
   
out_channel_release:

   qnx_channel_release(chnl);
   
out:

   return rc;
}


static
int handle_msgreply(struct qnx_process_entry* entry, struct qnx_io_reply* data)
{
   int rc = 0;
  
   struct qnx_internal_msgsend* send_data = qnx_process_entry_release_pending(entry, data->rcvid);
   if (likely(send_data))
   {
      if (send_data->data.msg.out.iov_base 
         && send_data->data.msg.out.iov_len > 0 
         && data->in.iov_len > 0)
      {
         send_data->reply.iov_len = 0;
         send_data->reply.iov_base = kmalloc(data->in.iov_len, GFP_USER);      
         
         if (likely(send_data->reply.iov_base))
         {      
            // copy data
            if (likely(copy_from_user(send_data->reply.iov_base, data->in.iov_base, data->in.iov_len)) == 0)
            {
               send_data->reply.iov_len = data->in.iov_len;
            }
            else
               rc = -EFAULT;
         }
         else
            rc = -ENOMEM;
      }
      else
      {         
         send_data->reply.iov_base = 0;
         send_data->reply.iov_len = 0;
      }
      
      send_data->status = rc < 0 ? rc : data->status;
      send_data->state = QNX_STATE_FINISHED;            

      // wake up the waiting process
//      printk("wakeup ok tid=%p, data=%p, rcvid=%d\n", send_data->task, send_data, send_data->rcvid);
      wake_up_process(send_data->task);            
   }
   else
      rc = -ESRCH;
      
   return rc;
}


static
int handle_msgerror(struct qnx_process_entry* entry, struct qnx_io_error_reply* data)
{
   int rc = 0;
  
   struct qnx_internal_msgsend* send_data = qnx_process_entry_release_pending(entry, data->rcvid);
   if (likely(send_data))
   {      
      send_data->reply.iov_base = 0;
      send_data->reply.iov_len = 0;

      send_data->status = data->error < 0 ? data->error : -data->error;
      send_data->state = QNX_STATE_FINISHED;      
      
      // wake up the waiting process
      wake_up_process(send_data->task);
   }
   else
      rc = -ESRCH;
      
   return rc;
}


static
int handle_msgread(struct qnx_process_entry* entry, struct qnx_io_read* data)
{
   // TODO move this to process_entry instead
   
   int rc;
   struct qnx_internal_msgsend* send_data;
   
   spin_lock(&entry->pending_lock);
   
   list_for_each_entry(send_data, &entry->pending, hook) 
   {
      if (send_data->rcvid == data->rcvid)
      {
         if (data->offset >= 0 && data->offset <= send_data->data.msg.in.iov_len)
         {
            // copy data
            int bytes_to_copy = min(send_data->data.msg.in.iov_len - data->offset, data->out.iov_len);
            
            if (likely(copy_to_user(data->out.iov_base, 
                                    send_data->data.msg.in.iov_base + data->offset, 
                                    bytes_to_copy) == 0)) 
            {
               rc = bytes_to_copy;
            }
            else
               rc = -EFAULT;
         }
         else
            rc = -EINVAL;
         
         goto out;
      }
   }

   rc = -ESRCH;
   
out:

   spin_unlock(&entry->pending_lock);
         
   return rc;
}


static
int handle_msgsend(struct qnx_process_entry* entry, long data)
{
   int rc;
   
   struct qnx_internal_msgsend snddata;
   struct qnx_connection conn;
   struct qnx_channel* chnl;
   
   if (unlikely((rc = qnx_internal_msgsend_init(&snddata, (struct qnx_io_msgsend*)data, entry->pid))))         
      return rc;

   conn = qnx_process_entry_find_connection(entry, snddata.data.msg.coid);              
   if (unlikely(!QNX_CONN_IS_VALID(conn)))
   {
      rc = -EBADF;
      goto out;
   }   

   pr_debug("MsgSend coid=%d\n", snddata.data.msg.coid);

   chnl = qnx_driver_data_find_channel(entry->driver, conn.pid, conn.chid);
   if (unlikely(!chnl))
   {
      rc = -EBADF;
      goto out;
   }
         
   snddata.receiver_pid = conn.pid;
            
   rc = handle_msgsend_internal_block(chnl, &snddata);                  
   // do not access chnl any more from here, it got released inside previous function

   // copy data back to userspace - if buffer is provided
   if (rc >= 0 && snddata.reply.iov_len > 0)
   {
      size_t bytes_to_copy = min(snddata.data.msg.out.iov_len, snddata.reply.iov_len);
      
      if (unlikely(copy_to_user(snddata.data.msg.out.iov_base, snddata.reply.iov_base, bytes_to_copy)))
         rc = -EFAULT;
   }               

out:
       
   qnx_internal_msgsend_destroy(&snddata);
   
   return rc;
}


/// loop until data can be sent or signal stops us from sending
static 
int busy_loop_add_new_message(struct qnx_channel* chnl, struct qnx_internal_msgsend* snddata)
{   
   int rc;
   
   while (unlikely((rc = qnx_channel_add_new_message(chnl, snddata)) < 0))
   {
      msleep_interruptible(50);
      
      if (unlikely(signal_pending(current)))
      {         
         kfree(snddata);
         
         rc = -ERESTARTSYS;         
         break;
      }
   }
   
   return rc;
}


static
int handle_msgsend_no_reply(struct qnx_process_entry* entry, long data)
{
   struct qnx_internal_msgsend* snddata = 0;
   struct qnx_connection conn;
   struct qnx_channel* chnl;
   int rc;
   
   if (unlikely((rc = qnx_internal_msgsend_init_noreply(&snddata, (struct qnx_io_msgsend*)data, entry->pid))))         
      return rc;

   // TODO move this code directly into qnx_process_entry, we are not 
   // interested in the connection, just in the channel...
   if (unlikely(!QNX_CONN_IS_VALID((conn = qnx_process_entry_find_connection(entry, snddata->data.msg.coid)))))
   {
      kfree(snddata);
      return -EBADF;
   }   

   pr_debug("MsgSendNoReply coid=%d\n", snddata->data.msg.coid);

   if (unlikely(!(chnl = qnx_driver_data_find_channel(entry->driver, conn.pid, conn.chid))))
   {
      kfree(snddata);
      return -EBADF;
   }
         
   snddata->receiver_pid = conn.pid;
            
   rc = busy_loop_add_new_message(chnl, snddata); 
   
   qnx_channel_release(chnl);
   
   return 0;
}


static
int handle_msgsendv(struct qnx_process_entry* entry, long data)
{
   int rc;
   
   struct qnx_io_msgsendv send_data = { 0 };
   struct qnx_connection conn;
   struct qnx_channel* chnl;
   struct qnx_internal_msgsend snddata;
   
   struct iovec buf_in[QNX_MAX_IOVEC_LEN];
   struct iovec buf_out[QNX_MAX_IOVEC_LEN];

   struct iovec* in = buf_in;
   struct iovec* out = buf_out;

   if (copy_from_user(&send_data, (void*)data, sizeof(struct qnx_io_msgsendv)))
   {
      rc = -EFAULT;
      goto out; 
   }
   
   if (unlikely(send_data.in_len > QNX_MAX_IOVEC_LEN))
   {            
      if (unlikely(!(in = (struct iovec*)kmalloc(sizeof(struct iovec) * send_data.in_len, GFP_USER))))
      {
         rc = -ENOMEM;
         goto out;
      }
   }         

   if (unlikely(send_data.out_len > QNX_MAX_IOVEC_LEN))
   {
      if (unlikely(!(out = (struct iovec*)kmalloc(sizeof(struct iovec) * send_data.out_len, GFP_USER))))
      {         
         rc = -ENOMEM;
         goto out_clean_in;
      }
   }
                                      
   if (unlikely(copy_from_user(in, send_data.in, sizeof(struct iovec) * send_data.in_len)
      || copy_from_user(out, send_data.out, sizeof(struct iovec) * send_data.out_len)))                  
   {
      rc = -EFAULT;
      goto out_clean_out;
   }
   
   // replace the pointers...
   send_data.in = in;
   send_data.out = out;

   conn = qnx_process_entry_find_connection(entry, send_data.coid);
   if (unlikely(!QNX_CONN_IS_VALID(conn)))
   {
      rc = -EBADF;
      goto out_clean_out;
   }

   chnl = qnx_driver_data_find_channel(entry->driver, conn.pid, conn.chid);
   if (unlikely(!chnl))
   {
      rc = -EBADF;
      goto out_clean_out;
   }    
   
   if (unlikely((rc = qnx_internal_msgsend_initv(&snddata, &send_data, entry->pid))))
      goto out_clean_out;  

   snddata.receiver_pid = conn.pid; 
   
   rc = handle_msgsend_internal_block(chnl, &snddata);                                    
   // do not access chnl any more from here

   // copy data back to userspace - if buffer is provided
   if (rc >= 0 && snddata.reply.iov_len > 0)
   {
      size_t bytes_to_copy = min(snddata.data.msg.out.iov_len, snddata.reply.iov_len);   

      if (memcpy_toiovec(out, snddata.reply.iov_base, bytes_to_copy))
         rc = -EFAULT;
   }

   qnx_internal_msgsend_destroyv(&snddata);
      
out_clean_out:

   QNX_FREE_IF_NOT(out, buf_out);
   
out_clean_in:    

   QNX_FREE_IF_NOT(in, buf_in);
      
out:    
   return rc;
}


static
int handle_msgsend_noreplyv(struct qnx_process_entry* entry, long data)
{
   int rc;
   
   struct qnx_io_msgsendv send_data = { 0 };
   struct qnx_connection conn;
   struct qnx_channel* chnl;
   struct qnx_internal_msgsend* snddata = 0;
   
   struct iovec buf_in[QNX_MAX_IOVEC_LEN];   

   struct iovec* in = buf_in;

   if (copy_from_user(&send_data, (void*)data, sizeof(struct qnx_io_msgsendv)))
   {
      rc = -EFAULT;
      goto out; 
   }
      
   if (unlikely(send_data.in_len > QNX_MAX_IOVEC_LEN))
   {            
      if (unlikely(!(in = (struct iovec*)kmalloc(sizeof(struct iovec) * send_data.in_len, GFP_USER))))
      {
         rc = -ENOMEM;
         goto out;
      }
   }            
                                      
   if (unlikely(copy_from_user(in, send_data.in, sizeof(struct iovec) * send_data.in_len)))                  
   {
      rc = -EFAULT;
      goto out_clean;
   }
   
   // replace the pointers...
   send_data.in = in;   

   conn = qnx_process_entry_find_connection(entry, send_data.coid);
   if (unlikely(!QNX_CONN_IS_VALID(conn)))
   {
      rc = -EBADF;
      goto out_clean;
   }

   chnl = qnx_driver_data_find_channel(entry->driver, conn.pid, conn.chid);
   if (unlikely(!chnl))
   {
      rc = -EBADF;
      goto out_clean;
   }    
   
   if (unlikely((rc = qnx_internal_msgsend_init_noreplyv(&snddata, &send_data, entry->pid))))
      goto out_clean;  

   snddata->receiver_pid = conn.pid;   
   
   rc = busy_loop_add_new_message(chnl, snddata);   
      
   // FIXME do we need this if we use RCU for channels?
   qnx_channel_release(chnl);
         
out_clean:    

   QNX_FREE_IF_NOT(in, buf_in);
   
out:    
   return rc;
}


// -----------------------------------------------------------------------------


static
int qnxcomm_open(struct inode* n, struct file* f)
{
   struct qnx_process_entry* entry;
   
   if (f->f_mode & O_RDWR)
   {
      pr_info("Open called from pid=%d\n", current_get_pid_nr(current));
      
      if (qnx_driver_data_is_process_available(&driver_data, current_get_pid_nr(current)))
         return -ENOSPC;
      
      entry = (struct qnx_process_entry*)kmalloc(sizeof(struct qnx_process_entry), GFP_USER);
      if (unlikely(!entry))
         return -ENOMEM;
            
      qnx_process_entry_init(entry, &driver_data);

      f->private_data = entry;
      qnx_driver_data_add_process(&driver_data, entry);      
   }
   else
   {
      pr_info("Open called for poll fd from pid=%d\n", current_get_pid_nr(current));
      
      entry = qnx_driver_data_find_process(&driver_data, current_get_pid_nr(current));      
      
      if (entry)      
         f->private_data = entry;
   }
      
   return 0;
}


static
int qnxcomm_close(struct inode* n, struct file* f)
{
   int rc = 0;
   
   if (f->f_mode & O_RDWR)
   {
      pr_info("Got close for pid=%d\n", current_get_pid_nr(current));   
      
      if (f->private_data)
      {      
         struct qnx_process_entry* entry = QNX_PROC_ENTRY(f);
         qnx_driver_data_remove(&driver_data, entry->pid);
         qnx_process_entry_release(entry);
         
         f->private_data = 0;
      }
   }
   else
   {
      pr_info("Got close for pollfd from pid=%d\n", current_get_pid_nr(current));   
      
      if (f->private_data)      
      {
         rc = qnx_process_entry_remove_pollfd(QNX_PROC_ENTRY(f), f);
         qnx_process_entry_release(QNX_PROC_ENTRY(f));      
      }
   }
   
   return rc;
}


static
unsigned int qnxcomm_poll(struct file* f, struct poll_table_struct* ptable)
{
   unsigned int mask = 0;
   
   struct qnx_process_entry* entry = QNX_PROC_ENTRY(f);
   struct qnx_channel* chnl = qnx_process_entry_find_channel_from_file(entry, f);
   
   if (chnl)
   {
      poll_wait(f, &chnl->waiting_queue,  ptable);
      
      if (atomic_read(&chnl->num_waiting) > 0)
         mask |= POLLIN | POLLRDNORM;
   }
   else
      mask |= POLLHUP;
   
   return mask;
}


static 
long qnxcomm_ioctl(struct file* f, unsigned int cmd, unsigned long data)
{      
   int rc = 0;
   
   if (unlikely(!data))
      return -EINVAL;   
 
   if (unlikely(!f->private_data))
      return -ENOTTY;   
   
   // this happens to be the case after a fork. The userspace library
   // cares about this return value...
   if (unlikely(current_get_pid_nr(current) != QNX_PROC_ENTRY(f)->pid))
      return -ENOSPC;
   
   switch(cmd)
   {
   case QNX_IO_CHANNELCREATE:      
      rc = qnx_process_entry_add_channel(QNX_PROC_ENTRY(f));
      pr_info("ChannelCreate chid=%d\n", rc);      
      break;
   
   case QNX_IO_CHANNELDESTROY:
      rc = qnx_process_entry_remove_channel(QNX_PROC_ENTRY(f), data);
      pr_info("ChannelDestroy chid=%ld, rc=%d\n", data, rc);     
      break;
   
   case QNX_IO_CONNECTDETACH:
      rc = qnx_process_entry_remove_connection(QNX_PROC_ENTRY(f), data);               
      pr_info("ConnectDetach chid=%ld, rc=%d\n", data, rc);     
      break;
      
   case QNX_IO_CONNECTATTACH:      
      {
         struct qnx_io_attach attach_data = { 0 };
      
         if (copy_from_user(&attach_data, (void*)data, sizeof(struct qnx_io_attach)) == 0)
         {            
            rc = qnx_process_entry_add_connection(QNX_PROC_ENTRY(f), &attach_data);
            pr_info("ConnectAttach to chid=%d coid=%d\n", attach_data.chid, rc);
         }
         else
            rc = -EFAULT;
      }      
      break;
      
   case QNX_IO_MSGSEND:         
      rc = handle_msgsend(QNX_PROC_ENTRY(f), data);
      break;
   
   case QNX_IO_MSGSENDNOREPLY:         
      rc = handle_msgsend_no_reply(QNX_PROC_ENTRY(f), data);
      break;
   
   case QNX_IO_MSGSENDPULSE:      
      rc = handle_msgsendpulse(QNX_PROC_ENTRY(f), data);
      break;
      
   case QNX_IO_MSGRECEIVE:      
      rc = handle_msgreceive(QNX_PROC_ENTRY(f), data);
      break;
   
   case QNX_IO_MSGREPLY:      
      {
         struct qnx_io_reply reply_data = { 0 };
      
         if (likely(copy_from_user(&reply_data, (void*)data, sizeof(struct qnx_io_reply)) == 0))
         {              
            rc = handle_msgreply(QNX_PROC_ENTRY(f), &reply_data);
         }
         else
            rc = -EFAULT;
      }      
      break;
      
   case QNX_IO_MSGERROR:      
      {
         struct qnx_io_error_reply reply_data = { 0 };
      
         if (likely(copy_from_user(&reply_data, (void*)data, sizeof(struct qnx_io_error_reply)) == 0))
         {              
            rc = handle_msgerror(QNX_PROC_ENTRY(f), &reply_data);
         }
         else
            rc = -EFAULT;
      }
      break;
   
   case QNX_IO_MSGREAD:      
      {
         struct qnx_io_read io_data = { 0 };
      
         if (likely(copy_from_user(&io_data, (void*)data, sizeof(struct qnx_io_read)) == 0))
         {              
            rc = handle_msgread(QNX_PROC_ENTRY(f), &io_data);
         }
         else
            rc = -EFAULT;
      }      
      break;

   case QNX_IO_MSGSENDV:            
      rc = handle_msgsendv(QNX_PROC_ENTRY(f), data);      
      break;      
   
   case QNX_IO_MSGSENDNOREPLYV:            
      rc = handle_msgsend_noreplyv(QNX_PROC_ENTRY(f), data);      
      break;      
     
   case QNX_IO_REGISTER_POLLFD:
      rc = qnx_process_entry_add_pollfd(QNX_PROC_ENTRY(f), f, data);
      break;
      
   default:
      rc = -EINVAL;
      break;
   }
   
   return rc;
}


static 
struct file_operations fops = {
   .open = &qnxcomm_open,
   .unlocked_ioctl = &qnxcomm_ioctl,
   .compat_ioctl = &qnxcomm_ioctl,
   .poll = &qnxcomm_poll,
   .release = &qnxcomm_close
};


static
int __init qnxcomm_init(void)
{
   if (alloc_chrdev_region(&dev_number, 0, 1, "QnxComm") <0)
      return -EIO;
        
   instance = cdev_alloc();
   if (!instance)
      goto free_region;
      
   instance->owner = THIS_MODULE;
   instance->ops = &fops;
    
   if (cdev_add(instance, dev_number, 1))
      goto free_cdev;
    
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)    
    if (!qnx_proc_init(&driver_data))
      goto del_inst;
#endif      
      
   the_class = class_create(THIS_MODULE, "QnxComm");
   dev = device_create(the_class, 0, dev_number, 0, "%s", "qnxcomm");
    
   qnx_driver_data_init(&driver_data);
    
   dev_info(dev, "QnxComm init\n");
   return 0;
    
del_inst:
   cdev_del(instance);
       
free_cdev:
   kobject_put(&instance->kobj);

free_region:
   unregister_chrdev_region(dev_number, 1);
   return -EIO;
}


static
void __exit qnxcomm_cleanup(void)
{
   dev_info(dev, "QnxComm deinit\n");
   
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)       
   qnx_proc_destroy(&driver_data);    
#endif   
   
   device_destroy(the_class, dev_number);
   class_destroy(the_class);
   cdev_del(instance);
   unregister_chrdev_region(dev_number, 1);
}


module_init(qnxcomm_init);
module_exit(qnxcomm_cleanup);
