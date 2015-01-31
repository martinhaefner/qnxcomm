#include <gtest/gtest.h>
#include <thread>

#include "qnxcomm.h"


namespace {
   
void receiverthread(int chid)
{
   struct _pulse pulse;   
   
   int rcvid = MsgReceive(chid, &pulse, sizeof(pulse), 0);
   EXPECT_EQ(0, rcvid);   
   EXPECT_EQ(42, pulse.code);   
   EXPECT_EQ(4711, pulse.value.sival_int);   
   
   // FIXME check also msginfo
   rcvid = MsgReceive(chid, &pulse, sizeof(pulse), 0);
   EXPECT_EQ(0, rcvid);   
   EXPECT_EQ(42, pulse.code);   
   EXPECT_EQ(4712, pulse.value.sival_int);   
   
   int rc = MsgReply(rcvid, 0, 0, 0);   
   EXPECT_EQ(-1, rc);
   EXPECT_EQ(ESRCH, errno);
   
   rc = MsgError(rcvid, ENOTSUP);   
   EXPECT_EQ(-1, rc);
   EXPECT_EQ(ESRCH, errno);
   
   rc = MsgReply(42, 0, 0, 0);   
   EXPECT_EQ(-1, rc);
   EXPECT_EQ(ESRCH, errno);
   
   rc = MsgError(42, EINVAL);   
   EXPECT_EQ(-1, rc);
   EXPECT_EQ(ESRCH, errno);   
}

}


class MsgSendPulseTest : public testing::Test 
{
protected:

   void SetUp() 
   {
      chid_ = ChannelCreate(0);
      coid_ = ConnectAttach(0, 0, chid_, 0, 0);
      
      t_ = std::move(std::thread(&receiverthread, chid_));
   }
   
   
   void TearDown()
   {
      t_.join();      
      
      ChannelDestroy(chid_);
      ConnectDetach(coid_);
   }
      
   
   int chid_;
   int coid_;
   std::thread t_;
};


TEST_F(MsgSendPulseTest, basic)
{                      
   // now send pulse            
   int rc = MsgSendPulse(coid_, 0, 42, 4711);
   EXPECT_EQ(0, rc);   
      
   rc = MsgSendPulse(coid_, 0, 42, 4712);
   EXPECT_EQ(0, rc);         
   
   rc = MsgSendPulse(4711, 0, 42, 4713);
   EXPECT_EQ(-1, rc);   
   EXPECT_EQ(errno, EBADF);
}   
