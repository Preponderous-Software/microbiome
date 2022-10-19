MAIN_FILE = src/mbapp.cpp
TESTS_FILE = src/tests.cpp
PROJECT_FILES = src/microorganism.cpp src/microbiome.cpp src/appConfig.cpp src/simulation.cpp src/result.cpp
ENVLIBCPP_FILES = env-lib-cpp/src/entity.cpp env-lib-cpp/src/environment.cpp env-lib-cpp/src/grid.cpp env-lib-cpp/src/location.cpp

all: mbapp tests

mbapp: src/mbapp.cpp
	g++ $(MAIN_FILE) $(PROJECT_FILES) $(ENVLIBCPP_FILES)  -o mb_app

tests: src/tests.cpp
	g++ $(PROJECT_FILES) $(ENVLIBCPP_FILES) $(TESTS_FILE) -o tests