#!/bin/bash
# Example pre-receive hook for Git server
# Place this in your Git repository's hooks/ directory on the server
# Make it executable: chmod +x pre-receive-hook-example.sh

# This hook runs on the server before accepting any push
# It can check for passwords, secrets, or run pre-commit checks

while read oldrev newrev refname; do
    # Get the list of commits being pushed
    commits=$(git rev-list $oldrev..$newrev)
    
    for commit in $commits; do
        # Check each commit for sensitive information
        # Example: Check for common password patterns
        if git diff-tree --no-commit-id --name-only -r $commit | xargs grep -i "password\|secret\|api_key" 2>/dev/null; then
            echo "ERROR: Potential sensitive information detected in commit $commit"
            echo "Please remove passwords/secrets before pushing."
            exit 1
        fi
        
        # Alternatively, run pre-commit checks
        # This requires pre-commit to be installed on the server
        # git checkout $commit
        # pre-commit run --all-files
        # if [ $? -ne 0 ]; then
        #     echo "ERROR: Pre-commit checks failed for commit $commit"
        #     exit 1
        # fi
    done
done

exit 0

