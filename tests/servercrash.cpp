#include <gtest/gtest.h>
#include <stdlib.h>
#include <fstream>

#include "qnxcomm.h"


const char* const cmds[] = {
   "./crashapp --abort &",
   "./crashapp --exit &",
   "./crashapp --channeldestroy &" 
};


template<int Idx>
class ServerCrashTest : public testing::Test 
{
protected:

   void SetUp() 
   {
      EXPECT_NE(system("./crashapp --abort &"), -1);
   
      for(int i=0; i<10; ++i)
      {
         struct stat st;
         if (stat("/tmp/gagaga", &st) == 0)
            break;
         
         usleep(100000);
      }
      
      std::ifstream f("/tmp/gagaga");
      assert(f);
      
      pid_t pid;
      int chid;
      f >> pid >> chid;
      
      coid_ = ConnectAttach(0, pid, chid, 0, 0);
      EXPECT_GT(coid_, 0);   
   }
   
   void TearDown()
   {
      unlink("/tmp/gagaga");
   }
   
   int coid_;
};


class ServerAbortTest : public ServerCrashTest<0> { };
class ServerExitTest : public ServerCrashTest<1> { };
class ServerChannelDestroyTest : public ServerCrashTest<2> { };


TEST_F(ServerAbortTest, msgsend)
{                      
   // now send a message and wait for the reply      
   char buf[80];   
   strcpy(buf, "Hallo Welt");
   
   int rc = MsgSend(coid_, buf, strlen(buf) + 1, buf, sizeof(buf));
   EXPECT_EQ(-1, rc);
   EXPECT_EQ(ESRCH, errno);
   
   EXPECT_EQ(0, ConnectDetach(coid_));  
}


TEST_F(ServerChannelDestroyTest, msgsend)
{                      
   // now send a message and wait for the reply      
   char buf[80];   
   strcpy(buf, "Hallo Welt");
   
   int rc = MsgSend(coid_, buf, strlen(buf) + 1, buf, sizeof(buf));
   EXPECT_EQ(-1, rc);
   EXPECT_EQ(ESRCH, errno);
   
   EXPECT_EQ(0, ConnectDetach(coid_));  
}


TEST_F(ServerExitTest, msgsend)
{                      
   // now send a message and wait for the reply      
   char buf[80];   
   strcpy(buf, "Hallo Welt");
   
   int rc = MsgSend(coid_, buf, strlen(buf) + 1, buf, sizeof(buf));
   EXPECT_EQ(-1, rc);
   EXPECT_EQ(ESRCH, errno);
   
   EXPECT_EQ(0, ConnectDetach(coid_));  
}
