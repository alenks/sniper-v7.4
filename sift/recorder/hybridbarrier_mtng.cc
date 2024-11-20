
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
   
using namespace mtng;
namespace hybridbarrier_mtng{
THREADID master_thread_id = 0;
//**common mtng
VOID sendInstruction(THREADID threadid, ADDRINT addr, UINT32 size,  BOOL is_branch, BOOL taken, BOOL is_predicate, BOOL executing, BOOL isbefore, BOOL ispause)
{
   // We're still called for instructions in the same basic block as ROI end, ignore these
   if (!thread_data[threadid].output)
   {
      thread_data[threadid].num_dyn_addresses = 0;
      return;
   }

   // Reconstruct basic blocks (we could ask Pin, but do it the same way as TraceThread will do it)
   if (thread_data[threadid].bbv_end || thread_data[threadid].bbv_last != addr)
   {
      // We're the start of a new basic block
      thread_data[threadid].bbv->count(thread_data[threadid].bbv_base, thread_data[threadid].bbv_count);
      thread_data[threadid].bbv_base = addr;
      thread_data[threadid].bbv_count = 0;
   }
   thread_data[threadid].bbv_count++;
   thread_data[threadid].bbv_last = addr + size;
   // Force BBV end on non-taken branches
   thread_data[threadid].bbv_end = is_branch;
}
//
VOID insertCall(INS ins, IPOINT ipoint,  BOOL is_branch, BOOL taken)
{
   INS_InsertCall(ins, ipoint,
      AFUNPTR(sendInstruction),
      IARG_THREAD_ID,
      IARG_ADDRINT, INS_Address(ins),
      IARG_UINT32, UINT32(INS_Size(ins)),
      IARG_BOOL, is_branch,
      IARG_BOOL, taken,
      IARG_BOOL, INS_IsPredicated(ins),
      IARG_EXECUTING,
      IARG_BOOL, ipoint == IPOINT_BEFORE,
      IARG_BOOL, INS_Opcode(ins) == XED_ICLASS_PAUSE,
      IARG_END);
}


static VOID traceCallback(TRACE trace, void *v) {

      if (current_mode != Sift::ModeDetailed) {
        BBL bbl_head = TRACE_BblHead(trace);
        for (BBL bbl = bbl_head; BBL_Valid(bbl); bbl = BBL_Next(bbl))
         for(INS ins = BBL_InsHead(bbl); ; ins = INS_Next(ins))
         {
            // For memory instructions, collect all addresses at IPOINT_BEFORE

            bool is_branch = INS_IsBranch(ins) && INS_HasFallThrough(ins);

            if (is_branch && INS_IsValidForIpointTakenBranch(ins) && INS_IsValidForIpointAfter(ins))
            {
               insertCall(ins, IPOINT_AFTER,         true  /* is_branch */, false /* taken */);
               insertCall(ins, IPOINT_TAKEN_BRANCH,  true  /* is_branch */, true  /* taken */);
            }
            else
            {
               // Whenever possible, use IPOINT_AFTER as this allows us to process addresses after the application has used them.
               // This ensures that their logical to physical mapping has been set up.
               insertCall(ins, INS_IsValidForIpointAfter(ins) ? IPOINT_AFTER : IPOINT_BEFORE,  false /* is_branch */, false /* taken */);
            }

            if (ins == BBL_InsTail(bbl))
               break;
         }
      }
}









    class Key 
    {
        public:
        ADDRINT pc;
        Key()=default;
        ~Key()=default;
        Key( ADDRINT pc_para ) : pc(pc_para)
        {
        }
	    bool operator <(const Key & rhs) const
    	{
        	return pc < rhs.pc;
	    }
	    bool operator !=(const Key & rhs) const
    	{
        	return pc != rhs.pc;
	    }

    };
    enum class DataLevel: uint32_t {
        Unknown = 0,
        FromSelf = 1,
        FromLocal = 2, // from prior barrier
        FromGlobal = 3, 
        FromReClustering = 4,
        FromOtherCluster = 5,
        FromGlobalAvg = 6,
    };

    class Feature {
        public:
            uint64_t fs;
            double ipc;
            double Freq = 2.66;
            uint64_t max_ins_num;
            uint64_t ins_nums[MAX_NUM_THREADS_DEFAULT];
            uint64_t  cluster_id;
            uint32_t idx;
            ADDRINT pc;
            DataLevel feature_level;
            bool solved;
            std::string toJson() {
                std::stringstream out;
                out << " { \n";
                out << "\"max_ins_num\" : " << max_ins_num << " , \n";
                out << "\"fs\" : " << fs << " , \n";
                out << "\"clusterid\" : " << cluster_id << " , \n";
                out << "\"pc\" : " << pc << " , \n";
                out << "\"feature_level\" : " << (int)feature_level << " \n";
                out << " }";
                return out.str();
            }


            Feature( uint64_t fs_, uint64_t max_ins_num_ ): fs(fs_),max_ins_num(max_ins_num_){

                ipc = max_ins_num * 1e5 / (fs/Freq);
            }

            Feature( uint64_t fs_, const std::vector<uint64_t> & insnums ):
            fs(fs_), solved(true) //use for detailed mode
            {
                feature_level = DataLevel::FromSelf; 
                max_ins_num = 0;
                for( uint32_t i = 0 ; i < MAX_NUM_THREADS_DEFAULT ; i++ ) {
                    ins_nums[i] = insnums[i];
//                    max_ins_num = std::max(max_ins_num,ins_nums[i]);
                    max_ins_num = max_ins_num + ins_nums[i];
                }
                ipc = max_ins_num * 1e5 / (fs/Freq);
            }
            Feature(): fs(0), feature_level( DataLevel::Unknown ), solved(false){
             
            }
            void extrapolate( const Feature * detailed ,DataLevel feature_level_in = DataLevel::FromLocal) {
                solved = true;
                double fs_double = (double)detailed->fs  / (double) detailed->max_ins_num * max_ins_num;
                fs = (uint64_t) fs_double;
                ipc = max_ins_num * 1e5 / (fs/Freq);
                feature_level = feature_level_in; 
            }
            void set_cluster_id( uint64_t cluster_id_) {
                cluster_id = cluster_id_;
            }
            Feature(  const std::vector<uint64_t> & insnums ): solved(false){
                 max_ins_num = 0;
                 for( uint32_t i = 0 ; i < MAX_NUM_THREADS_DEFAULT ; i++ ) {
                    ins_nums[i] = insnums[i];
                    max_ins_num = max_ins_num + ins_nums[i];
                }
                fs = 0;
            }
            void add( const Feature * l ) {
                fs += l->fs;
                max_ins_num += l->max_ins_num;
                ipc = max_ins_num * 1e5 / (fs/Freq);
            }
    };
    std::ostream & operator<<( std::ostream & out, const Feature & feature ) {
        out << "fs : " << feature.fs/1e15 << " pc: " << feature.pc << " insnum: " << feature.max_ins_num << "clusterid: " << feature.cluster_id << " ipc:" << feature.ipc  << " solved " << feature.solved;
        return out;
    }
//std::vector<uint64_t> no_pointer;
class Predictor { 
        int32_t history_queue_size = 0;
//        std::unordered_map<uint64_t, uint64_t> hash2clusterid;//use history to predict cluster id
        std::unordered_map<uint64_t, Feature*> clusterid2feature;//use history to predict cluster id
        std::unordered_map<uint64_t, int> unknown_clusterid;//use history to predict cluster id
        std::unordered_map<uint64_t, uint32_t> clusterid2number;//use history to predict cluster id
        uint64_t most_frequent_cluster_id;
        uint64_t max_num_clusterid = 0;
        std::vector<uint32_t> idxes_in_historyqueue;
        uint64_t last_predicted_cluster_id=0;
        bool first_update = true;
        std::vector<uint64_t> & cluster_id_queue;
        mtng::SimMode & mtng_current_mode;
    public:
//            Predictor()= delete;
//
            Predictor( std::vector<uint64_t>& cluster_id_queue_para, mtng::SimMode & mtng_current_mode_para  ): cluster_id_queue(cluster_id_queue_para), mtng_current_mode(mtng_current_mode_para) {
            };
//            Predictor():cluster_id_queue( no_pointer ){
//            };
//            Predictor & operator=(const Predictor & predictor ) {
//                this->cluster_id_queue = predictor.cluster_id_queue;
//                return *this;
//            }
            
            bool try2FixFeature( Feature *  feature , DataLevel datalevel=DataLevel::FromLocal,uint64_t old_cluster_id = -1) {
                uint64_t cluster_id = feature->cluster_id;
                auto clusterid2feature_it =  clusterid2feature.find(cluster_id);
                if(clusterid2feature_it == clusterid2feature.end()) return false;
                feature->extrapolate( clusterid2feature_it->second , datalevel);
                if(old_cluster_id != (uint64_t)-1) {
                
                }
                return true;
            }
uint64_t maxFreq( uint64_t *arr, int n) {
    //using moore's voting algorithm
    int res = 0;
    int count = 1;
    for(int i = 1; i < n; i++) {
        if(arr[i] == arr[res]) {
            count++;
        } else {
            count--;
        }
         
        if(count == 0) {
            res = i;
            count = 1;
        }
         
    }
    
    return arr[res];
}
            
            uint32_t get_similar_size( int32_t idx1, int32_t idx2 ) {
            
//                int32_t global_idx = cluster_id_queue.size()-1;
                uint32_t length=0;
                while( idx1 >= 0 && idx2>=0 ) {
                    if(cluster_id_queue[idx1] == cluster_id_queue[idx2]) {
                        length++;
                        idx1--;
                        idx2--;
                    } else {
                        break;
                    }
                }
                return length;
            }
            uint64_t get_cluster_id_from_history() {
                if(cluster_id_queue.size()<=0) return -1; 
                
                int32_t cur_idx = cluster_id_queue.size()-1;
                uint32_t max_length=0;     
                std::vector<uint32_t> potential_result;
                potential_result.reserve( idxes_in_historyqueue.size() );
                for( auto elem : idxes_in_historyqueue) { // depth first search
                    auto tmp_length = get_similar_size( (int32_t)elem-1, cur_idx );
                    if(tmp_length > max_length) {
                        max_length = tmp_length;
                        potential_result.clear();
                        potential_result.push_back( elem );
                        //result_idx = elem;
                    } else if (tmp_length == max_length) {
                        
                        potential_result.push_back( elem );
                    }
                }
                if(max_length>0) {

                    //uint32_t result_idx;
                    std::vector<uint64_t > cluster_vec; 
                    cluster_vec.reserve( potential_result.size() );
                    for( auto elem : potential_result) {
                        cluster_vec.push_back( cluster_id_queue[elem] );
                    }
    				uint64_t cluster_id = maxFreq(&cluster_vec[0],cluster_vec.size());
                    uint32_t num = 0;
                    for( auto elem : cluster_vec ) {
                        if(elem == cluster_id ) {
                            num++;
                        }
                    }
                    if( num >= cluster_vec.size()/2 ) {
                    //uint64_t cluster_id = cluster_id_queue[ result_idx ];

                        return cluster_id;
                    } else {
                        return -1;
                    }
                } else {
                    return -1;
                }
            }


            std::pair<mtng::SimMode,uint32_t> decideNextSimMode( ) {
                uint64_t cluster_id = 0;
                uint64_t hash = 0;
                mtng::SimMode next_mode;
                if( unknown_clusterid.size() == 0 ) { // only have one cluster
                    cluster_id = last_predicted_cluster_id;
                    
                    next_mode =  mtng::SimMode::FastForwardMode;
                    return std::make_pair<mtng::SimMode,uint32_t>( next_mode,cluster_id );
                } else {
                    cluster_id = get_cluster_id_from_history();

                    if( cluster_id == (uint64_t)-1 ) {
                        cluster_id = most_frequent_cluster_id;
                    }

                }
                LOG2(INFO) << "predict cluster id : " << cluster_id << " "<<hash <<" " << history_queue_size << "\n";
                //last_predicted_cluster_id = cluster_id;
                auto clusterid2feature_it = clusterid2feature.find(cluster_id);
                
                if( clusterid2feature_it == clusterid2feature.end()  ) {
                    next_mode = mtng::SimMode::DetailMode;
                } else {
                    next_mode =  mtng::SimMode::FastForwardMode;
                }
                return std::make_pair<mtng::SimMode,uint32_t>( next_mode,cluster_id );
            }
            bool if_predict_right( uint64_t cluster_id ) {
                if(first_update) {
                    first_update = false;
                    return true;
                } 
                if( last_predicted_cluster_id == cluster_id ) {
                    return true;
                } else {
                    return false;
                }
            }
            uint64_t get_hash_value( int32_t idx ) {
                uint64_t seed = 0;
                int32_t j = 0; 
                for( int i = idx; j < history_queue_size ; i--, j++ ) {
                    auto value = i >= 0 ? cluster_id_queue[ i ] : -1;
                    seed ^= std::hash<uint64_t>()( value ) + 0x9e3779b9 + (seed << 6) + (seed >> 2); 
                }
                
                return seed;
            }
#define MAX_HISTORY_QUEUE (16)
            void onlyUpdateClusterFeature( uint64_t cluster_id,  Feature * feature ) { // give up if exiting cluster id
               auto cluster_it = clusterid2feature.find(cluster_id);
               if( cluster_it == clusterid2feature.end() ) {
                   clusterid2feature[cluster_id] = feature;
               }
            }

            void updateCurPredictor( uint64_t cluster_id,  Feature * feature ) {
                //update feature data
                idxes_in_historyqueue.push_back( cluster_id_queue.size()-1 );
                assert( cluster_id_queue[idxes_in_historyqueue[idxes_in_historyqueue.size()-1]] ==cluster_id );
                if( mtng_current_mode == mtng::SimMode::DetailMode ) {
                    auto cluster_it = clusterid2feature.find(cluster_id);
                    if( cluster_it == clusterid2feature.end() ) {
                        clusterid2feature[cluster_id] = feature;
                    } else {
                        cluster_it->second->add(feature);
                    }
                    auto unknowncluster_it = unknown_clusterid.find(cluster_id);
                    if( unknowncluster_it != unknown_clusterid.end() ) {
                        unknown_clusterid.erase(unknowncluster_it);
                    }

                } else {

                    auto cluster_it = clusterid2feature.find(cluster_id);
                    
                    if( cluster_it == clusterid2feature.end() ) { //find unknown cluster id, enable history queue to find that
                        auto unknowncluster_it = unknown_clusterid.find(cluster_id);
                        if( unknowncluster_it == unknown_clusterid.end() ) {
                            unknown_clusterid[ cluster_id ] = 1;
                        } else {
                            unknowncluster_it->second++;
                        }
                    }
                }
                auto clusterid2number_it = clusterid2number.find(cluster_id);
                if(clusterid2number_it == clusterid2number.end() ) {
                    clusterid2number[cluster_id] = 1;
                    if(max_num_clusterid == 0) {
                        most_frequent_cluster_id = cluster_id;
                    }
                } else {
                    auto & num = clusterid2number[cluster_id] ;
                    num++;
                    if(num > max_num_clusterid) {
                        max_num_clusterid = num;
                        most_frequent_cluster_id = cluster_id; 
                    }
                }
                //update predictor
                if( ! if_predict_right( cluster_id ) ) {
                    LOG2(INFO) << "predict : " << last_predicted_cluster_id << " actual cluster id : " << cluster_id << "\n"; 
                    //update_predictor_size(); 
                } else {
                    
                    LOG2(INFO) << "predict success : " <<  cluster_id << "\n"; 
                }

                last_predicted_cluster_id = cluster_id;
            }
    };

    class OMPFeature : public Feature {
        std::vector<Feature*> feature_intra_barrier;
        OMPFeature( std::vector<Feature *> intra_barrier_features ): feature_intra_barrier(intra_barrier_features) {
        
        }
    };

class Mtng;
class OMPPredictor:public Predictor{
    public:
    std::vector<Feature*> unsolved_feature;
    bool is_easy_omp_region() {
        return unsolved_feature.size() == 0 ;
    }
    void Update( std::vector<Feature*> & inter_features ) {
        for( auto elem : inter_features ) {
            if( ! elem->solved ) {
                unsolved_feature.push_back(elem);
            }
        }
    }
    OMPPredictor( std::vector<uint64_t> & cluster_id_queue_para, mtng::SimMode &mtng_current_mode_para ) : Predictor( cluster_id_queue_para, mtng_current_mode_para  ) {
    }
    void RepairData( Mtng * mtng_ptr );

};
    class Mtng
    {
        const uint32_t maximum_thread_num = MAX_NUM_THREADS_DEFAULT;
//        std::unordered_map< Key , bool > pc_record;
        std::map< Key , bool > pc_record;
//        std::vector<Record> record_vec;
        uint64_t prior_key;
        std::vector<ADDRINT> prior_call_pc_vec;
        std::vector<uint64_t> ins_num_vec;
        std::vector<uint64_t> ins_num_interval_vec;
        std::vector<uint64_t> bbv_ins_num_vec;
        std::vector<uint64_t> bbv_ins_interval_num_vec;
        uint64_t bbv_ins_interval_num_sum;
        mtng::SimMode mtng_current_mode = mtng::SimMode::DetailMode;
        std::vector<int> detect_unknow_block_in_fast_forward_mode;
        bool record_unknow_enable; 
        const uint64_t threshold;
        const uint64_t minimum_threshold;
//        const static uint32_t threadshold = 1000000;
        uint64_t next_target = 0;
        std::vector<uint64_t> interval_ins_num_vec;
        PIN_LOCK intra_lock;
        bool enter_lock = false;
        ADDRINT region_begin_pc;
        ADDRINT region_end_pc;
        ADDRINT omp_region_begin_pc;
        ADDRINT omp_region_end_pc;
        Predictor * last_intraomp_predictor;
        OMPPredictor * last_omp_predictor;
        bool easy_omp_region;
        uint64_t fs;
        uint64_t prior_cluster_id;
        uint64_t prior_fs;
        uint64_t current_barrier;
        uint64_t next_pc_if_not_branch;
        bool is_branch_inst; 
        double time_sum;
        std::vector<std::unordered_map< uint64_t , uint32_t>> intrabarrier_bbvs;
        std::vector<std::unordered_map<uint64_t, uint32_t>> record_unknown_bbvs;
        uint64_t intra_id = 0;
        std::unordered_map<uint64_t,Predictor*> hash2predictor;
        std::unordered_map<uint64_t,OMPPredictor*> hash2omppredictor;
        /////////unknown cluster
        std::unordered_map<uint64_t, int> unknown_clusters;
        std::unordered_map<uint64_t, Feature*> cluster2feature;
        std::vector<uint64_t> cluster_id_queue;
        std::vector<uint32_t>fast_forward_sync_with_backend;
#define queue_size 1
        ///output
        std::vector<Feature*> feature_stack;
        std::vector<Feature*> feature_stack_interbarrier;
        std::vector< OMPFeature* > feature_stack_interbarrier_vec;
        Feature *feature;
        public:
        
        void repairClusterIdQueue( uint32_t idx  , uint64_t cluster_id) {
            cluster_id_queue[idx] = cluster_id;
        }
        uint32_t max_thread_num;
        std::string toJson() {
            std::stringstream out ;
            //////////feature stack
            out << "{";
            out << "\"time_sum\": " << time_sum << ",\n";
            out << "\"feature_stack\" : " << "\n";
            out << "[ \n";
            if(feature_stack.size() > 0) {
                for( uint32_t i = 0 ; i < feature_stack.size() - 1; i++ ) {
                    auto elem = feature_stack[i];
                    out << elem->toJson() << ",\n";
                }
                out << feature_stack[feature_stack.size()-1]->toJson() << "\n";
            }

            out << "] \n";
            
            out << "}";
            return out.str();
        }
        std::vector<int> thread_active_vec;
        Mtng() : threshold(KnobSampledRegionSize.Value() * 1000),minimum_threshold(KnobMinimumSampledRegionSize.Value()*1000),current_barrier(0)  {
            std::cout << "Sampled Region Size : " << threshold << std::endl;

            std::cout << "Minimum Sampled Region Size : " << minimum_threshold << std::endl;
            prior_cluster_id = 0;
            prior_key = 0;
            region_begin_pc = 0;
            region_end_pc = 0;
            omp_region_begin_pc = 0;
            omp_region_end_pc = 0;
            last_intraomp_predictor = NULL;
            last_omp_predictor = NULL;
            easy_omp_region = false;

            max_thread_num = 1;
            prior_call_pc_vec.resize( maximum_thread_num,0 ); 
            ins_num_vec.resize( maximum_thread_num,0 );
            detect_unknow_block_in_fast_forward_mode.resize( maximum_thread_num,0 );
            record_unknow_enable = false;
            next_pc_if_not_branch = 0;
            is_branch_inst = false;
            record_unknown_bbvs.resize(maximum_thread_num);

            ins_num_interval_vec.resize( maximum_thread_num,0 );
            bbv_ins_num_vec.resize( maximum_thread_num,0 );
            bbv_ins_interval_num_vec.resize( maximum_thread_num,0 );
            interval_ins_num_vec.resize( maximum_thread_num,0 );
            thread_active_vec.resize( maximum_thread_num,0 );
            intrabarrier_bbvs.resize(maximum_thread_num);

            fast_forward_sync_with_backend.resize(maximum_thread_num,0);

            hash2predictor[region_begin_pc] = new Predictor( cluster_id_queue,mtng_current_mode );
            PIN_InitLock( & intra_lock );
            enter_lock = false;
            next_target = threshold;
            intra_id =0;
        }

    void intraBarrierInstruction( THREADID tid, ADDRINT addr ) {

            if(tid!=0) return;
                //detectUnknownBB( tid, 0 , addr) ;
                if( interval_ins_num_vec[tid] > next_target ) {
//                    PIN_GetLock( &intra_lock, tid ) ;
//                    enter_lock = true;          
                    auto ret = processRegion<mtng::RecordType::IntraBarrier>( addr ); 

//                    enter_lock = false;          
  //                  PIN_ReleaseLock(&intra_lock);
                    
                    changeSimMode( ret);
                }

    }



        uint64_t get_insnum() {
            uint64_t insnum_sum = 0;
            for(uint32_t ti = 0 ; ti < maximum_thread_num ; ti++ ) {
                uint64_t insnum = 0 ;
//              if(thread_data[ti].bbv != NULL && thread_data[ti].running) 
//              {
//                    insnum = thread_data[master_thread_id].output->Magic(SIM_CMD_GET_INS_NUM, ti, 0 );
//                    insnum = interval_ins_num_vec[ti] ;
                    insnum = get_bbv_thread_counter(ti);
                                        
                    auto insnum_sum_interval =   insnum - ins_num_vec[ti];                
                    ins_num_interval_vec[ti] = insnum_sum_interval;
                    ins_num_vec[ti] = insnum;
                
                    insnum_sum += insnum_sum_interval;
//                } else {
//                    ins_num_interval_vec[ti] = 0;
//                }
            }
            return insnum_sum;
        }

        Feature * get_ipc() {
            get_insnum();

            fs = thread_data[master_thread_id].output->Magic(SIM_CMD_GET_SIM_TIME, 0, 0 );
            LOG2(INFO) << "time: " <<  fs/1e15 << std::endl;
            uint64_t interval = fs - prior_fs;
            prior_fs = fs;
            return new Feature(  interval,  ins_num_interval_vec );
        }
        void init_ipc() {
            prior_fs = thread_data[master_thread_id].output->Magic(SIM_CMD_GET_SIM_TIME, 0, 0 );
            LOG2(INFO) << "time: " <<  prior_fs/1e15 << std::endl;
        }
        void recordState( ADDRINT addr , uint64_t cluster_id ) {
            if( mtng_current_mode == mtng::SimMode::DetailMode) {
                feature = get_ipc(); 

                updateGlobalInfo( region_begin_pc, cluster_id ); 
            } else {
                get_insnum();
                feature = new Feature(ins_num_interval_vec);
            }
            feature->pc = addr;
            feature->cluster_id = cluster_id;
        }
        
        
        void updateCurPredictor( ADDRINT pcaddr, uint64_t cluster_id ) {
            
            auto  predictor_it = hash2predictor.find(pcaddr); ///different pc has different predictor
            assert(predictor_it!=hash2predictor.end());
            auto & predictor = predictor_it->second;
            cluster_id_queue.push_back(cluster_id);
            predictor->updateCurPredictor( cluster_id, feature );

        }
        void updateGlobalInfo( ADDRINT pcaddr, uint64_t cluster_id ) {

            auto cluster2feature_it = cluster2feature.find( cluster_id ); ///different pc has different predictor
            if( cluster2feature_it == cluster2feature.end() ) {
                cluster2feature[cluster_id] = feature;
            } else {
                cluster2feature_it->second->add(feature);
            }
        }
        std::pair<Feature*,bool> getGlobalInfo( ADDRINT pcaddr, uint64_t cluster_id ) {
            
            auto cluster2feature_it = cluster2feature.find( cluster_id ); ///different pc has different predictor

            if( cluster2feature_it == cluster2feature.end() ) {

                return std::make_pair<Feature*,bool>( NULL, false);
            } else {
                return std::make_pair<Feature*,bool>( cluster2feature_it->second, true );
            }
        }



        void fini( ADDRINT pcaddr ) {
            
            processRegion<mtng::RecordType::EndPgm>(pcaddr);
//            update_sim();
//            RecordType record_type = RecordType::SingleThread;
//            Record record = Record(record_type, SimMode::DetailMode, fs,ins_num_vec,0 );
//            record_vec.push_back(record);
        }
        void fflush() {
//            std::ofstream output ;
//            std::string res_dir = KnobMtngDir.Value();
//            std::string full_path = res_dir + "/" + "mtng.json";
//            
//            output.open( full_path.c_str() , std::ofstream::out );
////            output << RecordtoJson( record_vec );
//            output.close();
        }
        void add_pc( ADDRINT pc_addr ) {
            Key key = Key(pc_addr);
            //pc_record[ key ] = true;
            pc_record.insert( std::pair<Key,bool>(key,true));
        }
        bool has_key( ADDRINT pc_addr ) {
//            std::cerr << pc_addr << " " << prior_call_pc_vec[0] << "\n";
            if( pc_record.find( Key(prior_call_pc_vec[0]) ) == pc_record.end() ) {
                add_pc(prior_call_pc_vec[0]);
                return false;
            } else {
                return true;
            }
        }

        void changeSimMode( mtng::SimMode ret ) {
              if( mtng_current_mode == ret ) {
                    return;
              }
              mtng_current_mode = ret;
              if( ret == mtng::SimMode::FastForwardMode ) {
                   gotoFastForwardMode();
              } else if (ret == mtng::SimMode::DetailMode) {
                   gotoDetailMode();
              }

        }
        void gotoDetailMode() {
            for( uint32_t i = 0 ; i < MAX_NUM_THREADS_DEFAULT ; i++ ) {
                fast_forward_sync_with_backend[ i ] = 1;
            }
            if (thread_data[master_thread_id].icount_reported > 0)
            {
               thread_data[master_thread_id].output->InstructionCount(thread_data[master_thread_id].icount_reported);
               thread_data[master_thread_id].icount_reported = 0;
            }
            fast_forward_sync_with_backend[master_thread_id] = 0;

            setInstrumentationMode( Sift::ModeDetailed );
            thread_data[master_thread_id].output->Magic(SIM_CMD_ROI_START, 0, 0);
            init_ipc();
        }

//        #define ONLY_DETAILED_MODE
        void gotoFastForwardMode() {
            for (uint32_t i = 0 ; i < detect_unknow_block_in_fast_forward_mode.size() ; i++) {
                detect_unknow_block_in_fast_forward_mode[i] = 0;
            }
#ifndef ONLY_DETAILED_MODE
            setInstrumentationMode(Sift::ModeIcount);
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

        }

        UInt64 * updateBBV( ){

            UInt64 * bbv = get_bbv_space();   
            memcpy( bbv, &global_m_bbv_counts[0], MAX_NUM_THREADS_DEFAULT * NUM_BBV * sizeof(uint64_t) );
            return bbv;
        }

        uint64_t getLastRegionClusterID() {
            {
                uint64_t current_ins = get_bbv_thread_counter(0);
                uint64_t ins_interval = current_ins - bbv_ins_num_vec[0];
                if( ins_interval < minimum_threshold ) { // we just has a small region and want to change the simulation mode
                    return SMALL_REGION_LABEL;
                } 
            }

            for(uint32_t ti = 0 ; ti < maximum_thread_num ; ti++ ) {
//                uint64_t current_ins = interval_ins_num_vec[ti];
                //uint64_t current_ins = thread_data[ti].bbv->getInstructionCount();
                uint64_t current_ins = get_bbv_thread_counter(ti);
                
                bbv_ins_num_vec[ti] = current_ins;

            }
//            UInt64 * bbv;
//            bbv = updateBBV(  );
            
            uint64_t cluster_id = add_bbv_intra_bbv( global_m_bbv_counts, bbv_ins_num_vec,max_thread_num, mtng_current_mode );
            return cluster_id;

        }
        mtng::SimMode decideNextSimMode( ADDRINT pcaddr ) {
            ///get predictor
                 
            LOG2(INFO) <<"predict pc : " <<  pcaddr << std::endl;
            mtng::SimMode ret_mode;


            if(!easy_omp_region) {
                auto predictor_it = hash2predictor.find(pcaddr); ///different pc has different predictor
                if( predictor_it == hash2predictor.end() ) { //never saw this pc before; go to detailed mode
                    hash2predictor[pcaddr] = new Predictor( cluster_id_queue, mtng_current_mode);
                    ret_mode = mtng::SimMode::DetailMode;
                } else {
                    Predictor * predictor = predictor_it->second;
                    auto result_mode_cluster = predictor->decideNextSimMode();
                    auto next_mode = result_mode_cluster.first;
                    if( next_mode == mtng::SimMode::DetailMode  ) { //disable use global infomation at first
                        auto result_global_feature_cluster = getGlobalInfo( pcaddr, result_mode_cluster.second);
                        if( result_global_feature_cluster.second ) {
                            next_mode = mtng::SimMode::FastForwardMode;
                            predictor->onlyUpdateClusterFeature( result_mode_cluster.second , result_global_feature_cluster.first );
                        }
                    }
                    ret_mode = next_mode;
                }
            } else {
                ret_mode = mtng::SimMode::FastForwardMode;
            }
            return ret_mode;
        }

        template< mtng::RecordType record_type >
        mtng::SimMode processRegion( ADDRINT pcaddr  ) {

//            Record record;
            uint64_t cluster_id = getLastRegionClusterID();
            if( cluster_id == SMALL_REGION_LABEL) {
                return mtng_current_mode; 
            }

            LOG2(INFO) << record_type << "\n";

            next_target = interval_ins_num_vec[0] + threshold;
            LOG2(INFO) << "goto Analysis" << "\n";
            region_begin_pc = region_end_pc;
            region_end_pc = pcaddr;
            
            LOG2(INFO) << "clusterid:" << cluster_id << "\n";
            recordState( region_begin_pc,cluster_id );         
            feature->idx=feature_stack.size();
            feature_stack.push_back(feature);
            feature_stack_interbarrier.push_back(feature);
            updateCurPredictor( region_begin_pc, cluster_id);

            if(record_type == mtng::RecordType::MultiThread || record_type == mtng::RecordType::SingleThread) {
                LOG2(INFO) << record_type <<  "\n";
                omp_region_begin_pc = omp_region_end_pc;
                omp_region_end_pc = pcaddr;


                if( last_omp_predictor != NULL ) {
                    last_omp_predictor->Update(feature_stack_interbarrier);
                    last_omp_predictor->RepairData( this  );
                } 
                
                auto hash2omppredictor_it = hash2omppredictor.find( omp_region_end_pc );
                OMPPredictor * omp_predictor;
                if( hash2omppredictor_it == hash2omppredictor.end() ) {
                    easy_omp_region = false;
                    omp_predictor = new OMPPredictor( cluster_id_queue,mtng_current_mode );
                    hash2omppredictor[ omp_region_end_pc ] = omp_predictor;
                } else {
                    omp_predictor = hash2omppredictor_it->second ;
                    easy_omp_region = omp_predictor->is_easy_omp_region();
                }
                last_omp_predictor = omp_predictor;
                feature_stack_interbarrier.clear();
                if(easy_omp_region) {
                    LOG2(INFO) << "\033[1;32m[InterBarrier ] easy omp begin\033[0m"
                        << std::endl;

                } else {
                 LOG2(INFO) << "\033[1;31m[InterBarrier   ] hard omp start ""\033[0m\n"
                << std::endl;

                }
            }

            LOG2(INFO) << *feature << "\n";
    
            auto ret = decideNextSimMode(region_end_pc);
            LOG2(INFO) << "next mode:" << ret << "\n";
            return ret;
        }
        bool trySolveFeatureByLocalInfo( Feature * feature, DataLevel datalevel = DataLevel::FromLocal, uint64_t old_cluster_id = -1 ) {
            auto pc = feature->pc;
            auto predictor_it = hash2predictor.find(pc);
            if( predictor_it == hash2predictor.end() ) return false;
            auto & predictor = predictor_it->second;
            return predictor->try2FixFeature( feature,datalevel,old_cluster_id);
        }

        bool trySolveFeatureByGlobalInfo( Feature * feature, DataLevel datalevel=DataLevel::FromGlobal ) {
            auto cluster_id = feature->cluster_id;
            auto find_feature_it = cluster2feature.find( cluster_id );
            if(find_feature_it != cluster2feature.end()) {
                feature->extrapolate( find_feature_it->second, datalevel );
                return true;
            }
            return false;

        }

        bool trySolveFeatureByReclustering( uint32_t index, Feature * feature ) {
            auto old_cluster_id = feature->cluster_id;
            auto freq= feature->Freq;
            auto cluster_id = final_intrabarrier_cluster_id( index, freq, max_thread_num);


            if(cluster_id == (uint64_t)-1) {
                return false;
            }
            feature->cluster_id = cluster_id;
            bool ret = trySolveFeatureByLocalInfo(feature, DataLevel::FromReClustering, old_cluster_id );
            if(ret) {
                return ret;
            }
            ret = trySolveFeatureByGlobalInfo(feature,DataLevel::FromReClustering);
            return ret;
        }


        bool trySolveFeatureByClosedFeature( Feature * feature, Feature * last_feature ) {
            feature->extrapolate( last_feature, DataLevel::FromOtherCluster );
            return true;
        }
        void repairData() {
            time_sum=0;
            Feature * last_large_feature = NULL;
            std::vector<uint32_t> unsolved_feature;
            uint64_t ins_num_sum = 0 ;

            for( uint32_t i = 0 ; i < feature_stack.size() ; i++ ) {
                auto feature = feature_stack[i];
                if(!feature->solved) {
                    bool ret2 = trySolveFeatureByLocalInfo(feature);
                    if(!ret2) {

                        bool ret = trySolveFeatureByGlobalInfo( feature );
                        if(!ret) {

                        ret = trySolveFeatureByReclustering( i,feature );    
                        
                        if(!ret) {
                            if( last_large_feature != NULL) {
                                trySolveFeatureByClosedFeature( feature , last_large_feature );
                            } else {
                                unsolved_feature.push_back( i );
                                continue;
                            }
                        }
                        }
                    }
                }
                if(feature->max_ins_num > minimum_threshold ) {
                    last_large_feature = feature_stack[i];
                }
                time_sum += feature->fs;
                ins_num_sum += feature->max_ins_num;
            }
            Feature average = Feature( time_sum,ins_num_sum);
            for( auto elem : unsolved_feature ) { //these feature are really hard
                feature_stack[elem]->extrapolate( &average, DataLevel::FromGlobalAvg ); 
            }
            std::cout << "time sum: " << time_sum /1e15 << std::endl;
            std::ofstream final_file;
            std::string res_dir = KnobMtngDir.Value();
            std::string full_path = res_dir + "/" + "intra_mtng.json";
            final_file.open( full_path.c_str(),std::ofstream::out );
            final_file << toJson();
            final_file.close();
        }
        static Mtng * toMtng( VOID * v ) {
            Mtng * mtng_ptr = reinterpret_cast<Mtng*>(v); 
            if( mtng_ptr == nullptr ) {
                cerr << "null Failed";
            }
            return mtng_ptr;
        }
        
    };

void OMPPredictor::RepairData( Mtng * mtng_ptr ) {
        int unsolved_idx = 0;
        for( uint32_t i = 0 ; i < unsolved_feature.size() ; i++ ) {
            Feature * feature = unsolved_feature[i];
            if(!feature->solved) {
                bool ret2 = mtng_ptr->trySolveFeatureByLocalInfo(feature);
                if(!ret2) {

                    bool ret = mtng_ptr->trySolveFeatureByGlobalInfo( feature );
                    if(!ret) {

                    ret = mtng_ptr->trySolveFeatureByReclustering( feature->idx,feature);  
                    
                    if(!ret) {
                         unsolved_feature[unsolved_idx] = feature;
                         unsolved_idx++;
                    } else {
                        mtng_ptr->repairClusterIdQueue( feature->idx  , feature->cluster_id);
                    }
                    }
                }
            }
        }
        if( unsolved_idx == 0) {
            unsolved_feature.clear();
        } else{
            unsolved_feature.resize(unsolved_idx);
        }
}


     void recordInsCount( THREADID threadid, uint32_t insnum, ADDRINT pc_addr, VOID * v  ) {
//        if(threadid != 0 ) return;
        Mtng * mtng_ptr = Mtng::toMtng(v);
        mtng_ptr->recordInsCount(threadid, insnum, pc_addr);

    }
   
    void processOmpBegin( THREADID threadid, ADDRINT pc_addr, VOID * v  ) {
        if(threadid!=0) return;
        Mtng * mtng_ptr = Mtng::toMtng(v);
        auto ret = mtng_ptr->processRegion<mtng::RecordType::MultiThread>(pc_addr);
        mtng_ptr->changeSimMode( ret);
//        mtng_ptr->detectUnknownBB( threadid, 0 , pc_addr) ;
       

    }
    void processOmpEnd( THREADID threadid, ADDRINT pc_addr, VOID * v  ) {
        if(threadid!=0) return;
        Mtng * mtng_ptr = Mtng::toMtng(v);
        auto ret = mtng_ptr->processRegion<mtng::RecordType::SingleThread>(pc_addr);

        mtng_ptr->changeSimMode( ret);
    }


    VOID sendIntraBarrierInstruction(THREADID threadid, ADDRINT addr, void * v)
    {
        if(threadid != 0 ) return;
    //void intraBarrierInstruction( THREADID tid, ADDRINT addr, UINT32 size, BOOL is_branch ) {
        Mtng * mtng_ptr = Mtng::toMtng(v);
        mtng_ptr->intraBarrierInstruction(threadid, addr);

    }



VOID
traceCallbackForBarrierPoint(TRACE trace, void* v)
{
  RTN rtn = TRACE_Rtn(trace);

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

            //comments : this is for intra barrier; detect branch instructions
         bool if_insert_intra_barrier = false;
         for(INS ins = BBL_InsHead(bbl); ; ins = INS_Next(ins))
         {

            //bool is_branch = INS_IsBranch(ins) && INS_HasFallThrough(ins);
            bool is_branch = INS_IsBranch(ins) ;

            if (is_branch )
            {
                if_insert_intra_barrier = true;
                break;

       //        insertIntraBarrierRegionCall(ins, IPOINT_BEFORE,    true  /* is_branch */, v /* taken */);
            }

            if (ins == BBL_InsTail(bbl))
               break;
         }
         if( if_insert_intra_barrier) {
             INS_InsertPredicatedCall(tail, IPOINT_BEFORE,
                      AFUNPTR(sendIntraBarrierInstruction),
                      IARG_THREAD_ID,
                      IARG_INST_PTR,
                      IARG_ADDRINT, v,
                      IARG_END);

         }
            //comments: end
      }
  }
  //comments: for small barrier region
  if (RTN_Valid(rtn)    
      && RTN_Address(rtn) == TRACE_Address(trace))
  {
      std::string rtn_name = RTN_Name(rtn).c_str();
      BBL bbl = TRACE_BblHead(trace);
      INS ins = BBL_InsHead(bbl);
#define OMP_FN_BEGIN "_omp_fn"
      if (
           (rtn_name.find( OMP_FN_BEGIN ) != std::string::npos)
         )
      {
                 INS_InsertPredicatedCall( ins, IPOINT_BEFORE, (AFUNPTR)processOmpBegin,
                     IARG_THREAD_ID,
                     IARG_INST_PTR,
                     IARG_ADDRINT, v,
                     IARG_END);
           
      }

  }
  //INS_IsRet
}
VOID
processExit( THREADID threadid, ADDRINT pcaddr, void *v)
    {

        if(threadid!=0) return;
        Mtng * mtng_ptr = Mtng::toMtng( v );
        mtng_ptr->fini(pcaddr);
        
        mtng_ptr->repairData();
        mtng_ptr->fflush();
    }
VOID Fini( INT32 code,void *v) {
    
        Mtng * mtng_ptr = Mtng::toMtng( v );
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
    VOID threadEnd(THREADID threadid, const CONTEXT *ctxt, INT32 flags, VOID *v) {

        Mtng * mtng_ptr = Mtng::toMtng( v );
        mtng_ptr->thread_active_vec[threadid] = 0;
        for( uint32_t i = 0 ; i < MAX_NUM_THREADS_DEFAULT ; i++ ) {
            if(mtng_ptr->thread_active_vec[i] ) {
                master_thread_id = i;
                break;
            }
        }
         std::cerr << "thread " << threadid << " master id: " << master_thread_id << " end\n " ;
    }
    VOID threadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v) {
        Mtng * mtng_ptr = Mtng::toMtng( v );
        mtng_ptr->thread_active_vec[threadid] = 1;
        if(threadid > mtng_ptr->max_thread_num - 1) {
            mtng_ptr->max_thread_num = threadid + 1;
        }
        for( uint32_t i = 0 ; i < MAX_NUM_THREADS_DEFAULT ; i++ ) {
            if(mtng_ptr->thread_active_vec[i] ) {
                master_thread_id = i;
                break;
            }
        }

        std::cerr << "thread " << threadid << " master id: " << master_thread_id << " begin\n " ;

    }


    void activate() {
        Mtng *  mtng_global = new Mtng();

        mtng_global->init_ipc();
        TRACE_AddInstrumentFunction(traceCallback, 0);
    //    TRACE_AddInstrumentFunction(detectUnknownBlock, mtng_global);
//        RTN_AddInstrumentFunction(routineCallback, 0);
        TRACE_AddInstrumentFunction(traceCallbackForBarrierPoint, (void*)mtng_global ); 
        PIN_AddFiniFunction(Fini, (void*)mtng_global);

        RTN_AddInstrumentFunction(routineCallback, mtng_global);
        PIN_AddThreadStartFunction(threadStart, mtng_global);
        PIN_AddThreadFiniFunction(threadEnd, mtng_global);

    }

};
