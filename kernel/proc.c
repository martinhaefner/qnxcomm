#include <linux/seq_file.h>

#include "qnxcomm_internal.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)    

#define QNX_PROC_ROOT_DIR    "qnxcomm"
#define QNX_PROC_CONNECTIONS "connections"
#define QNX_PROC_CHANNELS    "channels"

#define QNX_DRIVER_DATA(sf) ((struct qnx_driver_data*)sf->private) 


static int 
qnx_show_connections(struct seq_file *buf, void *v)
{
   struct qnx_driver_data* data = QNX_DRIVER_DATA(buf);
   struct qnx_process_entry* entry;
   
   int have_output = 0;
   
   rcu_read_lock();

   list_for_each_entry_rcu(entry, &data->process_entries, hook)
   {
      int i;
      
      have_output = 1;
      seq_printf(buf, "pid=%d:\n", entry->pid);

      // FIXME need 'max' indicator in qnx_connection_table
      for(i=0; i<entry->connections.capacity; ++i)
      {
         struct qnx_connection* conn = rcu_dereference(entry->connections.conn)[i];
         if (conn)
         {
            seq_printf(buf, "   %d => pid=%d, chid=%d\n", i, conn->pid, conn->chid);
         }
      }
      
      seq_printf(buf, "\n");
   }
         
   rcu_read_unlock();
   
   if (!have_output)
      seq_printf(buf, "<no processes attached>\n");

   return 0;
}


static int 
qnx_show_channels(struct seq_file *buf, void *v)
{
   struct qnx_driver_data* data = QNX_DRIVER_DATA(buf);
   
   int have_output = 0;
   
   rcu_read_lock();
   
   if (!list_empty(&data->process_entries))
   {
      struct qnx_process_entry* entry;
   
      list_for_each_entry_rcu(entry, &data->process_entries, hook)
      {
         if (!list_empty(&entry->channels))
         {
            struct qnx_channel* chnl;
            
            have_output = 1;
            seq_printf(buf, "pid=%d: ", entry->pid);
         
            list_for_each_entry_rcu(chnl, &entry->channels, hook)
            {
               seq_printf(buf, "%d ", chnl->chid);
            }
            
            seq_printf(buf, "\n");
         }         
      }
   }
   
   rcu_read_unlock();
   
   if (!have_output)
      seq_printf(buf, "<no processes attached>\n");
   
   return 0;
}


static int
qnx_open(struct inode *inode, struct file *file)
{
   if (!strncmp(QNX_PROC_CONNECTIONS, file->f_path.dentry->d_name.name, 2))
   {
      return single_open(file, qnx_show_connections, PDE_DATA(inode));
   }
   else if (!strncmp(QNX_PROC_CHANNELS, file->f_path.dentry->d_name.name, 2))
   {
      return single_open(file, qnx_show_channels, PDE_DATA(inode));
   }
   else
      return 0;
}


static 
const struct file_operations fops = {
   .owner   = THIS_MODULE,
   .open = qnx_open,
   .read = seq_read,
   .llseek  = seq_lseek,
   .release = single_release,
};


int qnx_proc_init(struct qnx_driver_data* data)
{
   struct proc_dir_entry* dir;
   
   if ((dir = proc_mkdir(QNX_PROC_ROOT_DIR, 0))
       && proc_create_data(QNX_PROC_CONNECTIONS, 0664, dir, &fops, data)
       && proc_create_data(QNX_PROC_CHANNELS, 0664, dir, &fops, data))
      return 1;
   
   remove_proc_subtree(QNX_PROC_ROOT_DIR, 0);
   return 0;
}


void qnx_proc_destroy(struct qnx_driver_data* data)
{
   remove_proc_subtree(QNX_PROC_ROOT_DIR, 0);
}

#endif   // LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
