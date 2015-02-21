#ifndef __QNXCOMM_CONNECTION_TABLE_H
#define __QNXCOMM_CONNECTION_TABLE_H


#include <linux/spinlock.h>

#include "connection.h"


// forward decl
struct qnx_connection;


typedef void(*ct_callback_t)(int coid, struct qnx_connection*, void*);


struct qnx_connection_table_data
{
   size_t capacity;   ///< capacity of conn array
   int max;           ///< maximum fd set 
   
   struct qnx_connection* conn[1];   
};


struct qnx_connection_table
{
   struct qnx_connection_table_data* data;      
   spinlock_t lock;
};


// ---------------------------------------------------------------------


// FIXME remove 'extern'
extern int qnx_connection_table_init(struct qnx_connection_table* table);
extern void qnx_connection_table_destroy(struct qnx_connection_table* table);
extern int qnx_connection_table_add(struct qnx_connection_table* table, struct qnx_connection* conn);
extern int qnx_connection_table_remove(struct qnx_connection_table* table, int coid);
extern struct qnx_connection qnx_connection_table_retrieve(struct qnx_connection_table* table, int coid);

/// these functions must only be called within a rcu_read_lock critical section.
extern int qnx_connection_table_is_empty(struct qnx_connection_table* table);
extern int qnx_connection_table_for_each(struct qnx_connection_table* table, ct_callback_t func, void* arg);


#endif   // __QNXCOMM_CONNECTION_TABLE_H
