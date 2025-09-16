# Till Federation Admin Commands

## Overview

The `till federate admin` commands are **owner-only** administrative functions for managing the global Till federation. These commands allow the repository owner to:

1. Process all federation gists and collect statistics
2. Delete processed gists to maintain privacy
3. View aggregated federation status from a secret gist

## Authentication

Only the repository owner (`ckoons`) can run admin commands. The system verifies ownership by checking:
```bash
gh api user --jq .login  # Must match "ckoons"
```

## Commands

### `till federate admin process`

Processes all Till federation gists and aggregates their data.

**What it does:**
1. Verifies you are the repo owner
2. Creates or finds the secret admin gist
3. Searches for all "Till Federation Status" gists
4. Reads each gist's status data
5. Aggregates statistics (platforms, trust levels, activity)
6. Saves report to secret gist
7. **Deletes all processed public gists**

**Usage:**
```bash
till federate admin process
```

**Output Example:**
```
Till Federation Admin - Process
================================

Using secret gist: e7eaf994d07da5eac1e1f42c4c73269b

Searching for Till Federation gists...
  Processing gist c3d5b2163f49a68153f0369173566eb8... OK (site: M4-1758041306-6807)
    Deleting gist c3d5b2163f49a68153f0369173566eb8... DELETED

=== Process Summary ===
Found:      1 gists
Processed:  1 sites
Malformed:  0 gists
Deleted:    1 gists
Report:     Secret gist e7eaf994d07da5eac1e1f42c4c73269b
```

### `till federate admin status`

Displays the aggregated federation status from the secret gist.

**Options:**
- `--stats` - Show statistics only (default)
- `--full` - Show detailed site information

**Usage:**
```bash
# Statistics only
till federate admin status

# Full details including all sites
till federate admin status --full
```

**Output Example (stats mode):**
```
Till Federation Admin - Status
===============================

Last Processed: 2025-01-16T16:48:34Z
Total Sites:    1

=== Statistics ===
By Platform:
  darwin    : 1
  linux     : 0

By Trust Level:
  anonymous : 0
  named     : 1
  trusted   : 0

Activity:
  Last 24h:   1 sites
  Last 7d:    1 sites

Processing:
  Found:      1 gists
  Processed:  1 sites
  Malformed:  0 gists
  Deleted:    1 gists

Secret Gist: e7eaf994d07da5eac1e1f42c4c73269b
```

**Output Example (full mode):**
```
[... statistics as above ...]

=== All Sites ===

Site ID: M4-1758041306-6807
  Hostname:     M4
  Platform:     darwin
  Trust Level:  named
  Last Sync:    2025-01-16 16:48:26
  Installations: 5
```

## Data Structure

The secret gist contains JSON with the following structure:

```json
{
  "last_processed": "2025-01-16T16:48:34Z",
  "total_sites": 1,
  "sites": {
    "M4-1758041306-6807": {
      "hostname": "M4",
      "platform": "darwin",
      "trust_level": "named",
      "last_sync": 1758041306,
      "installation_count": 5,
      "gist_id": "c3d5b2163f49a68153f0369173566eb8",
      "processed_at": 1758041314
    }
  },
  "statistics": {
    "by_platform": {
      "darwin": 1,
      "linux": 0
    },
    "by_trust_level": {
      "named": 1
    },
    "active_last_24h": 1,
    "active_last_7d": 1,
    "total_found": 1,
    "total_processed": 1,
    "total_malformed": 0,
    "total_deleted": 1
  },
  "malformed": []
}
```

## Privacy Model

1. **Public Gists**: Created by federation members, contain site status
2. **Secret Gist**: Created by admin, contains aggregated data
3. **Process & Delete**: Admin processes public gists then deletes them
4. **No Trace**: After processing, no public record remains

## Workflow

### Regular Admin Workflow
```bash
# 1. Process all federation gists (deletes them)
till federate admin process

# 2. View aggregated statistics
till federate admin status

# 3. View detailed site info if needed
till federate admin status --full
```

### Finding the Secret Gist
```bash
# The secret gist ID is stored locally
cat ~/.till/admin.json

# Or view your secret gists
gh gist list --limit 100 | grep "Till Federation Admin"

# View the secret gist content directly
gh gist view <secret-gist-id>
```

## Implementation Details

### Files Created
- `~/.till/admin.json` - Stores secret gist ID and metadata
- Secret gist on GitHub - Contains aggregated federation data

### Discovery Method
The admin process searches for gists with "Till Federation Status" in the description:
```bash
gh gist list --limit 1000 | grep "Till Federation Status"
```

### Error Handling
- **Malformed Gists**: Tracked in the report under "malformed" array
- **Failed Deletions**: Reported but don't stop processing
- **Missing Data**: Sites without site_id are marked as malformed

## Security Considerations

1. **Owner-Only**: Hard-coded check for repository owner
2. **Secret Storage**: Admin gist is secret (not public)
3. **Token Security**: Uses gh CLI for authentication
4. **Data Privacy**: Public gists deleted after processing

## Limitations

1. Only processes gists visible to the owner
2. Limited to 1000 gists per search
3. Requires gh CLI with gist permissions
4. Owner verification is hard-coded to "ckoons"

## Future Enhancements

Potential improvements:
- Scheduled processing via cron
- Historical data retention in secret gist
- Federation member notifications
- Geographic distribution tracking
- Performance metrics collection