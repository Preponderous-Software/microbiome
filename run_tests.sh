# Remove old executables
rm -f ./tests ./mb_tests

# Compile Catch2 tests (default)
make catch2_tests

# Run Catch2 tests
./mb_tests

echo ""
echo "Catch2 tests completed."

# Optionally run legacy tests for comparison
echo "Running legacy tests for comparison..."
make tests
./tests
