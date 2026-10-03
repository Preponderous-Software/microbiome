#include <ctime>
#include <cstring>

#include "header/logger.h"

Logger::Logger(std::string filename) {
    this->filename = filename;
#ifdef __EMSCRIPTEN__
    // In the browser there is nobody to read a log file, and an in-memory one
    // would only grow for as long as the tab stays open.
    debug = false;
    return;
#endif
    file.open(filename);
}

/**
 * Logs a message to the file if debug is set to true.
*/
void Logger::log(std::string message) {
    if (!debug) {
        return;
    }
    // prepare timestamp
    time_t timestamp = time(0);
    char* timestampString = ctime(&timestamp);
    timestampString[strlen(timestampString) - 1] = '\0';

    // print timestamp + message to logfile
    file << "[" << timestampString << "] " << message << std::endl;
}