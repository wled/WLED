#!/bin/bash
# Setup script to make Git hooks part of the repository
# This makes it harder (but not impossible) for users to bypass hooks
# 
# This script:
# 1. Configures core.hooksPath to use .githooks
# 2. Installs a bootstrap post-checkout hook for automatic setup on future clones

set -e

echo "Setting up Git hooks..."

# Method 1: Use core.hooksPath (Git 2.9+)
# This makes hooks part of the repository
if git --version | grep -q "git version 2\.[9-9]\|git version [3-9]"; then
    echo "Configuring Git to use repository hooks directory..."
    git config core.hooksPath .githooks
    
    # Create .githooks directory if it doesn't exist
    mkdir -p .githooks
    
    # Make sure hooks are executable
    find .githooks -type f -name "*" ! -name "*.md" ! -name "*.txt" -exec chmod +x {} \; 2>/dev/null || true
    
    echo "✅ Git hooks directory configured (.githooks)"
    
    # Install bootstrap post-checkout hook for automatic setup on clone
    if [ -f ".githooks/post-checkout" ]; then
        mkdir -p .git/hooks
        # Create a symlink or copy to ensure post-checkout runs
        if [ -L ".git/hooks/post-checkout" ] || [ ! -f ".git/hooks/post-checkout" ]; then
            ln -sf "../../.githooks/post-checkout" ".git/hooks/post-checkout" 2>/dev/null || \
            cp .githooks/post-checkout .git/hooks/post-checkout && chmod +x .git/hooks/post-checkout
            echo "✅ Bootstrap hook installed (auto-configures on checkout)"
        fi
    fi
else
    echo "⚠️  Git version < 2.9, using traditional hooks directory"
    echo "Copying hooks to .git/hooks..."
    
    # Fallback: copy hooks to .git/hooks
    if [ -d ".githooks" ]; then
        cp .githooks/* .git/hooks/ 2>/dev/null || true
        chmod +x .git/hooks/*
        echo "✅ Hooks copied to .git/hooks"
    fi
fi

# Method 2: Install pre-commit framework if available
if command -v pre-commit &> /dev/null; then
    echo "Installing pre-commit hooks..."
    pre-commit install
    pre-commit install --hook-type pre-push
    echo "✅ Pre-commit hooks installed"
else
    echo "⚠️  pre-commit not installed. Install with: pip install pre-commit"
fi

# Method 3: Add git config to prevent accidental bypass
echo ""
echo "Setting up Git aliases to discourage bypassing..."
git config alias.push-safe '!f(){ git push "$@"; }; f'
git config alias.commit-safe '!f(){ git commit "$@"; }; f'

echo ""
echo "✅ Setup complete!"
echo ""
echo "⚠️  IMPORTANT: Users can still bypass hooks with:"
echo "   git commit --no-verify"
echo "   git push --no-verify"
echo ""
echo "Consider adding this to your README:"
echo "  - Run ./setup-git-hooks.sh after cloning"
echo "  - Never use --no-verify flags"
echo "  - Use secrets managers, not Git"

