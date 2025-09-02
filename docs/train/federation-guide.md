# Federation Guide

## Understanding Federation

Federation allows multiple Tekton instances to:
- Discover each other
- Share components and solutions
- Collaborate on development
- Learn from other implementations

Think of it as a social network for Tekton instances.

## Federation Modes

### Solo Mode
- **Privacy**: Complete - invisible to others
- **Updates**: Still uses Till for component updates
- **Use Case**: Personal projects, experimentation
- **Registry**: Not listed anywhere

```bash
till install tekton --mode solo
```

### Observer Mode
- **Privacy**: Listed but read-only
- **Updates**: Can see what others offer
- **Use Case**: Learning, monitoring
- **Registry**: Appears in federation listings

```bash
till install tekton --name alice.research.edu --mode observer
till federate init --mode observer
```

### Member Mode
- **Privacy**: Controlled sharing
- **Updates**: Can exchange with trusted members
- **Use Case**: Collaboration, sharing
- **Registry**: Full federation participant

```bash
till install tekton --name alice.research.edu --mode member
till federate init --mode member
```

## Naming Your Tekton

### Naming Convention

Follow the pattern: `name.category.geography`

Examples:
- `alice.research.edu`
- `bob.development.uk`
- `acme.production.us`

### Why Names Matter

1. **Identity**: Your unique identifier in the federation
2. **Discovery**: Others find you by name
3. **Trust**: Names build reputation over time
4. **Hierarchy**: You control sub-names (alice.research.edu.test)

### Name Registration

First-come, first-served:
```bash
# If 'alice' is available, you get the clean namespace
till install tekton --name alice --mode member

# If taken, you must qualify
till install tekton --name alice.research.edu --mode member
```

## Joining the Federation

### Step 1: Install as Observer or Member

```bash
till install tekton \
  --name alice.research.edu \
  --mode member
```

### Step 2: Initialize Federation

```bash
till federate init
```

This will:
1. Generate a public/private keypair
2. Create a registration branch on GitHub
3. Submit your information to the registry
4. Start synchronizing with other Tektons

### Step 3: Verify Registration

```bash
till federate status
```

Expected output:
```
Federation Status: Active
Mode: member
Name: alice.research.edu
Public Key: Registered
Last Sync: 2024-01-01 12:00:00
```

## Managing Relationships

### Viewing Other Tektons

```bash
# See all federation members
tekton status --federation

# Filter by domain
tekton status --federation --domain research

# Show only members
tekton status --federation --members-only
```

### Trust Levels

Configure how you interact with others:

```json
{
  "relationships": {
    "bob.research.uk": {
      "role": "member",
      "trust": "high",
      "accept_updates": true
    },
    "eve.unknown.net": {
      "role": "observer",
      "trust": "low",
      "accept_updates": false
    }
  }
}
```

### Asymmetric Relationships

You can trust Bob as a member while Bob only observes you:
- You accept Bob's updates
- Bob doesn't accept yours
- Natural and expected

## Sharing Components

### As a Member

When you develop something useful:

1. **Package Component**: Prepare for sharing
2. **Announce Availability**: Update your public registry
3. **Accept Requests**: Respond to access requests

### Finding Components

```bash
# Browse available components
till browse components

# Search for specific functionality
till search "authentication"

# View component details
till info bob.research.uk/auth-module
```

## Federation Workflow

### Daily Operations

1. **Morning Sync**:
```bash
till sync
# Fetches updates, checks federation
```

2. **Check Status**:
```bash
till federate status
tekton status --federation
```

3. **Review Updates**:
```bash
till update check
```

### Leaving Federation

If you need to leave:

```bash
till federate leave
```

This will:
1. Create a deregistration branch
2. Remove you from the registry
3. Clean up federation data

You can rejoin later with the same or different name.

## Security and Privacy

### What's Shared

**Public Information**:
- Your chosen name
- Public key
- Endpoint (if you choose)
- Capabilities list
- Components you offer

**Private Information** (Never Shared):
- Private key
- Internal configuration
- Relationship details
- Trust levels
- Local modifications

### Identity Verification

Every update is signed with your private key:
- Prevents impersonation
- Proves ownership of name
- Ensures message integrity

## Best Practices

### For Observers

1. **Learn First**: Watch how others organize
2. **Test Locally**: Try components in Coder environments
3. **Upgrade Later**: Become a member when ready

### For Members

1. **Start Small**: Share simple components first
2. **Document Well**: Help others use your work
3. **Be Responsive**: Address issues and questions
4. **Build Trust**: Consistent quality builds reputation

### For Everyone

1. **Regular Syncs**: Run `till sync` daily
2. **Keep Updated**: Apply security updates promptly
3. **Respect Privacy**: Don't probe solo instances
4. **Report Issues**: Help improve the federation

## Troubleshooting

### Can't Register

```
Error: Registration failed
```

**Check**:
1. GitHub authentication: `gh auth status`
2. Name availability: Try a different name
3. Network connection: Can you reach GitHub?

### Not Appearing in Federation

**Verify**:
1. Registration completed: `till federate status`
2. Mode is correct: Not `solo`
3. Branch was pushed: Check GitHub

### Can't See Other Tektons

**Try**:
1. Sync first: `till sync`
2. Check mode: Solo can't see others
3. Verify connection: Network issues?

## Advanced Federation

### Multiple Federations

A Tekton can participate in multiple federations:
- Research federation
- Company federation  
- Regional federation

### Custom Registries

Point to different registries:
```bash
till federate init --registry https://custom.registry.org
```

### Federation Analytics

Monitor your federation participation:
```bash
till federate analytics
# Shows connections, shares, reputation
```

## Examples

### Research Collaboration

Alice and Bob are researchers:

```bash
# Alice shares her NLP module
alice$ till share nlp-module --description "Advanced NLP processing"

# Bob discovers and uses it
bob$ till search nlp
bob$ till install alice.research.edu/nlp-module
```

### Company Development

ACME Corporation uses federation internally:

```bash
# Production Tekton
till install tekton --name acme.production.internal --mode member

# Development Tektons
till install tekton --name acme.dev.internal.team1 --mode member
till install tekton --name acme.dev.internal.team2 --mode member

# All can share within the company federation
```

### Learning Observer

Charlie wants to learn:

```bash
# Join as observer
till install tekton --name charlie.learning.home --mode observer

# Watch and learn
till sync
tekton status --federation

# Later upgrade to member
till federate upgrade --mode member
```

## Next Steps

1. **Choose Your Mode**: Solo, Observer, or Member
2. **Pick a Name**: Following name.category.geography
3. **Join Federation**: Run `till federate init`
4. **Explore**: See what others are building
5. **Contribute**: Share your improvements