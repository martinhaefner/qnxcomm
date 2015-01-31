#include "qnxcomm_internal.h"

#include <asm/uaccess.h>


void qnx_driver_data_init(struct qnx_driver_data* data)
{
   INIT_LIST_HEAD(&data->process_entries);
   init_rwsem(&data->process_entries_lock);
}


void qnx_driver_data_add_process(struct qnx_driver_data* data, struct qnx_process_entry* entry)
{
   down_write(&data->process_entries_lock);
   
   list_add(&entry->hook, &data->process_entries);
   
   up_write(&data->process_entries_lock);
}


struct qnx_process_entry* qnx_driver_data_find_process(struct qnx_driver_data* data, pid_t pid)
{      
   struct qnx_process_entry* entry;
   printk("searching process %d\n", pid);
   down_read(&data->process_entries_lock);
   
   list_for_each_entry(entry, &data->process_entries, hook)
   {
      if (entry->pid == pid) 
      {
         kref_get(&entry->refcnt);
         goto out;
      }
   }
   
   entry = 0;
    
out:
   up_read(&data->process_entries_lock);
   
   return entry;
}


int qnx_driver_data_is_process_available(struct qnx_driver_data* data, pid_t pid)
{
   struct qnx_process_entry* entry;
   
   pr_debug("searching process entry pid=%d\n", pid);
   
   down_read(&data->process_entries_lock);
   
   list_for_each_entry(entry, &data->process_entries, hook)
   {
      if (entry->pid == pid) 
         goto out;
   }
   
   entry = 0;
    
out:
   up_read(&data->process_entries_lock);
   
   return entry?1:0;
}


void qnx_driver_data_remove(struct qnx_driver_data* data, pid_t pid)
{
   struct list_head *iter;

   printk("remove for pid=%d tid=%d, tgid=%d\n", pid, current->pid, current->tgid);
   
   down_write(&data->process_entries_lock);
   
   list_for_each(iter, &data->process_entries) 
   {
      if (list_entry(iter, struct qnx_process_entry, hook)->pid == pid) 
      {
         list_del(iter);
         break;
      }
   }
      
   up_write(&data->process_entries_lock);
}


struct qnx_channel* qnx_driver_data_find_channel(struct qnx_driver_data* data, int pid, int chid)
{
   struct qnx_channel* chnl = 0;
   struct qnx_process_entry* proc = qnx_driver_data_find_process(data, pid);   
   
   if (proc)
   {
      chnl = qnx_process_entry_find_channel(proc, chid);
      qnx_process_entry_release(proc);
   }
      
   return chnl;
}
