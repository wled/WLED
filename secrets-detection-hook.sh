#!/bin/bash
# Enhanced hook that specifically checks for passwords and secrets
# Can be used as pre-receive, pre-push, or pre-commit hook

# Patterns to detect (add more as needed)
PATTERNS=(
    "password\s*[:=]\s*['\"]?[^'\"]+['\"]?"
    "api[_-]?key\s*[:=]\s*['\"]?[^'\"]+['\"]?"
    "secret\s*[:=]\s*['\"]?[^'\"]+['\"]?"
    "token\s*[:=]\s*['\"]?[^'\"]+['\"]?"
    "aws[_-]?access[_-]?key"
    "private[_-]?key"
)

# Files to check (or check all files)
FILES_TO_CHECK="${1:-$(git diff --cached --name-only --diff-filter=ACM)}"

ERRORS=0

for file in $FILES_TO_CHECK; do
    # Skip binary files
    if git check-attr --all "$file" | grep -q "binary: set"; then
        continue
    fi
    
    # Check each pattern
    for pattern in "${PATTERNS[@]}"; do
        if git diff --cached "$file" 2>/dev/null | grep -iE "$pattern" > /dev/null 2>&1; then
            echo "ERROR: Potential secret detected in $file"
            echo "Pattern matched: $pattern"
            ERRORS=$((ERRORS + 1))
        fi
    done
done

if [ $ERRORS -gt 0 ]; then
    echo ""
    echo "Please remove sensitive information before committing."
    echo "Consider using environment variables or a secrets manager."
    exit 1
fi

exit 0

