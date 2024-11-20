#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <syscall.h>
#include <vector>

#include <cstdio>
#include <cassert>
#include <unistd.h>
#include <sys/types.h>
#include <strings.h>
// stat is not supported in Pin 3.0
// #include <sys/stat.h>
#include <sys/syscall.h>
#include <string.h>
#include <pthread.h>

#include "pin.H"

#include "globals.h"
#include "recorder/bbv_count.h"
#include "threads.h"
#include "recorder_control.h"
#include "recorder_base.h"
#include "syscall_modeling.h"
#include "trace_rtn.h"
#include "emulation.h"
#include "sift_writer.h"
#include "sift_assert.h"
#include "pinboost_debug.h"
#include "hooks_init.h"
#include "tool_warmup.h"
//#include "tool_mtng.h"
#include "flowcontrol_main.h"
//#include "inst_count.h"
#include "mtng.h"
#include "intrabarrier_mtng.h"
#include "intrabarrier_analysis.h"
#include "hybridbarrier_mtng.h"
#include "emu.h"
#include "full_debug.h"
#include "dvfs.h"
#include "smarts.h"

void initFlowC()
{
    flowcontrol::flowcontrol* ins_count_tool = getFlowControl();
    ins_count_tool->activate();
}

void initMtr()
{
    mtr_enabled = true;
    PinToolWarmup* warmup_tool = getWarmupTool();
    warmup_tool->activate();
}

void initToolMtng( const AtomicType atomic_type )
{
//    ToolMtng* mtng_tool = getMtngTool();
    //mtng_tool->activate( atomic_type );
//    initFlowC();
    // test functions
    // testBbvUtil();
}

void testBbvUtil()
{
     // [debug] print dim-reduced(simpoint stype) bbvs 
     /* 
    const auto& bbvStandardprofile = Bbv_util::getBbvStandardProfile();
    for (const auto & i : bbvStandardprofile) // for each barrier
    {
            for (const auto & j : i) // for each thread
            {
                const auto & bbv_dim_reduced = Bbv_util::randomProjection(j);
                for (auto & k : bbv_dim_reduced) // print bbv
                {
                    std::cout << k << " ";
                }
                std::cout << "\n";
            }
            std::cout << "\n";
    }
    */

    /*
    const auto& bbvStandardprofile = Mtng::Bbv_util::getBbvStandardProfile();
    std::cout << "[BBV TEST] number of barriers loaded: " << bbvStandardprofile.size() << std::endl;
    int barrier_num = 0;
    for (const auto & barrierBbv : bbvStandardprofile) // for each barrier
    {
            std::vector<std::pair<uint64_t,uint64_t>> globalBbv;
            // find max threadBbv size for global bbvid offsetting
            uint64_t offset = Mtng::Bbv_util::getMaxBbvId();
            // std::cout << "BBV OFFSET == " << offset << std::endl;

            int threadNum = 0;
            for (const auto & threadBbv : barrierBbv) // for each thread
            {
                // globalBbv.insert( globalBbv.end(), j.begin(), j.end() );
                for (const auto & bb : threadBbv)
                {
                    // offset for threads
                    globalBbv.push_back({bb.first + (offset*threadNum) ,bb.second });
                }
                threadNum ++;
            }

            
            std::cout << "[GLOBAL BBV] \n";
            for (auto & i : globalBbv)
            {
                std::cout << i.first << ":" << i.second << " ";
            }
            std::cout << "[End of BBV]\n";
           

            int clusterId = Mtng::Bbv_util::findBbvCluster(globalBbv);
            std::cout << "[BBV   ] BBV at barrier: "<<  barrier_num << " Belongs to cluster: "<< clusterId << std::endl;
            barrier_num ++;
    }
*/


}


VOID Fini(INT32 code, VOID *v)
{
   thread_data[0].output->Magic(SIM_CMD_ROI_END, 0, 0);
   for (unsigned int i = 0 ; i < max_num_threads ; i++)
   {
//      for (unsigned int i = 0 ; i < max_num_threads ; i++)

      if (thread_data[i].output)
      {

         std::cerr << "[SIFT_RECORDER:  end of function\n " ;
         closeFile(i);
      }
   }

}

VOID Detach(VOID *v)
{
}

BOOL followChild(CHILD_PROCESS childProcess, VOID *val)
{
   if (any_thread_in_detail)
   {
      fprintf(stderr, "EXECV ignored while in ROI\n");
      return false; // Cannot fork/execv after starting ROI
   }
   else
      return true;
}

VOID forkBefore(THREADID threadid, const CONTEXT *ctxt, VOID *v)
{
   if(thread_data[threadid].output)
   {
      child_app_id = thread_data[threadid].output->Fork();
   }
}

VOID forkAfterInChild(THREADID threadid, const CONTEXT *ctxt, VOID *v)
{
   // Forget about everything we inherited from the parent
   routines.clear();
   bzero(thread_data, max_num_threads * sizeof(*thread_data));
   // Assume identity of child process
   app_id = child_app_id;
   num_threads = 1;
   // Open new SIFT pipe for thread 0
   thread_data[0].bbv = new Bbv(0);
   openFile(0);
}

bool assert_ignore()
{
   // stat is not supported in Pin 3.0
   // this code just check if the file exists or not
   // struct stat st;
   // if (stat((KnobOutputFile.Value() + ".sift_done").c_str(), &st) == 0)
   //    return true;
   // else
   //    return false;
   if(FILE *file = fopen((KnobOutputFile.Value() + ".sift_done").c_str(), "rb")){
      fclose(file);
      return true;
   }
   else{
      return false;
   } 
}

void __sift_assert_fail(__const char *__assertion, __const char *__file,
                        unsigned int __line, __const char *__function)
     __THROW
{
   if (assert_ignore())
   {
      // Timing model says it's done, ignore assert and pretend to have exited cleanly
      exit(0);
   }
   else
   {
      std::cerr << "[SIFT_RECORDER] " << __file << ":" << __line << ": " << __function
                << ": Assertion `" << __assertion << "' failed." << std::endl;
      abort();
   }
}

int main(int argc, char **argv)
{
   if (PIN_Init(argc,argv))
   {
      std::cerr << "Error, invalid parameters" << std::endl;
      std::cerr << KNOB_BASE::StringKnobSummary() << std::endl;
      exit(1);
   }
   PIN_InitSymbols();
   time_stamp1 = getSystemTime();
   time_stamp_begin = time_stamp1;
   if (KnobMaxThreads.Value() > 0)
   {
      max_num_threads = KnobMaxThreads.Value();
   }
   init_global_bbv(); 
   size_t thread_data_size = max_num_threads * sizeof(*thread_data);
   auto posix_memalign_ret = posix_memalign((void**)&thread_data, LINE_SIZE_BYTES, thread_data_size); 
   std::cout << thread_data_size << " " <<  max_num_threads << " " <<  sizeof(*thread_data) << " " << sizeof(thread_data_t) << " " << posix_memalign_ret << std::endl;  
   if ( posix_memalign_ret != 0)
   {
      std::cerr << "Error, posix_memalign() failed" << std::endl;
      exit(1);
   }
   bzero(thread_data, thread_data_size);

   PIN_InitLock(&access_memory_lock);
   PIN_InitLock(&new_threadid_lock);

   app_id = KnobSiftAppId.Value();
   blocksize = KnobBlocksize.Value();
   fast_forward_target = KnobFastForwardTarget.Value();
   detailed_target = KnobDetailedTarget.Value();
   if (KnobEmulateSyscalls.Value() || (!KnobUseROI.Value() && !KnobMPIImplicitROI.Value()))
   {
      if (app_id < 0)
         findMyAppId();
   }
   if (fast_forward_target == 0 && !KnobUseROI.Value() && !KnobMPIImplicitROI.Value())
   {
      in_roi = true;
      setInstrumentationMode(Sift::ModeDetailed);
      thread_data[0].output->Magic(SIM_CMD_ROI_START, 0, 0);
      openFile(0);
   }
   else if (KnobEmulateSyscalls.Value())
   {
      in_roi = false;
      setInstrumentationMode(Sift::ModeIcount);
      openFile(0);
   }

   // When attaching with --pid, there could be a number of threads already running.
   // Manually call NewThread() because the normal method to start new thread pipes (SYS_clone)
   // will already have happened
   if (PIN_GetInitialThreadCount() > 1)
   {
      sift_assert(thread_data[PIN_ThreadId()].output);
      for (UINT32 i = 1 ; i < PIN_GetInitialThreadCount() ; i++)
      {
         thread_data[PIN_ThreadId()].output->NewThread();
      }
   }

#ifdef PINPLAY_SUPPORTED
   if (KnobReplayer.Value())
   {
      if (KnobEmulateSyscalls.Value())
      {
         std::cerr << "Error, emulating syscalls is not compatible with PinPlay replaying." << std::endl;
         exit(1);
      }
      pinplay_engine.Activate(argc, argv, false /*logger*/, KnobReplayer.Value() /*replayer*/);
   }
#else
   if (KnobReplayer.Value())
   {
      std::cerr << "Error, PinPlay support not compiled in. Please use a compatible pin kit when compiling." << std::endl;
      exit(1);
   }
#endif

   if (KnobEmulateSyscalls.Value())
   {
      if (!KnobUseResponseFiles.Value())
      {
         std::cerr << "Error, Response files are required when using syscall emulation." << std::endl;
         exit(1);
      }

      initSyscallModeling();
   }



   initThreads();

   AtomicType atomic_type = AtomicType::BarrierPoint;
   if( KnobAtomicType.Value() == 0 ) {
        atomic_type = AtomicType::BarrierPoint;
   } else if( KnobAtomicType.Value() == 1 ) {
        atomic_type = AtomicType::AtomicRegion;
   } else {
        atomic_type = AtomicType::HybridMode;
   }

   initRecorderControl();
   initRecorderBase();
   initEmulation();
   std::cerr <<__FUNCTION__ <<" "  << (int)atomic_type << " " << (int)AtomicType::BarrierPoint << std::endl;
/*
   if (KnobMtngEnable.Value())
   {
        //initInsCount(); 
        initToolMtng( atomic_type );
        initMtr();
   }
*/

   mtr_enabled = false;
   if (KnobRoutineTracing.Value())
      initRoutineTracer();

   PIN_AddFiniFunction(Fini, 0);
   PIN_AddDetachFunction(Detach, 0);

   PIN_AddFollowChildProcessFunction(followChild, 0);
   if (KnobEmulateSyscalls.Value())
   {
      PIN_AddForkFunction(FPOINT_BEFORE, forkBefore, 0);
      PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, forkAfterInChild, 0);
   }

   pinboost_register("SIFT_RECORDER", KnobDebug.Value());
    
   Hooks::hooks_init(getHooksManager());
   int64_t mtng_version = KnobMtngEnable.Value();
   if (mtng_version!=0)
   {
//        mtng::activate();
        if(mtng_version==1) { //1 means intrabarrier version
     
            std::cout << "intrabarrier version enable\n";
            intrabarrier_mtng::activate( KnobDVFSEnable.Value()) ;
            
            initMtr();
        } else if(mtng_version==2){ // 2 means hybrid version
            std::cout << "hybridbarrier version enable\n";
            hybridbarrier_mtng::activate();
            initMtr();
        } else if(mtng_version==100){ // 100 means smarts
            std::cout << "smarts enable\n";
            SMARTS::activate(KnobDVFSEnable.Value());
            initMtr();
        } else if(mtng_version==1004) { // 1004 means only enabling fast-forward mode
            std::cout << "fast forward version enable\n";
            //fastforward_mtng::activate();
            EMU::activate();
        } else if(mtng_version==1005) { // 1005 means only enabling fast-forward mode; and analytical data
            std::cout << "fast forward version enable\n";
            //fastforward_mtng::activate();
            EMU::activate();
            intrabarrier_analysis::activate(KnobDVFSEnable.Value());
        } else if(mtng_version==1006) { // 1006 means only enabling fast-forward mode; analytical code and mtr
            std::cout << "fast forward version enable\n";
            //fastforward_mtng::activate();
            EMU::activate();
            intrabarrier_analysis::activate( KnobDVFSEnable.Value()) ;
            initMtr();
        } else if(mtng_version==1007) { // 1006 means only enabling fast-forward mode and mtr
            std::cout << "pure mtr version enable\n";
            //fastforward_mtng::activate();
            EMU::activate();
            initMtr();
        }



        in_roi = true;
        setInstrumentationMode(Sift::ModeDetailed);

        thread_data[0].output->Magic(SIM_CMD_ROI_START, 0, 0);

        //initInsCount(); 
//        initToolMtng( atomic_type );
//        if( KnobMTREnable.Value() ) {
//        }
   } else {
//       std::cout << "Fast Forwatd Target : "<< fast_forward_target << "\n";
        if (fast_forward_target == 0 )
        {
           in_roi = true;
           setInstrumentationMode(Sift::ModeDetailed);
           thread_data[0].output->Magic(SIM_CMD_ROI_START, 0, 0);
        }
        else 
        {
           in_roi = true;
           setInstrumentationMode(Sift::ModeDetailed);
           thread_data[0].output->Magic(SIM_CMD_ROI_START, 0, 0);


            intrabarrier_mtng::activate( KnobDVFSEnable.Value()) ;
           EMU::activate();
        }
        FullDebug::activate();
   }
   if( KnobDVFSEnable.Value() ) {
       DVFSConfigure::activate( mtng_version!=0 );  
   }
    PIN_StartProgram();
   return 0;
}
