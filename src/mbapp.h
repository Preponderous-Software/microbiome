#ifndef Microbiome_Application_h
#define Microbiome__Application_h

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

#include "microbiome.h"
#include "microorganism.h"

#include <vector>

using namespace envlibcpp;

class MicrobiomeApplication {
    public:
        MicrobiomeApplication();
        bool run();
    private:
        Microbiome* microbiome;
};

#endif