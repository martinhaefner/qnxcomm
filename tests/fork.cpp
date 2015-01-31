#include <unistd.h>
#include <qnxcomm.h>

//
// TODO
// The module does not get notified when a process is getting forked.
// So we must care about these events.
// See article at http://lwn.net/Articles/153187/
// For short: must attach to pnotify_events
//

int main()
{
   int chid = ChannelCreate(0);
   
   if (fork() == 0)
   {
      // child
   }
   else
   {
      // parent
   }
   
   return 0;
}
