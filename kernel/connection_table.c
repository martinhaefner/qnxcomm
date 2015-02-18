#include "qnxcomm_internal.h"

#include <linux/fs.h>


#define QNX_INITIAL_TABLE_SIZE 64


static
int qnx_connection_table_reserve_unlocked(struct qnx_connection_table* table, int newsize)
{
   struct qnx_connection** old;
   struct qnx_connection** new;
   int oldsize = 0;
   
   if (unlikely(newsize > 1024/*FIXME somehow get sysctl_nr_open*/))
      return -EMFILE;
      
   if (likely(newsize <= table->capacity))
      return table->capacity;
   
   oldsize = table->capacity;
   old = table->conn;

   new = (struct qnx_connection**)kmalloc(sizeof(struct qnx_connection*) * newsize, GFP_USER);   
   if (unlikely(!new))
      return -ENOMEM;
   
   if (oldsize > 0)
      memcpy(new, table->conn, sizeof(struct qnx_connection*) * oldsize);
   
   memset(new + oldsize, 0, sizeof(struct qnx_connection*) * (newsize - oldsize));
   
   rcu_assign_pointer(table->conn, new);
   table->capacity = newsize;   // FIXME do we need a separate mem barrier here?
   
   synchronize_rcu();
   kfree(old);
   
   return oldsize;
}
   

int qnx_connection_table_init(struct qnx_connection_table* table)
{
   int rc;
   
   table->capacity = 0;
   table->conn = 0;
   
   spin_lock_init(&table->lock);
   
   rc = qnx_connection_table_reserve_unlocked(table, QNX_INITIAL_TABLE_SIZE);
   return rc > 0 ? 0 : rc;
}


void qnx_connection_table_destroy(struct qnx_connection_table* table)
{
   int i;
   
   for(i=0; i<table->capacity; ++i)
   {
      if (table->conn[i])
         kfree(table->conn[i]);
   }
   
   kfree(table->conn);
}


int qnx_connection_table_add(struct qnx_connection_table* table, struct qnx_connection* conn)
{
   int rc = 0;
   int i;
   
   spin_lock(&table->lock);
   
   for(i=1; i < table->capacity; ++i)
   {
      if (!table->conn[i])
      {
         rcu_assign_pointer(table->conn[i], conn);
         rc = i;
         break;
      }
   }
   
   if (unlikely(rc == 0))
   {
      rc = qnx_connection_table_reserve_unlocked(table, table->capacity << 1);
      if (likely(rc > 0))
      {
         rcu_assign_pointer(table->conn[table->capacity], conn);
         rc = table->capacity;
      }
   }
   
   spin_unlock(&table->lock);
   
   return rc;
}


int qnx_connection_table_remove(struct qnx_connection_table* table, int coid)
{
   int rc = 0;
   struct qnx_connection* conn = 0;
   
   spin_lock(&table->lock);
   
   // FIXME check for signed/unsigned stuff here...
   if (likely(coid < table->capacity))
   {
      conn = table->conn[coid];
   
      if (likely(conn))
         rcu_assign_pointer(table->conn[coid], NULL);
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
   
   rcu_read_lock();
   
   if (likely(coid < table->capacity))
   {
      conn = rcu_dereference(table->conn)[coid];
      if (likely(conn))
         rc = *conn;
   }
      
   rcu_read_unlock();
   
   return rc;
}
