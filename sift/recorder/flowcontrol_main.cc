
/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs 
 *  and could serve as the starting point for developing your first PIN tool
 */
//#include "globals.h"
#include "pin.H"
#include <iostream>
#include <fstream>
#if defined(PINPLAY)
#include "pinplay.H"
#include "control_manager.H"
//static PINPLAY_ENGINE *pinplay_engine;
static PINPLAY_ENGINE pp_pinplay_engine;
#endif
# include "flowcontrol_main.h"


#include "globals.h"
static flowcontrol::flowcontrol pp_flowcontrol;
namespace flowcontrol {
#define str(s) xstr(s)
#define xstr(s) #s

KNOB<UINT64>knobFlowcontrolQuantum(KNOB_MODE_WRITEONCE,
                       "pintool",
                       "flowcontrol:quantum",
                       str(DEFAULT_QUANTUM),
                       "Flow control quantum (instructions, default " str(DEFAULT_QUANTUM) ").");

KNOB<UINT64>knobFlowcontrolMaxThreads(KNOB_MODE_WRITEONCE,
                       "pintool",
                       "flowcontrol:maxthreads",
                       str( MAX_NUM_THREADS_DEFAULT),
                       "Flow control maximum number of threads (default " str(MAX_NUM_THREADS_DEFAULT) ").");

KNOB<BOOL>knobFlowcontrolVerbose(KNOB_MODE_WRITEONCE,
                      "pintool",
                      "flowcontrol:verbose",
                      str(DEFAULT_VERBOSE),
                      "Flow control maximum number of threads (default " str(DEFAULT_VERBOSE) ").");
  template<bool force_flowcontrol>
  VOID flowcontrol::countInsns_template(THREADID threadid, INT32 count, flowcontrol *fc) {
    if( fc->enable_flowcontrol || force_flowcontrol ) {
    
    fc->m_insncount[threadid] += count;

    if ( force_flowcontrol || fc->m_insncount[threadid] >= fc->m_flowcontrol_target[threadid]) {
      fc->lock.acquire();
      fc->m_barrier_acquire_list[threadid] = true;
      // Synchronize threads
      if (!fc->isBarrierReached("countInsns/Sync", threadid)) {
        fc->m_core_cond[threadid]->wait(fc->lock,100000);
        //fc->m_core_cond[threadid]->wait(fc->lock);
        fc->m_barrier_acquire_list[threadid] = false;
      } else {
        fc->doRelease(threadid);

        fc->m_barrier_acquire_list[threadid] = false;
      }
      fc->lock.release();
      // Everyone reached the barrier. Let's progress to the next target
      fc->m_flowcontrol_target[threadid] += fc->m_sync; 
    }

    // Signal threads if needed. Can't do this in doRelease as we need to be able to acquire the lock
    fc->finishRelease();
    }
  }
  VOID flowcontrol::countInsns( THREADID threadid, INT32 count, flowcontrol *fc ) 
  {
      countInsns_template<false>(threadid,count,fc);
  }
  VOID flowcontrol::sync( THREADID threadid, flowcontrol *fc ) 
  {
      if(KnobFlowControlLevel.Value() >= 1 ) {
          countInsns_template<true>(threadid, 0,fc);
      }
  }

#ifdef PINPLAY
  void flowcontrol::activate(PINPLAY_ENGINE *_pinplay_engine ) {

    (void)pinplay_engine;
#else
  void flowcontrol::activate( ) {

#endif
    m_sync = knobFlowcontrolQuantum.Value();
    // Are we disabled?
    if (m_sync == 0) {
      return;
    }
    enable_flowcontrol = false;
#if FC_DEBUG >= 1
//    lock.debug(true, &iolock);
#endif
    if(KnobFlowControlLevel.Value() == 0 ) return;
    // Save our maximum thread count
    m_max_num_threads = knobFlowcontrolMaxThreads.Value();
    printf("m_max_num_threads %d\n",m_max_num_threads);
	m_acc_count = m_sync * 100 * m_max_num_threads;

    // Initialize condition variables
    m_core_cond.resize(m_max_num_threads);
    for (int i = 0 ; i < m_max_num_threads ; i++) {
      m_core_cond[i] = new ConditionVariable();
    }

    // Initialize the flow control target
    m_flowcontrol_target.resize(m_max_num_threads, 0);

    // Initialize the instruction counts
    m_insncount.resize(m_max_num_threads, 0);

    // Initialize the flow control target
    m_active.resize(m_max_num_threads, 0);
    m_not_in_syscall.resize(m_max_num_threads, true);

    m_barrier_acquire_list.resize(m_max_num_threads, false);

#ifdef PINPLAY
    // Save the pinplay engine
    pinplay_engine = _pinplay_engine;
    if (pinplay_engine != NULL) {
        std::cerr << "pinplay_engine\n";
      pinplay_engine->RegisterSyncCallback(flowcontrol::handleSyncCallback, this);
    } else {
        std::cerr << "pinplay_engine unset\n";
    }
#endif

    PIN_AddThreadStartFunction(flowcontrol::handleThreadStart, this);
    PIN_AddThreadFiniFunction(flowcontrol::handleThreadFinish, this);
    
    if( KnobFlowControlLevel.Value() >= 2 ) {

        std::cerr << "[FLOWCONTROL] Enable \n" ;
        TRACE_AddInstrumentFunction(flowcontrol::handleTraceCallback, this);
    } else {
    
        std::cerr << "[FLOWCONTROL] Disable \n" ;
    }

    // Instead of using the syscall entry / exit functions, let's do this manually by looking for the syscall instruction below
    //PIN_AddSyscallEntryFunction(syscallEntryCallback, this);
    //PIN_AddSyscallExitFunction(syscallExitCallback, this);

    if (knobFlowcontrolVerbose.Value()) {
      std::cerr << "[FLOWCONTROL] Enabled. Sync quantum: " << m_sync << " Max threads: " << m_max_num_threads << " Pinplay Engine: " ;
#ifdef PINPLAY 
      std::cerr << (pinplay_engine == NULL ? "Disabled" : "Enabled") ;
#endif
      std::cerr << "\n";
    }

  }


}
#ifdef PINPLAY
using namespace CONTROLLER;
#endif
using std::cerr;
using std::string;
using std::endl;

/* ================================================================== */
// Global variables 
/* ================================================================== */

UINT64 insCount = 0;        //number of dynamically executed instructions
UINT64 bblCount = 0;        //number of dynamically executed basic blocks
UINT64 threadCount = 0;     //total number of threads, including main thread

std::ostream * out = &cerr;
#undef PINPLAY
#ifdef PINPLAY
CONTROL_MANAGER * control_manager = NULL;
static CONTROLLER::CONTROL_MANAGER control("pinplay:");
CONTROL_ARGS args("pinplay:","pintool:control:pinplay");

#endif

#if defined(PINPLAY)
#define KNOB_LOG_NAME  "log"
#define KNOB_REPLAY_NAME "replay"
#define KNOB_FAMILY "pintool:pinplay-driver"

KNOB_COMMENT pinplay_driver_knob_family2(KNOB_FAMILY, "PinPlay Driver Knobs");

KNOB<BOOL>KnobPinPlayReplayer(KNOB_MODE_WRITEONCE, KNOB_FAMILY,
                       KNOB_REPLAY_NAME, "0", "Replay a pinball");
KNOB<BOOL>KnobPinPlayLogger(KNOB_MODE_WRITEONCE,  KNOB_FAMILY,
                     KNOB_LOG_NAME, "0", "Create a pinball");

KNOB<string> KnobOutputFile2(KNOB_MODE_WRITEONCE,  "pintool",
    "o", "", "specify file name for MyPinTool output");

KNOB<BOOL>   KnobCount(KNOB_MODE_WRITEONCE,  "pintool",
    "count", "1", "count instructions, basic blocks and threads in the application");

#endif

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr << "This tool prints out the number of dynamically executed " << endl <<
            "instructions, basic blocks and threads in the application." << endl << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

/*!
 * Increase counter of the executed basic blocks and instructions.
 * This function is called for every basic block when it is about to be executed.
 * @param[in]   numInstInBbl    number of instructions in the basic block
 * @note use atomic operations for multi-threaded applications
 */
VOID CountBbl(UINT32 numInstInBbl)
{
    bblCount++;
    insCount += numInstInBbl;
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

/*!
 * Insert call to the CountBbl() analysis routine before every basic block 
 * of the trace.
 * This function is called every time a new trace is encountered.
 * @param[in]   trace    trace to be instrumented
 * @param[in]   v        value specified by the tool in the TRACE_AddInstrumentFunction
 *                       function call
 */
VOID Trace(TRACE trace, VOID *v)
{
    // Visit every basic block in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        // Insert a call to CountBbl() before every basic bloc, passing the number of instructions
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)CountBbl, IARG_UINT32, BBL_NumIns(bbl), IARG_END);
    }
}

/*!
 * Increase counter of threads in the application.
 * This function is called for every thread created by the application when it is
 * about to start running (including the root thread).
 * @param[in]   threadIndex     ID assigned by PIN to the new thread
 * @param[in]   ctxt            initial register state for the new thread
 * @param[in]   flags           thread creation flags (OS specific)
 * @param[in]   v               value specified by the tool in the 
 *                              PIN_AddThreadStartFunction function call
 */
VOID ThreadStart(THREADID threadIndex, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    threadCount++;
}

/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the 
 *                              PIN_AddFiniFunction function call
 */
/*VOID Fini(INT32 code, VOID *v)
{
}
*/
/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments, 
 *                              including pin -t <toolname> -- ...
 */
/*
int main(int argc, char *argv[])
{
    (void)pinplay_engine;
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid 
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }

    PIN_InitSymbols();

    pinplay_engine = &pp_pinplay_engine;
    pinplay_engine->Activate(argc, argv, KnobPinPlayLogger, KnobPinPlayReplayer);
    control.Activate();
    control_manager = &control;

    string fileName = KnobOutputFile.Value();

    if (!fileName.empty()) { out = new std::ofstream(fileName.c_str());}

    if (KnobCount)
    {
        // Register function to be called to instrument traces
        TRACE_AddInstrumentFunction(Trace, 0);

        // Register function to be called for every thread before it starts running
        PIN_AddThreadStartFunction(ThreadStart, 0);

        // Register function to be called when the application exits
        PIN_AddFiniFunction(Fini, 0);
    }
    
    pp_flowcontrol.activate(pinplay_engine);
    // Start the program, never returns
    PIN_StartProgram();
    return 0;
}
*/
/* ===================================================================== */
/* eof */
/* ===================================================================== */
