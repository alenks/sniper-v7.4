/*
 * This file is covered under the Interval Academic License, see LICENCE.academic
 */

#ifndef __ROB_CONTENTION_SUNNYCOVE_H
#define __ROB_CONTENTION_SUNNYCOVE_H

#include "rob_contention.h"
#include "contention_model.h"
#include "core_model_sunnycove.h"
#include "dynamic_micro_op_sunnycove.h"

#include <vector>

class RobContentionSunnycove : public RobContention {
   private:
      const CoreModel *m_core_model;
      uint64_t m_cache_block_mask;
      ComponentTime m_now;

      // port contention
      bool ports[DynamicMicroOpSunnycove::UOP_PORT_SIZE];
      int ports_generic0156;
      int ports_generic015;
      int ports_generic01;
      int ports_23;
      int ports_4879;

      std::vector<SubsecondTime> alu_used_until;

   public:
      RobContentionSunnycove(const Core *core, const CoreModel *core_model);

      void initCycle(SubsecondTime now);
      bool tryIssue(const DynamicMicroOp &uop);
      bool noMore();
      void doIssue(DynamicMicroOp &uop);
};

#endif // __ROB_CONTENTION_SUNNYCOVE_H
