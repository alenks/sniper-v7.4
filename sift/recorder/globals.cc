#include "globals.h"
#include "hooks_manager.h"
#include "tool_warmup.h"
//#include "tool_mtng.h"
#include "flowcontrol_main.h"
//#include "inst_count.h"
uint64_t current_barrier = 0;
KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "trace", "output");
KNOB<UINT64> KnobBlocksize(KNOB_MODE_WRITEONCE, "pintool", "b", "0", "blocksize");
KNOB<UINT64> KnobUseROI(KNOB_MODE_WRITEONCE, "pintool", "roi", "0", "use ROI markers");
KNOB<UINT64> KnobMPIImplicitROI(KNOB_MODE_WRITEONCE, "pintool", "roi-mpi", "0", "Implicit ROI between MPI_Init and MPI_Finalize");
KNOB<UINT64> KnobFastForwardTarget(KNOB_MODE_WRITEONCE, "pintool", "f", "0", "instructions to fast forward");
KNOB<UINT64> KnobDetailedTarget(KNOB_MODE_WRITEONCE, "pintool", "d", "0", "instructions to trace in detail (default = all)");
KNOB<UINT64> KnobUseResponseFiles(KNOB_MODE_WRITEONCE, "pintool", "r", "0", "use response files (required for multithreaded applications or when emulating syscalls, default = 0)");
KNOB<UINT64> KnobEmulateSyscalls(KNOB_MODE_WRITEONCE, "pintool", "e", "0", "emulate syscalls (required for multithreaded applications, default = 0)");
KNOB<BOOL>   KnobSendPhysicalAddresses(KNOB_MODE_WRITEONCE, "pintool", "pa", "0", "send logical to physical address mapping");
KNOB<UINT64> KnobFlowControl(KNOB_MODE_WRITEONCE, "pintool", "flow", "1000", "number of instructions to send before syncing up");
KNOB<UINT64> KnobFlowControlFF(KNOB_MODE_WRITEONCE, "pintool", "flowff", "100000", "number of instructions to batch up before sending instruction counts in fast-forward mode");
//KNOB<UINT64> KnobFlowControlFF(KNOB_MODE_WRITEONCE, "pintool", "flowff", "18446744073709551615", "number of instructions to batch up before sending instruction counts in fast-forward mode");

KNOB<UINT64> KnobAtomicType( KNOB_MODE_WRITEONCE, "pintool", "atomic-type", "0", "Atomic type, 0 is barrier, 1 is atomic region, 2 is hybrid mode, 3 is pure fast-forward mode; 4 only enables analytical mode; 5 enables mtr and analytical mode");

KNOB<BOOL>  KnobMTREnable( KNOB_MODE_WRITEONCE, "pintool", "mtr", "1", "0 is disable ,1 is enable");

KNOB<UINT32>  KnobFlowControlLevel( KNOB_MODE_WRITEONCE, "pintool", "flowcontrol", "1", "0 is disable ,1 is only enable flowcontrol between each atomic region, 2 is enable flowcontrol each fixed number of instruction");
KNOB<BOOL>  KnobDVFSEnable( KNOB_MODE_WRITEONCE, "pintool", "dvfs", "0", "0 is disable ,1 is enable");
KNOB<std::string>  KnobDVFSDir( KNOB_MODE_WRITEONCE, "pintool", "dvfs-dir", "0", "dvfs configure file");


KNOB<std::string>  KnobArch( KNOB_MODE_WRITEONCE, "pintool", "arch", "gainestown", "micro architecture");

KNOB<INT64> KnobSiftAppId(KNOB_MODE_WRITEONCE, "pintool", "s", "0", "sift app id (default = 0)");
KNOB<BOOL> KnobRoutineTracing(KNOB_MODE_WRITEONCE, "pintool", "rtntrace", "0", "routine tracing");
KNOB<BOOL> KnobRoutineTracingOutsideDetailed(KNOB_MODE_WRITEONCE, "pintool", "rtntrace_outsidedetail", "0", "routine tracing");
KNOB<BOOL> KnobDebug(KNOB_MODE_WRITEONCE, "pintool", "debug", "1", "start debugger on internal exception");

KNOB<BOOL> KnobRecordBarrierInFull(KNOB_MODE_WRITEONCE, "pintool", "record-full-mode", "0", "whether enable marker at each begin and end of omp region in full mode");

KNOB<BOOL> KnobVerbose(KNOB_MODE_WRITEONCE, "pintool", "verbose", "0", "verbose output");
KNOB<UINT64> KnobStopAddress(KNOB_MODE_WRITEONCE, "pintool", "stop", "0", "stop address (0 = disabled)");
KNOB<UINT64> KnobMaxThreads(KNOB_MODE_WRITEONCE, "pintool", "maxthreads", "0", "maximum number of threads (0 = default)");

KNOB_COMMENT pinplay_driver_knob_family(KNOB_FAMILY, "PinPlay SIFT Recorder Knobs");
KNOB<BOOL>KnobReplayer(KNOB_MODE_WRITEONCE, KNOB_FAMILY,
                       KNOB_REPLAY_NAME, "0", "Replay a pinball");

KNOB<UINT64>KnobExtraePreLoaded(KNOB_MODE_WRITEONCE, "pintool", "extrae", "0", "Extrae preloaded");

// BBV KNOBS
KNOB<INT64> KnobMtngEnable(KNOB_MODE_WRITEONCE, "pintool", "mtng","0", "enable mtng simulation; 2 hybrid mode; 1004 only enables fastforward; 1005 only enables fastforward and analytical data; 1006 enables fastforward, analytical data and mrt");
KNOB<BOOL> KnobFastForwardMode(KNOB_MODE_WRITEONCE, "pintool", "fast-forward","0", "enable only fast forward simulation");
KNOB<double> KnobClusterThreshold(KNOB_MODE_WRITEONCE, "pintool", "cluster-threshold","0.05", "cluster threshold");

KNOB<uint64_t> KnobSampledRegionSize(KNOB_MODE_WRITEONCE, "pintool", "region-size","50000", "Sampled region size (K)");
KNOB<uint64_t> KnobMinimumSampledRegionSize(KNOB_MODE_WRITEONCE, "pintool", "minimum-region-size","20000", "Sampled region size (K)");
KNOB<uint32_t> KnobSmartsFFWRegionNum(KNOB_MODE_WRITEONCE, "pintool", "smarts-ffw-num","10", "fast forward region num");

KNOB<std::string> KnobMtngDir(KNOB_MODE_WRITEONCE, "pintool", "mtng-dir", ".", "mtng data directory containing outputs from BarrierPoint and Simpoint");
KNOB<std::string> KnobMtngClusterType(KNOB_MODE_WRITEONCE, "pintool", "cluster-type", "bbv", "mtng cluster type such as bbv, ldv or sv");

#ifdef PINPLAY_SUPPORTED
PINPLAY_ENGINE pinplay_engine;
#endif /* PINPLAY_SUPPORTED */
bool mtr_enabled;
INT32 app_id;
INT32 num_threads = 0;
UINT32 max_num_threads = MAX_NUM_THREADS_DEFAULT;
UINT64 blocksize;
UINT64 fast_forward_target = 0;
UINT64 detailed_target = 0;
PIN_LOCK access_memory_lock;
PIN_LOCK new_threadid_lock;
std::deque<ADDRINT> tidptrs;
INT32 child_app_id = -1;
BOOL in_roi = false;
BOOL allow_in_roi = false;
BOOL any_thread_in_detail = false;
Sift::Mode current_mode = Sift::ModeIcount;
std::unordered_map<ADDRINT, bool> routines;

extrae_image_t extrae_image;

HooksManager hooks_manager;
HooksManager* getHooksManager() {return &hooks_manager;}

PinToolWarmup warmup_tool;
PinToolWarmup* getWarmupTool() {return &warmup_tool;}

namespace flowcontrol{
flowcontrol flowcontrol_tool;
};

flowcontrol::flowcontrol*  getFlowControl() {
return &flowcontrol::flowcontrol_tool;
}

std::vector<uint64_t> global_m_bbv_counts;
std::vector<uint64_t> global_m_bbv_counters;
void init_global_bbv() {
    global_m_bbv_counts.resize( MAX_NUM_THREADS_DEFAULT * NUM_BBV, 0 );
    global_m_bbv_counters.resize( MAX_NUM_THREADS_DEFAULT, 0 );
}
uint64_t get_bbv_thread_dim( uint32_t tid, uint32_t dim ) {
    return global_m_bbv_counts[ tid * NUM_BBV + dim ];
}
uint64_t get_bbv_thread_counter( uint32_t tid ) {
    return global_m_bbv_counters[ tid  ];
}

double getSystemTime()
    {
        struct timeval timer;
        gettimeofday(&timer, 0);
        return ((double)(timer.tv_sec) + (double)(timer.tv_usec)*1.0e-6);
    }
 double time_stamp1;
 double time_stamp_begin;
 double time_stamp_end;

//ToolMtng mtng_tool;
//ToolMtng* getMtngTool() {return &mtng_tool;}
//InsCount ins_count_tool;
//InsCount * getInsCount() { return &ins_count_tool;}
