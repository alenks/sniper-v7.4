#include "hooks_manager.h"
#include "log.h"

const char* HookType::hook_type_names[] = {
   "HOOK_PERIODIC",
   "HOOK_PERIODIC_INS",
   "HOOK_SIM_START",
   "HOOK_SIM_END",
   "HOOK_ROI_BEGIN",
   "HOOK_ROI_END",
   "HOOK_CPUFREQ_CHANGE",
   "HOOK_MAGIC_MARKER",
   "HOOK_MAGIC_USER",
   "HOOK_INSTR_COUNT",
   "HOOK_THREAD_CREATE",
   "HOOK_THREAD_START",
   "HOOK_THREAD_EXIT",
   "HOOK_THREAD_STALL",
   "HOOK_THREAD_RESUME",
   "HOOK_THREAD_MIGRATE",
   "HOOK_INSTRUMENT_MODE",
   "HOOK_PRE_STAT_WRITE",
   "HOOK_SYSCALL_ENTER",
   "HOOK_SYSCALL_EXIT",
   "HOOK_APPLICATION_START",
   "HOOK_APPLICATION_EXIT",
   "HOOK_APPLICATION_ROI_BEGIN",
   "HOOK_APPLICATION_ROI_END",
   "HOOK_SIGUSR1",
   "HOOK_ATOMIC_BEGIN",
   "HOOK_ATOMIC_END",
   "HOOK_APPLICATION_ATOMIC_BEGIN",
   "HOOK_APPLICATION_ATOMIC_END",
   "HOOK_APPLICATION_BARRIER_BEGIN",
   "HOOK_APPLICATION_BARRIER_END",
   "HOOK_APPLICATION_OMP_BEGIN",
   "HOOK_APPLICATION_OMP_END",
   "HOOK_UNKNOWN_TYPE",
};
static_assert((HookType::HOOK_TYPES_MAX+1) == (sizeof(HookType::hook_type_names) / sizeof(HookType::hook_type_names[0]) ),
              "Not enough values in HookType::hook_type_names");

const char *HookType::getHookName(hook_type_t hook_type)
{
   if (hook_type >= HookType::HOOK_TYPES_MAX)
   {
      return hook_type_names[HookType::HOOK_TYPES_MAX];
   }
   else
   {
      return hook_type_names[hook_type];
   }
}

HooksManager::HooksManager()
{
}

void HooksManager::registerHook(HookType::hook_type_t type, HookCallbackFunc func, UInt64 argument, HookCallbackOrder order)
{
   m_registry[type].push_back(HookCallback(func, argument, order));
}

SInt64 HooksManager::callHooks(HookType::hook_type_t type, UInt64 arg, bool expect_return, std::vector<SInt64> *return_values)
{

   for(unsigned int order = 0; order < NUM_HOOK_ORDER; ++order)
   {
      for(std::vector<HookCallback>::iterator it = m_registry[type].begin(); it != m_registry[type].end(); ++it)
      {
         if (it->order == (HookCallbackOrder)order)
         {
            SInt64 result = it->func(it->arg, arg);
            if (return_values)
               return_values->push_back(result);
            if (expect_return && result != -1)
               return result;
         }
      }
   }

   return -1;
}
