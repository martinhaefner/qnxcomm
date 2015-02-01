#include <cassert>
#include <cstring>
#include <fstream>
#include <unistd.h>

#include "qnxcomm.h"


int main(int argc, char** argv)
{
   assert(argc == 2);
   int chid = ChannelCreate(0);
   assert(chid > 0);
   
   std::ofstream f("/tmp/gagaga");
   assert(f);
   f << getpid() << " " << chid << std::endl;
   
   char buf[32];
   int rcvid = MsgReceive(chid, buf, sizeof(buf), 0);
   assert(rcvid > 0);
   
   if (!strcmp(argv[1], "--abort"))
      abort();
   
   if (!strcmp(argv[1], "--exit"))
      return 0;
   
   if (!strcmp(argv[1], "--channeldestroy"))
      ChannelDestroy(chid);
   
   return 0;
}
