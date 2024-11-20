
/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs 
 *  and could serve as the starting point for developing your first PIN tool
 */
//#include "globals.h"
#include "pin.H"
#include "lock.h"
#include "cond.h"
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
#include <cmath>
 
namespace SMARTS{
    THREADID max_thread_num = 1;
    template<bool dvfs_enable>
    class Feature {
        public:
            uint32_t idx;
            uint64_t fs;
            double timeperins;
            uint64_t max_ins_num;
            uint64_t ins_nums[MAX_NUM_THREADS_DEFAULT];
            uint32_t freq;
            ADDRINT pc;
            bool solved;
            std::string toJson() {
                std::stringstream out;
                out << " { \n";
                out << "\"max_ins_num\" : " << max_ins_num << " , \n";
                out << "\"fs\" : " << fs << " , \n";
                out << "\"solved\" : " << (int)solved << " , \n";
                out << insertElem( "insnums", ins_nums, max_thread_num ) <<  " , \n";
                out << "\"freq\" : " << freq << "  \n";
                out << " }";
                return out.str();
            }
            void SetFreq(uint32_t freq_in) {
                freq= freq_in;
            }

            Feature( uint64_t fs_, const std::vector<uint64_t> & insnums ):
            fs(fs_), solved(true) //use for detailed mode
            {
                max_ins_num = 0;
                for( uint32_t i = 0 ; i < MAX_NUM_THREADS_DEFAULT ; i++ ) {
                    ins_nums[i] = insnums[i];
                    max_ins_num = max_ins_num + ins_nums[i];
                }
                timeperins = (double)fs / (double)max_ins_num;
            }
            Feature(): fs(0),  solved(false){
                 
            }
            Feature(  const std::vector<uint64_t> & insnums ):solved(false){
                 max_ins_num = 0;
                 for( uint32_t i = 0 ; i < MAX_NUM_THREADS_DEFAULT ; i++ ) {
                    ins_nums[i] = insnums[i];
                    max_ins_num = max_ins_num + ins_nums[i];
                }
                fs = 0;
            }
    };
    std::ostream & operator<<( std::ostream & out, const Feature<false> & feature ) {
        out << "fs : " << feature.fs/1e15   << " insnum: " << feature.max_ins_num <<  " solved " << feature.solved;
        
        return out;
    }

    
    std::ostream & operator<<( std::ostream & out, const Feature<true> & feature ) {
        out << "fs : " << feature.fs/1e15   << " insnum: " << feature.max_ins_num <<  " solved " << feature.solved;
        return out;
    }
    class VirtualSMARTS{
        public:    
        virtual void intraBarrierInstruction( THREADID tid, ADDRINT addr ) = 0 ;
        virtual void recordInsCount( THREADID tid, uint32_t insnum ,ADDRINT addr) =0;
        virtual void changeSimMode( mtng::SimMode ret ) =0;
        virtual void fini( ADDRINT pcaddr ) = 0 ;
        virtual void collectData(ADDRINT pcaddr,uint32_t freq) = 0 ;
        virtual void init_ipc(uint32_t tid) = 0;

        virtual void threadStart( THREADID threadid ) =0 ;
        virtual void SetFreq(uint32_t freq_in) = 0;
        static VirtualSMARTS * toSMARTS( void * v) {
            VirtualSMARTS * mtng_ptr = reinterpret_cast<VirtualSMARTS*>(v);
            if( mtng_ptr == nullptr ) {
                cerr << "null Failed";
            }
            return mtng_ptr;
        }
        virtual ~VirtualSMARTS()=default;
    };

    template<bool dvfs_enable>
    class SMARTS : public VirtualSMARTS
    {
        mtng::RecordType last_record_type = mtng::RecordType::SingleThread;
        std::vector<uint64_t> m_threads_to_signal;
        const uint32_t maximum_thread_num = MAX_NUM_THREADS_DEFAULT;
        std::vector<uint64_t> ins_num_vec;
        std::vector<uint32_t> fast_forward_sync_with_backend;
        std::vector<uint64_t> ins_num_interval_vec;
        std::vector<uint64_t> bbv_ins_num_vec;
        std::vector<uint64_t> bbv_ins_interval_num_vec;
        mtng::SimMode mtng_current_mode = mtng::SimMode::DetailMode;
//#define THREDSHOLD_SMARTS 1000000
        uint64_t next_target = 0;
        std::vector<uint64_t> interval_ins_num_vec;
        ///////for dvfs support
        uint32_t freq;
        uint32_t next_freq;
        //
        uint64_t fs;
        uint64_t prior_fs;
        uint64_t next_pc_if_not_branch;
        bool is_branch_inst; 
        double time_sum;
        const uint64_t threshold;
        const uint32_t fastforwardnum  ;
        uint64_t current_barrier;
        const uint32_t warmupnum = 0;
        const uint32_t detailmodenum = 1 ;
        std::vector<uint32_t> enable_warmup_vec;
        uint64_t intra_id = 0;
        ///output
        std::vector<Feature<dvfs_enable>> feature_stack;
        Feature<dvfs_enable> feature;
        public:
        
        std::string toJson() {
            std::stringstream out ;
            //////////feature stack
            out << "{";
            out << "\"time_sum\": " << time_sum << ",\n";
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
        std::vector<int> thread_active_vec;
        SMARTS() :  threshold(KnobSampledRegionSize.Value() * 1000),fastforwardnum(KnobSmartsFFWRegionNum.Value()),current_barrier(0) {

            std::cout << "Sampled Region Size : " << threshold << std::endl;
            std::cout << "FFW Region Num : " << fastforwardnum << std::endl;
            max_thread_num = 1;
            ins_num_vec.resize( maximum_thread_num,0 );
            fast_forward_sync_with_backend.resize( maximum_thread_num,0 );

            enable_warmup_vec.resize(maximum_thread_num,0);
            ins_num_interval_vec.resize( maximum_thread_num,0 );
            bbv_ins_num_vec.resize( maximum_thread_num,0 );
            bbv_ins_interval_num_vec.resize( maximum_thread_num,0 );
            interval_ins_num_vec.resize( maximum_thread_num,0 );
            
            resetCount() ;
        }

    void intraBarrierInstruction( THREADID tid, ADDRINT addr ) {

            if(tid!=0) return;
             
            if( interval_ins_num_vec[tid] > next_target ) {
                auto ret = processRegion<mtng::RecordType::IntraBarrier>( addr );                     
                changeSimMode( ret);
            }
    }

        void resetCount() {
            next_target = interval_ins_num_vec[0] + threshold;
            //std::cout << "next target : "<<next_target<<"\n";
        }



        uint64_t get_insnum() {
            uint64_t insnum_sum = 0;
            for(uint32_t ti = 0 ; ti < maximum_thread_num ; ti++ ) {
                uint64_t insnum = 0 ;
                    insnum = interval_ins_num_vec[ti];
                                        
                    auto insnum_sum_interval =   insnum - ins_num_vec[ti];
                    ins_num_vec[ti] = insnum;
                    ins_num_interval_vec[ti] = insnum_sum_interval;
                
                    insnum_sum += insnum_sum_interval;
            }
            return insnum_sum;
        }

        Feature<dvfs_enable> get_ipc() {
            get_insnum();
            uint32_t master_thread_id = 0;
            fs = thread_data[master_thread_id].output->Magic(SIM_CMD_GET_SIM_TIME, 0, 0 );
            //LOG2(INFO) << "time: " <<  fs/1e15 << std::endl;
            uint64_t interval = fs - prior_fs;
            prior_fs = fs;
            return Feature<dvfs_enable>(  interval,  ins_num_interval_vec);
        }
        void init_ipc(uint32_t tid) {
            prior_fs = thread_data[tid].output->Magic(SIM_CMD_GET_SIM_TIME, 0, 0 );
            //LOG2(INFO) << "time: " <<  prior_fs/1e15 << std::endl;
        }
        void recordState(  ) {
            if( mtng_current_mode == mtng::SimMode::DetailMode) {
                feature = get_ipc(); 
            } else {
                get_insnum();
                feature =  Feature<dvfs_enable>(ins_num_interval_vec);
            }
        }
        
        



        void fini( ADDRINT pcaddr ) {
            
            processRegion<mtng::RecordType::EndPgm>(pcaddr);

            repairData();

        }
        void enable_warmup() {
            getWarmupTool()->startWriteWarmupInstructions();
            for( uint32_t i = 0 ; i < MAX_NUM_THREADS_DEFAULT ; i++ ) {
                enable_warmup_vec[i] = 1;
            }
        }
        void disable_warmup() {
            getWarmupTool()->stopWriteWarmupInstructions();
            
            for( uint32_t i = 0 ; i < MAX_NUM_THREADS_DEFAULT ; i++ ) {
                enable_warmup_vec[i] = 0;
                //start_warmup_vec[i] = 0;
            }
        }

        void changeSimMode( mtng::SimMode ret ) {
              //std::cout << "next mode:"<<ret<<std::endl ;
              if( mtng_current_mode == ret ) {
                    return;
              }
              mtng_current_mode = ret;
              if( ret == mtng::SimMode::FastForwardMode ) {
                   gotoFastForwardMode();
//              } else if(ret ==mtng::SimMode::WarmUpMode) {
//                   enable_warmup();
              } else if (ret == mtng::SimMode::DetailMode) {
//                   gotoDetailMode(0);
                   enable_warmup();
              }
        }
        void gotoDetailMode(uint32_t tid) {
            for( uint32_t i = 0 ; i < MAX_NUM_THREADS_DEFAULT ; i++ ) {
                fast_forward_sync_with_backend[ i ] = 1;
            }
            if (thread_data[tid].icount_reported > 0)
            {
               thread_data[tid].output->InstructionCount(thread_data[tid].icount_reported);
               thread_data[tid].icount_reported = 0;
            }
            fast_forward_sync_with_backend[0] = 0;


            setInstrumentationMode( Sift::ModeDetailed );
            thread_data[tid].output->Magic(SIM_CMD_ROI_START, 0, 0);
            init_ipc(tid);
        }
        void gotoWarmUpMode(uint32_t tid) {
            for( uint32_t i = 0 ; i < MAX_NUM_THREADS_DEFAULT ; i++ ) {
                fast_forward_sync_with_backend[ i ] = 1;
            }
            if (thread_data[tid].icount_reported > 0)
            {
               thread_data[tid].output->InstructionCount(thread_data[tid].icount_reported);
               thread_data[tid].icount_reported = 0;
            }
            uint32_t master_thread_id = 0;
            fast_forward_sync_with_backend[master_thread_id] = 0;

            setInstrumentationMode( Sift::ModeMemory );
            thread_data[tid].output->Magic(SIM_CMD_ROI_START, 0, 0);
        }

//        #define ONLY_DETAILED_MODE
        void gotoFastForwardMode() {
#ifndef ONLY_DETAILED_MODE
            setInstrumentationMode(Sift::ModeIcount);
            uint32_t master_thread_id = 0;
            thread_data[master_thread_id].output->Magic(SIM_CMD_ROI_END, 0, 0);
            for( uint32_t i = 0 ; i < MAX_NUM_THREADS_DEFAULT ; i++ ) {
                fast_forward_sync_with_backend[ i ] = 1;
            }
            uint32_t tid = master_thread_id;
            if (thread_data[tid].icount_reported > 0)
            {
               thread_data[tid].output->InstructionCount(thread_data[tid].icount_reported);
               thread_data[tid].icount_reported = 0;
            }
            fast_forward_sync_with_backend[tid] = 0;

#endif
        }
        bool warmupfinish() {
             bool warmup_finished = true;
             for( uint32_t i = 0 ; i < MAX_NUM_THREADS_DEFAULT ; i++ ) {
                 if(thread_active_vec[i]&&enable_warmup_vec[i]==2) { // just wait these warmup thread that started
//                     LOG2(INFO) << i << " " << thread_active_vec[i] << " " << enable_warmup_vec[i];
                     warmup_finished = false;
                     break;
                 }
             }

             if(warmup_finished) {
                return true;
             } else {
                return false;
             }
        }
        void recordInsCount( THREADID tid, uint32_t insnum ,ADDRINT addr) {
            interval_ins_num_vec[tid] += insnum;
            if(fast_forward_sync_with_backend[tid]) {
               if (thread_data[tid].icount_reported > 0)
               {
                  thread_data[tid].output->InstructionCount(thread_data[tid].icount_reported);
                  thread_data[tid].icount_reported = 0;
               }
               fast_forward_sync_with_backend[tid] = 0;
            }

            ///for warmup
            if(enable_warmup_vec[tid]==1) {
                getWarmupTool()->writeWarmupInstructions(tid);
                if( tid == 0 ){
                    disable_warmup();
 //                   gotoWarmUpMode(0);

                    gotoDetailMode(0);
                } 
            }

        }
        void threadStart( THREADID threadid ) {
           if(threadid > max_thread_num - 1) {
                max_thread_num = threadid + 1;
           }
        }


        mtng::SimMode decideNextSimMode( ) {
            intra_id++;
            uint32_t index_mod = intra_id % ( fastforwardnum + warmupnum +  detailmodenum  );
            mtng::SimMode next_mode;
            if( index_mod < detailmodenum ) {
                
                next_mode = mtng::SimMode::DetailMode;
            } else if ( index_mod < detailmodenum + fastforwardnum ) {
                next_mode = mtng::SimMode::FastForwardMode;
            } else {
                next_mode = mtng::SimMode::WarmUpMode;
            }
            return next_mode;
        }
        //dvfs support
        void collectData(ADDRINT pcaddr, uint32_t freq) {
            
            next_freq = freq;
            auto ret = processRegion<mtng::RecordType::HardwareEvent>(pcaddr);
            changeSimMode( ret);
        }

        template< mtng::RecordType record_type >
        mtng::SimMode processRegion( ADDRINT pcaddr  ) {
            resetCount();
            recordState( );
            feature.SetFreq( freq );
            feature.idx = feature_stack.size();
            //std::cout <<feature <<std::endl;
            feature_stack.push_back(feature);
            if( record_type == mtng::RecordType::HardwareEvent ) {
                freq = next_freq;
            }
            auto ret = decideNextSimMode();
            return ret;
        }

        void repairData() {
            //uint64_t ins_num_sum=0;
            double fs_detailed =  0;
            //uint64_t insnum_detailed = 0;
            double timeperins = 0;
            for( auto & elem : feature_stack ) { 
                if(elem.solved) {
                    timeperins = elem.timeperins;
                    fs_detailed += elem.fs;
                } else {
                    elem.timeperins = timeperins;
                    elem.fs = elem.max_ins_num * timeperins;
                    fs_detailed += elem.fs;
                }
            }
            
            time_sum = fs_detailed;
            std::cout << "time sum: " << time_sum /1e15 << std::endl;
           // std::cout << "ins num: " << ins_num_sum << std::endl;
           // std::cout << "sampled ins num: " << insnum_detailed << std::endl;
            std::ofstream final_file;
            std::string res_dir = KnobMtngDir.Value();
            std::string full_path = res_dir + "/" + "smarts.json";
            final_file.open( full_path.c_str(),std::ofstream::out );
            final_file << toJson();
            final_file.close();
        }
        static SMARTS * toSMARTS( VOID * v ) {
            SMARTS * mtng_ptr = reinterpret_cast<SMARTS*>(v); 
            if( mtng_ptr == nullptr ) {
                cerr << "null Failed";
            }
            return mtng_ptr;
        }
        void SetFreq( uint32_t freq_in ) {
            freq = freq_in;
        }

};
     void recordInsCount( THREADID threadid, uint32_t insnum, ADDRINT pc_addr, VOID * v  ) {
        auto * mtng_ptr = VirtualSMARTS::toSMARTS(v);
        mtng_ptr->recordInsCount(threadid, insnum, pc_addr);

        mtng_ptr->intraBarrierInstruction(threadid, pc_addr);
    }
   



VOID
traceCallbackForBarrierPoint(TRACE trace, void* v)
{

    ///comments: for intra-barrier
      for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) 
      {
         INS tail = BBL_InsTail(bbl);
               ///comments: keep this function;
            INS_InsertPredicatedCall( tail, IPOINT_BEFORE, (AFUNPTR) recordInsCount ,
            IARG_THREAD_ID,
            IARG_UINT32, BBL_NumIns(bbl),
            IARG_INST_PTR,
            IARG_ADDRINT, v,
            IARG_END);

      }
}
VOID
processExit( THREADID threadid, ADDRINT pcaddr, void *v)
    {

        if(threadid!=0) return;
        VirtualSMARTS * mtng_ptr = VirtualSMARTS::toSMARTS( v );
        mtng_ptr->fini(pcaddr);
        
    }
VOID Fini( INT32 code,void *v) {
    
        VirtualSMARTS * mtng_ptr = VirtualSMARTS::toSMARTS( v );
        delete mtng_ptr;
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
    VOID threadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v) {
        auto * mtng_ptr = VirtualSMARTS::toSMARTS( v );
        mtng_ptr->threadStart(threadid);
    }

    void * mtng_global_void;
    template<bool dvfs_enable2>
    void activate( ) {
        VirtualSMARTS * mtng_global = new SMARTS<dvfs_enable2>();
        mtng_global_void = mtng_global; 

        uint32_t freq = thread_data[0].output->Magic( SIM_CMD_MHZ_GET, 0,0);

        mtng_global->init_ipc(0);
        mtng_global->SetFreq( freq );


        PIN_AddThreadStartFunction(threadStart, mtng_global);
        TRACE_AddInstrumentFunction(traceCallbackForBarrierPoint, mtng_global);
        PIN_AddFiniFunction(Fini, (void*)mtng_global);

        RTN_AddInstrumentFunction(routineCallback, mtng_global);
    }
    void collectDataDVFS( uint32_t freq ,ADDRINT pcaddr) {
        VirtualSMARTS * mtng_global = VirtualSMARTS::toSMARTS( mtng_global_void );
        mtng_global->collectData(pcaddr,freq);
    }
    
    void activate(bool dvfs_enable) {
        if(dvfs_enable) {
            activate<true>();
        } else {
            activate<false>();
        }
    }
};
