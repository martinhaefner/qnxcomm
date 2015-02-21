#ifndef __QNXCOMM_PROC_H
#define __QNXCOMM_PROC_H


// forward decl
struct qnx_driver_data;


/// construction and destruction
extern int qnx_proc_init(struct qnx_driver_data* data);

extern void qnx_proc_destroy(struct qnx_driver_data* data);


#endif   // __QNXCOMM_PROC_H
