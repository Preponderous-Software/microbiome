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
        int survivingMicroorganisms = -1;
        int deadMicroorganisms = -1;
        int energy = -1;
        int ticksElapsed = -1;
};

#endif