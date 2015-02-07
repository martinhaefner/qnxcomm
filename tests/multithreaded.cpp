#include <gtest/gtest.h>
#include <thread>

#include "qnxcomm.h"


namespace {

const int NUM_REQUESTS = 2000;
const int NUM_THREADS = 20;


void senderthread(int chid)
{
   const char sendbuf[] = "Hallo Welt";
   char recvbuf[32];
   
   int coid = ConnectAttach(0, 0, chid, 0, 0);
   EXPECT_GT(coid, 0);

   for(int i=0; i<NUM_REQUESTS; ++i)
   {
      EXPECT_EQ(0, MsgSend(coid, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf))); 
      EXPECT_EQ(0, strcmp(recvbuf, "Super Show"));
   }
   
   EXPECT_EQ(0, ConnectDetach(coid));
}

}


TEST(qnxcomm, multithreaded) 
{
   int i;
   
   int chid = ChannelCreate(0);
   EXPECT_GT(chid, 0);

   std::thread threads[NUM_THREADS];
   
   for (i=0 ; i<NUM_THREADS; ++i)
   {
      threads[i] = std::move(std::thread(&senderthread, chid));
   }
   
   int rcvid;
   const char sendbuf[] = "Super Show";
   char recvbuf[32];
   
   for(i=0; i<NUM_THREADS * NUM_REQUESTS; ++i)
   {
      rcvid = MsgReceive(chid, recvbuf, sizeof(recvbuf), 0);
      EXPECT_GT(rcvid, 0);
      
      EXPECT_EQ(0, strcmp(recvbuf, "Hallo Welt"));
      EXPECT_EQ(0, MsgReply(rcvid, 0, sendbuf, sizeof(sendbuf)));
   }
   
   for (i=0 ; i<NUM_THREADS; ++i)
   {
      threads[i].join();
   }
   
   EXPECT_EQ(0, ChannelDestroy(chid));
}
