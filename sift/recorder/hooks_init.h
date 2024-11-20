#ifndef __HOOKS_INIT_H__
#define __HOOKS_INIT_H__

class HooksManager;
class MtngManager;

#include "pin.H"
namespace Hooks {
extern PIN_LOCK hooks_lock;
void
hooks_init(HooksManager* _hooks_manager);
extern uint32_t last_mode ;
};

#endif /* __HOOKS_INIT_H__ */
