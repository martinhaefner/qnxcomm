#include <gtest/gtest.h>
#include <thread>

#include "qnxcomm.h"


namespace {

void receiverthread(int chid)
{
   char buf[80];
   memset(buf, 0xFF, sizeof(buf));
      
   int rcvid = MsgReceive(chid, buf, sizeof(buf), 0);
   EXPECT_GT(rcvid, 0);
   EXPECT_EQ(0, strcmp(buf, "Hallo Welt"));
   
   strcpy(buf, "Super!");
   int rc = MsgReply(rcvid, 0, buf, strlen(buf) + 1);
   EXPECT_EQ(0, rc);
   
   // -------------------------------------------------------------
   
   // FIXME check fields of _msginfo
   rcvid = MsgReceive(chid, buf, sizeof(buf), 0);
   EXPECT_GT(rcvid, 0);
   EXPECT_EQ(0, strcmp(buf, "Hallo Welt"));
   
   strcpy(buf, "Super!");
   rc = MsgReply(rcvid, 5, buf, strlen(buf) + 1);
   EXPECT_EQ(0, rc);
   
   // -------------------------------------------------------------
   
   rcvid = MsgReceive(chid, buf, sizeof(buf), 0);
   EXPECT_GT(rcvid, 0);
   EXPECT_EQ(0, strcmp(buf, "Hallo Welt"));
      
   rc = MsgError(rcvid, EINVAL);
   EXPECT_EQ(0, rc);
}

}


TEST(qnxcomm, msgsend) 
{
   int chid = ChannelCreate(0);
   EXPECT_GT(chid, 0);
   
   int coid = ConnectAttach(0, 0, chid, 0, 0);
   EXPECT_GT(coid, 0);
 
   std::thread t(&receiverthread, chid);
   
   // now send message and wait for reply      
   char buf[80];   
   strcpy(buf, "Hallo Welt");
   
   int rc = MsgSend(coid, buf, strlen(buf) + 1, buf, sizeof(buf));
   EXPECT_EQ(0, rc);
   EXPECT_EQ(0, strcmp(buf, "Super!"));
   
   strcpy(buf, "Hallo Welt");
   rc = MsgSend(coid, buf, strlen(buf) + 1, buf, sizeof(buf));
   EXPECT_EQ(5, rc);
   EXPECT_EQ(0, strcmp(buf, "Super!"));
      
   strcpy(buf, "Hallo Welt");
   rc = MsgSend(coid, buf, strlen(buf) + 1, buf, sizeof(buf));
   EXPECT_EQ(-1, rc);
   EXPECT_EQ(EINVAL, errno);
   
   strcpy(buf, "Hallo Welt");
   rc = MsgSend(4711, buf, strlen(buf) + 1, buf, sizeof(buf));
   EXPECT_EQ(-1, rc);
   EXPECT_EQ(EBADF, errno);
   
   t.join(); 
   
   EXPECT_EQ(0, ChannelDestroy(chid));
   EXPECT_EQ(0, ConnectDetach(coid));  
}
