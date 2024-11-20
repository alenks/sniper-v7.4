
/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs 
 *  and could serve as the starting point for developing your first PIN tool
 */
//#include "globals.h"
#include "pin.H"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <map>
#include "control_manager.H"
#define OMP_BEGIN "GOMP_parallel_start"
#define OMP_END "GOMP_parallel_end"
#include "sim_api.h"
#include "bbv_count.h"
#include "globals.h"
#include "hooks_manager.h"
#include "recorder_control.h"
#include "sift/sift_format.h"
#include "sift_assert.h"
#include "sim_api.h"
#include "threads.h"
#include <cstdint>
#include <set>
#include <utility>
#include <sstream>
#include "threads.h"
#include "cond.h"
#include "tool_warmup.h"
#include <set>
#include "mtng.h"
#include "to_json.h"
#include "log2.h"
#include "bbv_count_cluster.h"
//#define tuple pair
#include "tuple_hash.h"
using namespace std;
//#define MARKER_INSERT
#include <queue>
#include <deque>
#include <iostream>
#include <functional>
//#include <tuple>
#include <utility>


namespace FullDebug {
    uint32_t global_maxthread_num;
    class Feature {
        public:
            uint64_t fs;
            uint64_t max_ins_num;
            uint64_t ins_nums[MAX_NUM_THREADS_DEFAULT];
            uint64_t  cluster_id;
            uint32_t freq;
            bool solved;
            std::string toJson() {
                std::stringstream out;
                out << " { \n";
                out << "\"max_ins_num\" : " << max_ins_num << " , \n";
                out << "\"freq\" : " << freq << " , \n";
                out << "\"fs\" : " << fs << ",  \n";
                out << insertElem("insnums",ins_nums,global_maxthread_num)<< "\n";
                out << " }";
                return out.str();
            }
            void SetFreq(uint32_t freq_in) {
                freq= freq_in;
            }

            Feature( uint64_t fs_, uint64_t max_ins_num_ ): fs(fs_),max_ins_num(max_ins_num_){
            }

            Feature( uint64_t fs_, const std::vector<uint64_t> & insnums ):
            fs(fs_), solved(true) //use for detailed mode
            {
                max_ins_num = 0;
                for( uint32_t i = 0 ; i < MAX_NUM_THREADS_DEFAULT ; i++ ) {
                    ins_nums[i] = insnums[i];
                    max_ins_num += ins_nums[i];

                }
            }
            Feature(): fs(0), solved(false){
             
            }
            void set_cluster_id( uint64_t cluster_id_) {
                cluster_id = cluster_id_;
            }
            Feature(  const std::vector<uint64_t> & insnums ): solved(false){
                 max_ins_num = 0;
                 for( uint32_t i = 0 ; i < MAX_NUM_THREADS_DEFAULT ; i++ ) {
                    ins_nums[i] = insnums[i];
                    max_ins_num += ins_nums[i];
                }
                fs = 0;
            }
            void add( const Feature & l ) {
                fs += l.fs;
                max_ins_num += l.max_ins_num;
            }
    };

    class FullDebug{
        uint32_t freq;
        const uint32_t threadshold = 10000; 
        //const uint64_t terminate_insnum = 1000000000;  //billion
        const uint64_t terminate_insnum =   10000000;  //billion
        uint64_t next_target = threadshold;
        const uint32_t maximum_thread_num = MAX_NUM_THREADS_DEFAULT;
        uint32_t max_thread_num;
        std::vector<uint64_t> ins_num_vec;
        std::vector<uint64_t> ins_num_interval_vec;
        std::vector<uint64_t> interval_ins_num_vec;
        uint64_t prior_fs;
        uint64_t fs;

        std::vector<Feature> feature_stack;
public:
        void SetFreq( uint32_t freq_in ) {
            freq = freq_in;
        }
        FullDebug() {
            max_thread_num = 1;
            ins_num_vec.resize( maximum_thread_num,0 );
            ins_num_interval_vec.resize( maximum_thread_num,0 );
            interval_ins_num_vec.resize( maximum_thread_num,0 );

        } 

        uint64_t get_insnum() {
            uint64_t insnum_sum = 0;
            for(uint32_t ti = 0 ; ti < maximum_thread_num ; ti++ ) {
                uint64_t insnum = 0 ;
                insnum = interval_ins_num_vec[ti];
                        
                auto insnum_sum_interval =   insnum - ins_num_vec[ti];  
                ins_num_interval_vec[ti] = insnum_sum_interval;
                ins_num_vec[ti] = insnum;
            }
            return insnum_sum;
        }

        Feature get_ipc() {
            get_insnum();

            fs = thread_data[0].output->Magic(SIM_CMD_GET_SIM_TIME, 0, 0 );
            uint64_t interval = fs - prior_fs;
            prior_fs = fs;
            return Feature(  interval,  ins_num_interval_vec );
        }
        void init_ipc() {
            get_insnum();       
            prior_fs = thread_data[0].output->Magic(SIM_CMD_GET_SIM_TIME, 0, 0 );
        }

        void collectData() {
            auto feature = get_ipc();
            if( feature.fs == 0 ) {
                if(feature.max_ins_num!=0) {
                    if(feature_stack.size() > 0) {
                        feature.fs = (uint64_t)((double)feature_stack[feature_stack.size()-1].fs / feature_stack[feature_stack.size()-1].max_ins_num * feature.max_ins_num);
                        if(feature.fs!=0) {
                            feature.SetFreq(freq);
                            feature_stack.push_back(feature);
                        }
                    }
                }
                return;

            } else { 
                feature.SetFreq(freq);
                feature_stack.push_back(feature);
            }
        }
        std::string toJson() {
            std::stringstream out ;
            //////////feature stack
            out << "{";
            out << "\"feature_stack\" : " << "\n";
            out << "[ \n";
            if(feature_stack.size() > 0) {
                for( uint32_t i = 0 ; i < feature_stack.size() - 1; i++ ) {
                    auto & elem = feature_stack[i];
                    out << elem.toJson() << ",\n";
                }
                out << feature_stack[feature_stack.size()-1].toJson() << "\n";
            }

            out << "] \n";
            
            out << "}";
            return out.str();
        }
        void fflush() {
            std::ofstream final_file;
            std::string res_dir = KnobMtngDir.Value();
            std::string full_path = res_dir + "/" + "full.json";
            
            global_maxthread_num = max_thread_num;
            final_file.open( full_path.c_str(),std::ofstream::out );
            final_file << toJson();
            final_file.close();
        
        }
        ~FullDebug() {
        }
        static FullDebug * toFullDebug( VOID * v ) {
            FullDebug * full_debug = reinterpret_cast<FullDebug*>(v); 
            if( full_debug == nullptr ) {
                cerr << "null Failed";
            }
            return full_debug;
        }
        void resetCount() {
            next_target = interval_ins_num_vec[0] + threadshold;
        }
        void recordInsCount( THREADID tid, uint32_t insnum ,ADDRINT addr) {
            interval_ins_num_vec[tid] += insnum;
            if(tid == 0 ) {
            if( interval_ins_num_vec[tid] > next_target ) {
                collectData();
                next_target = interval_ins_num_vec[tid] + threadshold;
            }
#ifdef TERMINATE
            if( interval_ins_num_vec[tid] > TERMINATE ) {
                PIN_ExitApplication(0);
            }
            
#endif
            }
        }
        void threadStart( THREADID threadid ) {
           if(threadid > max_thread_num - 1) {
                max_thread_num = threadid + 1;
           }
        }

    };


void recordInsCount( THREADID threadid, uint32_t insnum, ADDRINT pc_addr, VOID * v  ) {
    //if( threadid == 0 ) 
    {
        
        FullDebug * fulldebug_ptr = FullDebug::toFullDebug(v);
        fulldebug_ptr->recordInsCount( threadid, insnum,pc_addr );
    }
}
static VOID traceCallback(TRACE trace, void *v) {

        BBL bbl_head = TRACE_BblHead(trace);
        for (BBL bbl = bbl_head; BBL_Valid(bbl); bbl = BBL_Next(bbl)) {

            INS tail = BBL_InsTail(bbl);
            INS_InsertPredicatedCall( tail, IPOINT_BEFORE, (AFUNPTR) recordInsCount ,
            IARG_THREAD_ID,
            IARG_UINT32, BBL_NumIns(bbl),
            IARG_INST_PTR,
            IARG_ADDRINT, v,
            IARG_END);

        }
}
VOID Fini( INT32 code,void *v) {
    
        FullDebug * mtng_ptr = FullDebug::toFullDebug( v );
        delete mtng_ptr;
}
VOID
processExit( THREADID threadid, ADDRINT pcaddr, void *v)
    {

        if(threadid!=0) return;
        FullDebug * mtng_ptr = FullDebug::toFullDebug( v );
        mtng_ptr->collectData();
        mtng_ptr->fflush();

    }

static void routineCallback(RTN rtn, VOID *v)
{

  RTN_Open(rtn);
  if (RTN_Valid(rtn) )
  {

      std::string rtn_name = RTN_Name(rtn).c_str();
      if ( rtn_name == "_exit" || rtn_name == "_Exit" )
      {
                 RTN_InsertCall( rtn, IPOINT_BEFORE, (AFUNPTR)processExit,
                     IARG_THREAD_ID,
                     IARG_INST_PTR,
                     IARG_ADDRINT,v,
                     IARG_END);
           
      }
  }

   RTN_Close(rtn);
}
    VOID
    threadStart(THREADID thread_id, CONTEXT* ctxt, INT32 flags, VOID* v)
    {
        FullDebug * mtng_ptr = FullDebug::toFullDebug( v );
        mtng_ptr->threadStart(thread_id);
    }



  FullDebug *  full_debug; 
    void activate() {
        cout << "activate\n";
        full_debug = new FullDebug();
        uint32_t freq = thread_data[0].output->Magic( SIM_CMD_MHZ_GET, 0,0);
        cout << "init freq: "<<freq<<"\n";
        full_debug->SetFreq(freq);
        full_debug->init_ipc();
        TRACE_AddInstrumentFunction(traceCallback, full_debug);
        PIN_AddFiniFunction(Fini, (void*)full_debug);
        RTN_AddInstrumentFunction(routineCallback, full_debug);
        PIN_AddThreadStartFunction(threadStart, full_debug);
    }
    void collectDataDVFS( uint32_t freq, ADDRINT pcaddr ) {
        full_debug->collectData();
        full_debug->SetFreq(freq);
        full_debug->resetCount();
    }

}
