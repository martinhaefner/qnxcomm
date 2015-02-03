#ifndef __QNXCOMM_DRIVER_H
#define __QNXCOMM_DRIVER_H


/*
 * In order to setup permissions correctly, add a file named
 * 
 *   /etc/udev/rules.d/50-qnxcomm.rules
 * 
 * with the following contents:
 * 
 *   KERNEL=="qnxcomm", MODE="0666"
 */


#ifndef __QNXCOMM_H
#define __QNXCOMM_H


#include <linux/uio.h>
#include <asm-generic/siginfo.h>


// TODO how to handle this? - driver internal data vs. userspace library,
// but same contents
#define _NTO_SIDE_CHANNEL ((~0u ^ (~0u >> 1)) >> 1)


struct _msg_info 
{
   uint32_t  nd;         ///< client node address, always 0    
   pid_t     pid;        ///< sending clients pid      
   int32_t   tid;        ///< sending client thread tid 
   int32_t   chid;       ///< chid which receives the message
   int32_t   scoid;      ///< (sender) connection id as seen by the client
   int32_t   coid;       ///< connection id as seen by the server (same as scoid)
   int32_t   msglen;     ///< equal to srcmsglen
   int32_t   srcmsglen;  ///< length of MsgSend input data
   int32_t   dstmsglen;  ///< length of MsgSend output data
   int16_t   priority;   ///< priority of message, i.e. thread priority or pulse priority
   int16_t   flags;      ///< unused
   uint32_t  reserved;   ///< unused
};


struct _pulse
{   
   uint16_t                    type;
   uint16_t                    subtype;   
   int8_t                      code;
   uint8_t                     zero[3];
   union sigval                value;
   int32_t                     scoid;   ///< FIXME unset
};


#endif   // __QNXCOMM_H


struct qnx_io_attach
{
   pid_t pid;
   int chid;
   unsigned int index;
   int flags;
};


struct qnx_io_msgsend
{
   int coid;
   int timeout_ms;    // timeout in milliseconds
      
   struct iovec in;
   struct iovec out;
};


struct qnx_io_msgsendv
{
   int coid;
   int timeout_ms;    // timeout in milliseconds
   
   struct iovec* in;
   int in_len;
   
   struct iovec* out;
   int out_len;
};



struct qnx_io_msgsendpulse
{
   int coid;
   
   int code;
   int value;   
};


struct qnx_io_receive
{
   int chid;
   int timeout_ms;     // timeout in milliseconds
   
   struct iovec out;
   
   struct _msg_info info;
};


struct qnx_io_reply
{
   int rcvid;
   int status;
   
   struct iovec in;
};


struct qnx_io_error_reply
{
    int rcvid;
    int error;
};


struct qnx_io_read
{
    int rcvid;
    int offset;
    
    struct iovec out;
};


#define QNXCOMM_MAGIC 'q'


#define QNX_IO_CHANNELCREATE   _IO(QNXCOMM_MAGIC,  1)
#define QNX_IO_CHANNELDESTROY _IOW(QNXCOMM_MAGIC,  2, int)

#define QNX_IO_CONNECTATTACH  _IOW(QNXCOMM_MAGIC,  3, struct qnx_io_attach)
#define QNX_IO_CONNECTDETACH  _IOW(QNXCOMM_MAGIC,  4, int)

#define QNX_IO_MSGSEND        _IOW(QNXCOMM_MAGIC,  5, struct qnx_io_msgsend)
#define QNX_IO_MSGSENDPULSE   _IOW(QNXCOMM_MAGIC,  6, struct qnx_io_msgsendpulse)

#define QNX_IO_MSGRECEIVE    _IOWR(QNXCOMM_MAGIC,  7, struct qnx_io_receive)
#define QNX_IO_MSGREPLY       _IOW(QNXCOMM_MAGIC,  8, struct qnx_io_reply)
#define QNX_IO_MSGERROR       _IOW(QNXCOMM_MAGIC,  9, struct qnx_io_error_reply)
#define QNX_IO_MSGREAD        _IOW(QNXCOMM_MAGIC, 10, struct qnx_io_read)

#define QNX_IO_MSGSENDV       _IOW(QNXCOMM_MAGIC, 11, struct qnx_io_msgsendv)

// FIXME add MsgReceivePulse and move them all to scatter gather versions


#endif   // __QNXCOMM_DRIVER_H
