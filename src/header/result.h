#ifndef Result_h
#define Result_h

#include "simulation.h"

/**
 * The Result class represents the results of a simulation.
 * @author Daniel McCoy Stephenson
 */
class Result {
    public:
        Result() = delete;
        Result(Simulation* simulation);
        size_t getSurvivingMicroorganisms();
        size_t getDeadMicroorganisms();
        size_t getEnergy();
        size_t getTicksElapsed();
        void print();
    private:
        size_t survivingMicroorganisms;
        size_t deadMicroorganisms;
        size_t energy;
        size_t ticksElapsed;
};

#endif