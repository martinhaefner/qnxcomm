#include <thread>
#include <sys/syscall.h>
#include <string.h>
#include <unistd.h>

#include "qnxcomm.h"


void thread_runner(int chid)
{
   printf("pid=%d, tid=%ld\n", getpid(), syscall(SYS_gettid));
   char buf[1024];
   _msg_info info;
   
   uint64_t timeout = 2 * 1000*1000*1000; /*== 2 seconds */
   TimerTimeout(CLOCK_MONOTONIC, 0, 0, &timeout, 0);
   sleep(1);
   int rcvid = MsgReceive(chid, buf, sizeof(buf), &info);
   printf("rcvid=%d, errno=%d, text='%s'\n", rcvid, errno, buf);
   
   strcpy(buf, "Super Show!");
   int rc = MsgReply(rcvid, 0, buf, strlen(buf)+1);
   printf("rc=%d\n", rc); 
   printf("from pid=%d\n", info.pid);
   
   rcvid = MsgReceive(chid, buf, sizeof(buf), &info);
   printf("rcvid=%d -> pulse?!\n", rcvid);      
   printf("from pid=%d\n", info.pid);
}


int main(int argc, const char** argv)
{   
   fprintf(stderr, "Hallo\n");
   
   char buf[1024];
   
   if (argc > 1)
   {
      fprintf(stderr, "Hallo 1\n");
      _msg_info info;
      
      if (!strcmp(argv[1], "--client"))
      {
         int pid = atoi(argv[2]);         
         int chid = atoi(argv[3]);         
         int coid = ConnectAttach(0, pid, chid, 0, 0);
         printf("coid=%d\n", coid);
         sleep(10);
         int rc = MsgSend(coid, buf, strlen(buf)+1, buf, 1024);
         printf("rc=%d\n", rc);
      }
      else if (!strcmp(argv[1], "--server"))
      {
         fprintf(stderr, "Hallo 2\n");
         int chid = ChannelCreate(0);   
         printf("pid=%d, chid=%d\n", getpid(), chid);
         
         int rcvid = MsgReceive(chid, buf, sizeof(buf), &info);
         printf("rcvid=%d\n", rcvid);         
      }      
      else if (!strcmp(argv[1], "--crash-server"))
      {
         int chid = ChannelCreate(0);   
         printf("pid=%d, chid=%d\n", getpid(), chid);
         
         int rcvid = MsgReceive(chid, buf, sizeof(buf), &info);         
         printf("rcvid=%d\n", rcvid);
         
         abort();
      }
   }
   else
   {   
      int chid = ChannelCreate(0);   
      printf("chid=%d\n", chid);
      
      int coid = ConnectAttach(0, 0, chid, 0, 0);
      printf("coid=%d\n", coid);
      
      std::thread t(thread_runner, chid);
            
      strcpy(buf, "Hallo Welt");
     
#if 0
      struct iovec in[2] = { { buf, 5}, { buf + 5, strlen(buf)-4 } };      
      struct iovec out[2] = { { buf, 7}, { buf + 9, 1024 - 9 } };      
      int rc = MsgSendv(coid, in, 2, out, 2);   
      buf[7] = '\0';   
      printf("rc=%d, text='%s%s'\n", rc, buf, buf+9);
#else
      uint64_t timeout = 5ULL*1000*1000*1000;
      //sleep(10);
      
      int ret = TimerTimeout(CLOCK_MONOTONIC, 0, 0, &timeout, 0);
      printf("ret=%d\n", ret);
      int rc = MsgSend(coid, buf, strlen(buf)+1, buf, 1024);
      printf("rc=%d, errno=%d, text='%s'\n", rc, errno, buf);
#endif      
      
      rc = MsgSendPulse(coid, 0, 42, 7);
      printf("send pulse rc=%d\n", rc);
      
      t.join();
      
      rc = ConnectDetach(coid);
      printf("detach rc=%d\n", rc);
      
      rc = ChannelDestroy(chid);
      printf("detach rc=%d\n", rc);      
   }      
   
   return 0;
}
