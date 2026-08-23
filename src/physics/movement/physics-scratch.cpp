#include "physics-scratch.h"

PhysicsScratch& physicsScratch()
{
    static thread_local PhysicsScratch sScratch;
    return sScratch;
}
