#!/bin/bash
# Bootstrap script to install hooks on first clone
# This creates a post-checkout hook in .git/hooks that will auto-configure hooksPath
# Run this once after cloning, or add it to your setup process

set -e

REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null)

if [ -z "$REPO_ROOT" ]; then
    echo "Error: Not in a Git repository"
    exit 1
fi

cd "$REPO_ROOT"

# Create .git/hooks directory if it doesn't exist
mkdir -p .git/hooks

# Copy post-checkout hook to .git/hooks (one-time bootstrap)
if [ -f ".githooks/post-checkout" ]; then
    cp .githooks/post-checkout .git/hooks/post-checkout
    chmod +x .git/hooks/post-checkout
    echo "✅ Bootstrap hook installed"
    
    # Run it once to configure hooksPath
    .git/hooks/post-checkout "" "" ""
else
    echo "⚠️  .githooks/post-checkout not found"
    exit 1
fi

echo ""
echo "✅ Hooks are now configured!"
echo "   Future checkouts will automatically configure hooks."

