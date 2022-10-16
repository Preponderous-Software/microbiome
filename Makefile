PROJECT_FILES = src/microbiome.cpp src/microorganism.cpp
ENVLIBCPP_FILES = env-lib-cpp/src/entity.cpp env-lib-cpp/src/environment.cpp env-lib-cpp/src/grid.cpp env-lib-cpp/src/location.cpp

all: microbiome

microbiome: src/microbiome.cpp
	g++ $(PROJECT_FILES) $(ENVLIBCPP_FILES)  -o microbiome_executable