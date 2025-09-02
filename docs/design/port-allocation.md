# Port Allocation Strategy

## Overview

Till manages port allocation for multiple Tekton instances running on the same host. Each Tekton instance requires approximately 100 ports split between component services and AI specialists.

## Port Ranges

### Primary Tekton
- **Component Ports**: 8000-8099
- **AI Specialist Ports**: 45000-45099

### Coder Environments
- **Coder-A**: 8100-8199 (components), 45100-45199 (AI)
- **Coder-B**: 8200-8299 (components), 45200-45299 (AI)
- **Coder-C**: 8300-8399 (components), 45300-45399 (AI)
- **Coder-D**: 8400-8499 (components), 45400-45499 (AI)
- ... continues as needed

## Component Port Mapping

Each component has a fixed offset from the base port:

```
Component            Offset   Example (base=8000)
-------------------------------------------------
ENGRAM_PORT          +0       8000
HERMES_PORT          +1       8001
ERGON_PORT           +2       8002
RHETOR_PORT          +3       8003
TERMA_PORT           +4       8004
ATHENA_PORT          +5       8005
PROMETHEUS_PORT      +6       8006
HARMONIA_PORT        +7       8007
TELOS_PORT           +8       8008
SYNTHESIS_PORT       +9       8009
TEKTON_CORE_PORT     +10      8010
METIS_PORT           +11      8011
APOLLO_PORT          +12      8012
BUDGET_PORT          +13      8013
PENIA_PORT           +13      8013 (same as BUDGET)
SOPHIA_PORT          +14      8014
NOESIS_PORT          +15      8015
NUMA_PORT            +16      8016
AISH_PORT            +17      8017
AISH_MCP_PORT        +18      8018

Special Cases:
HEPHAESTUS_PORT      +80      8080
HEPHAESTUS_MCP_PORT  +88      8088
DB_MCP_PORT          +99      8099
```

## AI Port Mapping

AI ports follow the same offset pattern but from the AI base (45000):

```
Component            Offset   Example (base=45000)
-------------------------------------------------
ENGRAM_AI_PORT       +0       45000
HERMES_AI_PORT       +1       45001
ERGON_AI_PORT        +2       45002
... (same pattern as components)
HEPHAESTUS_AI_PORT   +80      45080
```

## Implementation Details

### Automatic Allocation

When installing with `--mode coder-a`, Till automatically:
1. Calculates the port offset: (A=1, B=2, C=3) * 100
2. Sets TEKTON_PORT_BASE = 8000 + offset
3. Sets TEKTON_AI_PORT_BASE = 45000 + offset

### Manual Override

Users can specify custom ports:
```bash
till install tekton --port-base 9000 --ai-port-base 46000
```

### .env.local Generation

Till generates explicit port assignments (no variable expansion):
```bash
# Base Configuration
TEKTON_PORT_BASE=8100
TEKTON_AI_PORT_BASE=45100

# Component Ports (all explicit)
ENGRAM_PORT=8100
HERMES_PORT=8101
ERGON_PORT=8102
...
```

## Special Considerations

### Budget/Penia Duality
Budget and Penia are the same component:
- Internal directories use "Budget"
- Both BUDGET_PORT and PENIA_PORT are set to the same value
- This is a historical artifact maintained for compatibility

### Hephaestus Web Ports
Hephaestus uses standard web ports (offset +80 and +88) to align with common HTTP conventions.

### Database MCP Port
The DB_MCP_PORT sits at the end of the range (+99) to avoid conflicts with component ports.

## Port Conflict Detection

Till should (TODO) implement port checking:
1. Check if ports are already in use
2. Warn user about conflicts
3. Suggest alternative port ranges

## Reserved Port Ranges

Avoid these ranges when setting custom ports:
- 0-1023: System/privileged ports
- 3000-3999: Common development servers
- 5000-5999: Common development services
- 8080, 8443: Standard web ports
- 27017: MongoDB
- 5432: PostgreSQL
- 3306: MySQL

## Best Practices

1. **Use defaults when possible**: The standard allocation works for most cases
2. **Document custom ports**: If using custom ports, document why
3. **Check before installing**: Use `netstat` or `lsof` to verify port availability
4. **Consistent spacing**: Keep 100-port gaps between Tekton instances
5. **Reserve ranges**: Plan ahead for additional Coder environments