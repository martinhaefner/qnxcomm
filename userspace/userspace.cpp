#define __STDC_LIMIT_MACROS 1

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <limits>

#include "qnxcomm.h"
#include "../kernel/qnxcomm_driver.h"


namespace {
   
int initialize()
{
   return ::open("/dev/qnxcomm", O_RDWR|O_CLOEXEC);
}

static int fd = initialize();


struct timeout_val
{
   inline
   void set(uint64_t the_ntime, uint64_t* the_otime)
   {
      ntime = the_ntime;
      otime = the_otime;
   }

   inline
   void finish()
   {
      if (otime != 0)
         *otime = 0;

      ntime = UINT64_MAX;
      otime = 0;
   }

   uint64_t  ntime;
   uint64_t* otime;
};


// timeout in nanoseconds for next qnxcomm "system call" stored in TLS entry
__thread timeout_val sTimerTimeout = { UINT64_MAX, 0 };


struct TimerStackSafe
{   
   inline
   ~TimerStackSafe()
   {
      sTimerTimeout.finish();
   }
   
   inline
   int get_timeout_ms()
   {
      uint64_t rc = sTimerTimeout.ntime / 1000000;
      return rc > std::numeric_limits<int>::max() ? -1 : rc;      
   }
};

}


extern "C"
int TimerTimeout(clockid_t id, int flags, const struct sigevent * notify, const uint64_t * ntime, uint64_t * otime)
{      
   if (!ntime || id != CLOCK_MONOTONIC)
   {
      errno = EINVAL;
      return -1;
   }

   uint64_t timeout = *ntime;      

   if (flags & TIMER_ABSTIME)
   {      
      struct timespec ts;
      ::clock_gettime(CLOCK_MONOTONIC, &ts); 
      
      uint64_t now = static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<uint64_t>(ts.tv_nsec);

      if (now > timeout)
         timeout = 0;
      else
         timeout -= now;
   }
      
   sTimerTimeout.set(timeout, otime);         
   return 0;
}


// ---------------------------------------------------------------------


extern "C" 
int ChannelCreate(unsigned flags)
{   
   int rc = -1;
   
   if (fd >= 0)
   { 	  
	  struct qnx_io_channelcreate data = { flags };
      rc = ioctl(fd, QNX_IO_CHANNELCREATE, &data);
   }
   else
      errno = ESRCH;
   
   return rc;
}


extern "C"
int ChannelDestroy(int chid)
{
   int rc = -1;
    
   if (fd >= 0)
   {
      rc = ioctl(fd, QNX_IO_CHANNELDESTROY, chid);
   }
   else
      errno = ESRCH;
       
   return rc;
}


// ---------------------------------------------------------------------


extern "C" 
int ConnectAttach(uint32_t nd, pid_t pid, int chid, unsigned index, int flags)
{
   int rc = -1;
   
   if (fd >= 0)
   { 
      if (!nd)
      {
         if (pid == 0)
            pid = ::getpid();
         
         struct qnx_io_attach data = { pid, chid, index, flags };
         rc = ioctl(fd, QNX_IO_CONNECTATTACH, &data);
      }
      else
         errno = EINVAL;
   }
   else
      errno = ESRCH;
   
   return rc;
}


extern "C"
int ConnectDetach(int coid)
{
   int rc = -1;
    
   if (fd >= 0)
   {
      rc = ioctl(fd, QNX_IO_CONNECTDETACH, coid);
   }
   else
      errno = ESRCH;
       
   return rc;
}


// ---------------------------------------------------------------------


extern "C" 
int MsgSend(int coid, const void* smsg, int sbytes, void* rmsg, int rbytes)
{
   int rc = -1;
   
   if (fd >= 0)
   {
      TimerStackSafe ttsf;
      struct qnx_io_msgsend io = { coid, ttsf.get_timeout_ms(), { const_cast<void*>(smsg), (size_t)sbytes }, { rmsg, (size_t)rbytes } };
      rc = ioctl(fd, QNX_IO_MSGSEND, &io);
   }
   else
      errno = ESRCH;
      
   return rc;
}


extern "C" 
int MsgSendPulse(int coid, int /*priority*/, int code, int value)
{
   int rc = -1;
   
   if (fd >= 0)
   {
      struct qnx_io_msgsendpulse io = { coid, code, value };
      rc = ioctl(fd, QNX_IO_MSGSENDPULSE, &io);
   }
   else
      errno = ESRCH;
      
   return rc;
}


// ---------------------------------------------------------------------


extern "C" 
int MsgReceive(int chid, void* msg, int bytes, struct _msg_info* info)
{
   int rc = -1;
   
   if (fd >= 0)
   {
      TimerStackSafe ttsf;
      struct qnx_io_receive io = { chid, ttsf.get_timeout_ms(), { msg, (size_t)bytes }, { 0 } };      
      rc = ioctl(fd, QNX_IO_MSGRECEIVE, &io);
      
      if (rc >= 0 && info)      
         memcpy(info, &io.info, sizeof(struct _msg_info));      
   }
   else
      errno = ESRCH;
      
   return rc;
}


extern "C" 
int MsgReply(int rcvid, int status, const void* msg, int size)
{
   int rc = -1;
   
   if (fd >= 0)
   {
      struct qnx_io_reply io = { rcvid, status, { const_cast<void*>(msg), (size_t)size } };
      rc = ioctl(fd, QNX_IO_MSGREPLY, &io);
   }
   else
      errno = ESRCH;
      
   return rc;
}


extern "C" 
int MsgError(int rcvid, int error)
{
   int rc = -1;
   
   if (fd >= 0)
   {
      struct qnx_io_error_reply io = { rcvid, error };
      rc = ioctl(fd, QNX_IO_MSGERROR, &io);
   }
   else
      errno = ESRCH;
      
   return rc;
}


extern "C"
int MsgRead(int rcvid, void* msg, int bytes, int offset)
{
   int rc = -1;
   
   if (fd >= 0)
   {
      struct qnx_io_read io = { rcvid, offset, { msg, (size_t)bytes } };      
      rc = ioctl(fd, QNX_IO_MSGREAD, &io);      
   }
   else
      errno = ESRCH;
      
   return rc;
}


// ---------------------------------------------------------------------


extern "C"
int MsgSendv(int coid, const struct iovec* siov, int sparts, const struct iovec* riov, int rparts)
{
   int rc = -1;
   
   if (fd >= 0)
   {
      TimerStackSafe ttsf;
      struct qnx_io_msgsendv io = { coid, ttsf.get_timeout_ms(), const_cast<struct iovec*>(siov), sparts, const_cast<struct iovec*>(riov), rparts };
      rc = ioctl(fd, QNX_IO_MSGSENDV, &io);
   }
   else
      errno = ESRCH;
      
   return rc;
}


extern "C"
int MsgReceivev(int chid, const struct iovec* riov, int rparts, struct _msg_info* info)
{   
   errno = ENOSYS;
   return -1;
}


extern "C"
int MsgReceivePulse(int chid, void * pulse, int bytes, struct _msg_info * info)
{
   errno = ENOSYS;
   return -1;
}


extern "C"
int MsgReadv(int rcvid, const struct iovec* riov, int rparts, int offset)
{   
   errno = ENOSYS;
   return -1;
}


extern "C"
int MsgWrite(int rcvid, const void* msg, int size, int offset)
{
   errno = ENOSYS;
   return -1;
}


extern "C"
int MsgReplyv(int rcvid, int status, const struct iovec* riov, int rparts)
{   
   errno = ENOSYS;
   return -1;
}


// -----------------------------------------------------------------------------


extern "C"
int ConnectAttachEx(uint32_t nd, pid_t pid, int chid, unsigned index, int flags, const char* /*mode*/)
{
   return ConnectAttach(nd, pid, chid, index, flags);   
}


extern "C"
int ChannelCreateEx(unsigned flags, const char* /*mode*/)
{
   return ChannelCreate(flags);
}



