
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
#include "emu.h"
#include "to_json.h"
using namespace std;
//#define MARKER_INSERT

namespace EMU{
    class Emu
    {
        public:
        
        Emu()  {
        }
        static Emu * toEmu( VOID * v ) {
            Emu * mtng_ptr = reinterpret_cast<Emu*>(v); 
            if( mtng_ptr == nullptr ) {
                cerr << "null Failed";
            }
            return mtng_ptr;
        }

    };

    VOID Fini( INT32 code,void *v)
    {
        Emu * emu_ptr = Emu::toEmu( v );
        delete emu_ptr;
    }
//    PIN_LOCK hooks_lock;
    void printRTNName( THREADID threadid, char *rtn_name ) {
        if(threadid==0) {
           std::cout
                  << "\033[1;32m[BBV   ] BBV simulated, start Sift::ModeIcount\033[0m"
                  << std::endl;

            in_roi = false;
            setInstrumentationMode(Sift::ModeIcount);
            thread_data[0].output->Magic(SIM_CMD_ROI_END, 0, 0);
        }
//        PIN_GetLock(&hooks_lock, 0);
//        std::cerr << "threadid" << threadid <<" " << rtn_name<<"\n";
//        PIN_ReleaseLock(&hooks_lock);
    }
    bool init_emu = false;
    static void routineCallback(RTN rtn, VOID *v)
    {
        RTN_Open(rtn);
        
        std::string rtn_name = RTN_Name(rtn).c_str();
        
//        std::cerr <<   rtn_name<<"\n";
//        if( rtn_name == "_init" && (!init_emu) ) {
       if (
          (
//           (rtn_name.find("GOMP_parallel_start") != std::string::npos) ||
//      (rtn_name.find("GOMP_parallel") != std::string::npos) ||
           (rtn_name == "_init"&& (!init_emu))
          )) {
       
        init_emu = true;
                 RTN_InsertCall( rtn, IPOINT_AFTER, (AFUNPTR)printRTNName,
                     IARG_THREAD_ID,
                     IARG_PTR,strdup(rtn_name.c_str()),
                     IARG_END);
        }

        RTN_Close(rtn);
    }



    void activate() {
//            in_roi = false;
//            setInstrumentationMode(Sift::ModeIcount);
//            thread_data[0].output->Magic(SIM_CMD_ROI_END, 0, 0);

        Emu *  mtng_global = new Emu();
        RTN_AddInstrumentFunction(routineCallback, 0);
        PIN_AddFiniFunction(Fini, (void*)mtng_global);
    }

};
