MAIN_FILE = src/mbapp.cpp
TESTS_FILE = src/tests.cpp
PROJECT_FILES = src/microorganism.cpp src/microbiome.cpp src/appConfig.cpp src/simulation.cpp src/result.cpp src/logger.cpp src/microorganismFactory.cpp
ENVLIBCPP_FILES = env-lib-cpp/src/entity.cpp env-lib-cpp/src/environment.cpp env-lib-cpp/src/grid.cpp env-lib-cpp/src/location.cpp

WARNING_FLAGS = -pedantic -Wall

all: mbapp tests

mbapp: src/mbapp.cpp
	g++ $(WARNING_FLAGS) $(MAIN_FILE) $(PROJECT_FILES) $(ENVLIBCPP_FILES)  -o mb_app

tests: src/tests.cpp
	g++ $(WARNING_FLAGS) $(PROJECT_FILES) $(ENVLIBCPP_FILES) $(TESTS_FILE) -o tests