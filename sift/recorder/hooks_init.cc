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
namespace Hooks {

PIN_LOCK hooks_lock;
HooksManager* hooks_manager;
VOID
handleMagic(THREADID threadid, ADDRINT cmd, ADDRINT arg0, ADDRINT arg1)
{
  PIN_GetLock(&hooks_lock, 0);

  std::cerr << "[SIFT_RECORDER:" << threadid
            << "] Hooks: Seen SIM_CMD type: " << cmd << "\n";

  switch (cmd) {
    case SIM_CMD_ROI_START:
      hooks_manager->callHooks(HookType::HOOK_APPLICATION_ROI_BEGIN, 0);
      hooks_manager->callHooks(HookType::HOOK_ROI_BEGIN, 0);
      break;
    case SIM_CMD_ROI_END:
      hooks_manager->callHooks(HookType::HOOK_APPLICATION_ROI_END, 0);
      hooks_manager->callHooks(HookType::HOOK_ROI_END, 0);
      break;
    case SIM_CMD_ATOMIC_BEGIN:
      // Application hooks always call when this instruction is seen by the
      // front-end. Non-application hooks can be restricted if disabled /
      // redirected
      hooks_manager->callHooks(HookType::HOOK_APPLICATION_ATOMIC_BEGIN, 0);
      hooks_manager->callHooks(HookType::HOOK_ATOMIC_BEGIN, 0);
      break;
    case SIM_CMD_ATOMIC_END:
      // Application hooks always call when this instruction is seen by the
      // front-end. Non-application hooks can be restricted if disabled /
      // redirected
      hooks_manager->callHooks(HookType::HOOK_APPLICATION_ATOMIC_END, 0);
      hooks_manager->callHooks(HookType::HOOK_ATOMIC_END, 0);
      break;
    default:
      std::cerr << "[SIFT_RECORDER:" << threadid
                << "] Hooks: Ignored SIM_CMD type: " << cmd << "\n";
      break;
  }

  PIN_ReleaseLock(&hooks_lock);
}
VOID
callHook(THREADID threadid, ADDRINT _hook, BOOL _update_atomic_count,uint64_t region_id = 0, std::string  region_boundary_type_str="" )
{
  PIN_GetLock(&hooks_lock, 0);
//  HooksManager::RegionType trace_type = string2TraceType( region_boundary_type_str );
  HooksManager::RegionType trace_type = HooksManager::RegionType::PointUndef;
  HookType::hook_type_t hook_type = static_cast<HookType::hook_type_t>(_hook);

  std::cerr << "[SIFT_RECORDER:" << threadid
            << "] Hooks: callHook() about to call HookType: "
            << HookType::getHookName(hook_type) << "\n";
  HooksManager::AtomicBegin args ;
  if( KnobAtomicType.Value()==(uint32_t)AtomicType::AtomicRegion ) {
    args = {
      thread_id : static_cast<thread_id_t>(threadid),
      region_id : region_id,
      region_type : trace_type
    };
  }   
//  std::cerr <<__FILE__ << " "  << __LINE__  << " " << __FUNCTION__ << " " << (int)hook_type << " \n";
  hooks_manager->callHooks(hook_type, reinterpret_cast<UInt64>(&args));

  PIN_ReleaseLock(&hooks_lock);
  

//   std::cerr <<__FILE__ << " "  << __LINE__  << " " << __FUNCTION__ << " "  << " \n";
}
VOID
atomic_traceCallbackForBarrierPoint(TRACE trace, void* v)
{
  RTN rtn = TRACE_Rtn(trace);
  if (RTN_Valid(rtn)    
      && RTN_Address(rtn) == TRACE_Address(trace))
  {
      std::string rtn_name = RTN_Name(rtn).c_str();
      BBL bbl = TRACE_BblHead(trace);
      INS ins = BBL_InsHead(bbl);
      if (
          (
           (rtn_name.find("GOMP_parallel_start") != std::string::npos) ||
           (rtn_name.find("GOMP_parallel") != std::string::npos)
          )
           &&
          !(
           (rtn_name.find("@plt") != std::string::npos) ||
           (rtn_name.find("GOMP_parallel_end") != std::string::npos)
          )
         )
      {
         std::cerr << "[SIFT_RECORDER] Hooks: Found OMP/barrier function " <<
      rtn_name << "\n";

         // For now, let's connect the barriers directly to the atomic start
          INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)callHook,
                                  IARG_THREAD_ID,
                                  IARG_ADDRINT, HookType::HOOK_APPLICATION_BARRIER_BEGIN, IARG_BOOL, false, IARG_END);

         // Let's call atomic, and update the count as needed
         /*
         INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)callHook,
                                  IARG_THREAD_ID,
                                  IARG_ADDRINT,
      HookType::HOOK_APPLICATION_ATOMIC_BEGIN, IARG_BOOL, true, IARG_END);*/
      }

      if (
          (
           (rtn_name.find("GOMP_parallel_end") != std::string::npos)
          )
           &&
          !(
           (rtn_name.find("@plt") != std::string::npos)
          )
         )
      {
         std::cerr << "[SIFT_RECORDER] Hooks: Found OMP/barrier End function " << rtn_name << "\n";

         // For now, let's connect the barriers directly to the atomic start
         INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)callHook,
                                  IARG_THREAD_ID,
                                  IARG_ADDRINT, HookType::HOOK_APPLICATION_BARRIER_END,
                                  IARG_END);
      }
  }

}


VOID
traceCallbackForBarrierPoint(TRACE trace, void* v)
{
  RTN rtn = TRACE_Rtn(trace);
  if (RTN_Valid(rtn)    
      && RTN_Address(rtn) == TRACE_Address(trace))
  {
      std::string rtn_name = RTN_Name(rtn).c_str();
      BBL bbl = TRACE_BblHead(trace);
      INS ins = BBL_InsHead(bbl);
    
      if (
           (rtn_name.find(".omp_fn.") != std::string::npos)
         )
      {
         std::cerr << "[SIFT_RECORDER] Hooks: Found OMP/barrier function " <<
      rtn_name << "\n";

         // For now, let's connect the barriers directly to the atomic start
         INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)callHook,
                                  IARG_THREAD_ID,
                                  IARG_ADDRINT,
                                  HookType::HOOK_APPLICATION_OMP_BEGIN, IARG_BOOL, false, IARG_END);
      }

      if (
          (
           (rtn_name.find("GOMP_parallel_start") != std::string::npos) ||
           (rtn_name.find("GOMP_parallel") != std::string::npos)
          )
           &&
          !(
           (rtn_name.find("@plt") != std::string::npos) ||
           (rtn_name.find("GOMP_parallel_end") != std::string::npos)
          )
         )
      {
         std::cerr << "[SIFT_RECORDER] Hooks: Found OMP/barrier function " <<
      rtn_name << "\n";

         // For now, let's connect the barriers directly to the atomic start
          INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)callHook,
                                  IARG_THREAD_ID,
                                  IARG_ADDRINT, HookType::HOOK_APPLICATION_BARRIER_BEGIN, IARG_BOOL, false, IARG_END);

         // Let's call atomic, and update the count as needed
         /*
         INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)callHook,
                                  IARG_THREAD_ID,
                                  IARG_ADDRINT,
      HookType::HOOK_APPLICATION_ATOMIC_BEGIN, IARG_BOOL, true, IARG_END);*/
      }
/*
      if (
          (
           (rtn_name.find("GOMP_parallel_end") != std::string::npos)
          )
           &&
          !(
           (rtn_name.find("@plt") != std::string::npos)
          )
         )
      {
         std::cerr << "[SIFT_RECORDER] Hooks: Found OMP/barrier End function " << rtn_name << "\n";

         // For now, let's connect the barriers directly to the atomic start
         INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)callHook,
                                  IARG_THREAD_ID,
                                  IARG_ADDRINT, HookType::HOOK_APPLICATION_BARRIER_END,
                                  IARG_END);
      }*/
  }

}
VOID
traceCallbackForHandleMagic(TRACE trace, void* v) {
  BBL bbl_head = TRACE_BblHead(trace);

  for (BBL bbl = bbl_head; BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
      // Simics-style magic instruction: xchg bx, bx
      if (INS_IsXchg(ins) && INS_OperandReg(ins, 0) == REG_BX &&
          INS_OperandReg(ins, 1) == REG_BX) {
        INS_InsertPredicatedCall(ins,
                                 IPOINT_BEFORE,
                                 (AFUNPTR)handleMagic,
                                 IARG_THREAD_ID,
                                 IARG_REG_VALUE,
                                 REG_GAX,
#ifdef TARGET_IA32
                                 IARG_REG_VALUE,
                                 REG_GDX,
#else
                                 IARG_REG_VALUE,
                                 REG_GBX,
#endif
                                 IARG_REG_VALUE,
                                 REG_GCX,
                                 IARG_END);
      }
    }
  }

}


void
hooks_init(HooksManager* _hooks_manager)
{

  PIN_InitLock(&hooks_lock);
  hooks_manager = _hooks_manager;

  if (hooks_manager) {
        TRACE_AddInstrumentFunction(traceCallbackForHandleMagic, 0);
  }
}
};
