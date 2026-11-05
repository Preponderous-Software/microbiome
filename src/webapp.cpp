#include <cstdlib>
#include <iostream>

#include "header/webServer.h"

int main() {
    return runWebApp(std::getenv("MICROBIOME_WEB_PORT"), std::cerr);
}
