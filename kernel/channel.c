#include "qnxcomm_internal.h"


static 
atomic_t gbl_next_channel_id = ATOMIC_INIT(0);   


static
int get_new_channel_id(void)
{
   return atomic_inc_return(&gbl_next_channel_id);
}


int qnx_channel_init(struct qnx_channel* chnl)
{
   kref_init(&chnl->refcnt);
   chnl->chid = get_new_channel_id();   

   INIT_LIST_HEAD(&chnl->waiting);
   atomic_set(&chnl->num_waiting, 0);
   
   init_waitqueue_head(&chnl->waiting_queue);
   
   spin_lock_init(&chnl->waiting_lock);
   
   return chnl->chid;
}


static 
void qnx_channel_free(struct kref* refcount)
{
   struct qnx_channel* chnl = container_of(refcount, struct qnx_channel, refcnt);
   
   struct list_head* iter;
   struct list_head* next;

   printk("qnx_channel_free called\n");

   spin_lock(&chnl->waiting_lock);
   
   list_for_each_safe(iter, next, &chnl->waiting)
   {      
      printk("Removing pending entry\n");
      qnx_internal_msgsend_cleanup_and_free(list_entry(iter, struct qnx_internal_msgsend, hook));
      list_del(iter);      
   }
   
   spin_unlock(&chnl->waiting_lock);   

   kfree(chnl);
}


void qnx_channel_release(struct qnx_channel* chnl)
{
   kref_put(&chnl->refcnt, &qnx_channel_free);
}


int qnx_channel_add_new_message(struct qnx_channel* chnl, struct qnx_internal_msgsend* data)
{
   spin_lock(&chnl->waiting_lock);
   list_add_tail(&data->hook, &chnl->waiting); 
   spin_unlock(&chnl->waiting_lock);
   
   atomic_inc(&chnl->num_waiting);
   wake_up(&chnl->waiting_queue);
   
   return 0;
}


int qnx_channel_remove_message(struct qnx_channel* chnl, int rcvid)
{
   int rc = 0;
   struct list_head* iter;
   
   spin_lock(&chnl->waiting_lock);
   
   list_for_each(iter, &chnl->waiting)
   {
      if (list_entry(iter, struct qnx_internal_msgsend, hook)->rcvid == rcvid)
      {
         list_del(iter);
         atomic_dec(&chnl->num_waiting);
         
         rc = 1;
         goto out;
      }
   }
   
out:

   spin_unlock(&chnl->waiting_lock);
   
   return rc;
}
