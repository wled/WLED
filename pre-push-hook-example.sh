#!/bin/bash
# Example pre-push hook (client-side)
# Place this in your repository's .git/hooks/pre-push
# Make it executable: chmod +x .git/hooks/pre-push
# 
# Note: This can be bypassed with --no-verify, so it's not foolproof
# But it helps catch issues before they reach the server

# Check if pre-commit is installed and enabled
if ! command -v pre-commit &> /dev/null; then
    echo "ERROR: pre-commit is not installed!"
    echo "Please install it with: pip install pre-commit"
    echo "Then enable it with: pre-commit install"
    exit 1
fi

# Check if pre-commit hooks are actually installed
if [ ! -f .git/hooks/pre-commit ]; then
    echo "ERROR: pre-commit hooks are not installed!"
    echo "Please run: pre-commit install"
    exit 1
fi

# Run pre-commit on all files that would be pushed
remote="$1"
url="$2"

z40=0000000000000000000000000000000000000000

while read local_ref local_sha remote_ref remote_sha
do
    if [ "$local_sha" = $z40 ]; then
        # Handle delete
        :
    else
        if [ "$remote_sha" = $z40 ]; then
            # New branch, examine all commits
            range="$local_sha"
        else
            # Update to existing branch, examine new commits
            range="$remote_sha..$local_sha"
        fi
        
        # Get list of files changed
        files=$(git diff --name-only $range)
        
        # Run pre-commit on changed files
        echo "Running pre-commit checks on pushed changes..."
        for file in $files; do
            if [ -f "$file" ]; then
                pre-commit run --files "$file"
                if [ $? -ne 0 ]; then
                    echo "ERROR: Pre-commit checks failed for $file"
                    echo "Please fix the issues before pushing."
                    exit 1
                fi
            fi
        done
    fi
done

exit 0

