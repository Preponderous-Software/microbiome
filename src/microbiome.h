#ifndef Microbiome_h
#define Microbiome_h

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <string>
#include <iostream>

#include "../env-lib-cpp/src/header/entity.h"
#include "../env-lib-cpp/src/header/environment.h"
#include "../env-lib-cpp/src/header/location.h"
#include "../env-lib-cpp/src/header/grid.h"

#include <vector>

using namespace envlibcpp;

class Microbiome {
    public:
        Microbiome();
        void generateEntities(int numEntities);
        void addEntitiesToEnvironment();
        void initiateEntityMovement();
        bool run();
    private:
        std::vector<Entity> entities;
        Environment* environment;
};

#endif