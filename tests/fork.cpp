#include <unistd.h>
#include <qnxcomm.h>

#include <thread>

//
// The driver checks for the pid in each ioctl message. If the pid
// is not associated with the current private_data then a -ENOSPC
// is returned which makes the userspace library to reconnect to the 
// driver for the newly created process. So, there should not be any
// performance degradation in case of normal applications, just in case
// of a fork there is a small overhead for the first message sent to
// the driver.
//


void runner(int pid, int chid)
{
   int coid = ConnectAttach(0, pid, chid, 0, 0);
   ConnectDetach(coid);
}


int main()
{
   int chid = ChannelCreate(0);
   int pid = getpid();
   
   if (fork() == 0)
   {
      // child
      int coid = ConnectAttach(0, pid, chid, 0, 0);      
      ConnectDetach(coid);
      
      std::thread t(&runner, pid, chid);
      t.join();
   }
   else
   {
      // parent
      sleep(10);
   }
   
   return 0;
}
