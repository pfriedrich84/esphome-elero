#!/usr/bin/env bash
# Apply GitHub branch protection rules to the 'main' branch.
#
# Prerequisites:
#   - gh CLI installed and authenticated as a repo admin
#   - Run: gh auth status   (to verify)
#
# Usage:
#   bash .github/scripts/protect-main.sh
#   bash .github/scripts/protect-main.sh owner/repo   # explicit repo
#
# This script is idempotent — re-running it updates the settings to the
# desired state (PUT replaces the full protection config).

set -euo pipefail

REPO="${1:-$(gh repo view --json nameWithOwner -q .nameWithOwner 2>/dev/null)}"

if [[ -z "${REPO}" ]]; then
  echo "Error: Could not determine repository."
  echo "Usage: $0 [owner/repo]"
  exit 1
fi

BRANCH="main"

echo "Applying branch protection to ${REPO} branch: ${BRANCH}"
echo ""

gh api \
  --method PUT \
  "/repos/${REPO}/branches/${BRANCH}/protection" \
  --input - <<'JSON'
{
  "required_status_checks": {
    "strict": true,
    "contexts": ["ci-ok"]
  },
  "enforce_admins": false,
  "required_pull_request_reviews": null,
  "restrictions": null,
  "required_linear_history": false,
  "allow_force_pushes": false,
  "allow_deletions": false
}
JSON

echo ""
echo "Branch protection applied successfully."
echo ""
echo "Settings:"
echo "  Required status check: ci-ok (branch must be up-to-date)"
echo "  PR review requirement: none"
echo "  Admin bypass:          allowed"
echo "  Force push:            blocked"
echo "  Branch deletion:       blocked"
echo ""
echo "Verify at: https://github.com/${REPO}/settings/branches"
