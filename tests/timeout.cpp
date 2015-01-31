#include <gtest/gtest.h>
#include <thread>

#include "qnxcomm.h"


namespace {

int timespec_subtract(struct timespec *result, struct timespec *x, struct timespec *y)
{
   if (x->tv_nsec < y->tv_nsec) 
   {
      long nsec = (y->tv_nsec - x->tv_nsec) / 1000000000 + 1;
      y->tv_nsec -= 1000000000 * nsec;
      y->tv_sec += nsec;
   }
   
   if (x->tv_nsec - y->tv_nsec > 1000000000) 
   {
      long nsec = (x->tv_nsec - y->tv_nsec) / 1000000000;
      y->tv_nsec += 1000000000 * nsec;
      y->tv_sec -= nsec;
   }

   result->tv_sec = x->tv_sec - y->tv_sec;
   result->tv_nsec = x->tv_nsec - y->tv_nsec;

   return x->tv_sec < y->tv_sec;
}

}


struct stop_watch
{
   stop_watch()
   {
      resume();
   }
   
   /// @return time elapsed in ms since last resume or construction
   int stop()
   {
      struct timespec end;
      clock_gettime(CLOCK_MONOTONIC, &end);
      
      struct timespec diff;
      timespec_subtract(&diff, &end, &start_);
      
      return diff.tv_sec * 1000 + diff.tv_nsec / 1000000;
   }
   
   void resume()
   {
      clock_gettime(CLOCK_MONOTONIC, &start_);
   }
   
   struct timespec start_; 
};


TEST(qnxcomm, stop_watch) 
{
   stop_watch w;
   sleep(1);
   int diff_ms = w.stop();
   
   EXPECT_GT(diff_ms, 950);
   EXPECT_LT(diff_ms, 1050);
}


TEST(qnxcomm, timeout) 
{
   int chid = ChannelCreate(0);
   EXPECT_GT(chid, 0);
   
   int coid = ConnectAttach(0, 0, chid, 0, 0);
   EXPECT_GT(coid, 0);
    
   // now send message and wait for reply      
   char buf[80];   
   strcpy(buf, "Hallo Welt");
   
   uint64_t timeout = 1 * 1000*1000*1000ULL;
   int rc = TimerTimeout(CLOCK_MONOTONIC, 0, 0, &timeout, 0);
   EXPECT_EQ(0, rc);
   
   rc = MsgSend(coid, buf, strlen(buf) + 1, buf, sizeof(buf));
   EXPECT_EQ(-1, rc);
   EXPECT_EQ(ETIMEDOUT, errno);
   
   rc = TimerTimeout(CLOCK_MONOTONIC, 0, 0, &timeout, 0);
   EXPECT_EQ(0, rc);
   
   rc = MsgReceive(chid, buf, sizeof(buf), 0);
   EXPECT_EQ(-1, rc);
   EXPECT_EQ(ETIMEDOUT, errno);
   
   EXPECT_EQ(0, ChannelDestroy(chid));
   EXPECT_EQ(0, ConnectDetach(coid));  
}

