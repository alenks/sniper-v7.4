#ifndef __GLOBALS_H 
#define __GLOBALS_H

#include "sift/sift_format.h"

#include "pin.H"
#ifdef PINPLAY_SUPPORTED
# include "pinplay.H"
#endif

#include <unordered_map>
#include <deque>
//class InsCount;
class HooksManager;
class PinToolWarmup;
//class ToolMtng;
namespace flowcontrol{
    class flowcontrol;
}
//#define DEBUG_OUTPUT 1
#define DEBUG_OUTPUT 0

#define LINE_SIZE_BYTES 64
#define MAX_NUM_SYSCALLS 4096
#define MAX_NUM_THREADS_DEFAULT 128

extern KNOB<UINT32> KnobSmartsFFWRegionNum;
extern KNOB<UINT64> KnobMinimumSampledRegionSize;
extern KNOB<UINT64> KnobSampledRegionSize;
extern KNOB<double> KnobClusterThreshold;
extern KNOB<BOOL> KnobRecordBarrierInFull;
extern uint64_t current_barrier;
extern KNOB<UINT64> KnobAtomicType;
extern KNOB<BOOL> KnobDVFSEnable;
extern KNOB<std::string> KnobDVFSDir;
extern KNOB<std::string> KnobOutputFile;
extern KNOB<UINT64> KnobBlocksize;
extern KNOB<UINT64> KnobUseROI;
extern KNOB<UINT64> KnobMPIImplicitROI;
extern KNOB<UINT64> KnobFastForwardTarget;
extern KNOB<UINT64> KnobDetailedTarget;
extern KNOB<UINT64> KnobUseResponseFiles;
extern KNOB<UINT64> KnobEmulateSyscalls;
extern KNOB<BOOL>   KnobSendPhysicalAddresses;
extern KNOB<UINT64> KnobFlowControl;
extern KNOB<UINT64> KnobFlowControlFF;
extern KNOB<INT64> KnobSiftAppId;
extern KNOB<BOOL> KnobRoutineTracing;
extern KNOB<BOOL> KnobRoutineTracingOutsideDetailed;
extern KNOB<BOOL> KnobDebug;
extern KNOB<BOOL> KnobVerbose;
extern KNOB<UINT64> KnobStopAddress;
extern KNOB<UINT64> KnobMaxThreads;
extern KNOB<UINT64> KnobExtraePreLoaded;
extern KNOB<INT64> KnobMtngEnable;
extern KNOB<std::string> KnobMtngDir;
extern KNOB<std::string> KnobMtngClusterType;
extern KNOB<BOOL> KnobMTREnable;
extern KNOB<UINT32> KnobFlowControlLevel;
extern KNOB<std::string>  KnobArch;
# define KNOB_REPLAY_NAME "replay"
# define KNOB_FAMILY "pintool:sift-recorder"
extern KNOB_COMMENT pinplay_driver_knob_family;
extern KNOB<BOOL>KnobReplayer;
#ifdef PINPLAY_SUPPORTED
extern PINPLAY_ENGINE pinplay_engine;
#endif /* PINPLAY_SUPPORTED */

extern INT32 app_id;
extern INT32 num_threads;
extern UINT32 max_num_threads;
extern UINT64 blocksize;
extern UINT64 fast_forward_target;
extern UINT64 detailed_target;
extern PIN_LOCK access_memory_lock;
extern PIN_LOCK new_threadid_lock;
extern std::deque<ADDRINT> tidptrs;
extern INT32 child_app_id;
extern BOOL in_roi;
extern BOOL allow_in_roi;
extern BOOL any_thread_in_detail;
extern Sift::Mode current_mode;
extern const bool verbose;
extern std::unordered_map<ADDRINT, bool> routines;

double getSystemTime();

extern double time_stamp1;
extern double time_stamp_begin;
extern double time_stamp_end;


struct extrae_image_t {
  ADDRINT top_addr;
  ADDRINT bottom_addr;
  BOOL linked;
  BOOL got_init;
  BOOL got_fini;
};
extern extrae_image_t extrae_image;

enum class AtomicType {
    BarrierPoint = 0,
    AtomicRegion = 1,
    HybridMode = 2
};
#if defined(TARGET_IA32)
   typedef uint32_t syscall_args_t[6];
#elif defined(TARGET_INTEL64)
   typedef uint64_t syscall_args_t[6];
#endif

extern bool mtr_enabled;
extern HooksManager *getHooksManager();
extern PinToolWarmup *getWarmupTool();
//extern ToolMtng *getMtngTool();
#define NUM_BBV 16
extern flowcontrol::flowcontrol*  getFlowControl() ;
extern std::vector<uint64_t> global_m_bbv_counts;
extern std::vector<uint64_t> global_m_bbv_counters;
void init_global_bbv();
uint64_t get_bbv_thread_dim( uint32_t tid, uint32_t dim);
uint64_t get_bbv_thread_counter( uint32_t tid);
//extern InsCount * getInsCount() ;
#endif // __GLOBALS_H
