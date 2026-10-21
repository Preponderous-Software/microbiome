#!/bin/sh
# Builds and runs both Catch2 suites. Stops at the first failing build or suite
# and exits non-zero, so a failure in the first suite is not masked by a passing second run.
set -e

# Remove old executables
rm -f ./mb_tests ./mb_web_tests

# Compile Catch2 tests (default)
make catch2_tests

# Run Catch2 tests
./mb_tests

echo ""
echo "Catch2 tests completed."

# Run the web server's HTTP endpoint tests, which link Ulfius
echo "Running web server tests..."
make catch2_web_tests
./mb_web_tests
