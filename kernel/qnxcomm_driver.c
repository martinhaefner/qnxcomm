#include <asm/uaccess.h>
#include <linux/spinlock.h>
#include <linux/delay.h>

#include "qnxcomm_internal.h"


// TODO what about forking to another process?
// TODO use likely() in order to enhance performance
// TODO make sure the correct errnos are returned on error conditions


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Martin Haefner");
MODULE_DESCRIPTION("QNX like message passing for the Linux kernel");


#define QNX_PROC_ENTRY(f) ((struct qnx_process_entry*)f->private_data)

static 
struct qnx_driver_data driver_data;


static
int handle_msgsend(struct qnx_channel* chnl, struct qnx_internal_msgsend* send_data)
{   
   send_data->task = current;
   
   // set the state before it's getting scheduled out
   set_current_state(TASK_INTERRUPTIBLE);
   
   qnx_channel_add_new_message(chnl, send_data); 
   
   pr_debug("MsgSend(v) with timeout=%d ms\n", send_data->data->timeout_ms); 
   
   // now wait for MsgReply...   
   if (send_data->data->timeout_ms > 0)
   {
      if (msleep_interruptible(send_data->data->timeout_ms) == 0)
      {
         send_data->status = -ETIMEDOUT;
         goto interrupted;
      }      
   }
   else
      schedule();
   
   // break if we got a signal
   if (signal_pending(current))
   {
      send_data->status = -ERESTARTSYS;
   }
   else
      goto out;
   
interrupted:

   if (!qnx_channel_remove_message(chnl, send_data->rcvid))
   { 
      // object is already in processing
      
      //FIXME need refcounting here!
      //if (qnx_process_entry_release_pending(chnl->process, send_data->rcvid) == 0)
      {
         // FIXME we must wait until other process is finished accessing this object!
         // and return the correct values...somehow busy-loop
      }
   }
   
out:   
   
   qnx_channel_release(chnl);
   
   return send_data->status;
}


static
int handle_msgsendpulse(struct qnx_channel* chnl, struct qnx_internal_msgsend* send_data)
{
   qnx_channel_add_new_message(chnl, send_data);
   
   qnx_channel_release(chnl);
   
   return 0;
}


static
int handle_msgreceive(struct qnx_process_entry* entry, struct qnx_channel* chnl, struct io_receive* recv_data)
{
   int ret;
   void* ptr;
   struct qnx_internal_msgsend* send_data;
   size_t bytes_to_copy;      
   
   int rc = wait_event_interruptible_timeout(chnl->waiting_queue, 
               atomic_add_unless(&chnl->num_waiting, -1, 0) > 0, 
               msecs_to_jiffies(recv_data->timeout_ms));
   
   pr_debug("handle msgreceive rc=%d, num_waiting=%d\n", rc, atomic_read(&chnl->num_waiting));
   
   if (rc < 0)
   {
      ret = -ERESTARTSYS;
      goto out;
   }
   
   spin_lock(&chnl->waiting_lock);
   ptr = chnl->waiting.next;
   
   // empty?! FIXME maybe spurious wakeup here?!
   if (ptr == &chnl->waiting)
   {
      printk("Empty...\n");
      spin_unlock(&chnl->waiting_lock);      
      
      ret = -ETIMEDOUT;
      goto out;
   }
   
   send_data = list_entry(ptr, struct qnx_internal_msgsend, hook);   
   list_del_init(ptr);
   
   spin_unlock(&chnl->waiting_lock);
   
   // assign meta information
   memset(&recv_data->info, 0, sizeof(struct _msg_info));   
   
   recv_data->info.pid = send_data->sender_pid;               
   recv_data->info.chid = chnl->chid;   
   
   // pulse or message?
   if (send_data->rcvid == 0)
   {      
      pr_debug("pulse\n");
      
      recv_data->info.scoid = send_data->pulse->coid;            
      recv_data->info.coid = send_data->pulse->coid;      
            
      recv_data->info.msglen = 2 * sizeof(int);
      recv_data->info.srcmsglen = 2 * sizeof(int);
      recv_data->info.dstmsglen = sizeof(struct _pulse);
      
      if (recv_data->out.iov_len >= sizeof(struct _pulse))
      {      
         struct _pulse* pulse = (struct _pulse*)recv_data->out.iov_base;         
         
         int8_t code = send_data->pulse->code;         
         int value = send_data->pulse->value;
         
         if (put_user(code, &pulse->code) || copy_to_user(&pulse->value, &value, 4))
            ret = -EFAULT;         
         else
            ret = 0;
      }      
      
      kfree(send_data);
   }
   else
   {
      pr_debug("message\n");
      
      recv_data->info.scoid = send_data->data->coid;      
      recv_data->info.coid = send_data->data->coid;      
      
      recv_data->info.msglen = send_data->data->in.iov_len;      
      recv_data->info.srcmsglen = send_data->data->in.iov_len;      
      recv_data->info.dstmsglen = send_data->data->out.iov_len;
            
      // copy data
      bytes_to_copy = min(send_data->data->in.iov_len, recv_data->out.iov_len);
      
      if (copy_to_user(recv_data->out.iov_base, send_data->data->in.iov_base, bytes_to_copy))
         ret = -EFAULT;
      else
         ret = send_data->rcvid;
      
      qnx_process_entry_add_pending(entry, send_data);
   } 
   
out:
   qnx_channel_release(chnl);
   
   return ret;
}


static
int handle_msgreply(struct qnx_process_entry* entry, struct io_reply* data)
{
   int rc = 0;
  
   struct qnx_internal_msgsend* send_data = qnx_process_entry_release_pending(entry, data->rcvid);
   if (send_data)
   {
      if (send_data->data->out.iov_base && send_data->data->out.iov_len > 0)
      {
         send_data->reply.iov_base = kmalloc(data->in.iov_len, GFP_USER);
      
         // FIXME only copy bytes that can be read by client (min())
         send_data->reply.iov_len = data->in.iov_len;
      
         // copy data
         if (copy_from_user(send_data->reply.iov_base, data->in.iov_base, data->in.iov_len))
            rc = -EFAULT;
      }
      else
      {
         // FIXME use some sort of constructor to zero fields
         send_data->reply.iov_base = 0;
         send_data->reply.iov_len = 0;
      }
      
      send_data->status = data->status;

      // wake up the waiting process
      wake_up_process(send_data->task);
   }
   else
      rc = -ESRCH;
      
   return rc;
}


static
int handle_msgerror(struct qnx_process_entry* entry, struct io_error_reply* data)
{
   int rc = 0;
  
   struct qnx_internal_msgsend* send_data = qnx_process_entry_release_pending(entry, data->rcvid);
   if (send_data)
   {
      // FIXME use some sort of constructor to zero fields
      send_data->reply.iov_base = 0;
      send_data->reply.iov_len = 0;

      send_data->status = data->error < 0 ? data->error : -data->error;
      
      // wake up the waiting process
      wake_up_process(send_data->task);
   }
   else
      rc = -ESRCH;
      
   return rc;
}


static
int handle_msgread(struct qnx_process_entry* entry, struct io_read* data)
{
   int rc;
   
   struct qnx_internal_msgsend* send_data = qnx_process_entry_release_pending(entry, data->rcvid);
   if (send_data)
   {
      if (data->offset >= 0 && data->offset <= send_data->data->in.iov_len)
      {
         // copy data
         int bytes_to_copy = min(send_data->data->in.iov_len - data->offset, data->out.iov_len);
         if (copy_to_user(data->out.iov_base, send_data->data->in.iov_base + data->offset, bytes_to_copy) == 0)
            rc = bytes_to_copy;
         else
            rc = -EFAULT;
      }
      else
         rc = -EINVAL;
         
      qnx_process_entry_add_pending(entry, send_data);
   }
   else
      rc = -ESRCH;
      
   return rc;
}


// -----------------------------------------------------------------------------


// TODO move this to compatibility header
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,0,0)
static
int memcpy_toiovec(struct iovec *iov, unsigned char *kdata, int len)
{
         while (len > 0) {
                 if (iov->iov_len) {
                         int copy = min_t(unsigned int, iov->iov_len, len);
                         if (copy_to_user(iov->iov_base, kdata, copy))
                                 return -EFAULT;
                         kdata += copy;
                         len -= copy;
                         iov->iov_len -= copy;
                         iov->iov_base += copy;
                 }
                 iov++;
         }
 
         return 0;
}
#endif


// -----------------------------------------------------------------------------


static
int qnxcomm_open(struct inode* n, struct file* f)
{
   if (!qnx_driver_data_is_process_available(&driver_data, current_get_pid_nr(current)))
   {
      struct qnx_process_entry* entry = (struct qnx_process_entry*)kmalloc(sizeof(struct qnx_process_entry), GFP_USER);
      if (entry)
      {
         qnx_process_entry_init(entry, &driver_data);
      
         f->private_data = entry;
         qnx_driver_data_add_process(&driver_data, entry);
      
         pr_info("Open called from pid=%d\n", current_get_pid_nr(current));
         return 0;
      }
      else
         return -ENOMEM;
   }
   else
      return -ENOSPC;
}


static
int qnxcomm_close(struct inode* n, struct file* f)
{
   pr_info("Got close for pid=%d\n", current_get_pid_nr(current));   
   
   if (f->private_data)
   {
      struct qnx_process_entry* entry = QNX_PROC_ENTRY(f);
      qnx_driver_data_remove(&driver_data, entry->pid);
      qnx_process_entry_release(entry);
      
      f->private_data = 0;
   }
   
   return 0;
}


static 
long qnxcomm_ioctl(struct file* f, unsigned int cmd, unsigned long data)
{
   int rc = -ENOSYS;
 
   if (!f->private_data)
      return -EBADF;
      
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
      if (data)
      {
         struct io_attach attach_data = { 0 };
      
         if (copy_from_user(&attach_data, (void*)data, sizeof(struct io_attach)) == 0)
         {            
            rc = qnx_process_entry_add_connection(QNX_PROC_ENTRY(f), &attach_data);
            pr_info("ConnectAttach to chid=%d coid=%d\n", attach_data.chid, rc);
         }
         else
            rc = -EFAULT;
      }
      else
         rc = -EINVAL;

      break;
      
   case QNX_IO_MSGSEND:
      if (data)
      {                  
         struct io_msgsend send_data = { 0 };
      
         if (copy_from_user(&send_data, (void*)data, sizeof(struct io_msgsend)) == 0)
         {            
            struct qnx_connection conn = qnx_process_entry_find_connection(QNX_PROC_ENTRY(f), send_data.coid);
            pr_debug("MsgSend coid=%d\n", send_data.coid);
            if (conn.coid)
            {
               struct qnx_channel* chnl = qnx_driver_data_find_channel(&driver_data, conn.pid, conn.chid);
               if (chnl)
               {
                  struct qnx_internal_msgsend snddata;
                  if ((rc = qnx_internal_msgsend_init(&snddata, &send_data, QNX_PROC_ENTRY(f)->pid)) == 0)
                  {
                     rc = handle_msgsend(chnl, &snddata);                  
                     // do not access chnl any more from here
                     
                     pr_debug("MsgSend finished rc=%d\n", rc);
                        
                     // copy data back to userspace - if buffer is provided
                     if (rc >= 0 && snddata.reply.iov_len > 0)
                     {
                        size_t bytes_to_copy = min(snddata.data->out.iov_len, snddata.reply.iov_len);
                           
                        if (copy_to_user(snddata.data->out.iov_base, snddata.reply.iov_base, bytes_to_copy))
                           rc = -EFAULT;
                     }
   
                     qnx_internal_msgsend_destroy(&snddata);
                  }
               }
               else
                  rc = -EBADF;
            }
            else
               rc = -EBADF;
         }
         else
            rc = -EFAULT;
      }
      else
         rc = -EINVAL;

      break;
   
   case QNX_IO_MSGSENDPULSE:
      if (data)
      {
         // must allocate data (or reuse some other object)...
         struct qnx_internal_msgsend* snddata = (struct qnx_internal_msgsend*)kmalloc(sizeof(struct qnx_internal_msgsend) + sizeof(struct io_msgsendpulse), GFP_USER);
         qnx_internal_msgsend_init_pulse(snddata, (struct io_msgsendpulse*)(snddata+1), QNX_PROC_ENTRY(f)->pid);                  
      
         if (copy_from_user(snddata->pulse, (void*)data, sizeof(struct io_msgsendpulse)) == 0)
         {            
            struct qnx_connection conn = qnx_process_entry_find_connection(QNX_PROC_ENTRY(f), snddata->pulse->coid);
            pr_debug("MsgSendPulse coid=%d\n", snddata->pulse->coid);
            if (conn.coid)
            {
               struct qnx_channel* chnl = qnx_driver_data_find_channel(&driver_data, conn.pid, conn.chid);
               if (chnl)
               {                  
                  rc = handle_msgsendpulse(chnl, snddata);   
               }
               else
                  rc = -EBADF;
            }
            else
               rc = -EBADF;
         }
         else
            rc = -EFAULT;
            
         if (rc != 0)         
            kfree(snddata);         
      }
      else
         rc = -EINVAL;

      break;
      
   case QNX_IO_MSGRECEIVE:
      if (data)
      {
         struct io_receive recv_data = { 0 };
      
         if (copy_from_user(&recv_data, (void*)data, sizeof(struct io_receive)) == 0)
         {                      
            struct qnx_channel* chnl = qnx_process_entry_find_channel(QNX_PROC_ENTRY(f), recv_data.chid);
            if (chnl)
            {
               rc = handle_msgreceive(QNX_PROC_ENTRY(f), chnl, &recv_data);
               
               if (rc >= 0 && copy_to_user((void*)data, &recv_data, sizeof(struct io_receive)))
                  rc = -EFAULT;
                  
               printk("MsgReceive finished rcvid=%d\n", rc);
            }
            else
               rc = -EBADF;
         }
         else
            rc = -EFAULT;
      }
      else
         rc = -EINVAL;

      break;
   
   case QNX_IO_MSGREPLY:
      if (data)
      {
         struct io_reply reply_data = { 0 };
      
         if (copy_from_user(&reply_data, (void*)data, sizeof(struct io_reply)) == 0)
         {              
            rc = handle_msgreply(QNX_PROC_ENTRY(f), &reply_data);
         }
         else
            rc = -EFAULT;
      }
      else
         rc = -EINVAL;

      break;
      
   case QNX_IO_MSGERROR:
      if (data)
      {
         struct io_error_reply reply_data = { 0 };
      
         if (copy_from_user(&reply_data, (void*)data, sizeof(struct io_error_reply)) == 0)
         {              
            rc = handle_msgerror(QNX_PROC_ENTRY(f), &reply_data);
         }
         else
            rc = -EFAULT;
      }
      else
         rc = -EINVAL;

      break;
   
   case QNX_IO_MSGREAD:
      if (data)
      {
         struct io_read io_data = { 0 };
      
         if (copy_from_user(&io_data, (void*)data, sizeof(struct io_read)) == 0)
         {              
            rc = handle_msgread(QNX_PROC_ENTRY(f), &io_data);
         }
         else
            rc = -EFAULT;
      }
      else
         rc = -EINVAL;

      break;

   case QNX_IO_MSGSENDV:
      if (data)
      {
         struct io_msgsendv send_data = { 0 };
      
         if (copy_from_user(&send_data, (void*)data, sizeof(struct io_msgsendv)) == 0)
         {
            struct qnx_connection conn = qnx_process_entry_find_connection(QNX_PROC_ENTRY(f), send_data.coid);
            if (conn.coid)
            {
               struct qnx_channel* chnl = qnx_driver_data_find_channel(&driver_data, conn.pid, conn.chid);
               if (chnl)
               {
                  struct iovec in[send_data.in_len];
                  struct iovec out[send_data.out_len];
                  
                  struct io_msgsend tmp = { 0 };
                  struct qnx_internal_msgsend snddata;
                                                      
                  if (copy_from_user(in, send_data.in, sizeof(in)) == 0
                     && copy_from_user(out, send_data.out, sizeof(out)) == 0)                  
                  {
                     send_data.in = in;
                     send_data.out = out;                                    

                     if (!qnx_internal_msgsend_initv(&snddata, &tmp, &send_data, QNX_PROC_ENTRY(f)->pid))
                     {
                        rc = handle_msgsend(chnl, &snddata);                                    
                        // do not access chnl any more from here
                        
                        // copy data back to userspace - if buffer is provided
                        if (rc >= 0 && snddata.reply.iov_len > 0)
                        {
                           size_t bytes_to_copy = min(snddata.data->out.iov_len, snddata.reply.iov_len);   
                           
                           if (memcpy_toiovec(out, snddata.reply.iov_base, bytes_to_copy))
                              rc = -EFAULT;
                        }
                        
                        qnx_internal_msgsend_destroyv(&snddata);
                     }
                     else
                        rc = -EFAULT;
                  }
                  else
                     rc = -EFAULT;
               }
               else
                  rc = -EBADF;
            }
            else
               rc = -EBADF;
         }
         else
            rc = -EFAULT;
      }
      else
         rc = -EINVAL;
      
   default:
      // NOOP return -ENOSYS
      break;
   }
   
   return rc;
}


static 
struct file_operations fops = {
   .open = &qnxcomm_open,
   .unlocked_ioctl = &qnxcomm_ioctl,
   .compat_ioctl = &qnxcomm_ioctl,
   .release = &qnxcomm_close
};


static dev_t dev_number;
static struct cdev* instance;
static struct class* the_class;
static struct device* dev;


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
    
    if (!qnx_proc_init(&driver_data))
      goto del_inst;
      
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
   
   qnx_proc_destroy(&driver_data);    
   
   device_destroy(the_class, dev_number);
   class_destroy(the_class);
   cdev_del(instance);
   unregister_chrdev_region(dev_number, 1);
}


module_init(qnxcomm_init);
module_exit(qnxcomm_cleanup);