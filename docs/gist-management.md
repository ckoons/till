# GitHub Gist Management for Till Federation

## Quick Reference

### View Your Gists
```bash
# List all your gists (default: 10 most recent)
gh gist list

# List more gists
gh gist list --limit 50

# List only public gists
gh gist list --public

# List only secret gists
gh gist list --secret

# Filter gists by description/content
gh gist list --filter "Till Federation"

# Search content of gists (slower, but thorough)
gh gist list --filter "site_id" --include-content
```

### View Specific Gist
```bash
# View gist content in terminal
gh gist view <gist-id>

# View specific file from gist
gh gist view <gist-id> --filename status.json

# Open gist in browser
gh gist view <gist-id> --web
```

### Delete Gists
```bash
# Delete a specific gist
gh gist delete <gist-id>

# Delete with confirmation prompt
gh gist delete <gist-id> --confirm
```

### Edit Gists
```bash
# Edit gist interactively
gh gist edit <gist-id>

# Edit specific file
gh gist edit <gist-id> --filename status.json
```

### Create Gists
```bash
# Create from file
gh gist create status.json --desc "Till Status"

# Create public gist
gh gist create status.json --public

# Create from stdin
echo '{"status":"active"}' | gh gist create - --desc "Quick status"
```

## Till Federation Gist Management

### Finding Till Gists
```bash
# List all Till federation gists
gh gist list --limit 50 | grep "Till Federation"

# Show with IDs for management
gh gist list --limit 50 --filter "Till Federation"
```

### Cleaning Up Old Till Gists
```bash
# View old Till gists
gh gist list --limit 100 | grep "Till Federation"

# Delete specific old gist
gh gist delete <old-gist-id>
```

### Batch Operations
```bash
# List all Till gist IDs
gh gist list --limit 100 | grep "Till Federation" | cut -f1

# Delete all Till gists (CAREFUL!)
for gist in $(gh gist list --limit 100 | grep "Till Federation" | cut -f1); do
    echo "Deleting $gist"
    gh gist delete "$gist"
done
```

## Till-Specific Commands

### View Your Federation Gist
```bash
# Get your current gist ID from Till
till federate status

# View the content
gh gist view <your-gist-id>
```

### Manual Gist Cleanup
```bash
# If Till leaves orphaned gists
till federate leave --delete-gist

# Or manually delete
gh gist delete <gist-id>
```

### Verify Gist Content
```bash
# Check what Till is reporting
GIST_ID=$(till federate status | grep "Gist ID:" | awk '{print $3}')
gh gist view $GIST_ID | jq .
```

## API Access

### Using gh api
```bash
# List gists via API
gh api gists

# Get specific gist
gh api gists/<gist-id>

# Delete via API
gh api gists/<gist-id> --method DELETE
```

### Check Rate Limits
```bash
# See your API rate limit status
gh api rate_limit
```

## Automation Scripts

### Clean Old Till Gists
```bash
#!/bin/bash
# cleanup-till-gists.sh - Remove old Till federation gists

echo "Finding Till Federation gists..."
GISTS=$(gh gist list --limit 100 | grep "Till Federation" | cut -f1)

if [ -z "$GISTS" ]; then
    echo "No Till gists found"
    exit 0
fi

echo "Found gists:"
echo "$GISTS"
echo
read -p "Delete all these gists? (y/N) " -n 1 -r
echo

if [[ $REPLY =~ ^[Yy]$ ]]; then
    for gist in $GISTS; do
        echo "Deleting $gist..."
        gh gist delete "$gist"
    done
    echo "Done!"
else
    echo "Cancelled"
fi
```

### Monitor Federation Status
```bash
#!/bin/bash
# monitor-federation.sh - Check all Till federation gists

echo "Till Federation Gists Status"
echo "============================"

for gist in $(gh gist list --limit 100 | grep "Till Federation" | cut -f1); do
    echo
    echo "Gist: $gist"
    content=$(gh gist view $gist --filename status.json 2>/dev/null)
    if [ $? -eq 0 ]; then
        echo "$content" | jq -r '
            "  Site ID: " + .site_id,
            "  Hostname: " + .hostname,
            "  Last Sync: " + (.last_sync | todate),
            "  Trust Level: " + .trust_level
        '
    else
        echo "  Failed to read gist"
    fi
done
```

## Common Tasks

### 1. Find and Delete Orphaned Till Gists
```bash
# List all your Till gists
gh gist list --limit 100 --filter "Till Federation"

# Delete ones not in use
# (Check with `till federate status` first)
gh gist delete <orphaned-gist-id>
```

### 2. Check Active Federation Gist
```bash
# From Till
till federate status

# Direct check
GIST_ID=<your-gist-id>
gh gist view $GIST_ID --web  # Opens in browser
```

### 3. Backup Gist Data
```bash
# Download all gists
gh gist list --limit 100 | while read -r id desc rest; do
    gh gist clone "$id" "gist-backup-$id"
done
```

### 4. Federation Gist Health Check
```bash
# Quick health check of your federation gist
till federate status
GIST_ID=$(till federate status | grep "Gist ID:" | awk '{print $3}')
if [ -n "$GIST_ID" ]; then
    echo "Checking gist $GIST_ID..."
    gh gist view $GIST_ID | jq -r '"Status: OK, Last sync: " + (.last_sync | todate)'
fi
```

## Troubleshooting

### Gist Not Found
```bash
# Verify gist exists
gh gist view <gist-id>

# If not found, rejoin federation
till federate leave
till federate join --named
```

### Too Many Gists
```bash
# Count your gists
gh gist list --limit 1000 | wc -l

# Clean up old ones
gh gist list --limit 1000 | tail -100 | cut -f1 | xargs -I {} gh gist delete {}
```

### Permission Issues
```bash
# Check your token scopes
gh auth status

# Refresh with gist scope
gh auth refresh -s gist
```

## Best Practices

1. **Regular Cleanup**: Periodically check for orphaned Till gists
2. **Single Active Gist**: Each Till installation should have only one active gist
3. **Use --delete-gist**: When leaving federation, use the flag to clean up
4. **Monitor Rate Limits**: Be aware of GitHub API limits (5000/hour for authenticated requests)
5. **Backup Important Data**: Gists can be deleted; keep local copies if needed

## Summary

The `gh gist` command provides complete control over your gists:
- `gh gist list` - View all your gists
- `gh gist view <id>` - See gist content
- `gh gist delete <id>` - Remove gists
- `gh gist edit <id>` - Modify gists

For Till federation, gists are automatically managed, but you can always use these commands for manual cleanup or troubleshooting.