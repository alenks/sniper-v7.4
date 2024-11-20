#ifndef MAGIC_SERVER_H
#define MAGIC_SERVER_H

#include "fixed_types.h"
#include "progress.h"
#include "bbv_count.h"
#include <vector>
#define MAX_NUM_THREADS 256
class MagicServer
{
   public:
      // data type to hold arguments in a HOOK_MAGIC_MARKER callback
      struct MagicMarkerType {
         thread_id_t thread_id;
         core_id_t core_id;
         UInt64 arg0, arg1;
         const char* str;
      };

      // BBV information 
      std::vector<std::vector<UInt64>> intraBbvLists[MAX_NUM_THREADS];
      std::vector<UInt64> intraBbvIdxLists[MAX_NUM_THREADS];
      std::vector<UInt64> intraprevBbvs[MAX_NUM_THREADS];

      std::vector<std::vector<UInt64>> interBbvLists[MAX_NUM_THREADS];
      std::vector<UInt64> interBbvIdxLists[MAX_NUM_THREADS];
      std::vector<UInt64> interprevBbvs[MAX_NUM_THREADS];

//      std::vector<UInt64> bbvs[MAX_NUM_THREADS];

      MagicServer();
      ~MagicServer();

      UInt64 Magic(thread_id_t thread_id, core_id_t core_id, UInt64 cmd, UInt64 arg0, UInt64 arg1);
      bool inROI(void) const { return m_performance_enabled; }
      static UInt64 getGlobalInstructionCount(void);

      // To be called while holding the thread manager lock
      UInt64 Magic_unlocked(thread_id_t thread_id, core_id_t core_id, UInt64 cmd, UInt64 arg0, UInt64 arg1);
      UInt64 setFrequency(UInt64 core_number, UInt64 freq_in_mhz);
      UInt64 getFrequency(UInt64 core_number);

      void enablePerformance();
      void disablePerformance();
      UInt64 setPerformance(bool enabled);

      UInt64 setInstrumentationMode(UInt64 sim_api_opt);

      void setProgress(float progress) { m_progress.setProgress(progress); }

   private:
      bool m_performance_enabled;
      Progress m_progress;
};

#endif // SYNC_SERVER_H
