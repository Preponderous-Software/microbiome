MAIN_FILE = src/mbapp.cpp
CATCH2_TEST_FILE = test/test_microbiome.cpp test/test_usage_reporting.cpp test/test_usd_recording.cpp
CATCH2_WEB_TEST_FILE = test/test_web_server.cpp
WEBAPP_FILE = src/webapp.cpp
PROJECT_FILES = src/microorganism.cpp src/microbiome.cpp src/appConfig.cpp src/simulation.cpp src/result.cpp src/logger.cpp src/microorganismFactory.cpp src/biomatter.cpp src/usdRecording.cpp
WEB_FILES = src/webServer.cpp
ENVLIBCPP_FILES = env-lib-cpp/src/entity.cpp env-lib-cpp/src/environment.cpp env-lib-cpp/src/grid.cpp env-lib-cpp/src/location.cpp

PROJECT_HEADERS = $(wildcard src/header/*.h) $(wildcard src/header/*.hpp) $(wildcard src/header/vendor/*/*.hpp)
ENVLIBCPP_HEADERS = $(wildcard env-lib-cpp/src/header/*.h)
CATCH2_HEADERS = $(wildcard test/lib/*.hpp)
HEADERS = $(PROJECT_HEADERS) $(ENVLIBCPP_HEADERS)

WARNING_FLAGS = -pedantic -Wall
# C++17 for the vendored usd-recorder writer (std::variant/std::optional); the
# browser build (web/build.sh) already compiles as C++17.
STD_FLAGS = -std=c++17
# The console app reports its version with its startup event (usageReporting.h).
VERSION_FLAGS = -DMICROBIOME_VERSION='"$(shell cat version.txt)"'
ULFIUS_FLAGS = $(shell pkg-config --cflags --libs libulfius jansson)

# The friendly target names below are aliases; the recipes are keyed on the
# binaries they actually produce, and each names every source it is built from.
# Without that, a test binary would be considered up to date whenever it was
# newer than its test file alone, and a stale binary re-run after a change to
# any other translation unit. `test` needs .PHONY for a second reason: a
# directory named test/ exists, so make would otherwise treat it as a file target.
.PHONY: all mbapp catch2_tests catch2_web_tests test webapp web strata clean og-image-header

all: mbapp catch2_tests catch2_web_tests webapp

mbapp: mb_app

# Catch2 tests that need no Ulfius
catch2_tests: mb_tests

# Catch2 tests of the web server's HTTP endpoints; they link Ulfius like mb_webapp
catch2_web_tests: mb_web_tests

# Default test target points to catch2_tests
test: mb_tests

webapp: mb_webapp

mb_app: $(MAIN_FILE) $(PROJECT_FILES) $(ENVLIBCPP_FILES) $(HEADERS) version.txt
	g++ $(STD_FLAGS) $(WARNING_FLAGS) $(VERSION_FLAGS) -pthread $(MAIN_FILE) $(PROJECT_FILES) $(ENVLIBCPP_FILES)  -o mb_app

mb_tests: $(CATCH2_TEST_FILE) $(PROJECT_FILES) $(ENVLIBCPP_FILES) $(HEADERS) $(CATCH2_HEADERS)
	g++ $(STD_FLAGS) $(WARNING_FLAGS) -pthread -I test $(PROJECT_FILES) $(ENVLIBCPP_FILES) $(CATCH2_TEST_FILE) -o mb_tests

mb_web_tests: $(CATCH2_WEB_TEST_FILE) $(PROJECT_FILES) $(WEB_FILES) $(ENVLIBCPP_FILES) $(HEADERS) $(CATCH2_HEADERS)
	g++ $(STD_FLAGS) $(WARNING_FLAGS) -pthread -I test $(PROJECT_FILES) $(WEB_FILES) $(ENVLIBCPP_FILES) $(CATCH2_WEB_TEST_FILE) $(ULFIUS_FLAGS) -o mb_web_tests

mb_webapp: $(WEBAPP_FILE) $(PROJECT_FILES) $(WEB_FILES) $(ENVLIBCPP_FILES) $(HEADERS)
	g++ $(STD_FLAGS) $(WARNING_FLAGS) -pthread $(PROJECT_FILES) $(WEB_FILES) $(ENVLIBCPP_FILES) $(WEBAPP_FILE) $(ULFIUS_FLAGS) -o mb_webapp

# Browser (Emscripten) build into web/build/; needs em++ on PATH, so it is not
# part of `all`. See web/build.sh and the README's "Play in your browser".
web:
	web/build.sh

# The opt-in micro-environment on the Strata engine (docs/STRATA.md) into
# build-strata/: mb_strata and its tests. Needs CMake >= 3.24 (set CMAKE=...
# to pick one) and SDL3's X11 build dependencies, so it is not part of `all`;
# run its tests with `ctest --test-dir build-strata`. `strata` is phony because
# a directory of that name exists.
CMAKE ?= cmake
strata:
	$(CMAKE) -S strata -B build-strata -DCMAKE_BUILD_TYPE=Release
	$(CMAKE) --build build-strata -j4

# The viewer's link-preview card is compiled into the web server, so the
# binary needs no files beside it. After redrawing assets/og.png, run this and
# commit both; the web server suite (./mb_web_tests) fails if they differ.
og-image-header: assets/og.png
	( echo '// Generated from assets/og.png by `make og-image-header`; do not edit.'; \
	  echo '#ifndef OgImage_h'; echo '#define OgImage_h'; \
	  cd assets && xxd -i og.png | sed 's/^unsigned/static const unsigned/'; \
	  echo '#endif' ) > src/header/ogImage.h

clean:
	rm -f mb_app mb_tests mb_web_tests mb_webapp
