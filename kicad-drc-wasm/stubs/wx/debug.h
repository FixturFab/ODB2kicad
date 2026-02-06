#pragma once

#include <cassert>

#define wxASSERT(cond) assert(cond)
#define wxASSERT_MSG(cond, msg) assert(cond)
#define wxCHECK(cond, rc) do { if(!(cond)) return rc; } while(0)
#define wxCHECK_MSG(cond, rc, msg) do { if(!(cond)) return rc; } while(0)
#define wxCHECK_RET(cond, msg) do { if(!(cond)) return; } while(0)
#define wxCHECK2(cond, op) do { if(!(cond)) { op; } } while(0)
#define wxCHECK2_MSG(cond, op, msg) do { if(!(cond)) { op; } } while(0)
#define wxFAIL
#define wxFAIL_MSG(msg)
#define wxCOMPILE_TIME_ASSERT(cond, msg) static_assert(cond, #msg)
#define wxCOMPILE_TIME_ASSERT2(cond, msg, name) static_assert(cond, #msg)
