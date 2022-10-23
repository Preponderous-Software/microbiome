#ifndef Logger_h
#define Logger_h

#include <string>
#include <iostream>
#include <fstream>

class Logger {
    public:
        Logger(std::string filename);
        void log(std::string message);

    private:
        std::string filename;
        std::ofstream file;
        bool debug = true;
};

#endif