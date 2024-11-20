#include "hooks_init.h"
#include "bbv_count.h"
#include "globals.h"
#include "hooks_manager.h"
#include "recorder_control.h"
#include "sift/sift_format.h"
#include "sift_assert.h"
#include "sim_api.h"
#include "threads.h"
//#include "tool_mtng.h"
#include <cstdint>
#include <set>
#include <utility>
#include <sstream>
//#include "tool_mtng.h"
#include "threads.h"
#include "cond.h"
#include "tool_warmup.h"
#include <set>
#include "full_debug.h"
#include "intrabarrier_mtng.h"
namespace DVFSConfigure{
typedef void (*CollectFunction)(uint32_t a,ADDRINT pcaddr);
CollectFunction collectDataDVFS;

class DVFSConfigure{
    uint64_t instruction_target ;
    std::vector<uint64_t> instruction_target_vec;
    std::vector< uint64_t > dvfs_configure;
    std::vector< uint32_t > active_of_thread;
    uint32_t freq;
    uint32_t dvfs_idx = 0;
    uint64_t global_ins_num = 0;
    std::vector<uint32_t > change_frequency_of_thread;
    public:
    DVFSConfigure () {
#define BASEMENT_INSNUM 100000000
        uint64_t basement_ins = BASEMENT_INSNUM;
        uint64_t tmp[] = { 1995, 1330,  2660 };
        uint64_t instruction_target_tmp[] = { 6*basement_ins, 2 * basement_ins , 14*basement_ins };
        int n = sizeof(tmp) / sizeof(tmp[0]);
        dvfs_configure = std::vector<uint64_t>( tmp, tmp + n );
        instruction_target_vec = std::vector<uint64_t>( instruction_target_tmp,instruction_target_tmp+n );
        instruction_target = instruction_target_vec.back();
//        std::cout <<  "dvfs:\n";
//        for(auto elem : dvfs_configure) {
//            std::cout << elem << "\n";
//        }
        active_of_thread.resize( MAX_NUM_THREADS_DEFAULT,0 );
        change_frequency_of_thread.resize( MAX_NUM_THREADS_DEFAULT,0 );
        dvfs_idx = 0;
    }
    void SetFreq( uint32_t freq_in ) {
        freq = freq_in;
    }
    uint32_t checkFreqs(THREADID threadid, ADDRINT pc_addr,UInt32 ins_num)
    { 
      const uint32_t main_thread_id = 0; 
        
      if(threadid == main_thread_id) {
          if( global_ins_num > instruction_target )
          {
              global_ins_num = 0;
              freq = dvfs_configure[  dvfs_idx ];
              instruction_target = instruction_target_vec[dvfs_idx];
              std::cout << "Freq : " << freq << "\n";
              collectDataDVFS( freq,pc_addr );  

                std::cout << freq << "\n";
              for( uint32_t ti = 1 ; ti < MAX_NUM_THREADS_DEFAULT ; ti++) {
                  if( active_of_thread[ti]) {
                      change_frequency_of_thread[ti ] = 1;
                  }
              } 
              thread_data[threadid].output->Magic( SIM_CMD_MHZ_SET, threadid, freq );
              dvfs_idx ++;
              if( dvfs_idx >= dvfs_configure.size()) {
                dvfs_idx = 0;
              }
          } 
          global_ins_num += ins_num;
      }
      if( change_frequency_of_thread[ threadid ] ) {
          thread_data[threadid].output->Magic( SIM_CMD_MHZ_SET, threadid, freq );
          change_frequency_of_thread[ threadid] = 0;
      }
      return freq;
    }
    VOID
    threadStart(THREADID thread_id)
    {

      thread_data[thread_id].output->Magic( SIM_CMD_MHZ_SET, thread_id, freq );
      active_of_thread[thread_id] = 1;
    }
    VOID
    threadFini(THREADID thread_id)
    {

      active_of_thread[ thread_id ] = 0;
    }

    static DVFSConfigure * toDVFSConfigure(void *v) {
      DVFSConfigure * mtng_ptr = reinterpret_cast<DVFSConfigure*>(v); 
      if( mtng_ptr == nullptr ) {
          std::cerr << "null Failed";
      }
      return mtng_ptr; 
    }

};


VOID checkFreqs(THREADID thread_id, ADDRINT pc_addr,UInt32 ins_num,void *v) {
    auto dvfs_configure_ptr = DVFSConfigure::toDVFSConfigure(v);
    dvfs_configure_ptr->checkFreqs( thread_id,pc_addr,ins_num );
}

VOID
traceCallbackFreq(TRACE trace, void* v)
{
  for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
  
        BBL_InsertCall(bbl, IPOINT_BEFORE,(AFUNPTR)checkFreqs, IARG_THREAD_ID,IARG_INST_PTR, IARG_UINT32 ,BBL_NumIns(bbl), IARG_ADDRINT, v,IARG_END);
  }
}
VOID
threadStart(THREADID thread_id, CONTEXT* ctxt, INT32 flags, VOID* v)
{
  if (thread_id >= MAX_NUM_THREADS_DEFAULT) {
    std::cerr
      << "Error: More threads requested than we have allocated space for (MAX="
      << MAX_NUM_THREADS_DEFAULT << ", id=" << thread_id << ")" << std::endl;
    PIN_RemoveInstrumentation();
  }
  auto dvfs_configure_ptr = DVFSConfigure::toDVFSConfigure(v);
  dvfs_configure_ptr->threadStart( thread_id );
}
static void handleThreadFinish(THREADID thread_id, const CONTEXT *ctxt, INT32 flags, VOID *v) 
{

    auto dvfs_configure_ptr = DVFSConfigure::toDVFSConfigure(v);
    dvfs_configure_ptr->threadFini( thread_id );

}

DVFSConfigure dvfs_configure;

    void activate( bool if_mtng) {
        
        std::cout << "DVFS Enable\n";
        
        uint32_t freq = thread_data[0].output->Magic( SIM_CMD_MHZ_GET, 0,0);
        dvfs_configure.SetFreq(freq);
        TRACE_AddInstrumentFunction(traceCallbackFreq, &dvfs_configure);
        PIN_AddThreadFiniFunction( handleThreadFinish, &dvfs_configure);
        PIN_AddThreadStartFunction(threadStart, &dvfs_configure);
        if(if_mtng) {
            collectDataDVFS = intrabarrier_mtng::collectDataDVFS ;
        } else {
            collectDataDVFS = FullDebug::collectDataDVFS;
        }
    }
}
