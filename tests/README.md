# Till Test Suite

Comprehensive test suite for Till (Tekton Lifecycle Manager).

## Structure

```
tests/
├── functional/          # Unit and functional tests
│   ├── test_basic.sh   # Basic command tests
│   └── test_hold_release.sh  # Hold/Release functionality
├── integration/        # End-to-end integration tests  
│   └── test_full_workflow.sh  # Complete workflow tests
├── fixtures/          # Test data and mock files
└── run_tests.sh      # Main test runner
```

## Running Tests

### Run All Tests
```bash
# From till directory
./tests/run_tests.sh

# Or with make
make test
```

### Run Specific Test Suites
```bash
# Only functional tests
./tests/run_tests.sh --functional-only

# Only integration tests
./tests/run_tests.sh --integration-only

# Verbose output
./tests/run_tests.sh --verbose
```

### Run Individual Tests
```bash
# Run specific functional test
./tests/functional/test_basic.sh

# Run specific integration test
./tests/integration/test_full_workflow.sh
```

## Test Categories

### Functional Tests

**test_basic.sh**
- Version and help commands
- Command-specific help pages
- Invalid command handling
- Discovery without installations
- Hold/Release status commands
- Schedule commands
- Environment variable handling
- Lock file handling

**test_hold_release.sh**
- Single component holds
- Multi-component holds
- Time-based holds (duration and end time)
- Force holds
- Hold all components
- Release operations
- Expired hold cleanup
- Hold status verification

### Integration Tests

**test_full_workflow.sh**
- Mock Tekton installation discovery
- Hold/sync integration
- Schedule management
- Git operations in repos
- Complex workflows (hold→sync→release→sync)
- Registry persistence
- Concurrent operation handling

## Test Output

Tests use color-coded output:
- 🟢 Green: Test passed
- 🔴 Red: Test failed  
- 🟡 Yellow: Test in progress
- 🔵 Blue: Information

### Summary Format
```
Tests run:    15
Tests passed: 14 ✓
Tests failed: 1  ✗
```

## Writing New Tests

### Test Template
```bash
#!/bin/bash
set -e

# Import test functions
source "$(dirname $0)/../test_helpers.sh"

# Test implementation
run_test "Test name"
if [ condition ]; then
    pass "Test passed"
else
    fail "Test failed" "Error details"
fi
```

### Best Practices
1. Clean up after tests (use cleanup function)
2. Use descriptive test names
3. Test both success and failure cases
4. Mock external dependencies
5. Keep tests independent
6. Use meaningful assertions

## CI Integration

The test suite can be integrated with CI/CD pipelines:

```yaml
# Example GitHub Actions
- name: Build Till
  run: make

- name: Run Tests
  run: ./tests/run_tests.sh
```

## Troubleshooting

### Common Issues

**Tests fail with "Till executable not found"**
- Build Till first: `make`
- Or specify path: `TILL=/path/to/till ./tests/run_tests.sh`

**Permission denied errors**
- Make scripts executable: `chmod +x tests/*.sh tests/*/*.sh`

**Mock installations not cleaned up**
- Tests clean up in `/tmp/till-*` directories
- Manual cleanup: `rm -rf /tmp/till-integration-test-*`

## Test Coverage

Current test coverage includes:
- ✅ Basic commands and help
- ✅ Hold/Release functionality  
- ✅ Discovery and registry
- ✅ Sync with holds
- ✅ Schedule management
- ✅ Lock file handling
- ✅ Environment variables
- ✅ Multi-component operations
- ✅ Time-based holds
- ✅ Concurrent operations

## Contributing

When adding new features to Till:
1. Add corresponding tests
2. Ensure all tests pass
3. Update this README if needed
4. Run full test suite before committing