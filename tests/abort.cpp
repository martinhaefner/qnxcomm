#include <cstdlib>
#include <thread>

#include "qnxcomm.h"


// abort from other thread than mainthread is no problem. The channel 
// will be closed.


void thread_runner()
{
   int chnl = ChannelCreate(0);   
   abort();
}


int main()
{
   std::thread t(&thread_runner);
   t.join();
   
   return 0;
}
