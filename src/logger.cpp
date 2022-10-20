#include <ctime>
#include <cstring>

#include "header/logger.h"

Logger::Logger(std::string filename) {
    this->filename = filename;
    file.open(filename);
}

void Logger::log(std::string message) {
    // prepare timestamp
    time_t timestamp = time(0);
    char* timestampString = ctime(&timestamp);
    timestampString[strlen(timestampString) - 1] = '\0';

    // print timestamp + message to logfile
    file << "[" << timestampString << "] " << message << std::endl;
}