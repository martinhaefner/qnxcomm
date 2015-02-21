#include <gtest/gtest.h>
#include <thread>

#include "qnxcomm.h"


TEST(MsgSend, disconnected) 
{
   int chid = ChannelCreate(0);
   EXPECT_GT(chid, 0);
   
   int coid = ConnectAttach(0, 0, chid, 0, 0);
   EXPECT_GT(coid, 0);
 
   EXPECT_EQ(0, ChannelDestroy(chid));
 
   // try send message      
   char buf[80];   
   strcpy(buf, "Hallo Welt");
   
   int rc = MsgSend(coid, buf, strlen(buf) + 1, buf, sizeof(buf));
   int error = errno;
   EXPECT_EQ(-1, rc);
   EXPECT_EQ(EBADF, error);   
   
   EXPECT_EQ(0, ConnectDetach(coid));  
}

