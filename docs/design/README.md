# Till Design Documentation

## Overview

Till is a lightweight C program that serves as the lifecycle manager for Tekton installations. It handles installation, updates, component management, and federation between Tekton instances.

## Core Design Principles

1. **No Hardcoded Values**: All configuration lives in `till_config.h`
2. **Single Binary**: Portable C program with no runtime dependencies except git/gh
3. **Federation First**: Designed for multiple Tekton instances from the start
4. **Automatic Port Allocation**: Smart port management for multiple instances
5. **JSON Configuration**: Human-readable, Git-friendly configuration files

## Architecture

```
till (executable)
├── Configuration Layer (till_config.h)
├── Installation Module (till_install.c)
├── Federation Module (till_federate.c) [TODO]
├── Sync Module (till_sync.c) [TODO]
└── JSON Management (cJSON library)
```

## Key Components

### 1. Configuration Management

All configuration is centralized in `till_config.h`:
- GitHub URLs and repository names
- Port ranges and defaults
- File paths and directory structures
- Command limits and buffer sizes

**Never hardcode values elsewhere in the code.**

### 2. Installation System

The installation module handles:
- Cloning Tekton from GitHub
- Generating `.env.local` files
- Port allocation for multiple instances
- Registration in `till-private.json`

### 3. Port Allocation Strategy

See [Port Allocation](port-allocation.md) for detailed documentation.

### 4. Federation System

The federation system enables:
- Solo installations (invisible)
- Observer mode (read-only visibility)
- Member mode (bidirectional trust)

See [Federation Design](federation.md) for details.

## File Structure

```
~/projects/github/till/
├── src/                    # C source files
│   ├── till.c             # Main program
│   ├── till_config.h      # All configuration
│   ├── till_install.c/h   # Installation module
│   └── cJSON.c/h          # JSON library
├── .till/                 # Local configuration (not in Git)
│   └── tekton/
│       ├── federation/    # Federation configs
│       └── installed/     # Component tracking
├── docs/                  # Documentation
│   ├── design/           # Architecture and design
│   └── train/            # User training
└── Makefile              # Cross-platform build
```

## Development Guidelines

### Adding New Features

1. **Configuration First**: Add constants to `till_config.h`
2. **Module Design**: Create separate .c/.h files for major features
3. **Error Handling**: Always check return values and provide clear error messages
4. **Platform Compatibility**: Test on both Mac and Linux

### Code Style

- Use clear, descriptive function names
- Comment complex logic
- Keep functions focused (single responsibility)
- Use static for internal functions
- Always validate user input

### Building Till

```bash
make         # Build till
make clean   # Clean build files
make debug   # Build with debug symbols
make install # Install to /usr/local/bin
```

## Testing

Before committing changes:
1. Test basic commands (`till --help`, `till --version`)
2. Test installation flow
3. Verify JSON generation
4. Check port allocation logic
5. Test on both Mac and Linux if possible

## Future Enhancements

- [ ] Federation branch management
- [ ] Component version tracking
- [ ] Watch daemon implementation
- [ ] MCP server for Till operations
- [ ] Automated testing suite

## Important Notes

- Till is separate from Tekton - it manages Tekton but doesn't depend on it
- The `.till/` directory contains local state and should never be committed
- GitHub authentication is handled by the user (via ssh-agent or gh auth)
- Port conflicts are the user's responsibility (Till will warn but not prevent)