# Till Global Federation Implementation Sprint

## Sprint Overview
**Duration**: 2 weeks  
**Goal**: Implement asynchronous federation system using GitHub Gists as infrastructure  
**Version**: 1.0.0  
**Start Date**: 2025-01-12  

## Architecture Summary
- **Central Repository**: `tekton/till-federation` (public GitHub repo)
- **Per-Till Storage**: Individual GitHub Gists per installation
- **Communication**: Asynchronous via GitHub API (pull/push model)
- **Security**: Local holds always respected, all directives opt-in

---

## Phase 1: Foundation (Days 1-3)

### Task 1.1: Create Federation Data Structures
**Priority**: High  
**Files**: `src/till_federation.h`, `src/till_federation.c`

```c
typedef struct {
    char site_id[128];
    char gist_id[64];
    char github_token[256];  // encrypted
    char trust_level[32];    // anonymous|named|trusted
    time_t last_sync;
    int auto_sync;
} federation_config_t;

typedef struct {
    char id[64];
    char type[32];
    char target[32];
    char condition[256];
    char action[1024];
    int priority;
    int report_back;
} directive_t;
```

### Task 1.2: Implement Secure Token Storage
**Priority**: High  
**Function**: Store GitHub token encrypted in `.till/federation.json`
- Use platform keychain where available
- Fallback to encrypted file with key derivation
- Never store plaintext tokens

### Task 1.3: Create Federation Commands Structure
**Priority**: High  
**Commands to implement**:
```
till federate join [--anonymous|--named|--trusted]
till federate pull
till federate push  
till federate status
till federate leave
till federate sync   # Combined pull + process + push
```

**Test Checklist**:
- [ ] Can parse federation subcommands
- [ ] Help text displays correctly
- [ ] Invalid commands show proper errors

---

## Phase 2: GitHub Integration (Days 4-6)

### Task 2.1: Implement GitHub API Wrapper
**Priority**: High  
**Functions needed**:
```c
int github_create_gist(const char *token, const char *description, 
                      const char *content, char *gist_id_out);
int github_update_gist(const char *token, const char *gist_id, 
                       const char *filename, const char *content);
int github_get_file(const char *repo, const char *path, 
                   char *content_out, size_t max_size);
int github_get_gist(const char *gist_id, char *content_out, size_t max_size);
```

### Task 2.2: Implement Rate Limiting
**Priority**: Medium  
- Track API calls with timestamps
- Implement exponential backoff
- Cache menu-of-the-day locally (1 hour TTL)

### Task 2.3: Create Gist Management
**Priority**: High  
**Gist structure**:
```
gist-name: "till-{site_id}"
├── manifest.json     # Till identity and capabilities
├── status.json       # Current status and health
├── results.json      # Latest directive results
└── telemetry.json    # Anonymous usage data
```

**Test Checklist**:
- [ ] Can create new gist with token
- [ ] Can update existing gist
- [ ] Handles API errors gracefully
- [ ] Rate limiting works

---

## Phase 3: Federation Join/Leave (Days 7-8)

### Task 3.1: Implement `till federate join`
**Priority**: High  
**Flow**:
1. Generate unique site_id: `{hostname}-{timestamp}-{random}`
2. Prompt for GitHub token (or use --token flag)
3. Create gist with initial manifest
4. Store config in `.till/federation.json`
5. Optionally register in central registry

**Test Checklist**:
- [ ] Generates unique site IDs
- [ ] Creates gist successfully
- [ ] Stores encrypted token
- [ ] Handles existing federation gracefully

### Task 3.2: Implement `till federate leave`  
**Priority**: Medium  
**Flow**:
1. Delete local federation.json
2. Optionally delete gist
3. Remove from central registry if registered

---

## Phase 4: Menu Processing (Days 9-11)

### Task 4.1: Implement Menu-of-the-Day Fetching
**Priority**: High  
**URL**: `https://raw.githubusercontent.com/tekton/till-federation/main/menu-of-the-day/latest.json`

**Menu format**:
```json
{
  "date": "2025-01-12",
  "version": "1.0",
  "directives": [
    {
      "id": "unique-directive-id",
      "type": "update|patch|telemetry|script",
      "target": "till|tekton|component",
      "condition": "version < 1.2.0",
      "action": "command to run",
      "priority": "low|medium|high|critical",
      "report_back": true
    }
  ],
  "announcements": []
}
```

### Task 4.2: Implement Directive Processing
**Priority**: High  
**Logic**:
1. Download menu-of-the-day
2. For each directive:
   - Check if already completed (by ID)
   - Evaluate condition
   - Check against local holds
   - Execute if approved
   - Store results

### Task 4.3: Implement Condition Evaluator
**Priority**: High  
**Conditions to support**:
- Version comparisons: `version < 1.2.0`
- Component existence: `has_component(tekton)`
- Platform checks: `platform == darwin`
- Till configuration: `till_configured == yes`
- Combination: `version < 1.2.0 && platform == linux`

**Test Checklist**:
- [ ] Fetches latest menu successfully
- [ ] Caches menu appropriately
- [ ] Evaluates conditions correctly
- [ ] Respects holds
- [ ] Tracks completed directives

---

## Phase 5: Sync and Reporting (Days 12-13)

### Task 5.1: Implement `till federate sync`
**Priority**: High  
**Flow**:
1. Pull menu-of-the-day
2. Process applicable directives
3. Collect results and telemetry
4. Update gist with status
5. Log all actions

### Task 5.2: Implement Results Collection
**Priority**: Medium  
**Collect**:
- Directive execution results (stdout, stderr, exit code)
- Timestamp of execution
- System telemetry (if opted in)
- Error messages

### Task 5.3: Implement Telemetry
**Priority**: Low  
**Anonymous data only**:
- Till version
- Platform (darwin/linux)
- Number of Tekton installations
- Component list (no identifying info)
- Sync frequency

**Test Checklist**:
- [ ] Full sync cycle works
- [ ] Results uploaded to gist
- [ ] Telemetry respects privacy settings
- [ ] Logs are comprehensive

---

## Phase 6: Testing & Polish (Day 14)

### Task 6.1: Create Test Menu-of-the-Day
**Priority**: High  
Create test menus with various directives:
- Simple echo commands
- Version-conditional updates
- Platform-specific directives
- High-priority security patches

### Task 6.2: Integration Testing
**Priority**: High  
**Test scenarios**:
1. Fresh federation join
2. Daily sync with new directives
3. Sync with all directives already completed
4. Network failure handling
5. Invalid token handling
6. Malformed menu handling

### Task 6.3: Documentation
**Priority**: Medium  
- Update README with federation commands
- Create FEDERATION.md guide
- Add examples of menu-of-the-day files
- Document privacy levels

---

## Implementation Checklist

### Core Files to Create/Modify
- [ ] `src/till_federation.h` - Header with structures and function declarations
- [ ] `src/till_federation.c` - Main federation implementation
- [ ] `src/till_github.c` - GitHub API wrapper
- [ ] `src/till_github.h` - GitHub API header
- [ ] `src/till_commands.c` - Add federate command
- [ ] `src/till.c` - Add federate to command table
- [ ] `Makefile` - Add new source files

### Configuration Files
- [ ] `.till/federation.json` - Local federation config
- [ ] Example `menu-of-the-day.json`
- [ ] Example gist manifests

### Test Files
- [ ] `tests/federation/test_join.sh`
- [ ] `tests/federation/test_sync.sh`
- [ ] `tests/federation/test_conditions.sh`
- [ ] Mock menu-of-the-day files

---

## Success Criteria

1. **Joining Federation**
   - Can join at any trust level
   - Token stored securely
   - Gist created successfully

2. **Daily Operations**
   - Menu fetched and cached
   - Directives evaluated correctly
   - Holds always respected
   - Results uploaded to gist

3. **Error Handling**
   - Network failures don't break till
   - Invalid menus rejected safely
   - Token problems reported clearly

4. **Privacy**
   - Anonymous mode truly anonymous
   - No sensitive data in gists
   - Telemetry is opt-in and minimal

5. **Performance**
   - Sync completes in < 30 seconds
   - Minimal GitHub API usage
   - Efficient caching

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| GitHub API rate limits | Local caching, exponential backoff |
| Token security | Encryption, platform keychain integration |
| Malicious directives | All directives opt-in, condition checking |
| Network failures | Graceful degradation, continue local operation |
| Federation spam | Registry moderation, trust levels |

---

## Daily Development Log

### Day 1 (2025-01-12)
- [ ] Create federation.h/c structure
- [ ] Implement basic command parsing
- [ ] Create federation.json format

### Day 2
- [ ] Implement GitHub API basics
- [ ] Test gist creation
- [ ] Token encryption

### Day 3
- [ ] Complete `federate join` command
- [ ] Test with real GitHub account
- [ ] Error handling

### Days 4-14
- [ ] Continue per plan above
- [ ] Daily testing
- [ ] Documentation updates

---

## Notes

- Start with minimum viable federation (just join + pull)
- Add features incrementally
- Test with real GitHub account early
- Consider mock mode for testing without GitHub
- Keep it simple - this is infrastructure, not the main product

---

## Post-Sprint Enhancements

Future improvements after core federation works:
1. P2P host discovery via gists
2. Distributed command execution
3. Federation-wide statistics dashboard
4. Automated security updates
5. Component sharing marketplace