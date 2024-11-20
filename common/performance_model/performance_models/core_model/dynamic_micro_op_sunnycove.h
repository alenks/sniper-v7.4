#ifndef __DYNAMIC_MICRO_OP_SUNNYCOVE
#define __DYNAMIC_MICRO_OP_SUNNYCOVE

#include "dynamic_micro_op.h"

extern "C" {
#include <xed-iclass-enum.h>
}

class MicroOp;

class DynamicMicroOpSunnycove : public DynamicMicroOp
{
   public:

      enum uop_port_t {
         UOP_PORT0,
         UOP_PORT1,
         UOP_PORT23,
         UOP_PORT5,
         UOP_PORT6,
         UOP_PORT01,
         UOP_PORT015,
         UOP_PORT0156,
         UOP_PORT4879,
         UOP_PORT_SIZE,
      };
      uop_port_t uop_port;

      enum uop_alu_t {
         UOP_ALU_NONE = 0,
         UOP_ALU_TRIG,
         UOP_ALU_SIZE,
      };
      uop_alu_t uop_alu;

      enum uop_bypass_t {
         UOP_BYPASS_NONE,
         UOP_BYPASS_LOAD_FP,
         UOP_BYPASS_FP_STORE,
         UOP_BYPASS_SIZE
      };
      uop_bypass_t uop_bypass;

      static uop_port_t getPort(const MicroOp *uop);
      static uop_bypass_t getBypassType(const MicroOp *uop);
      static uop_alu_t getAlu(const MicroOp *uop);

      virtual const char* getType() const;

      DynamicMicroOpSunnycove(const MicroOp *uop, const CoreModel *core_model, ComponentPeriod period);

      uop_port_t getPort(void) const { return uop_port; }
      uop_bypass_t getBypassType(void) const { return uop_bypass; }
      uop_alu_t getAlu(void) const { return uop_alu; }

      static const char * PortTypeString(DynamicMicroOpSunnycove::uop_port_t port);
};

#endif // __DYNAMIC_MICRO_OP_SUNNYCOVE
