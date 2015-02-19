#include "qnxcomm_internal.h"

#include <linux/fs.h>


#define QNX_INITIAL_TABLE_SIZE 64


static inline
size_t qnx_connection_table_get_capacity(struct qnx_connection_table* table)
{
   return table->data ? table->data->capacity : 0;
}


static
int qnx_connection_table_reserve_unlocked(struct qnx_connection_table* table, int newsize)
{
   struct qnx_connection_table_data* old;
   struct qnx_connection_table_data* new;
   
   int oldsize = qnx_connection_table_get_capacity(table);
   
   if (unlikely(newsize > 1024   /*FIXME somehow get sysctl_nr_open*/))
      return -EMFILE;
      
   if (likely(newsize <= oldsize))
      return oldsize;
      
   old = table->data;
   
   new = (struct qnx_connection_table_data*)kmalloc(sizeof(struct qnx_connection_table_data) 
         + sizeof(struct qnx_connection*) * newsize /*exactly -1*/, GFP_USER);   
      
   if (unlikely(!new))
      return -ENOMEM;
   
   if (oldsize > 0)
      memcpy(new->conn, old->conn, sizeof(struct qnx_connection*) * oldsize);
   
   memset(new->conn + oldsize, 0, sizeof(struct qnx_connection*) * (newsize - oldsize));
   
   new->capacity = newsize; 
   new->max = 0;
   
   rcu_assign_pointer(table->data, new);   
   
   synchronize_rcu();
   kfree(old);
   
   return oldsize;
}
   

int qnx_connection_table_init(struct qnx_connection_table* table)
{
   int rc;
      
   table->data = 0;
   
   spin_lock_init(&table->lock);
   
   rc = qnx_connection_table_reserve_unlocked(table, QNX_INITIAL_TABLE_SIZE);
   return rc > 0 ? 0 : rc;
}


void qnx_connection_table_destroy(struct qnx_connection_table* table)
{
   int i;
   
   for(i=0; i < qnx_connection_table_get_capacity(table); ++i)   // FIXME use max here instead of capacity
   {
      if (table->data->conn[i])
         kfree(table->data->conn[i]);
   }
   
   kfree(table->data);
}


int qnx_connection_table_add(struct qnx_connection_table* table, struct qnx_connection* conn)
{
   int rc = 0;
   int i;
   
   spin_lock(&table->lock);
      
   for(i=1; i<qnx_connection_table_get_capacity(table); ++i)
   {
      if (!table->data->conn[i])
      {
         rcu_assign_pointer(table->data->conn[i], conn);
         rc = i;
         break;
      }
   }

   if (unlikely(rc == 0))
   {
      rc = qnx_connection_table_reserve_unlocked(table, qnx_connection_table_get_capacity(table) << 1);
      
      if (likely(rc > 0))
      {
         rcu_assign_pointer(table->data->conn[table->data->capacity], conn);
         rc = table->data->capacity;
      }
   }
   
   spin_unlock(&table->lock);
   
   return rc;
}


int qnx_connection_table_remove(struct qnx_connection_table* table, int coid)
{
   int rc = -EINVAL;
   
   struct qnx_connection* conn = 0;
   
   spin_lock(&table->lock);
   
   // FIXME check for signed/unsigned stuff here...
   if (likely(coid < qnx_connection_table_get_capacity(table)))
   {
      conn = table->data->conn[coid];
   
      if (likely(conn))
      {
         rcu_assign_pointer(table->data->conn[coid], NULL);
         rc = 0;
      }      
   }
   
   spin_unlock(&table->lock);
   
   synchronize_rcu();
   kfree(conn);
   
   return rc;
}


struct qnx_connection qnx_connection_table_retrieve(struct qnx_connection_table* table, int coid)
{
   struct qnx_connection rc = { 0 , 0 };
   struct qnx_connection* conn;
   
   struct qnx_connection_table_data* data;
   
   rcu_read_lock();
   
   data = rcu_dereference(table->data);   
   
   if (likely(data))
   {   
      if (likely(coid < data->capacity))
      {               
         conn = rcu_dereference(data->conn[coid]);
         
         if (likely(conn))
            rc = *conn;
      }
   }         
   
   rcu_read_unlock();
   
   return rc;
}
