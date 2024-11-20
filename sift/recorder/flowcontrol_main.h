#include "pin.H"
#undef PINPLAY
#include "lock.h"
#ifdef PINPLAY 
    #include "pinplay.H"
#endif
#include <assert.h>
#include <vector>

#include "cond.h"

#include <iostream>
#include <numeric>

#define FC_DEBUG 0

namespace flowcontrol {
//#define THREE0 000
#define DEFAULT_QUANTUM 1000000
#define MAX_NUM_THREADS_DEFAULT 128
//#define DEFAULT_MAXTHREADS MAX_NUM_THREADS_DEFAULT
#define DEFAULT_VERBOSE 0

class flowcontrol {

private:
#ifdef PINPLAY
  PINPLAY_ENGINE *pinplay_engine = NULL;
#endif
  std::vector<ConditionVariable*> m_core_cond;
  std::vector<bool> m_active;
  std::vector<bool> m_not_in_syscall;
  std::vector<bool> m_barrier_acquire_list;
  std::vector<uint64_t> m_flowcontrol_target;
  std::vector<uint64_t> m_insncount;
  std::vector<uint64_t> m_threads_to_finish;
  std::vector<uint64_t> m_threads_to_signal;
  uint64_t m_sync = 0;
  uint64_t m_acc_count = 0;
  int m_max_num_threads = 0;
  Lock lock;
  bool enable_flowcontrol = false;
#if FC_DEBUG >= 1
  Lock iolock;
#endif

public:

  // Call activate() to allow for initialization with static allocation
  flowcontrol() {}

  // Cleanup conditional variables
  ~flowcontrol() {
    for (int i = 0 ; i < m_max_num_threads ; i++) {
      if (m_core_cond[i]) {
        delete m_core_cond[i];
      }
    }
  }
  void enable_ff() {
    enable_flowcontrol = true;
//    enable_flowcontrol = false;return;
    
    for( int i = 0 ; i < m_max_num_threads ; i++  ) {
        if (m_active[i] ) {
            m_flowcontrol_target[i] = m_insncount[i] + m_sync;    
            
        }
    }
  }
  void disable_ff() {
    enable_flowcontrol = false;

  }
#ifdef PINPLAY
  static VOID handleSyncCallback(BOOL enter, THREADID threadid, THREADID remote_threadid, UINT64 remote_icount, VOID* v) {
    flowcontrol *fc = reinterpret_cast<flowcontrol*>(v);
    fc->syncCallback(enter, threadid, remote_threadid, remote_icount);
  }
#endif
  static VOID handleThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v) {
    flowcontrol *fc = reinterpret_cast<flowcontrol*>(v);
    fc->threadStart(threadid, ctxt, flags);
  }

  static VOID threadFinishHelper(VOID *arg)
  {
    flowcontrol *fc = reinterpret_cast<flowcontrol*>(arg);
    {
      ScopedLock sl(fc->lock);
      for (auto i : fc->m_threads_to_finish) {
        fc->m_active[i] = false;
        fc->m_insncount[i] = false;
         
      }
      fc->m_threads_to_finish.clear();
    }
  }

  static VOID handleThreadFinish(THREADID threadid, const CONTEXT *ctxt, INT32 flags, VOID *v) {
    flowcontrol *fc = reinterpret_cast<flowcontrol*>(v);
#if FC_DEBUG >= 1
    {
      ScopedLock sl(fc->iolock);
      std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] Called\n" << std::flush;
    }
#endif

    // might be under sync/pinplay paused
    //assert(fc->m_active[threadid]);

    {
      ScopedLock sl(fc->lock);
      fc->m_threads_to_finish.push_back(threadid);
    }
    PIN_SpawnInternalThread(flowcontrol::threadFinishHelper, v, 0, NULL);
  }

  static VOID handleTraceCallback(TRACE trace, VOID *v) {
    flowcontrol *fc = reinterpret_cast<flowcontrol*>(v);
    fc->traceCallback(trace);
  }
  template<bool force_flowcontrol>
  static VOID countInsns_template(THREADID threadid, INT32 count, flowcontrol *fc) ;

  static VOID countInsns(THREADID threadid, INT32 count, flowcontrol *fc) ;
  static VOID sync(THREADID threadid,  flowcontrol *fc) ;

  // Hold lock
  bool isBarrierReached(std::string msg = "", THREADID threadid = -1) {
#if FC_DEBUG >= 3
    {
      ScopedLock sl(iolock);
      std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] msg: " << msg << "\n" << std::flush;
    }
#endif
    // Have all running threads reached the barrier yet?
    bool any_active = false;
    for (int i = 0 ; i < m_max_num_threads ; i++) {
      if (m_active[i] && (m_not_in_syscall[i]) && (!m_barrier_acquire_list[i])) {
        // Not yet. Wait a bit.
#if FC_DEBUG >= 2
        {
          ScopedLock sl(iolock);
          std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] active: " << m_active[i] << " & !barrier: " << !m_barrier_acquire_list[i] << " threadid= " << i << " is still active" << "\n" << std::flush;
        }
#endif
        any_active = true;
        break;
      }
    }
    if (any_active) {
      return false;
    }
    // Excellent. All valid entries are there now
    return true;
  }

  // Hold lock
  void doRelease(THREADID threadid = -1) {
#if FC_DEBUG >= 3
    {
      ScopedLock sl(iolock);
      std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] icount=" << (m_insncount[threadid]) << "\n" << std::flush;
    }
#endif
    std::vector<bool> bal(m_barrier_acquire_list);
    /*
    for(int i = 0 ; i < m_max_num_threads ; i++) {
      if (bal[i]) {
#if FC_DEBUG >= 3
        {
          ScopedLock sl(iolock);
          std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] clearing bal " << i << "\n" << std::flush;
        }
#endif
        m_barrier_acquire_list[i] = false;
      }
    }*/
    for(int i = 0 ; i < m_max_num_threads ; i++) {
      if (bal[i]) {
        if (static_cast<int>(threadid) != i) {
#if FC_DEBUG >= 3
          {
            ScopedLock sl(iolock);
            std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] signaling thread " << i << "\n" << std::flush;
          }
#endif
          m_threads_to_signal.push_back(i);
          //m_core_cond[i]->signal();
        }
      }
    }
  }

  // Do not hold the lock
  void finishRelease() {
    std::vector<uint64_t> m_lcl_tts(m_threads_to_signal);
    m_threads_to_signal.clear();

    for (auto tts : m_lcl_tts) {
      m_core_cond[tts]->signal();
    }
  }
#ifdef PINPLAY
  void activate(PINPLAY_ENGINE *_pinplay_engine = NULL) ;
#else

  void activate( ) ;
#endif
  void syncCallback(BOOL enter, THREADID threadid, THREADID remote_threadid, UINT64 remote_icount) {
#if FC_DEBUG >= 1
    {
      ScopedLock sl(iolock);
      std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] enter=" << enter << " remote_threadid=" << remote_threadid << " remote_icount=" << remote_icount << "\n" << std::flush;
    }
#endif

    {
      ScopedLock sl(lock);

      // If there is already all threads (except this one) in the barrier, be sure to wake everyone up
      if (enter) {
	// enter
	assert(m_active[threadid]);
	m_active[threadid] = false; 

	// We are no longer active. Check for barriers before we leave
	if (isBarrierReached("syncCallback", threadid)) {
	  doRelease(threadid);
	} else {
    
    }

      } else {
	// exit
	assert(!m_active[threadid]);
	m_active[threadid] = true;
      }
    }

    // Signal threads if needed. Can't do this in doRelease as we need to be able to acquire the lock
    finishRelease();

  }

  void threadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags) {
#if FC_DEBUG >= 1
    {
      ScopedLock sl(iolock);
      std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] Called\n" << std::flush;
    }
#endif

    assert(!m_active[threadid]);

    m_flowcontrol_target[threadid] = m_sync;
    m_active[threadid] = true;
    m_not_in_syscall[threadid] = true;

  }

  static VOID syscallEntryCallbackHelper(THREADID threadid, VOID *v)
  {
      flowcontrol *fc = reinterpret_cast<flowcontrol*>(v);
#if FC_DEBUG >= 1
      {
        ScopedLock sl(fc->iolock);
        std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] Called\n" << std::flush;
      }
#endif

      {
        ScopedLock sl(fc->lock);
        // Potential issue with sync and syscalls together. We might need to split the active variable
        assert(fc->m_not_in_syscall[threadid]);
        /*if (!fc->m_not_in_syscall[threadid]) {
          {
            ScopedLock sl(fc->iolock);
            std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] WARNING: Value of m_not_in_syscall is false already.\n" << std::flush;
          }
        }*/
        fc->m_not_in_syscall[threadid] = false; 

        // We are no longer active. Check for barriers before we leave
        if (fc->isBarrierReached("syncCallback", threadid)) {
          fc->doRelease(threadid);
        }
      }

      // Signal threads if needed. Can't do this in doRelease as we need to be able to acquire the lock
      fc->finishRelease();

  }

  static VOID syscallEntryCallback(THREADID threadid, CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard, VOID *v)
  {
    syscallEntryCallbackHelper(threadid, v);
  }

  static VOID syscallExitCallbackHelper(THREADID threadid, VOID *v)
  {
      flowcontrol *fc = reinterpret_cast<flowcontrol*>(v);
      assert(!fc->m_not_in_syscall[threadid]);
      /*if(fc->m_not_in_syscall[threadid]) {
        {
          ScopedLock sl(fc->iolock);
          std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] WARNING: Value of m_not_in_syscall is true already.\n" << std::flush;
        }
      }*/
      {
        ScopedLock sl(fc->lock);
        fc->m_not_in_syscall[threadid] = true;
      }
#if FC_DEBUG >= 1
      {
        ScopedLock sl(fc->iolock);
        std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] Called\n" << std::flush;
      }
#endif
  }

  static VOID syscallExitCallback(THREADID threadid, CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard, VOID *v)
  {
    syscallExitCallbackHelper(threadid, v);
  }

  void traceCallback(TRACE trace) {
    BBL bbl_head = TRACE_BblHead(trace);
    for (BBL bbl = bbl_head; BBL_Valid(bbl); bbl = BBL_Next(bbl)) {

      bool syscalls_in_bb = false;
      // Commenting out syscall check as we encounter runtime issues with it
      // Are there any syscalls here?
      
      for(INS ins = BBL_InsHead(bbl); INS_Valid(ins) ; ins = INS_Next(ins))
      {
        if (INS_IsSyscall(ins))
        {
          syscalls_in_bb = true;
          break;
        }
      }

      // Setup callbacks
      if (syscalls_in_bb)
      {
          
        for(INS ins = BBL_InsHead(bbl); INS_Valid(ins) ; ins = INS_Next(ins))
        {
//          if (INS_IsSyscall(ins) && INS_HasFallThrough(ins) )
          if (INS_IsSyscall(ins) && INS_IsValidForIpointAfter(ins) )
          {
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(flowcontrol::syscallEntryCallbackHelper), IARG_THREAD_ID, IARG_ADDRINT, this, IARG_END);
             INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)flowcontrol::countInsns, IARG_THREAD_ID, IARG_UINT32, (UINT32) 1, IARG_ADDRINT, this, IARG_END);
            INS_InsertPredicatedCall(ins, IPOINT_AFTER, AFUNPTR(flowcontrol::syscallExitCallbackHelper), IARG_THREAD_ID, IARG_ADDRINT, this, IARG_END);
          }
          else
          {
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)flowcontrol::countInsns, IARG_THREAD_ID, IARG_UINT32, (UINT32) 1, IARG_ADDRINT, this, IARG_END);
          }
        }
      }
      else
      {
        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)flowcontrol::countInsns, IARG_THREAD_ID, IARG_UINT32, BBL_NumIns(bbl), IARG_ADDRINT, this, IARG_END);
      }
    }
  }

};

}
