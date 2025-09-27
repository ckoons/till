#
# till
#
  Prepare your ground, plant your seeds, share your harvest.

  ## What is till?

  till is a lightweight configuration and deployment tool that manages
  infrastructure through a single JSON file and a simple C program. Like
  tilling soil before planting, till prepares your systems for deployment
  with minimal complexity and maximum clarity.

  ## Philosophy

  - **One JSON file** - Complete configuration truth
  - **One C program** - No dependencies, no frameworks
  - **One command** - Deploy or hold, your choice
  - **One federation** - Share if you want, or work alone

  ## Installation

  ```bash
  git clone https://github.com/tekton/till
  cd till
  make
  
  # Install globally (requires sudo)
  sudo make install
  
  # OR install locally (no sudo required)
  make install-user
  # Then add ~/.local/bin to your PATH
  ```

  That's it. No package managers, no virtual environments, no containers
  required.

  Quick Start

  Solo Mode

  # Initialize your field

  # Deploy everything
  till deploy

  # Hold production 
  till hold production

  Federation Mode

  # Join the federation

  # Pull shared seeds
  till pull

  # Share your harvest
  till push

  Core Commands

  till status                    # What's growing?
  till install tekton            # Install new Tekton instance
  till sync                      # Update till and all Tektons
  till repair                    # Check and fix configuration issues
  till host add <name> <user>@<host>  # Add remote host
  till host sync                 # Sync hosts across all machines
  till deploy [target]           # Plant seeds
  till hold [target]             # Let field lie fallow
  till release [target]          # Remove holds
  till pull                      # Get federation updates
  till push                      # Share your config
  till menu add <component> <repo> [version] [availability]  # Add to menu
  till menu remove <component>   # Remove from menu

  Scheduling

  Natural language scheduling - no cron syntax needed:

  till thursday deploy production
  till "june 11 10:30" hold staging
  till tomorrow release development
  till "next week" pull federation

  Configuration

  Everything lives in till.json:

  {
    "version": "1.0.0",
    "site": "my-farm",
    "deployments": {
      "production": {
        "state": "DEPLOY",
        "components": ["aish", "numa", "engram"]
      },
      "staging": {
        "state": "HOLD",
        "hold_info": {
          "who": "casey",
          "why": "testing new features",
          "when": "2025-01-12T10:00:00Z"
        }
      }
    }
  }

  The HOLD Mechanism

  Any object can be held, preventing automatic deployment:

  till hold production --reason "quarterly review"
  till holds --verbose              # Review all holds
  till release production            # Remove hold

  Holds are sacred - till will never deploy held objects until explicitly
  released.

  Federation

  Optional participation in the Tekton federation:

  1. Anonymous - Pull configurations, never report back
  2. Named - Visible in federation, basic sharing
  3. Trusted - Full bidirectional configuration sharing

  Join at your comfort level, leave anytime.

  Design Principles

  1. Simplicity First - If it's not simple, it's not included
  2. User Sovereignty - Your systems, your rules
  3. Transparent Operation - One JSON file you can read and edit
  4. Zero Lock-in - Delete till, keep your JSON, nothing breaks
  5. Federation not Empire - Share by choice, never by force

  Why till?

  - No Dependencies - Single binary, runs everywhere
  - No Complexity - Read the entire source in an afternoon
  - No Vendor Lock - Your config is just JSON
  - No Surprises - What you configure is what deploys
  - No Magic - Everything till does is visible and auditable

  Building from Source

  # Requirements: C compiler, make
  make clean
  make
  make test
  sudo make install

  # Uninstall completely
  sudo make uninstall

  Federation Registry

  To join the federation, submit a PR to add your site:

  {
    "site_name": "your-site",
    "public_key": "optional-public-key",
    "trust_level": "anonymous|named|trusted"
  }

  Configuration Examples

  See examples/ for:
  - Single machine deployment
  - Multi-site federation
  - CI/CD integration
  - Scheduled deployments
  - Emergency rollback procedures

  FAQ

  Q: Why C?A: No dependencies, runs everywhere, you can audit the entire
  codebase.

  Q: Why not YAML?A: JSON is sufficient, parseable by everything, and
  doesn't pretend to be human-friendly while being a footgun.

  Q: Is this production-ready?A: till deploys real infrastructure daily.
  Simple tools are production-ready by default.

  Q: How does this compare to Kubernetes?A: till is 10KB. Kubernetes is
  10GB. Both deploy containers. You choose.

  Support

  - Issues: https://github.com/tekton/till/issues
  - Federation: https://tekton.ai/federation
  - Philosophy: Read the source, it's shorter than most documentation

  License

  MIT - Use freely, modify freely, share freely.

  Credits

  Built for the Tekton Federation by engineers who prefer working tools
  over working groups.

  "Infrastructure as code, the way it was meant to be - simple, visible, 
  and under your control."

  ---
  Remember: Like tilling soil, deploying infrastructure should be simple,
  repeatable, and under your complete control. till gives you exactly that,
   nothing more, nothing less.
  ```
:
