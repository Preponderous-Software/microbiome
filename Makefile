ENVLIBCPP_PROJECT_TEMPLATE_FILES = src/main.cpp
ENVLIBCPP_FILES = env-lib-cpp/src/entity.cpp env-lib-cpp/src/environment.cpp env-lib-cpp/src/grid.cpp env-lib-cpp/src/location.cpp

all: env-lib-cpp-project-template

env-lib-cpp-project-template: src/main.cpp
	g++ $(ENVLIBCPP_PROJECT_TEMPLATE_FILES) $(ENVLIBCPP_FILES)  -o main_executable