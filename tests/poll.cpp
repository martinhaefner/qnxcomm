#include <gtest/gtest.h>

#include <thread>
#include <sys/poll.h>

#include "qnxcomm.h"


namespace {

void senderthread(int chid, bool wait)
{
   int coid = ConnectAttach(0, 0, chid, 0, 0);
   EXPECT_GT(coid, 0);
   
   char buf[16];
   strcpy(buf, "Hallo Welt");
   
   if (wait)
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
   
   int rc = MsgSend(coid, buf, strlen(buf)+1, buf, sizeof(buf));
   EXPECT_EQ(0, rc);
   EXPECT_EQ(0, strcmp(buf, "Supi"));   
   
   EXPECT_EQ(0, ConnectDetach(coid));
}

}


TEST(PollTest, setup) 
{
   int chid = ChannelCreate(0);
   EXPECT_GT(chid, 0);
   
   int fd = MsgReceivePollFd(chid);
   EXPECT_GT(fd, 0);
      
   EXPECT_EQ(0, close(fd));   
   
   fd = MsgReceivePollFd(4711);
   int error = errno;
   EXPECT_EQ(-1, fd);
   EXPECT_EQ(ESRCH, error);   
      
   EXPECT_EQ(0, ChannelDestroy(chid));   
}


TEST(PollTest, timeout) 
{
   int chid = ChannelCreate(0);
   EXPECT_GT(chid, 0);
   
   int fd = MsgReceivePollFd(chid);
   EXPECT_GT(fd, 0);
      
   struct pollfd fds = { fd, POLLIN, 0 };
   int rc = poll(&fds, 1, 500);
   EXPECT_EQ(0, rc);
   
   EXPECT_EQ(0, close(fd));      
   EXPECT_EQ(0, ChannelDestroy(chid));   
}


TEST(PollTest, data_after_poll) 
{
   int chid = ChannelCreate(0);
   EXPECT_GT(chid, 0);
   
   int fd = MsgReceivePollFd(chid);
   EXPECT_GT(fd, 0);
      
   std::thread t(senderthread, chid, true);
      
   struct pollfd fds = { fd, POLLIN, 0 };
   int rc = poll(&fds, 1, 1000);   
   EXPECT_GT(rc, 0);
   
   char buf[16];   
   int rcvid = MsgReceive(chid, buf, sizeof(buf), 0);   
   EXPECT_GT(rcvid, 0);
   
   strcpy(buf, "Supi");
   rc = MsgReply(rcvid, 0, buf, strlen(buf) + 1);
   EXPECT_EQ(0, rc);
   
   t.join();
   
   EXPECT_EQ(0, close(fd));      
   EXPECT_EQ(0, ChannelDestroy(chid));   
}


TEST(PollTest, data_before_poll) 
{
   int chid = ChannelCreate(0);
   EXPECT_GT(chid, 0);
   
   int fd = MsgReceivePollFd(chid);
   EXPECT_GT(fd, 0);
      
   std::thread t(senderthread, chid, false);
   std::this_thread::sleep_for(std::chrono::milliseconds(500));
   
   struct pollfd fds = { fd, POLLIN, 0 };
   int rc = poll(&fds, 1, 1000);   
   EXPECT_GT(rc, 0);
   
   char buf[16];   
   int rcvid = MsgReceive(chid, buf, sizeof(buf), 0);   
   EXPECT_GT(rcvid, 0);
   
   strcpy(buf, "Supi");
   rc = MsgReply(rcvid, 0, buf, strlen(buf) + 1);
   EXPECT_EQ(0, rc);
   
   t.join();
   
   EXPECT_EQ(0, close(fd));      
   EXPECT_EQ(0, ChannelDestroy(chid));   
}
