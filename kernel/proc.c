#include <linux/seq_file.h>

#include "qnxcomm_internal.h"

#define QNX_PROC_ROOT_DIR "qnxcomm"
#define QNX_PROC_CONNECTIONS "connections"
#define QNX_PROC_CHANNELS "channels"


static int 
qnx_show_connections(struct seq_file *buf, void *v)
{
   struct qnx_driver_data* data = (struct qnx_driver_data*)buf->private;
   struct qnx_process_entry* entry;
   struct qnx_connection* conn;
   int have_output = 0;
   
   read_lock(&data->process_entries_lock);
   
   if (!list_empty(&data->process_entries))
   {
      list_for_each_entry(entry, &data->process_entries, hook)
      {
         read_lock(&entry->connections_lock);
         
         if (!list_empty(&entry->connections))
         {
            have_output = 1;
            seq_printf(buf, "pid=%d:\n", entry->pid);
         
            list_for_each_entry(conn, &entry->connections, hook)
            {
               seq_printf(buf, "   %d => pid=%d, chid=%d\n", conn->coid, conn->pid, conn->chid);
            }
            
            seq_printf(buf, "\n");
         }
                     
         read_unlock(&entry->connections_lock);
      }
   }
   
   read_unlock(&data->process_entries_lock);
   
   if (!have_output)
      seq_printf(buf, "<no processes attached>\n");
   
   return 0;
}


static int 
qnx_show_channels(struct seq_file *buf, void *v)
{
   struct qnx_driver_data* data = (struct qnx_driver_data*)buf->private;
   struct qnx_process_entry* entry;
   struct qnx_channel* chnl;
   int have_output = 0;
   
   read_lock(&data->process_entries_lock);
   
   if (!list_empty(&data->process_entries))
   {
      list_for_each_entry(entry, &data->process_entries, hook)
      {
         read_lock(&entry->channels_lock);
       
         if (!list_empty(&entry->channels))
         {
            have_output = 1;
            seq_printf(buf, "pid=%d: ", entry->pid);
         
            list_for_each_entry(chnl, &entry->channels, hook)
            {
               seq_printf(buf, "%d ", chnl->chid);
            }
            
            seq_printf(buf, "\n");
         }
         
         read_unlock(&entry->channels_lock);
      }
   }
   
   read_unlock(&data->process_entries_lock);
   
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