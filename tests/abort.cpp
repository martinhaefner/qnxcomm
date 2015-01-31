#include <cstdlib>
#include <thread>

#include "qnxcomm.h"


// abort from other thread than mainthread is no problem. The channel 
// will be closed.

// TODO will other process been unblocked on such an abort if he's waiting 
//      for a response in MsgSend?


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
