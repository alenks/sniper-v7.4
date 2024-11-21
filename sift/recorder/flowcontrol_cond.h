#ifndef COND_H
#define COND_H

#include "lock.h"

// Our condition variable interface is slightly different from
// pthreads in that the mutex associated with the condition variable
// is built into the condition variable itself.

class ConditionVariable
{
   public:
      ConditionVariable();
      ~ConditionVariable();

      // must acquire lock before entering wait. will own lock upon exit.
      void wait(Lock& _lock, uint64_t timeout_ns = 0);
      void signal();
      void broadcast();

   private:
      int m_futx;
      Lock m_lock;
      #ifdef TIME_LOCKS
      TotalTimer* _timer;
      #endif
};

// hack
//#include "flowcontrol_cond.cc"

#endif // COND_H
