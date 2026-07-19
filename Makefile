MAIN_FILE = src/mbapp.cpp
TESTS_FILE = src/tests.cpp
<<<<<<< HEAD
CATCH2_TEST_FILE = test/test_microbiome.cpp
PROJECT_FILES = src/microorganism.cpp src/microbiome.cpp src/appConfig.cpp src/simulation.cpp src/result.cpp src/logger.cpp src/microorganismFactory.cpp
=======
WEBAPP_FILE = src/webapp.cpp
PROJECT_FILES = src/microorganism.cpp src/microbiome.cpp src/appConfig.cpp src/simulation.cpp src/result.cpp src/logger.cpp src/microorganismFactory.cpp src/biomatter.cpp
WEB_FILES = src/webServer.cpp
>>>>>>> origin/master
ENVLIBCPP_FILES = env-lib-cpp/src/entity.cpp env-lib-cpp/src/environment.cpp env-lib-cpp/src/grid.cpp env-lib-cpp/src/location.cpp

WARNING_FLAGS = -pedantic -Wall
ULFIUS_FLAGS = $(shell pkg-config --cflags --libs libulfius jansson)

all: mbapp tests webapp

mbapp: src/mbapp.cpp
	g++ $(WARNING_FLAGS) $(MAIN_FILE) $(PROJECT_FILES) $(ENVLIBCPP_FILES)  -o mb_app

# Legacy tests (for backward compatibility)
tests: src/tests.cpp
<<<<<<< HEAD
	g++ $(WARNING_FLAGS) $(PROJECT_FILES) $(ENVLIBCPP_FILES) $(TESTS_FILE) -o tests

# Catch2 tests (new)
catch2_tests: test/test_microbiome.cpp
	g++ $(WARNING_FLAGS) -I test $(PROJECT_FILES) $(ENVLIBCPP_FILES) $(CATCH2_TEST_FILE) -o mb_tests

# Default test target points to catch2_tests
test: catch2_tests

clean:
	rm -f mb_app tests mb_tests
=======
	g++ $(WARNING_FLAGS) -pthread $(PROJECT_FILES) $(WEB_FILES) $(ENVLIBCPP_FILES) $(TESTS_FILE) $(ULFIUS_FLAGS) -o tests

webapp: src/webapp.cpp
	g++ $(WARNING_FLAGS) -pthread $(PROJECT_FILES) $(WEB_FILES) $(ENVLIBCPP_FILES) $(WEBAPP_FILE) $(ULFIUS_FLAGS) -o mb_webapp
>>>>>>> origin/master
