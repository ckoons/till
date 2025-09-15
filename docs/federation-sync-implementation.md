# Till Federation Sync Implementation Plan

## Current Status
✅ **Completed:**
- Federation join/leave/status commands
- GitHub CLI (`gh`) integration for authentication
- Trust levels (anonymous, named, trusted)
- Site ID generation
- Global configuration storage

⏳ **Not Yet Implemented:**
- GitHub Gist creation/management
- Menu-of-the-day fetching
- Directive processing
- Sync operations (pull, push, sync)

## Implementation Phases

### Phase 1: GitHub Gist Operations
**Goal:** Actually create and manage gists for federation members

#### Tasks:
1. **Create Gist** (`till_federation_gist.c`)
   ```c
   int create_federation_gist(const char *token, const char *site_id, char *gist_id, size_t gist_id_size) {
       // Use gh api to create gist
       // gh api gists --field description="Till Federation Status" 
       //              --field public=true 
       //              --field files[status.json][content]='{"site_id":"..."}'
   }
   ```

2. **Update Gist**
   ```c
   int update_federation_gist(const char *token, const char *gist_id, const char *content) {
       // gh api gists/{id} --method PATCH --field files[status.json][content]='...'
   }
   ```

3. **Delete Gist**
   ```c
   int delete_federation_gist(const char *token, const char *gist_id) {
       // gh api gists/{id} --method DELETE
   }
   ```

### Phase 2: Menu-of-the-Day Implementation
**Goal:** Fetch and cache daily directives from repository

#### Repository Structure:
```
till/
└── federation/
    ├── menu-of-the-day.yaml     # Current directives
    ├── archives/                 # Historical menus
    │   └── 2025-01-15.yaml
    └── templates/                # Directive templates
```

#### Menu Format:
```yaml
version: 1
date: 2025-01-15
expires: 2025-01-16
directives:
  - id: update-till-2025-01-15
    type: update
    priority: normal
    condition: "version < 1.5.0"
    action: "till update"
    message: "New version available with federation features"
    
  - id: security-patch-001
    type: security
    priority: high
    condition: "all"
    action: "apply_patch security-2025-01.patch"
    message: "Critical security update"
```

#### Implementation:
```c
int fetch_menu_of_the_day(menu_t *menu) {
    // Option 1: Use gh to fetch from repo
    char *content = gh_fetch_file("ckoons/till", "main", "federation/menu-of-the-day.yaml");
    
    // Option 2: Direct HTTPS fetch
    char *content = https_fetch("https://raw.githubusercontent.com/ckoons/till/main/federation/menu-of-the-day.yaml");
    
    // Parse YAML content
    parse_menu_yaml(content, menu);
    
    // Cache locally
    save_menu_cache(menu);
}
```

### Phase 3: Directive Processing Engine
**Goal:** Evaluate conditions and execute actions

#### Condition Evaluator:
```c
typedef struct {
    char field[64];      // "version", "trust_level", "platform"
    char operator[8];    // "<", ">", "==", "!="
    char value[128];     // "1.5.0", "trusted", "mac"
} condition_t;

int evaluate_condition(const char *condition_str) {
    // Parse: "version < 1.5.0"
    // Compare with current state
    // Return 1 if true, 0 if false
}
```

#### Action Executor:
```c
typedef enum {
    ACTION_TILL_UPDATE,
    ACTION_APPLY_PATCH,
    ACTION_SET_CONFIG,
    ACTION_ENABLE_FEATURE,
    ACTION_CUSTOM_COMMAND
} action_type_t;

int execute_action(const char *action_str) {
    // Parse action
    // Validate safety
    // Execute with proper sandboxing
    // Log results
}
```

### Phase 4: Sync Commands Implementation

#### `till federate pull`
```c
int till_federate_pull(void) {
    // 1. Fetch menu-of-the-day
    menu_t menu;
    if (fetch_menu_of_the_day(&menu) != 0) {
        // Use cached menu if available
        load_cached_menu(&menu);
    }
    
    // 2. Process directives
    for (int i = 0; i < menu.directive_count; i++) {
        directive_t *d = &menu.directives[i];
        
        // Check if already processed
        if (is_directive_processed(d->id)) continue;
        
        // Evaluate condition
        if (evaluate_condition(d->condition)) {
            printf("Processing: %s\n", d->message);
            execute_action(d->action);
            mark_directive_processed(d->id);
        }
    }
}
```

#### `till federate push`
```c
int till_federate_push(void) {
    // 1. Collect status
    status_t status;
    collect_system_status(&status);
    
    // 2. Get token
    char token[256];
    get_gh_token(token, sizeof(token));
    
    // 3. Update gist
    char json[4096];
    create_status_json(&status, json, sizeof(json));
    update_federation_gist(token, config.gist_id, json);
}
```

#### `till federate sync`
```c
int till_federate_sync(void) {
    printf("Starting federation sync...\n");
    
    // 1. Pull directives
    printf("Step 1: Pulling menu-of-the-day...\n");
    till_federate_pull();
    
    // 2. Push status (if named/trusted)
    if (strcmp(config.trust_level, TRUST_ANONYMOUS) != 0) {
        printf("Step 2: Pushing status...\n");
        till_federate_push();
    }
    
    // 3. Update last sync
    config.last_sync = time(NULL);
    save_federation_config(&config);
    
    printf("✓ Sync complete\n");
}
```

## Testing Strategy

### Manual Testing:
```bash
# Test gist creation
till federate leave
till federate join --named
# Should create real gist

# Test menu fetching
till federate pull
# Should fetch from repo

# Test full sync
till federate sync
# Should pull and push
```

### Automated Tests:
```bash
tests/test_federation_sync.sh
tests/test_directive_processing.sh
tests/test_gist_operations.sh
```

## Security Considerations

1. **Directive Validation**
   - Whitelist allowed commands
   - Sandbox execution environment
   - Require signing for critical actions

2. **Token Handling**
   - Never log tokens
   - Use gh for token management
   - Validate token scopes before use

3. **Rate Limiting**
   - Cache menu-of-the-day for 24 hours
   - Throttle gist updates to once per hour
   - Respect GitHub API limits

## Implementation Priority

1. **Week 1:** Gist operations (create, update, delete)
2. **Week 2:** Menu fetching and caching
3. **Week 3:** Directive processing engine
4. **Week 4:** Full sync implementation

## Success Criteria

- [ ] Can create/update/delete gists via gh
- [ ] Can fetch menu-of-the-day from repository
- [ ] Can evaluate directive conditions
- [ ] Can execute safe actions
- [ ] Full sync cycle works end-to-end
- [ ] Appropriate error handling
- [ ] Security measures in place

## Next Steps

1. Start with gist operations using `gh api`
2. Create test menu-of-the-day.yaml in repository
3. Implement basic condition evaluator
4. Test with simple directives
5. Gradually add complexity