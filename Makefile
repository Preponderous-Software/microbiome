MAIN_FILE = src/mbapp.cpp
TESTS_FILE = src/tests.cpp
WEBAPP_FILE = src/webapp.cpp
PROJECT_FILES = src/microorganism.cpp src/microbiome.cpp src/appConfig.cpp src/simulation.cpp src/result.cpp src/logger.cpp src/microorganismFactory.cpp src/biomatter.cpp
WEB_FILES = src/webServer.cpp
ENVLIBCPP_FILES = env-lib-cpp/src/entity.cpp env-lib-cpp/src/environment.cpp env-lib-cpp/src/grid.cpp env-lib-cpp/src/location.cpp

WARNING_FLAGS = -pedantic -Wall
ULFIUS_FLAGS = $(shell pkg-config --cflags --libs libulfius jansson)

all: mbapp tests webapp

mbapp: src/mbapp.cpp
	g++ $(WARNING_FLAGS) $(MAIN_FILE) $(PROJECT_FILES) $(ENVLIBCPP_FILES)  -o mb_app

tests: src/tests.cpp
	g++ $(WARNING_FLAGS) -pthread $(PROJECT_FILES) $(WEB_FILES) $(ENVLIBCPP_FILES) $(TESTS_FILE) $(ULFIUS_FLAGS) -o tests

webapp: src/webapp.cpp
	g++ $(WARNING_FLAGS) -pthread $(PROJECT_FILES) $(WEB_FILES) $(ENVLIBCPP_FILES) $(WEBAPP_FILE) $(ULFIUS_FLAGS) -o mb_webapp
