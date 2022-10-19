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
        int getSurvivingMicroorganisms();
        int getDeadMicroorganisms();
        int getEnergy();
        int getTicksElapsed();
        void print();
    private:
        int survivingMicroorganisms;
        int deadMicroorganisms;
        int energy;
        int ticksElapsed;
};

#endif