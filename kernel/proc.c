#include <linux/seq_file.h>

#include "qnxcomm_internal.h"


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)    

#define QNX_PROC_ROOT_DIR    "qnxcomm"

#define QNX_PROC_CONNECTIONS    "connections"
#define QNX_PROC_CHANNELS       "channels"
#define QNX_PROC_BLOCKED_TASKS  "blocked"

#define QNX_DRIVER_DATA(sf) ((struct qnx_driver_data*)sf->private) 

// FIXME add listing of blocked tasks


void print_connection(int coid, struct qnx_connection* conn, void* arg)
{
   struct seq_file* buf = (struct seq_file*)arg;
   seq_printf(buf, "   %d => pid=%d, chid=%d\n", coid, conn->pid, conn->chid);         
}


static int 
qnx_show_connections(struct seq_file *buf, void *v)
{
   struct qnx_driver_data* data = QNX_DRIVER_DATA(buf);
   struct qnx_process_entry* entry;
   
   int have_output = 0;
   
   rcu_read_lock();

   list_for_each_entry_rcu(entry, &data->process_entries, hook)
   {
      if (!qnx_connection_table_is_empty(&entry->connections))
      {      
         have_output = 1;
         
         seq_printf(buf, "pid=%d:\n", entry->pid);      
         
         (void)qnx_connection_table_for_each(&entry->connections, &print_connection, buf);
         
         seq_printf(buf, "\n");
      }
   }
         
   rcu_read_unlock();
   
   if (!have_output)
      seq_printf(buf, "<no processes attached>\n");

   return 0;
}


static int 
qnx_show_blocked_tasks(struct seq_file *buf, void *v)
{
   struct qnx_driver_data* data = QNX_DRIVER_DATA(buf);
   struct qnx_process_entry* entry;
   
   int have_output = 0;
   
   rcu_read_lock();

   list_for_each_entry_rcu(entry, &data->process_entries, hook)
   {
      // waiting - send blocked
      struct qnx_channel* chnl;
      struct qnx_internal_msgsend* msg;
         
      list_for_each_entry_rcu(chnl, &entry->channels, hook)
      {
         spin_lock(&chnl->waiting_lock);

         list_for_each_entry(msg, &chnl->waiting, hook)
         {
            if (msg->task)
            {
               have_output = 1;
               seq_printf(buf, "tid=%d (coid=%d) => pid=%d, chid=%d [SEND]\n", current_get_tid_nr(msg->task), msg->data.msg.coid, entry->pid, chnl->chid);
            }
         }

         spin_unlock(&chnl->waiting_lock);
      }
      
      // pending - reply blocked         
      spin_lock(&entry->pending_lock);

      list_for_each_entry(msg, &entry->pending, hook)
      {
         have_output = 1;
         seq_printf(buf, "tid=%d (coid=%d) => pid=%d, chid=%d [REPLY]\n", current_get_tid_nr(msg->task), msg->data.msg.coid, entry->pid, msg->receiver_chid);
      }

      spin_unlock(&entry->pending_lock);
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
   struct qnx_process_entry* entry;
   
   int have_output = 0;
   
   rcu_read_lock();
      
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
   else if (!strncmp(QNX_PROC_BLOCKED_TASKS, file->f_path.dentry->d_name.name, 2))
   {
      return single_open(file, qnx_show_blocked_tasks, PDE_DATA(inode));
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
       && proc_create_data(QNX_PROC_CHANNELS, 0664, dir, &fops, data)
       && proc_create_data(QNX_PROC_BLOCKED_TASKS, 0664, dir, &fops, data))
      return 1;
   
   remove_proc_subtree(QNX_PROC_ROOT_DIR, 0);
   return 0;
}


void qnx_proc_destroy(struct qnx_driver_data* data)
{
   remove_proc_subtree(QNX_PROC_ROOT_DIR, 0);
}

#endif   // LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
