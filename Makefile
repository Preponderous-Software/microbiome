PROJECT_FILES = src/mbapp.cpp src/microorganism.cpp src/microbiome.cpp src/config.cpp src/simulation.cpp src/result.cpp
ENVLIBCPP_FILES = env-lib-cpp/src/entity.cpp env-lib-cpp/src/environment.cpp env-lib-cpp/src/grid.cpp env-lib-cpp/src/location.cpp

all: mbapp

mbapp: src/mbapp.cpp
	g++ $(PROJECT_FILES) $(ENVLIBCPP_FILES)  -o mb_app