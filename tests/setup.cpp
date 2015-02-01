#include <gtest/gtest.h>

#include "qnxcomm.h"


TEST(qnxcomm, setup) 
{
   int chid = ChannelCreate(0);
   EXPECT_GT(chid, 0);
   
   int coid = ConnectAttach(0, 0, chid, 0, 0);
   EXPECT_GT(coid, 0);
   
   int side_coid = ConnectAttach(0, ::getpid(), chid, _NTO_SIDE_CHANNEL, 0);
   EXPECT_GT(side_coid, _NTO_SIDE_CHANNEL);   
   
   EXPECT_EQ(0, ChannelDestroy(chid));
   EXPECT_EQ(0, ConnectDetach(coid));
   EXPECT_EQ(0, ConnectDetach(side_coid));   
   
   // try again...
   EXPECT_EQ(-1, ChannelDestroy(chid));
   EXPECT_EQ(EINVAL, errno);
   EXPECT_EQ(-1, ConnectDetach(coid));
   EXPECT_EQ(EINVAL, errno);
}


TEST(qnxcomm, setup_invalid) 
{
   EXPECT_EQ(-1, ConnectAttach(0, 0, 8899, 0, 0));
   EXPECT_EQ(ESRCH, errno);
   
   EXPECT_EQ(-1, ChannelDestroy(9988));
   EXPECT_EQ(EINVAL, errno);
   
   EXPECT_EQ(-1, ConnectDetach(9978));
   EXPECT_EQ(EINVAL, errno);
}
