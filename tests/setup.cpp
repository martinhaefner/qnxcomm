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
}
