/*
 * This file is covered under the Interval Academic License, see LICENCE.academic
 */

#include "interval_contention_sunnycove.h"
#include "core.h"
#include "stats.h"

IntervalContentionSunnycove::IntervalContentionSunnycove(const Core *core, const CoreModel *core_model)
   : m_core_model(core_model)
{
   for(unsigned int i = 0; i < DynamicMicroOpSunnycove::UOP_PORT_SIZE; ++i)
   {
      m_cpContrByPort[i] = 0;
      String name = String("cpContr_") + DynamicMicroOpSunnycove::PortTypeString((DynamicMicroOpSunnycove::uop_port_t)i);
      registerStatsMetric("interval_timer", core->getId(), name, &(m_cpContrByPort[i]));
   }
}

void IntervalContentionSunnycove::clearFunctionalUnitStats()
{
   for(unsigned int i = 0; i < (unsigned int)DynamicMicroOpSunnycove::UOP_PORT_SIZE; ++i)
   {
      m_count_byport[i] = 0;
   }
}

void IntervalContentionSunnycove::addFunctionalUnitStats(const DynamicMicroOp *uop)
{
   m_count_byport[uop->getCoreSpecificInfo<DynamicMicroOpSunnycove>()->getPort()]++;
}

void IntervalContentionSunnycove::removeFunctionalUnitStats(const DynamicMicroOp *uop)
{
   m_count_byport[uop->getCoreSpecificInfo<DynamicMicroOpSunnycove>()->getPort()]--;
}

uint64_t IntervalContentionSunnycove::getEffectiveCriticalPathLength(uint64_t critical_path_length, bool update_reason)
{
   DynamicMicroOpSunnycove::uop_port_t reason = DynamicMicroOpSunnycove::UOP_PORT_SIZE;
   uint64_t effective_cp_length = critical_path_length;

   // For the standard ports, check if we have exceeded our execution limit
   for (unsigned int i = 0 ; i < DynamicMicroOpSunnycove::UOP_PORT_SIZE ; i++)
   {
      // Skip shared ports
      if (i != DynamicMicroOpSunnycove::UOP_PORT0156
         && i != DynamicMicroOpSunnycove::UOP_PORT015
         && i != DynamicMicroOpSunnycove::UOP_PORT01
         && i != DynamicMicroOpSunnycove::UOP_PORT23
         && i != DynamicMicroOpSunnycove::UOP_PORT4879
         && effective_cp_length < m_count_byport[i]
      )
      {
         effective_cp_length = m_count_byport[i];
         reason = (DynamicMicroOpSunnycove::uop_port_t)i;
      }
   }
   // Check shared port usage
   // For shared ports 0, 1, 5, 6
   uint64_t port01_use = m_count_byport[DynamicMicroOpSunnycove::UOP_PORT0] + m_count_byport[DynamicMicroOpSunnycove::UOP_PORT1]
                       + m_count_byport[DynamicMicroOpSunnycove::UOP_PORT01];
   if (port01_use > (2*effective_cp_length))
   {
      effective_cp_length = (port01_use+1) / 2; // +1 to round up to the next cycle
      reason = DynamicMicroOpSunnycove::UOP_PORT01;
   }
   uint64_t port015_use = m_count_byport[DynamicMicroOpSunnycove::UOP_PORT0] + m_count_byport[DynamicMicroOpSunnycove::UOP_PORT1]
                        + m_count_byport[DynamicMicroOpSunnycove::UOP_PORT5] + m_count_byport[DynamicMicroOpSunnycove::UOP_PORT015];
   if (port015_use > (3*effective_cp_length))
   {
      effective_cp_length = (port015_use+2) / 3; // +2 to round up to the next cycle
      reason = DynamicMicroOpSunnycove::UOP_PORT015;
   }
   uint64_t port0156_use = m_count_byport[DynamicMicroOpSunnycove::UOP_PORT0] + m_count_byport[DynamicMicroOpSunnycove::UOP_PORT1]
                        + m_count_byport[DynamicMicroOpSunnycove::UOP_PORT5] + m_count_byport[DynamicMicroOpSunnycove::UOP_PORT6]
                        + m_count_byport[DynamicMicroOpSunnycove::UOP_PORT0156];
   if (port0156_use > (4*effective_cp_length))
   {
      effective_cp_length = (port0156_use+3) / 4; // +3 to round up to the next cycle
      reason = DynamicMicroOpSunnycove::UOP_PORT0156;
   }
   // For shared ports 2, 3
   uint64_t port23_use = m_count_byport[DynamicMicroOpSunnycove::UOP_PORT23];
   if (port23_use > (2*effective_cp_length)) // UOP_PORT23 represents the 2 instances of PORT 23 --> (m_count_byport for port 23) / 2 <= effective cp length
   {
       effective_cp_length = (port23_use+1) / 2; // +1 to round up to the next cycle
       reason = DynamicMicroOpSunnycove::UOP_PORT23;
   }
   uint64_t port4879_use = m_count_byport[DynamicMicroOpSunnycove::UOP_PORT4879];
   if (port4879_use > (2*effective_cp_length)) 
   {       
      effective_cp_length = (port4879_use+1) / 2;
      reason = DynamicMicroOpSunnycove::UOP_PORT4879;
   }

   if (update_reason && effective_cp_length > critical_path_length)
   {
      LOG_ASSERT_ERROR(reason != DynamicMicroOpSunnycove::UOP_PORT_SIZE, "There should be a reason for the cp extention, but there isn't");
      m_cpContrByPort[reason] += 1000000 * (effective_cp_length - critical_path_length) / effective_cp_length;
   }

   return effective_cp_length;
}
