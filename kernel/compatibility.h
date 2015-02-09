#ifndef QNXCOMM_COMPATIBILITY_H
#define QNXCOMM_COMPATIBILITY_H


#include <asm/uaccess.h>


#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,2,0)

static
int memcpy_toiovec(struct iovec *iov, unsigned char *kdata, int len)
{
         while (len > 0) {
                 if (iov->iov_len) {
                         int copy = min_t(unsigned int, iov->iov_len, len);
                         if (copy_to_user(iov->iov_base, kdata, copy))
                                 return -EFAULT;
                         kdata += copy;
                         len -= copy;
                         iov->iov_len -= copy;
                         iov->iov_base += copy;
                 }
                 iov++;
         }
 
         return 0;
}


static 
int memcpy_fromiovec(unsigned char *kdata, struct iovec *iov, int len)
{
   while (len > 0) 
   {
      if (iov->iov_len) 
      {
         int copy = min_t(unsigned int, len, iov->iov_len);
         if (copy_from_user(kdata, iov->iov_base, copy))
            return -EFAULT;
            
         len -= copy;
         kdata += copy;
         iov->iov_base += copy;
         iov->iov_len -= copy;
      }
      
      iov++;
   }

   return 0;
}

#endif   // LINUX_VERSION_CODE <= KERNEL_VERSION(3,2,0)


#endif   // QNXCOMM_COMPATIBILITY_H
