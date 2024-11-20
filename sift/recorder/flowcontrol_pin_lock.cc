#include <iostream>
#include "flowcontrol_pin_lock.h"

PinLock::PinLock()
{
   PIN_InitLock(&_lock);
   _lock._owner = -1;
}

PinLock::~PinLock()
{
}

void PinLock::acquire()
{
   //PIN_GetLock(&_lock, 1);
   if (m_debug) {
       if (_iolock) {
           _iolock->acquire();
       }
       std::cerr << "[acquire:" << PIN_ThreadId() << "] Last held by " << _lock._owner << "\n" << std::flush;
       if (_iolock) {
           _iolock->release();
       }
   }
   PIN_GetLock(&_lock, PIN_ThreadId());
}

void PinLock::release()
{
   if (m_debug) {
       if (_iolock) {
           _iolock->acquire();
       }
       std::cerr << "[release:" << PIN_ThreadId() << "] Last held by " << _lock._owner << "\n" << std::flush;
       if (_iolock) {
           _iolock->release();
       }
   }
   if (_lock._owner == -1) {
       if (_iolock) {
           _iolock->acquire();
       }
       std::cerr << "[release:" << PIN_ThreadId() << "] ERROR " << _lock._owner << " Expected owner != -1\n" << std::flush;
       if (_iolock) {
           _iolock->release();
       }
   }
   // Debugging, is int32
   _lock._owner = -1;
   PIN_ReleaseLock(&_lock);
}

#if 1
LockImplementation* LockCreator_Default::create()
{
   return new PinLock();
}
LockImplementation* LockCreator_RwLock::create()
{
   return new PinLock();
}
#endif

LockImplementation* LockCreator_Spinlock::create()
{
    return new PinLock();
}
