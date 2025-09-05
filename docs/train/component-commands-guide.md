# Till Component Commands Guide

## Overview

Till provides a unified interface to run component-specific commands through the `till run` command. This allows you to manage Tekton and its components without needing to know their exact installation paths or internal scripts.

## How It Works

Till automatically discovers executable commands in component `.tillrc/commands/` directories. Any executable file in these directories becomes available as a command.

## Basic Usage

### List Available Components

```bash
till run
```

This shows all components that have discoverable commands in their `.tillrc/commands/` directory.

### List Commands for a Component

```bash
till run tekton
```

This lists all available commands for the tekton component.

### Run a Command

```bash
till run tekton status
till run tekton start
till run tekton stop --force
```

Arguments after the command name are passed directly to the component command.

## Creating Component Commands

### Directory Structure

Component commands should be placed in the `.tillrc/commands/` directory within the component's root:

```
Tekton/
├── .tillrc/
│   └── commands/
│       ├── start
│       ├── stop
│       ├── status
│       └── build
```

### Command Requirements

1. **Executable**: Commands must be executable (`chmod +x`)
2. **Self-contained**: Should handle their own environment setup
3. **Exit codes**: Should return appropriate exit codes (0 for success)

### Example Command Script

```bash
#!/bin/bash
# Tekton status command

# Change to component root
cd "$(dirname "$0")/../.."

# Source environment if needed
if [ -f .env.local ]; then
    source .env.local
fi

# Perform status checks
echo "Tekton Status"
echo "============="
echo "Directory: $(pwd)"

# Check processes
if pgrep -f "tekton" > /dev/null; then
    echo "Status: Running"
else
    echo "Status: Not running"
fi

exit 0
```

## Environment Variables

Component commands have access to:

- Standard Till environment variables
- Component-specific environment from `.env` or `.env.local`
- Current working directory (set to component root)

## Common Component Commands

### Tekton Commands

- `till run tekton start` - Start Tekton services
- `till run tekton stop` - Stop Tekton services
- `till run tekton status` - Check Tekton status
- `till run tekton logs` - View Tekton logs
- `till run tekton build` - Build Tekton components

### Component-Specific Commands

Each component can define its own commands. For example:

- `till run numa analyze` - Run Numa analysis
- `till run rhetor prompt` - Launch Rhetor interface
- `till run hephaestus ui` - Start Hephaestus UI

## Advanced Usage

### Passing Complex Arguments

All arguments after the command name are passed directly:

```bash
till run tekton deploy --environment production --force --verbose
```

### Command Chaining

Component commands can be chained with standard shell operators:

```bash
till run tekton stop && till run tekton build && till run tekton start
```

### Scripting with Till Run

```bash
#!/bin/bash
# Restart all services

for component in tekton numa rhetor; do
    if till run $component status | grep -q "Running"; then
        echo "Restarting $component..."
        till run $component stop
        till run $component start
    fi
done
```

## Troubleshooting

### Command Not Found

If a command is not found:

1. Check the command exists: `ls -la <component>/.tillrc/commands/`
2. Verify it's executable: `ls -l <component>/.tillrc/commands/<command>`
3. Ensure component is discovered: `till discover --force`

### Permission Errors

Make sure commands are executable:

```bash
chmod +x <component>/.tillrc/commands/*
```

### Path Issues

Commands are executed with the component root as the working directory. Use relative paths from there or change directory explicitly in your script.

## Best Practices

1. **Keep commands simple**: Each command should do one thing well
2. **Use descriptive names**: Command names should be clear and intuitive
3. **Handle errors gracefully**: Check for dependencies and provide clear error messages
4. **Document commands**: Include help text or --help option
5. **Respect environment**: Don't modify global system state without warning

## Integration with Till

The `till run` command integrates with Till's discovery system:

- Automatically finds all Tekton installations
- Discovers commands in each component
- Handles path resolution and environment setup
- Provides consistent error handling

## Creating New Components

To make a new component work with `till run`:

1. Create `.tillrc/commands/` directory
2. Add executable command scripts
3. Ensure component is in a discoverable location
4. Run `till discover --force` if needed

## See Also

- [Commands Reference](../reference/commands.md#till-run)
- [Installation Guide](installation-guide.md)
- [Configuration Reference](../reference/configuration.md)