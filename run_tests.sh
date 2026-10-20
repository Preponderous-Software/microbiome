#!/bin/sh
# Builds and runs both test suites. Stops at the first failing build or suite
# and exits non-zero, so a Catch2 failure is not masked by a passing legacy run.
set -e

# Remove old executables
rm -f ./tests ./mb_tests

# Compile Catch2 tests (default)
make catch2_tests

# Run Catch2 tests
./mb_tests

echo ""
echo "Catch2 tests completed."

# Run the legacy tests, which also cover the web server's HTTP endpoints
echo "Running legacy tests..."
make tests
./tests
