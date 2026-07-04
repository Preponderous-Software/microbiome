#ifndef Biomatter_h
#define Biomatter_h

#include "../../env-lib-cpp/src/header/entity.h"

using namespace envlibcpp;

/**
 * The Biomatter class represents decomposing biomass left behind when a
 * microorganism dies. Living microorganisms can forage it for energy,
 * modeling nutrient recycling in a real microbial community.
 * @author Daniel McCoy Stephenson
 */
class Biomatter : public Entity {
    public:
        Biomatter(int id, std::string name, int energy);
        int getEnergy();
        void setEnergy(int energy);
        bool isDepleted();
    private:
        int energy;
};

#endif
