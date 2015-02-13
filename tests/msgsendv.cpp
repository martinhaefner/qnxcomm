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
   
   rcvid = MsgReceive(chid, buf, sizeof(buf), 0);
   EXPECT_GT(rcvid, 0);
   EXPECT_EQ(0, strcmp(buf, "Hallo Welt"));   
   
   strcpy(buf, "Super!");
   rc = MsgReply(rcvid, 5, buf, strlen(buf) + 1);
   EXPECT_EQ(0, rc);
   
   // -------------------------------------------------------------
   
   struct _msg_info info;
   rcvid = MsgReceive(chid, buf, sizeof(buf), &info);
   EXPECT_GT(rcvid, 0);
   EXPECT_EQ(0, strcmp(buf, "Hallo Welt"));
   
   EXPECT_EQ(0, info.nd);
   EXPECT_EQ(::getpid(), info.pid);   
   EXPECT_EQ(11, info.msglen);
   EXPECT_EQ(11, info.srcmsglen);
   EXPECT_EQ(35, info.dstmsglen);   
      
   rc = MsgError(rcvid, EINVAL);
   EXPECT_EQ(0, rc);
}


void zero_receiverthread(int chid)
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
   
   struct _msg_info info;
   rcvid = MsgReceive(chid, buf, sizeof(buf), &info);
   EXPECT_GT(rcvid, 0);
   EXPECT_EQ(0, strcmp(buf, "Hallo Welt"));   
   
   EXPECT_EQ(0, info.nd);
   EXPECT_EQ(::getpid(), info.pid);   
   EXPECT_EQ(11, info.msglen);
   EXPECT_EQ(11, info.srcmsglen);
   EXPECT_EQ(0, info.dstmsglen);   
   
   rc = MsgReply(rcvid, 5, 0, 0);
   EXPECT_EQ(0, rc);
   
   // -------------------------------------------------------------
   
   rcvid = MsgReceive(chid, buf, sizeof(buf), 0);
   EXPECT_GT(rcvid, 0);
   EXPECT_EQ(0, strcmp(buf, "Hallo Welt"));
   
   rc = MsgReply(rcvid, 42, 0, 0);
   EXPECT_EQ(0, rc);
}

}


TEST(MsgSendv, basics) 
{
   int chid = ChannelCreate(0);
   EXPECT_GT(chid, 0);
   
   int coid = ConnectAttach(0, 0, chid, 0, 0);
   EXPECT_GT(coid, 0);
 
   std::thread t(&receiverthread, chid);
   
   // now send message and wait for reply      
   char buf[80];   
   strcpy(buf, "Hallo +Welt");
   
   struct iovec  in[2] = { { buf, 6 }, { buf + 7, 5   } };
   struct iovec out[2] = { { buf, 3 }, { buf + 10, 32 } };
   int rc;
   
   rc = MsgSendv(coid, in, 2, out, 2);   
   EXPECT_EQ(0, rc);
   EXPECT_EQ(0, strncmp(buf, "Super!", 3));
   EXPECT_EQ(0, strcmp(buf+10, "er!"));
   
   strcpy(buf, "Hallo +Welt");
   rc = MsgSendv(coid, in, 2, out, 2);
   EXPECT_EQ(5, rc);
   EXPECT_EQ(0, strncmp(buf, "Super!", 3));
   EXPECT_EQ(0, strcmp(buf+10, "er!"));
      
   strcpy(buf, "Hallo +Welt");
   rc = MsgSendv(coid, in, 2, out, 2);
   EXPECT_EQ(-1, rc);
   EXPECT_EQ(EINVAL, errno);
   
   strcpy(buf, "Hallo +Welt");
   rc = MsgSendv(4711, in, 2, out, 2);
   EXPECT_EQ(-1, rc);
   EXPECT_EQ(EBADF, errno);
   
   t.join(); 
   
   EXPECT_EQ(0, ChannelDestroy(chid));
   EXPECT_EQ(0, ConnectDetach(coid));  
}


TEST(MsgSendv, noReplyData) 
{
   int chid = ChannelCreate(0);
   EXPECT_GT(chid, 0);
   
   int coid = ConnectAttach(0, 0, chid, 0, 0);
   EXPECT_GT(coid, 0);
 
   std::thread t(&zero_receiverthread, chid);
   
   // now send message and wait for reply      
   char buf[80];   
   strcpy(buf, "Hallo +Welt");
   
   struct iovec in[2] = { { buf, 6 }, { buf + 7, 5 } };
   int rc;
   
   rc = MsgSendv(coid, in, 2, 0, 0);
   EXPECT_EQ(0, rc);
   
   rc = MsgSendv(coid, in, 2, 0, 0);
   EXPECT_EQ(5, rc);
   
   rc = MsgSendv(coid, in, 2, in, 2);
   EXPECT_EQ(42, rc);
   EXPECT_EQ(0, strcmp(buf, "Hallo +Welt"));
   
   t.join(); 
   
   EXPECT_EQ(0, ChannelDestroy(chid));
   EXPECT_EQ(0, ConnectDetach(coid));  
}
