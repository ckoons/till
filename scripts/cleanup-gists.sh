#!/bin/bash
#
# cleanup-gists.sh - Clean up Till Federation gists
#
# This script helps manage and clean up Till federation gists
#

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Till Federation Gist Manager ===${NC}"
echo

# Function to get current Till gist
get_current_gist() {
    till federate status 2>/dev/null | grep "Gist ID:" | awk '{print $3}'
}

# Function to list Till gists
list_till_gists() {
    gh gist list --limit 100 | grep "Till Federation" | while IFS=$'\t' read -r id desc files visibility date; do
        echo -e "${GREEN}$id${NC} - $desc (${YELLOW}$date${NC})"
    done
}

# Get current active gist
CURRENT_GIST=$(get_current_gist)

# Main menu
while true; do
    echo "What would you like to do?"
    echo "1) List all Till Federation gists"
    echo "2) View current active gist"
    echo "3) Delete orphaned gists (keep active)"
    echo "4) Delete ALL Till gists"
    echo "5) Show gist details"
    echo "6) Exit"
    echo
    read -p "Choose [1-6]: " choice
    echo

    case $choice in
        1)
            echo -e "${BLUE}Your Till Federation Gists:${NC}"
            list_till_gists
            
            COUNT=$(gh gist list --limit 100 | grep -c "Till Federation")
            echo
            echo -e "Total: ${YELLOW}$COUNT${NC} gists"
            
            if [ -n "$CURRENT_GIST" ]; then
                echo -e "Active: ${GREEN}$CURRENT_GIST${NC}"
            else
                echo -e "Active: ${RED}None (not joined to federation)${NC}"
            fi
            ;;
            
        2)
            if [ -n "$CURRENT_GIST" ]; then
                echo -e "${BLUE}Current active gist: ${GREEN}$CURRENT_GIST${NC}"
                echo
                echo "Content:"
                gh gist view "$CURRENT_GIST" | jq . 2>/dev/null || gh gist view "$CURRENT_GIST"
                echo
                echo -e "View online: ${YELLOW}https://gist.github.com/$CURRENT_GIST${NC}"
            else
                echo -e "${RED}No active gist (not joined to federation)${NC}"
            fi
            ;;
            
        3)
            echo -e "${YELLOW}Finding orphaned gists...${NC}"
            
            if [ -z "$CURRENT_GIST" ]; then
                echo -e "${RED}Warning: No active gist found. All gists will be considered orphaned.${NC}"
                read -p "Continue? (y/N): " -n 1 -r
                echo
                if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                    continue
                fi
            fi
            
            ORPHANED=0
            for gist in $(gh gist list --limit 100 | grep "Till Federation" | cut -f1); do
                if [ "$gist" != "$CURRENT_GIST" ]; then
                    echo -e "Deleting orphaned gist: ${RED}$gist${NC}"
                    gh gist delete "$gist"
                    ((ORPHANED++))
                fi
            done
            
            if [ $ORPHANED -eq 0 ]; then
                echo -e "${GREEN}No orphaned gists found${NC}"
            else
                echo -e "${GREEN}Deleted $ORPHANED orphaned gist(s)${NC}"
            fi
            ;;
            
        4)
            COUNT=$(gh gist list --limit 100 | grep -c "Till Federation")
            
            if [ $COUNT -eq 0 ]; then
                echo -e "${GREEN}No Till gists to delete${NC}"
                continue
            fi
            
            echo -e "${RED}WARNING: This will delete ALL $COUNT Till Federation gists${NC}"
            echo "Gists to be deleted:"
            list_till_gists
            echo
            read -p "Are you sure? Type 'DELETE' to confirm: " confirm
            
            if [ "$confirm" = "DELETE" ]; then
                for gist in $(gh gist list --limit 100 | grep "Till Federation" | cut -f1); do
                    echo -e "Deleting: ${RED}$gist${NC}"
                    gh gist delete "$gist"
                done
                echo -e "${GREEN}All Till gists deleted${NC}"
            else
                echo "Cancelled"
            fi
            ;;
            
        5)
            read -p "Enter gist ID to view details: " gist_id
            if [ -n "$gist_id" ]; then
                echo
                echo -e "${BLUE}Gist Details for $gist_id:${NC}"
                gh gist view "$gist_id" | head -20
                echo
                echo -e "View full: ${YELLOW}gh gist view $gist_id${NC}"
                echo -e "View online: ${YELLOW}https://gist.github.com/$gist_id${NC}"
                echo -e "Delete: ${RED}gh gist delete $gist_id${NC}"
            fi
            ;;
            
        6)
            echo -e "${GREEN}Goodbye!${NC}"
            exit 0
            ;;
            
        *)
            echo -e "${RED}Invalid choice${NC}"
            ;;
    esac
    
    echo
    echo "Press Enter to continue..."
    read
    echo
done