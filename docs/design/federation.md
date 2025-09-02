# Federation Design

## Overview

Federation enables multiple Tekton instances to discover, communicate, and share resources. Each Tekton maintains sovereignty while participating in a trust-based network.

## Federation Modes

### Solo
- **Visibility**: Invisible to other Tektons
- **Registry**: Not listed in federation
- **Updates**: Uses Till for component updates
- **Use Case**: Private development, testing

### Observer
- **Visibility**: Listed in federation registry
- **Permissions**: Read-only access to others
- **Registry**: Appears in `tekton status --federation`
- **Use Case**: Monitoring, learning from others

### Member
- **Visibility**: Full federation participant
- **Permissions**: Bidirectional trust possible
- **Sharing**: Can share components and solutions
- **Use Case**: Collaborative development

## Naming Convention

### Fully Qualified Names (FQN)
```
name.category.geography[.qualifier]

Examples:
- kant.philosophy.us
- herman.development.usga
- herman.development.usga.coder-a
```

### Name Resolution
- First-come, first-served for base names
- Case-insensitive fuzzy matching
- Hierarchical namespaces (owner controls sub-names)

## Registration Process

### Branch-Based Registration

1. **Generate Keypair**: Create public/private key pair
2. **Create Branch**: `registration-[fqn]-[mode]`
3. **Push to GitHub**: Branch name contains all needed info
4. **Automatic Processing**: GitHub action updates registry
5. **Cleanup**: Local Till deletes branch after push

Example branch name:
```
registration-kant-philosophy-us-observer
```

### Deregistration

Similar process with `deregistration-` prefix:
```
deregistration-kant-philosophy-us
```

## Trust Model

### Individual Sovereignty
Each Tekton maintains its own relationship definitions:

```json
{
  "relationships": {
    "hume.philosophy.uk": {
      "role": "member",
      "trust_level": "high",
      "accept_updates": true
    },
    "marx.economics.de": {
      "role": "observer",
      "trust_level": "low",
      "accept_updates": false
    }
  }
}
```

### Asymmetric Relationships
- A can trust B as "member" while B only observes A
- No requirement for reciprocal trust
- Natural formation of trust clusters

## Federation Data

### Public Data (GitHub)
```json
{
  "identity": {
    "fqn": "kant.philosophy.us",
    "public_key": "ssh-rsa ...",
    "endpoint": "https://kant.example.com:8101",
    "capabilities": ["numa", "rhetor", "apollo"]
  }
}
```

### Private Data (Local)
```json
{
  "mode": "member",
  "private_key_path": ".till/credentials/private.key",
  "relationships": { ... }
}
```

## Branch Lifecycle

### Ephemeral Branches
- Branches have Time-To-Live (TTL)
- Automatic cleanup of expired branches
- Refresh mechanism for active participants

### TTL Values
- Public face: 72 hours
- Component sharing: 24 hours
- Handshake: 15 minutes
- Registration: Immediate deletion after processing

## Federation Commands

### Initialization
```bash
till federate init --mode observer --name kant.philosophy.us
```

### Status
```bash
till federate status
tekton status --federation
```

### Leaving
```bash
till federate leave
```

## Security Considerations

### Identity Verification
- Public key cryptography for identity proof
- Signed messages for updates
- No identity spoofing possible

### Trust Boundaries
- Each Tekton decides what to accept
- No central authority
- Natural reputation through interactions

## Implementation Status

- [x] Name validation and FQN support
- [x] Local registration in till-private.json
- [ ] Keypair generation
- [ ] Branch creation and pushing
- [ ] Federation status commands
- [ ] Registry synchronization
- [ ] Trust relationship management

## Future Enhancements

1. **Transitive Discovery**: Find Tektons through trusted connections
2. **Reputation System**: Share feedback about interactions
3. **Component Marketplace**: Browse and install shared components
4. **Federation Analytics**: Visualize the federation network
5. **Conflict Resolution**: Handle naming disputes

## Design Principles

1. **No Central Authority**: True peer-to-peer federation
2. **Working Code Wins**: Implementations define standards
3. **Natural Growth**: Communities form organically
4. **Privacy First**: Share only what you choose
5. **Simple Protocol**: Branch names carry information