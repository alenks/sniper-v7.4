#include "magic_server.h"
#include "sim_api.h"
#include "simulator.h"
#include "thread_manager.h"
#include "logmem.h"
#include "performance_model.h"
#include "fastforward_performance_model.h"
#include "core_manager.h"
#include "dvfs_manager.h"
#include "hooks_manager.h"
#include "stats.h"
#include "timer.h"
#include "thread.h"
#include <cstdlib> // abs
#include <limits.h>
#include "bbv_count.h"
#define THRESHOLD_DISTANCE 10000000000
#define CMP_NUM_REGIONS 2

MagicServer::MagicServer()
      : m_performance_enabled(false)
{
}

MagicServer::~MagicServer()
{ 
    // std::cerr << "\n [MAGIC SERVER] BBV:";
    // for (UInt64 i = 0; i < bbvList[0].size(); i++)
        // std::cerr << " " << bbvList.back()[i] + prevBbv[i];
    // std::cerr << "\n";

    // for (UInt64 i = 0; i < bbvList.size(); i++) {
        // std::cerr << "[MAGIC SERVER] BBV list (" << i << "): "; 
        // for (UInt64 j = 0; j < bbvList[i].size(); j++)
            // std::cerr << bbvList[i][j] << " ";
        // std::cerr << "\n";
    // }

//    std::cerr << "\n" << bbvIdxList.size() << " out of " << bbvList.size() << " simulated in detail\n\n" << "\nBBV Index List:";
//    for (UInt64 i = 0; i < bbvIdxList.size(); i++)
//       std::cerr << " " << bbvIdxList[i];
//    std::cerr << "\n\n"; 
}

UInt64 MagicServer::Magic(thread_id_t thread_id, core_id_t core_id, UInt64 cmd, UInt64 arg0, UInt64 arg1)
{
   ScopedLock sl(Sim()->getThreadManager()->getLock());

   return Magic_unlocked(thread_id, core_id, cmd, arg0, arg1);
}


UInt64 get_cluster_id( std::vector<UInt64> & prevBbv, 
                       std::vector<UInt64> & bbv,
                       std::vector<UInt64> & bbvIdxList,
                       std::vector<std::vector<UInt64>> & bbvList
        ) {
            UInt64 ret = 0;
//                // Update previously recorded BBV
//                if (!bbv.empty()) {
//                   if (prevBbv.empty()) {
//                      for (UInt64 i = 0; i < bbv.size(); i++)
//                         prevBbv.push_back(bbv[i]);
//                   }
//                   else {
//                      for (UInt64 i = 0; i < bbv.size(); i++)
//                         prevBbv[i] = bbv[i];
//                   }
//                }
//                
//                // Get BBV (i.e. m_bbv_counts_abs vector)
////                bbv = bbvCount->getBbvList();
//
//                // Update inter-barrier regions' BBV list
//                bbvList.push_back(bbv);
//
//                // Get relative BBV for the current region
//                // (i.e. BBV obtained - BBV obtained until the previous inter-barrier region)
//                if (!bbvList.empty() && !prevBbv.empty()) {
//                    for (UInt64 i = 0; i < bbv.size(); i++) { 
//                       bbvList.back()[i] = bbv[i] - prevBbv[i];
//                       std::cout << bbvList.back()[i] <<  " " ; 
//                    }
//                    std::cout << "\n";
//                }
//
//                int ret = -1;
//
//                // If current BBV is the first BBV recorded
//                // Add it to bbvIdxList, return BBV index (i.e., 0)
//                if (bbvList.empty()) { 
//                   bbvIdxList.push_back(0);
//                   printf("[MAGIC SERVER] Detailed (First region)\n");
//                   ret = 0;   
//                }
//
//                // Else, check if distance of current BBV is less than or
//                // equal to any previously simulated regions
//                // Compare with CMP_NUM_REGIONS regions
//                else {
//
//                    UInt64 distance = 0;
//                    unsigned long long minDistance = ULLONG_MAX;
//                    int numRegions = 0;
//                    int bbvIdx = 0;
//
//                    // Check distance from BBVs corresponding to each entry in the bbvIdxList
//                    for (UInt64 i = 0; i < bbvIdxList.size(); i++) {
//
//                       distance = 0;
//                       bbvIdx = bbvIdxList[i];
//                       
//                       // Get Manhattan distance 
//                       // (i.e. summation of absolute difference b/w respective coordinates)
//                       for (UInt64 j = 0; j < bbvList[bbvIdx].size(); j++)
//                           distance += abs(bbvList[bbvIdx][j] - bbvList.back()[j]);
//
//                       // printf("BBV Distance %ld\t Minimum distance %llu\t Num Regions Passed: (%d / %d)\n", distance, minDistance, numRegions, i+1);
//
//                       // If distance is less than THRESHOLD_DISTANCE
//                       // Update number of regions compared, minimum distance recorded and BBV index (ret)
//                       if (distance < THRESHOLD_DISTANCE) {
//                           numRegions++;
//                           if (distance < minDistance) {
//                              minDistance = distance;
//                              ret = bbvIdx;
//                           }
//
//                           // If distances have been compared for CMP_NUM_REGIONS, stop
//                           if (numRegions == CMP_NUM_REGIONS)
//                              break;
//                       }
//                    }
//
//                    if (numRegions)
//                       printf("[MAGIC SERVER] Fast-forward (%d regions compared, Minimum distance from BBV %d: %llu)\n", numRegions, ret, minDistance);
//
//                    // If the distance from all of the previously simulated BBVs exceeds THRESHOLD_DISTANCE
//                    // update bbvIdxList, return current BBV index
//                    if (ret == -1) {
//                       bbvIdx = bbvList.size() - 1;
//                       bbvIdxList.push_back(bbvIdx);
//                       printf("[MAGIC SERVER] Detailed (distances from all previously simulated BBVs exceed THRESHOLD_DISTANCE)\n");
//                       ret = bbvIdx;
//                    }
//                    }
        return (UInt64)ret;
}








UInt64 MagicServer::Magic_unlocked(thread_id_t thread_id, core_id_t core_id, UInt64 cmd, UInt64 arg0, UInt64 arg1)
{
   switch(cmd)
   {
      case SIM_CMD_ROI_TOGGLE:
         if (Sim()->getConfig()->getSimulationROI() == Config::ROI_MAGIC)
         {
            return setPerformance(! m_performance_enabled);
         }
         else
         {
            return 0;
         }
      case SIM_CMD_ROI_START:
         Sim()->getHooksManager()->callHooks(HookType::HOOK_APPLICATION_ROI_BEGIN, 0);
         if (Sim()->getConfig()->getSimulationROI() == Config::ROI_MAGIC)
         {
            return setPerformance(true);
         }
         else
         {
            return 0;
         }
      case SIM_CMD_ROI_END:
         Sim()->getHooksManager()->callHooks(HookType::HOOK_APPLICATION_ROI_END, 0);
         if (Sim()->getConfig()->getSimulationROI() == Config::ROI_MAGIC)
         {
            return setPerformance(false);
         }
         else
         {
            return 0;
         }
      case SIM_CMD_MHZ_SET:
         return setFrequency(arg0, arg1);
      case SIM_CMD_NAMED_MARKER:
      {
         char str[256];
         Core *core = Sim()->getCoreManager()->getCoreFromID(core_id);
         core->accessMemory(Core::NONE, Core::READ, arg1, str, 256, Core::MEM_MODELED_NONE);
         str[255] = '\0';

         MagicMarkerType args = { thread_id: thread_id, core_id: core_id, arg0: arg0, arg1: 0, str: str };
         Sim()->getHooksManager()->callHooks(HookType::HOOK_MAGIC_MARKER, (UInt64)&args);
         return 0;
      }
      case SIM_CMD_SET_THREAD_NAME:
      {
         char str[256];
         Core *core = Sim()->getCoreManager()->getCoreFromID(core_id);
         core->accessMemory(Core::NONE, Core::READ, arg0, str, 256, Core::MEM_MODELED_NONE);
         str[255] = '\0';

         Sim()->getStatsManager()->logEvent(StatsManager::EVENT_THREAD_NAME, SubsecondTime::MaxTime(), core_id, thread_id, 0, 0, str);
         Sim()->getThreadManager()->getThreadFromID(thread_id)->setName(str);
         return 0;
      }
      case SIM_CMD_GET_BARRIER_REACHED:
      {
         bool reached = Sim()->getClockSkewMinimizationServer()->onlyMainCoreRunning();
         UInt64 ret = (UInt64)reached;
         Sim()->getClockSkewMinimizationServer()->printState();
         return ret;
      }
      case SIM_CMD_GET_SIM_TIME:
      {
          StatsMetricBase * tmp = Sim()->getStatsManager()->getMetricObject("barrier", 0,"global_time");
          if(tmp == NULL) {
            return 0;
          } else { 
            UInt64 ret = tmp->recordMetric();
            return ret;
          }
//         Core *core = Sim()->getCoreManager()->getCoreFromID(core_id);
//         SubsecondTime curr_elapsed_time = core->getPerformanceModel()->getElapsedTime();
//         return curr_elapsed_time.getFS();
      }
      case SIM_CMD_GET_INS_NUM:
      {
          StatsMetricBase * tmp = Sim()->getStatsManager()->getMetricObject("core", arg0,"instructions");
          if(tmp == NULL) {
            return 0;
          } else {
            
            UInt64 ret = tmp->recordMetric();
            return ret;
          }
      }

      // Enable BBVs
      case SIM_CMD_ENABLE_BBV:
      {
          Sim()->getConfig()->setBBVsEnabled(true);
          return 0;
      }

      // Get BBV cluster IDs
      case SIM_CMD_GET_BBV:
      {

          // Get BBV information from backend
          BbvCount *bbvCount = Sim()->getCoreManager()->getCoreFromID(core_id)->getBbvCount();
          const UInt64 current_barrier_id = arg1;
          const UInt64 thread_id = arg0;
          const std::string bbv_name[] = {"bbv-0","bbv-1","bbv-2","bbv-3","bbv-4","bbv-5","bbv-6","bbv-7","bbv-8","bbv-9","bbv-10","bbv-11","bbv-12","bbv-13","bbv-14","bbv-15" };
//          bbvs[threadid]= std::vector<UInt64>(BbvCount::NUM_BBV);

          std::vector<UInt64> bbv = std::vector<UInt64>(BbvCount::NUM_BBV);

        
          std::cout << "bbv ";
          for (uint32_t i = 0 ; i < BbvCount::NUM_BBV; i++ ) {
              StatsMetricBase * tmp = Sim()->getStatsManager()->getMetricObject("core", thread_id , "bbv-0" );
              UInt64 ret2;
              if(tmp == NULL) {

                std::cout <<  "Empty ";
                ret2 = 0;
              } else {
            
                ret2 = tmp->recordMetric();
            
              }
              bbv[i] = ret2;
              std::cout << ret2 << " ";
          }
          std::cout << "\n";
          UInt64 ret; 
          if( current_barrier_id >= interBbvLists[thread_id].size() ) { // try to get inter barrier id for each thread
                ret = get_cluster_id( interprevBbvs[thread_id], 
                       bbv,
                       interBbvIdxLists[thread_id],
                       interBbvLists[thread_id]);
                intraBbvLists[thread_id].clear();
                intraBbvIdxLists[thread_id].clear();
                get_cluster_id( intraprevBbvs[thread_id], 
                       bbv,
                       intraBbvIdxLists[thread_id],
                       intraBbvLists[thread_id]);
          } {
                ret = get_cluster_id( intraprevBbvs[thread_id], 
                       bbv,
                       intraBbvIdxLists[thread_id],
                       intraBbvLists[thread_id]);
           
          }
          return ret;
      }

      case SIM_CMD_MARKER:
      {
         MagicMarkerType args = { thread_id: thread_id, core_id: core_id, arg0: arg0, arg1: arg1, str: NULL };
         Sim()->getHooksManager()->callHooks(HookType::HOOK_MAGIC_MARKER, (UInt64)&args);
         return 0;
      }
      case SIM_CMD_USER:
      {
         MagicMarkerType args = { thread_id: thread_id, core_id: core_id, arg0: arg0, arg1: arg1, str: NULL };
         return Sim()->getHooksManager()->callHooks(HookType::HOOK_MAGIC_USER, (UInt64)&args, true /* expect return value */);
      }
      case SIM_CMD_INSTRUMENT_MODE:
         return setInstrumentationMode(arg0);
      case SIM_CMD_MHZ_GET:
         return getFrequency(arg0);
      default:
         LOG_ASSERT_ERROR(false, "Got invalid Magic %lu, arg0(%lu) arg1(%lu)", cmd, arg0, arg1);
   }
   return 0;
}

UInt64 MagicServer::getGlobalInstructionCount(void)
{
   UInt64 ninstrs = 0;
   for (UInt32 i = 0; i < Sim()->getConfig()->getApplicationCores(); i++)
      ninstrs += Sim()->getCoreManager()->getCoreFromID(i)->getInstructionCount();
   return ninstrs;
}

static Timer t_start;
UInt64 ninstrs_start;
__attribute__((weak)) void PinDetach(void) {}

void MagicServer::enablePerformance()
{
//   Sim()->getStatsManager()->recordStats("roi-begin");
   ninstrs_start = getGlobalInstructionCount();
   t_start.start();

   Simulator::enablePerformanceModels();
   Sim()->setInstrumentationMode(InstMode::inst_mode_roi, true /* update_barrier */);
}

void MagicServer::disablePerformance()
{
   Simulator::disablePerformanceModels();
 //  Sim()->getStatsManager()->recordStats("roi-end");

   float seconds = t_start.getTime() / 1e9;
   UInt64 ninstrs = getGlobalInstructionCount() - ninstrs_start;
   UInt64 cycles = SubsecondTime::divideRounded(Sim()->getClockSkewMinimizationServer()->getGlobalTime(),
                                                Sim()->getCoreManager()->getCoreFromID(0)->getDvfsDomain()->getPeriod());
   printf("[SNIPER] Simulated %.1fM instructions, %.1fM cycles, %.2f IPC\n",
      ninstrs / 1e6,
      cycles / 1e6,
      float(ninstrs) / (cycles ? cycles : 1));
   printf("[SNIPER] Simulation speed %.1f KIPS (%.1f KIPS / target core - %.1fns/instr)\n",
      ninstrs / seconds / 1e3,
      ninstrs / seconds / 1e3 / Sim()->getConfig()->getApplicationCores(),
      seconds * 1e9 / (float(ninstrs ? ninstrs : 1.) / Sim()->getConfig()->getApplicationCores()));

   PerformanceModel *perf = Sim()->getCoreManager()->getCoreFromID(0)->getPerformanceModel();
   if (perf->getFastforwardPerformanceModel()->getFastforwardedTime() > SubsecondTime::Zero())
   {
      // NOTE: Prints out the non-idle ratio for core 0 only, but it's just indicative anyway
      double ff_ratio = double(perf->getFastforwardPerformanceModel()->getFastforwardedTime().getNS())
                      / double(perf->getNonIdleElapsedTime().getNS());
      double percent_detailed = 100. * (1. - ff_ratio);
      printf("[SNIPER] Sampling: executed %.2f%% of simulated time in detailed mode\n", percent_detailed);
   }

   fflush(NULL);

   Sim()->setInstrumentationMode(InstMode::inst_mode_end, true /* update_barrier */);
   PinDetach();
}

void print_allocations();

UInt64 MagicServer::setPerformance(bool enabled)
{
   if (m_performance_enabled == enabled)
      return 1;

   m_performance_enabled = enabled;

   //static bool enabled = false;
   static Timer t_start;
   //ScopedLock sl(l_alloc);

   if (m_performance_enabled)
   {
      printf("[SNIPER] Enabling performance models\n");
      fflush(NULL);
      t_start.start();
      logmem_enable(true);
      Sim()->getHooksManager()->callHooks(HookType::HOOK_ROI_BEGIN, 0);
   }
   else
   {
      Sim()->getHooksManager()->callHooks(HookType::HOOK_ROI_END, 0);
      printf("[SNIPER] Disabling performance models\n");
      float seconds = t_start.getTime() / 1e9;
      printf("[SNIPER] Leaving ROI after %.2f seconds\n", seconds);
      fflush(NULL);
      logmem_enable(false);
      logmem_write_allocations();
   }

   if (enabled)
      enablePerformance();
   else
      disablePerformance();

   return 0;
}

UInt64 MagicServer::setFrequency(UInt64 core_number, UInt64 freq_in_mhz)
{
   UInt32 num_cores = Sim()->getConfig()->getApplicationCores();
   UInt64 freq_in_hz;
   if (core_number >= num_cores)
      return 1;
   freq_in_hz = 1000000 * freq_in_mhz;

   printf("[SNIPER] Setting frequency for core %" PRId64 " in DVFS domain %d to %" PRId64 " MHz\n", core_number, Sim()->getDvfsManager()->getCoreDomainId(core_number), freq_in_mhz);

   if (freq_in_hz > 0)
      Sim()->getDvfsManager()->setCoreDomain(core_number, ComponentPeriod::fromFreqHz(freq_in_hz));
   else {
      Sim()->getThreadManager()->stallThread_async(core_number, ThreadManager::STALL_BROKEN, SubsecondTime::MaxTime());
      Sim()->getCoreManager()->getCoreFromID(core_number)->setState(Core::BROKEN);
   }

   // First set frequency, then call hooks so hook script can find the new frequency by querying the DVFS manager
   Sim()->getHooksManager()->callHooks(HookType::HOOK_CPUFREQ_CHANGE, core_number);

   return 0;
}

UInt64 MagicServer::getFrequency(UInt64 core_number)
{
   UInt32 num_cores = Sim()->getConfig()->getApplicationCores();
   if (core_number >= num_cores)
      return UINT64_MAX;

   const ComponentPeriod *per = Sim()->getDvfsManager()->getCoreDomain(core_number);
   return per->getPeriodInFreqMHz();
}

UInt64 MagicServer::setInstrumentationMode(UInt64 sim_api_opt)
{
   InstMode::inst_mode_t inst_mode;
   switch (sim_api_opt)
   {
   case SIM_OPT_INSTRUMENT_DETAILED:
      inst_mode = InstMode::DETAILED;
      break;
   case SIM_OPT_INSTRUMENT_WARMUP:
      inst_mode = InstMode::CACHE_ONLY;
      break;
   case SIM_OPT_INSTRUMENT_FASTFORWARD:
      inst_mode = InstMode::FAST_FORWARD;
      break;
   default:
      LOG_PRINT_ERROR("Unexpected magic instrument opt type: %lx.", sim_api_opt);
   }
   Sim()->setInstrumentationMode(inst_mode, true /* update_barrier */);

   return 0;
}
