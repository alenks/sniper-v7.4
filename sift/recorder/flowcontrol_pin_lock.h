#pragma once

#include "flowcontrol_lock.h"
#include "pin.H"

class PinLock : public LockImplementation
{
public:
   PinLock();
   ~PinLock();

   void acquire();
   void release();

   void debug(bool _enable, void *v) {
      m_debug = _enable;
      _iolock = reinterpret_cast<Lock*>(v);
   }

private:
   PIN_LOCK _lock;
   Lock *_iolock = NULL;
   bool m_debug = false;
};

// hack
//#include "flowcontrol_pin_lock.cc"
