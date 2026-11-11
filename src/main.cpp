#include <iostream>
#include <memory>

#include "header/appConfig.h"
#include "header/mbapp.h"
#include "header/usageReporting.h"

int main(int argc, char* argv[]) {
    // One startup event to trace, sent in the background once the arguments
    // are accepted; see header/usageReporting.h and the README's "Usage
    // reporting" section. Its destructor gives the event a moment to leave
    // when the program ends.
    std::unique_ptr<usage_reporting::UsageReporter> usageReporting;
    return runConsoleApp(argc, argv, std::cout, std::cerr, AppConfig(), [&usageReporting]() {
        usageReporting.reset(new usage_reporting::UsageReporter());
        usageReporting->reportStartup();
    });
}
