
/*
 * This file is covered under the Interval Academic License, see LICENCE.academic
 */

#include "rob_contention_sunnycove.h"
#include "core_model.h"
#include "dynamic_micro_op.h"
#include "core.h"
#include "config.hpp"
#include "simulator.h"
#include "memory_manager_base.h"

RobContentionSunnycove::RobContentionSunnycove(const Core *core, const CoreModel *core_model)
   : m_core_model(core_model)
   , m_cache_block_mask(~(core->getMemoryManager()->getCacheBlockSize() - 1))
   , m_now(core->getDvfsDomain())
   , alu_used_until(DynamicMicroOpSunnycove::UOP_ALU_SIZE, SubsecondTime::Zero())
{
}

void RobContentionSunnycove::initCycle(SubsecondTime now)
{
   m_now.setElapsedTime(now);
   memset(ports, 0, sizeof(bool) * DynamicMicroOpSunnycove::UOP_PORT_SIZE);
   ports_generic0156 = 0;
   ports_generic015 = 0;
   ports_generic01 = 0;
   ports_23 = 0;
   ports_4879 = 0;
}

bool RobContentionSunnycove::tryIssue(const DynamicMicroOp &uop)
{
   // Port contention
   // TODO: Maybe the scheduler is more intelligent and doesn't just assign the first uop in the ROB
   //       that fits a particular free port. We could give precedence to uops that have dependants, etc.
   // NOTE: mixes canIssue and doIssue, in the sense that ports* are incremented immediately.
   //       This works as long as, if we return true, this microop is indeed issued

   const DynamicMicroOpSunnycove *core_uop_info = uop.getCoreSpecificInfo<DynamicMicroOpSunnycove>();
   DynamicMicroOpSunnycove::uop_port_t uop_port = core_uop_info->getPort();
   if (uop_port == DynamicMicroOpSunnycove::UOP_PORT0156)
   {
      if (ports_generic0156 >= 4)
         return false;
      else
         ports_generic0156++;
   }
   else if (uop_port == DynamicMicroOpSunnycove::UOP_PORT015)
   {
      if (ports_generic015 >= 3)
         return false;
      else
         ports_generic015++;
   }
   else if (uop_port == DynamicMicroOpSunnycove::UOP_PORT01)
   {  
      if (ports_generic01 >= 2)
         return false;
      else
         ports_generic01++;
   }
   else if (uop_port == DynamicMicroOpSunnycove::UOP_PORT23)
   {
      if (ports_23 >= 2)
         return false;
      else
         ports_23++;
   }
   else if (uop_port == DynamicMicroOpSunnycove::UOP_PORT4879)
   {
      if (ports_4879 >= 2)
         return false;
      else
         ports_4879++;
   }
   else
   { // PORT0, PORT1, PORT5, PORT6
      if (ports[uop_port])
         return false;
      else if (ports_generic0156 >= 4)
         return false;
      else if (uop_port != DynamicMicroOpSunnycove::UOP_PORT6 && ports_generic015 >= 3)
         return false;
      else
      {
         if (uop_port != DynamicMicroOpSunnycove::UOP_PORT5 && uop_port != DynamicMicroOpSunnycove::UOP_PORT6 && ports_generic01 >= 2)
             return false;
         else {
            ports[uop_port] = true;
            ports_generic0156++;
            if (uop_port != DynamicMicroOpSunnycove::UOP_PORT6) {
               ports_generic015++;
               if (uop_port != DynamicMicroOpSunnycove::UOP_PORT5)
                   ports_generic01++;
            }
         }
      }
   }

   // ALU contention
   if (DynamicMicroOpSunnycove::uop_alu_t alu = core_uop_info->getAlu())
   {
      if (alu_used_until[alu] > m_now)
         return false;
   }

   return true;
}

void RobContentionSunnycove::doIssue(DynamicMicroOp &uop)
{
   const DynamicMicroOpSunnycove *core_uop_info = uop.getCoreSpecificInfo<DynamicMicroOpSunnycove>();
   DynamicMicroOpSunnycove::uop_alu_t alu = core_uop_info->getAlu();
   if (alu)
      alu_used_until[alu] = m_now + m_core_model->getAluLatency(uop.getMicroOp());
}

bool RobContentionSunnycove::noMore()
{
   // When we issued something to all ports in this cycle, stop walking the rest of the ROB
   if (ports_4879 >= 2 && ports_23 >= 2 && ports_generic0156 >= 4)
      return true;
   else
      return false;
}

