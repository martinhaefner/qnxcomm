#ifndef __QNXCOMM_H
#define __QNXCOMM_H


#include <stdint.h>
#include <sys/uio.h>
#include <signal.h>


#ifdef __cplusplus
extern "C" {
#endif   


#define _NTO_SIDE_CHANNEL ((int)((~0u ^ (~0u >> 1)) >> 1))
#define QNX_FLAG_NOREPLY    0x1

struct _msg_info 
{
   uint32_t  nd;         ///< client node address, always 0    
   pid_t     pid;        ///< sending clients pid      
   int32_t   tid;        ///< sending client thread tid (TODO currently unset)
   int32_t   chid;       ///< chid which receives the message
   int32_t   scoid;      ///< (sender) connection id as seen by the client
   int32_t   coid;       ///< connection id as seen by the server (same as scoid)
   int32_t   msglen;     ///< equal to srcmsglen
   int32_t   srcmsglen;  ///< length of MsgSend input data
   int32_t   dstmsglen;  ///< length of MsgSend output data
   int16_t   priority;   ///< priority of message, i.e. thread priority or pulse priority (TODO currently unset)
   int16_t   flags;      ///< may have the flag QNX_FLAG_NOREPLY set
   uint32_t  reserved;   ///< unused
};


struct _pulse
{   
   uint16_t                    type;    ///< TODO currently unset
   uint16_t                    subtype; ///< TODO currently unset
   int8_t                      code;    ///< pulse code
   uint8_t                     zero[3]; ///< padding
   union sigval                value;   ///< pulse value
   int32_t                     scoid;   ///< TODO currently unset
};


int ChannelCreate(unsigned flags);

int ChannelDestroy(int chid);

int ConnectAttach(uint32_t nd, pid_t pid, int chid, unsigned index, int flags);

int ConnectDetach(int coid);


int MsgSend(int coid, const void* smsg, int sbytes, void* rmsg, int rbytes);

int MsgSendv(int coid, const struct iovec* siov, int sparts, const struct iovec* riov, int rparts);

int MsgSendPulse(int coid, int priority, int code, int value);


int MsgReceive(int chid, void* msg, int bytes, struct _msg_info* info);

int MsgRead(int rcvid, void* msg, int bytes, int offset);

int MsgReply(int rcvid, int status, const void* msg, int size);

int MsgError(int rcvid, int error);


// TODO so far unimplemented
int MsgReceivev(int chid, const struct iovec* riov, int rparts, struct _msg_info* info);

int MsgReadv(int rcvid, const struct iovec* riov, int rparts, int offset);

int MsgWrite(int rcvid, const void* msg, int size, int offset);

int MsgReplyv(int rcvid, int status, const struct iovec* riov, int rparts);

int MsgReceivePulse(int chid, void * pulse, int bytes, struct _msg_info * info);


// timeout support
int TimerTimeout(clockid_t id, int flags, const struct sigevent * notify, const uint64_t * ntime, uint64_t * otime);


// -----------------------------------------------------------------------------


/**
 * Compatibility only. No behaviour change to ChannelCreate.
 */
int ChannelCreateEx(unsigned flags, const char* mode);

/**
 * Compatibility only. No behaviour change to ConnectAttachEx.
 */
int ConnectAttachEx(uint32_t nd, pid_t pid, int chid, unsigned index, int flags, const char* mode);


// -----------------------------------------------------------------------------


/**
 * Send message but don't wait for reply.
 * The maximum size for the message is 1k.
 * The function may block if no more internal slots are available.
 * You must not reply on such a message via MsgReply, nor can you
 * read more data after calling MsgReceive. A NoReply message can be
 * detected via the _msg_info structure flags (QNX_FLAG_NOREPLY is set).
 */
int MsgSendNoReply(int coid, const void* smsg, int sbytes);

/**
 * Send message but don't wait for reply.
 * The maximum size for the message is 1k.
 * The function may block if no more internal slots are available.
 * You must not reply on such a message via MsgReply, nor can you
 * read more data after calling MsgReceive. A NoReply message can be
 * detected via the _msg_info structure flags (QNX_FLAG_NOREPLY is set).
 */
int MsgSendNoReplyv(int coid, const struct iovec* siov, int sparts);

/**
 * Return a file descriptor to be used for polling for new messages
 * using select, poll or epoll. The fd may NOT be used to retrieve data,
 * use MsgReceive instead. The fd must be closed separately from 
 * ChannelDestroy. The file descriptor may not be used after a fork().
 */
int MsgReceivePollFd(int chid);


#ifdef __cplusplus
}
#endif  


#endif   // __QNXCOMM_H
