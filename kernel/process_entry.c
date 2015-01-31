#include "qnxcomm_internal.h"


void qnx_process_entry_init(struct qnx_process_entry* entry, struct qnx_driver_data* driver)
{
   kref_init(&entry->refcnt);
   entry->pid = task_pid_nr(current);
   
   INIT_LIST_HEAD(&entry->channels);
   INIT_LIST_HEAD(&entry->connections);
   INIT_LIST_HEAD(&entry->pending);
   
   rwlock_init(&entry->channels_lock);
   rwlock_init(&entry->connections_lock);
   spin_lock_init(&entry->pending_lock);   
   
   entry->driver = driver;
}


static 
void qnx_process_entry_free(struct kref* refcount)
{
   struct qnx_process_entry* entry = container_of(refcount, struct qnx_process_entry, refcnt);
   
   // FIXME splice the lists in order to reduce lock contention and delete offline   
   struct list_head *iter, *next;

   printk("qnx_process_entry_free called\n");
 
   write_lock(&entry->channels_lock);
   
   while(!list_empty(&entry->channels))
   {
      struct qnx_channel* chnl;
      printk("releasing channel...\n");
 
      chnl = list_first_entry(&entry->channels, struct qnx_channel, hook);
      list_del(&chnl->hook);
      
      qnx_channel_release(chnl);
   }
   
   write_unlock(&entry->channels_lock);
   
   printk("channels done\n");
 
   spin_lock(&entry->pending_lock);
   
   list_for_each_safe(iter, next, &entry->pending)
   {  
      printk("pending...\n");
     
      qnx_internal_msgsend_cleanup_and_free(list_entry(iter, struct qnx_internal_msgsend, hook));
      list_del(iter);      
   }
   
   spin_unlock(&entry->pending_lock);  
   
   printk("pendings done\n");
 
   write_lock(&entry->connections_lock);
   
   while(!list_empty(&entry->connections))
   {
      struct qnx_connection* conn;
      printk("connection...\n");
 
      conn = list_first_entry(&entry->connections, struct qnx_connection, hook);
      list_del(&conn->hook);
      
      kfree(conn);
   }

   write_unlock(&entry->connections_lock); 
   
   printk("finished\n");
 
   kfree(entry);
   
   printk("really finished\n");
}


void qnx_process_entry_release(struct qnx_process_entry* entry)
{
   kref_put(&entry->refcnt, &qnx_process_entry_free);
}


int qnx_process_entry_remove_channel(struct qnx_process_entry* entry, int chid)
{
   int rc = -EINVAL;
   
   struct list_head* iter;
   struct qnx_channel* chnl = 0;
   
   write_lock(&entry->channels_lock);
   
   list_for_each(iter, &entry->channels)
   {
      chnl = list_entry(iter, struct qnx_channel, hook);
      if (chnl->chid == chid)
      {
         list_del(iter);         
         qnx_channel_release(chnl);
         
         rc = 0;
         break;
      }    
   }
   
   write_unlock(&entry->channels_lock);
   
   return rc;
}


void qnx_process_entry_add_pending(struct qnx_process_entry* entry, struct qnx_internal_msgsend* data)
{
   spin_lock(&entry->pending_lock);
   
   list_add(&data->hook, &entry->pending);
   
   spin_unlock(&entry->pending_lock);
}


struct qnx_internal_msgsend* qnx_process_entry_release_pending(struct qnx_process_entry* entry, int rcvid)
{
   struct qnx_internal_msgsend* iter;
   
   spin_lock(&entry->pending_lock);
   
   list_for_each_entry(iter, &entry->pending, hook) 
   {
      if (iter->rcvid == rcvid)
      {
         list_del(&iter->hook);
         goto out;
      }
   }  
    
   iter = 0;
   
out:    

   spin_unlock(&entry->pending_lock);
   
   return iter;
}


int qnx_process_entry_add_channel(struct qnx_process_entry* entry)
{   
   int rc;
   struct qnx_channel* chnl = (struct qnx_channel*)kmalloc(sizeof(struct qnx_channel), GFP_USER);
   
   rc = qnx_channel_init(chnl);
//   chnl->process = entry;   // FIXME refcounted?
   
   write_lock(&entry->channels_lock);
   
   list_add(&chnl->hook, &entry->channels);
   
   write_unlock(&entry->channels_lock);
   
   return rc;
}


struct qnx_channel* qnx_process_entry_find_channel(struct qnx_process_entry* entry, int chid)
{
   struct qnx_channel* chnl;

   read_lock(&entry->channels_lock);
   
   list_for_each_entry(chnl, &entry->channels, hook) 
   {
      if (chnl->chid == chid)   
      {            
         kref_get(&chnl->refcnt);       
         goto out;
      }
   }
   
   chnl = 0;
   
out:   
   read_unlock(&entry->channels_lock);
    
   return chnl;
}


int qnx_process_entry_is_channel_available(struct qnx_process_entry* entry, int chid)
{
   struct qnx_channel* chnl;  
   
   read_lock(&entry->channels_lock);
   
   list_for_each_entry(chnl, &entry->channels, hook)
   {
      if (chnl->chid == chid)
         goto out;
   }
   
   chnl = 0;
   
out:
   read_unlock(&entry->channels_lock);
   
   return chnl?1:0;
}


int qnx_process_entry_add_connection(struct qnx_process_entry* entry, struct io_attach* att_data)
{
   int rc;
   
   struct qnx_process_entry* proc = qnx_driver_data_find_process(entry->driver, att_data->pid);
   if (proc)
   {      
      if (qnx_process_entry_is_channel_available(proc, att_data->chid))
      {
         struct qnx_connection* conn = (struct qnx_connection*)kmalloc(sizeof(struct qnx_connection), GFP_USER);
         
         rc = qnx_connection_init(conn, att_data->pid, att_data->chid, att_data->index);
            
         write_lock(&entry->connections_lock);
         
         list_add(&conn->hook, &entry->connections);
         
         write_unlock(&entry->connections_lock);
      }
      else
         rc = -ESRCH;
         
      qnx_process_entry_release(proc);
   }
   else
      rc = -ESRCH;
   
   return rc;
}


int qnx_process_entry_remove_connection(struct qnx_process_entry* entry, int coid)
{
   int rc = -EINVAL;   
   
   struct list_head* iter;
   struct qnx_connection* conn = 0;
   
   write_lock(&entry->connections_lock);
   
   list_for_each(iter, &entry->connections)
   {
      conn = list_entry(iter, struct qnx_connection, hook);
      if (conn->coid == coid)
      {
         list_del(iter);
         kfree(conn);
         
         rc = 0;   
         break;
      }    
   }
   
   write_unlock(&entry->connections_lock);
   
   return rc;
}


struct qnx_connection qnx_process_entry_find_connection(struct qnx_process_entry* entry, int coid)
{
   struct qnx_connection rc = { { 0 }, 0 };
   struct qnx_connection* conn;
   
   read_lock(&entry->connections_lock);

   list_for_each_entry(conn, &entry->connections, hook)
   {
      if (conn->coid == coid)
      { 
         rc.coid = coid;
         rc.pid = conn->pid;
         rc.chid = conn->chid;
         
         break;
      }
   }
   
   read_unlock(&entry->connections_lock);
    
   return rc;
}