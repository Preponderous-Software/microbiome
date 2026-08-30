MAIN_FILE = src/mbapp.cpp
TESTS_FILE = src/tests.cpp
CATCH2_TEST_FILE = test/test_microbiome.cpp
WEBAPP_FILE = src/webapp.cpp
PROJECT_FILES = src/microorganism.cpp src/microbiome.cpp src/appConfig.cpp src/simulation.cpp src/result.cpp src/logger.cpp src/microorganismFactory.cpp src/biomatter.cpp
WEB_FILES = src/webServer.cpp
ENVLIBCPP_FILES = env-lib-cpp/src/entity.cpp env-lib-cpp/src/environment.cpp env-lib-cpp/src/grid.cpp env-lib-cpp/src/location.cpp

PROJECT_HEADERS = $(wildcard src/header/*.h)
ENVLIBCPP_HEADERS = $(wildcard env-lib-cpp/src/header/*.h)
CATCH2_HEADERS = $(wildcard test/lib/*.hpp)
HEADERS = $(PROJECT_HEADERS) $(ENVLIBCPP_HEADERS)

WARNING_FLAGS = -pedantic -Wall
ULFIUS_FLAGS = $(shell pkg-config --cflags --libs libulfius jansson)

# The friendly target names below are aliases; the recipes are keyed on the
# binaries they actually produce, and each names every source it is built from.
# Without that, `make tests` would consider a `tests` binary up to date whenever
# it was newer than src/tests.cpp alone, and re-run a stale binary after a change
# to any other translation unit. `test` needs .PHONY for a second reason: a
# directory named test/ exists, so make would otherwise treat it as a file target.
.PHONY: all mbapp catch2_tests test webapp clean

all: mbapp tests catch2_tests webapp

mbapp: mb_app

# Legacy tests (for backward compatibility)
# The alias and the binary share a name here, so this rule is not phony.
tests: $(TESTS_FILE) $(PROJECT_FILES) $(WEB_FILES) $(ENVLIBCPP_FILES) $(HEADERS)
	g++ $(WARNING_FLAGS) -pthread $(PROJECT_FILES) $(WEB_FILES) $(ENVLIBCPP_FILES) $(TESTS_FILE) $(ULFIUS_FLAGS) -o tests

# Catch2 tests (new)
catch2_tests: mb_tests

# Default test target points to catch2_tests
test: mb_tests

webapp: mb_webapp

mb_app: $(MAIN_FILE) $(PROJECT_FILES) $(ENVLIBCPP_FILES) $(HEADERS)
	g++ $(WARNING_FLAGS) $(MAIN_FILE) $(PROJECT_FILES) $(ENVLIBCPP_FILES)  -o mb_app

mb_tests: $(CATCH2_TEST_FILE) $(PROJECT_FILES) $(ENVLIBCPP_FILES) $(HEADERS) $(CATCH2_HEADERS)
	g++ $(WARNING_FLAGS) -I test $(PROJECT_FILES) $(ENVLIBCPP_FILES) $(CATCH2_TEST_FILE) -o mb_tests

mb_webapp: $(WEBAPP_FILE) $(PROJECT_FILES) $(WEB_FILES) $(ENVLIBCPP_FILES) $(HEADERS)
	g++ $(WARNING_FLAGS) -pthread $(PROJECT_FILES) $(WEB_FILES) $(ENVLIBCPP_FILES) $(WEBAPP_FILE) $(ULFIUS_FLAGS) -o mb_webapp

clean:
	rm -f mb_app tests mb_tests mb_webapp
