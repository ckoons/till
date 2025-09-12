# Federation Design

## Overview

Federation in Till operates at two distinct layers:

1. **Infrastructure Layer (Local/Host Federation)**: Direct SSH-based management of hosts and deployments
2. **Application Layer (GitHub Federation)**: Discovery and collaboration between Tekton applications

Each layer serves different purposes while working together to create a complete federation ecosystem.

## Two-Layer Federation Architecture

### Infrastructure Layer (Local/Host)

The infrastructure layer provides direct control over physical and virtual machines:

**Characteristics**:
- SSH-based communication
- Full control if you have SSH access
- No GitHub dependency
- Managed entirely by Till
- Host-to-host relationships

**Use Cases**:
- Managing your own machines (laptop, desktop, servers)
- Team development environments
- Private cloud deployments
- CI/CD infrastructure

**Management**:
```bash
# Add and control hosts directly
till host add server user@server.com
till host deploy server
till host sync server
```

### Application Layer (GitHub)

The application layer enables Tekton applications to federate and collaborate:

**Characteristics**:
- GitHub-based registration
- Tribal membership model
- Component and solution sharing
- Public discovery
- Application-to-application relationships

**Use Cases**:
- Open source collaboration
- Component marketplace
- Public Tekton registry
- Cross-organization partnerships

**Management**:
```bash
# Join GitHub federation
till federate init --mode trusted --name alice.dev.us
```

### Layer Interaction

While independent, the layers complement each other:

1. **Infrastructure provides foundation**: Hosts run Tekton applications
2. **Applications provide functionality**: Federated apps run on managed hosts
3. **Till bridges both layers**: Single tool manages both infrastructure and applications

Example workflow:
```bash
# Infrastructure: Setup hosts
till host add prod deploy@prod.server.com
till host setup prod

# Deploy application
till host deploy prod primary.tekton.us

# Application: Join federation
ssh till-prod 'cd ~/Tekton && till federate init --mode trusted --name prod.company.us'
```

## Federation Modes (Application Layer)

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
registration-kant-philosophy-us-named
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
      "role": "trusted",
      "trust_level": "high",
      "accept_updates": true
    },
    "marx.economics.de": {
      "role": "named",
      "trust_level": "low",
      "accept_updates": false
    }
  }
}
```

### Asymmetric Relationships
- A can trust B as "trusted" while B only observes A
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
  "mode": "trusted",
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
till federate init --mode named --name kant.philosophy.us
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

### Overall Principles
1. **No Central Authority**: True peer-to-peer federation
2. **Working Code Wins**: Implementations define standards
3. **Natural Growth**: Communities form organically
4. **Privacy First**: Share only what you choose
5. **Simple Protocol**: Branch names carry information

### Infrastructure Layer Principles
1. **Direct Control**: SSH provides immediate access
2. **No Intermediaries**: Host-to-host communication
3. **Full Sovereignty**: Complete control over your hosts
4. **Local First**: Operates without internet if needed
5. **Simple Tools**: Only SSH and rsync required

### Application Layer Principles
1. **Tribal Membership**: Applications choose their tribes
2. **GitHub as Commons**: Shared neutral ground
3. **Voluntary Participation**: Join and leave freely
4. **Reputation Matters**: Build trust through actions
5. **Component Sharing**: Collaborate through code

## Comparison: Local vs GitHub Federation

| Aspect | Infrastructure Layer | Application Layer |
|--------|---------------------|-------------------|
| **Communication** | SSH | GitHub API/Git |
| **Authentication** | SSH Keys | GitHub Account |
| **Discovery** | Manual (till host add) | Automatic (GitHub) |
| **Scope** | Your hosts only | Global |
| **Dependencies** | None (SSH only) | GitHub |
| **Use Case** | Infrastructure management | Application collaboration |
| **Control** | Full (if SSH access) | Shared (tribal) |
| **Privacy** | Complete | Public or private repo |
| **Relationships** | Host-to-host | App-to-app |