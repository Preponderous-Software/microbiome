#ifndef Microorganism_h
#define Microorganism_h

#include "../env-lib-cpp/src/header/entity.h"

using namespace envlibcpp;

class Microorganism : public Entity {
    public:
        Microorganism(int id, std::string name);
        int getEnergy();
        int getEnergyConsumptionRate();
        void setEnergy(int energy);
        void setEnergyConsumptionRate(int energyConsumptionRate);
        void consumeEnergy();
        int getTimesMoved();
        void incrementTimesMoved();
        bool isDead();
    private:
        int minimumEnergy = 80;
        int energy = rand() % (100 - minimumEnergy) + minimumEnergy + 1;
        int energyConsumptionRate = rand() % 5 + 1;
        int timesMoved = 0;
};

#endif