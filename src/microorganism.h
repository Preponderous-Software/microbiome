#ifndef Microorganism_h
#define Microorganism_h

#include "../env-lib-cpp/src/header/entity.h"

using namespace envlibcpp;

class Microorganism : public Entity {
    public:
        Microorganism(int id, std::string name);
        int getTimesMoved();
        void incrementTimesMoved();
    private:
        int timesMoved = 0;
};

#endif