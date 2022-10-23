#ifndef MicroorganismFactory_h
#define MicroorganismFactory_h

#include "microorganism.h"

class MicroorganismFactory {
    public:
        MicroorganismFactory();
        Microorganism createMicroorganism();
    
    private:
        int entityCounter = 0;
};

#endif