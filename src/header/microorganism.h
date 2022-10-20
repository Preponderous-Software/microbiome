#ifndef Microorganism_h
#define Microorganism_h

#include "../../env-lib-cpp/src/header/entity.h"

using namespace envlibcpp;

/**
 * The Microorganism class represents an entity capable of existing in a 2D virtual environment.
 * @author Daniel McCoy Stephenson
 */
class Microorganism : public Entity {
    public:
        Microorganism(int id, std::string name);
        int getEnergy();
        int getMetabolicRate();
        void setEnergy(int energy);
        void setMetabolicRate(int energyConsumptionRate);
        void metabolize();
        int getTimesMoved();
        int getTimesEaten();
        void incrementTimesMoved();
        void incrementTimesEaten();
        bool isDead();
        void setReproductionThreshold(int reproductionThreshold);
        int getReproductionThreshold();
        bool canReproduce();
    private:
        int maxEnergy = 200;
        int maxMetabolicRateFactor = 5;
        int minimumEnergy = 100;
        int energy = rand() % (maxEnergy - minimumEnergy) + minimumEnergy + 1;
        int metabolicRate = rand() % maxMetabolicRateFactor + 1;
        int timesMoved = 0;
        int timesEaten = 0;
        int reproductionThreshold = 100;
};

#endif