# Guide: Forcing Pre-commit Checks Before Pushes

This guide explains how to prevent users from pushing code unless pre-commit checks pass, especially to catch passwords and sensitive information.

## Problem
- Pre-commit hooks exist in the repo but aren't always enabled locally
- Users might push code with passwords/secrets
- By the time it reaches the repo, it's too late (secrets are in Git history)

## ⚠️ CRITICAL: Preventing Secret Leaks vs. Blocking Merges

**IMPORTANT DISTINCTION:**
- **Pre-receive hooks**: Run BEFORE the commit enters the repository → **Secrets never leak**
- **CI/CD workflows**: Run AFTER the commit is pushed → **Secrets are already in the repo!**

If your goal is to **prevent secrets from ever entering the repository**, you MUST use a server-side pre-receive hook. CI/CD workflows can block merges, but the commit with secrets is already visible to anyone with repository access.

## Solutions (Ranked by Effectiveness)

### 1. Server-Side Pre-Receive Hook ⭐⭐⭐ **ONLY TRUE PREVENTION**

**Pros:**
- Cannot be bypassed by users
- Runs on the server BEFORE accepting any push
- **Commit is rejected before entering the repository** → Secrets never leak
- Works regardless of local Git configuration

**Cons:**
- Requires access to the Git server
- Need to maintain hooks on the server
- Limited options for cloud Git hosts (see below)

**Implementation:**
1. **Self-hosted Git servers** (GitLab, Gitea, Gogs, etc.):
   - Add a pre-receive hook in the repository's `hooks/` directory
   - See `pre-receive-hook-example.sh` for an example
   - Hook runs before commit is accepted → secrets never enter repo

2. **GitHub Cloud**:
   - ❌ **No native pre-receive hooks** for regular repositories
   - ✅ **GitHub Enterprise Server** supports pre-receive hooks
   - ✅ **GitHub Apps** with push event webhooks (but runs after push)
   - ⚠️ Consider using **branch protection** + **required status checks** (but commit still enters repo)

3. **GitLab Cloud**:
   - ✅ **Push Rules** (Premium/Ultimate): Can reject pushes before acceptance
   - ✅ **Webhooks** can be configured but typically run after push
   - ✅ **GitLab Enterprise** supports pre-receive hooks

4. **Bitbucket Cloud**:
   - ❌ Limited server-side hook support
   - ✅ **Bitbucket Server** supports pre-receive hooks

### 2. CI/CD Workflows (GitHub Actions, GitLab CI) ⚠️ **DOES NOT PREVENT LEAKS**

**Pros:**
- Works with GitHub, GitLab, Bitbucket cloud
- Can block merges via branch protection rules
- Easy to set up and maintain
- Good for catching issues in PRs

**Cons:**
- ⚠️ **Code is pushed FIRST, then checked** → **Secrets are already in the repository!**
- Commit is visible in the PR/branch even if merge is blocked
- Anyone with repo access can see the commit with secrets
- Requires branch protection rules to be effective
- **NOT suitable for preventing secret leaks** - only for blocking merges

**When to use:**
- As a secondary defense layer
- For code quality checks (not secret prevention)
- When you can't use pre-receive hooks

**Implementation:**
1. Create a workflow file (see `github-actions-example.yml`)
2. Enable branch protection rules:
   - Settings → Branches → Add rule
   - Require status checks to pass
   - Require the pre-commit check workflow

### 3. Client-Side Pre-Push Hook ⚠️ **CAN BE BYPASSED**

**Pros:**
- Catches issues before they reach the server
- Good user experience (fast feedback)

**Cons:**
- Can be bypassed with `git push --no-verify`
- Users can delete the hook locally
- Not foolproof

**Implementation:**
- See `pre-push-hook-example.sh`
- Install with: `cp pre-push-hook-example.sh .git/hooks/pre-push && chmod +x .git/hooks/pre-push`
- Or add to your repo setup script

### 4. Pre-Commit Hook with Secrets Detection 🔍 **DETECTION ONLY**

**Pros:**
- Runs automatically on commit
- Good for local development

**Cons:**
- Can be bypassed with `git commit --no-verify`
- Users must have it installed

**Implementation:**
```yaml
# .pre-commit-config.yaml
repos:
  - repo: https://github.com/Yelp/detect-secrets
    rev: v1.4.0
    hooks:
      - id: detect-secrets
        args: ['--baseline', '.secrets.baseline']
```

## Recommended Approach: Multi-Layer Defense

**For Secret Prevention (in order of importance):**
1. **Pre-receive hook** (server-side): ⭐ **ONLY way to truly prevent leaks** - blocks commit before it enters repo
2. **Pre-push hook** (client-side): Catches issues before push, but can be bypassed
3. **Pre-commit hook** (client-side): Catches issues early, good UX, but can be bypassed

**For Code Quality (secondary):**
4. **CI/CD workflows**: Blocks merges, but commit is already in repo (secrets already leaked if present)

**Note:** If you're on GitHub/GitLab cloud without Enterprise, you cannot truly prevent commits from entering the repository. Your options are:
- Use pre-push hooks and rely on team discipline (not foolproof)
- Migrate to a self-hosted solution with pre-receive hooks
- Use GitLab Premium/Ultimate Push Rules
- Accept that CI/CD will catch it after the fact (and secrets may leak)

## Tools for Secret Detection

- **detect-secrets**: Yelp's tool for finding secrets
- **git-secrets**: AWS's tool
- **truffleHog**: Finds secrets in Git history
- **gitleaks**: Fast secret scanner

## Example: Complete Setup

### Step 1: Add pre-commit config
```yaml
# .pre-commit-config.yaml
repos:
  - repo: https://github.com/Yelp/detect-secrets
    rev: v1.4.0
    hooks:
      - id: detect-secrets
```

### Step 2: Add GitHub Actions workflow
```yaml
# .github/workflows/pre-commit.yml
name: Pre-commit
on: [push, pull_request]
jobs:
  pre-commit:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v4
      - run: pip install pre-commit
      - run: pre-commit run --all-files
```

### Step 3: Enable branch protection
- GitHub: Settings → Branches → Require status checks
- GitLab: Settings → Repository → Protected branches

### Step 4: (Optional) Add pre-push hook
- Include in your repo setup/onboarding documentation
- Or add to a setup script that runs automatically

## Notes

- **Git history**: Once secrets are pushed, they're in history FOREVER. Even if you:
  - Force-push to remove them
  - Use `git-filter-repo` to rewrite history
  - Delete the repository
  The secrets may still exist in:
  - Local clones on other machines
  - Forked repositories
  - Git server backups
  - CI/CD logs and artifacts
  - **Always rotate exposed secrets immediately!**

- **Bypassing hooks**: 
  - Client-side hooks can be bypassed with `--no-verify`
  - Server-side pre-receive hooks **cannot be bypassed** (this is why they're critical)

- **Performance**: Server-side hooks should be fast (< 1 second ideally); consider:
  - Caching results
  - Async processing for heavy checks
  - Only checking changed files, not entire repo

## Cloud Git Host Limitations

**GitHub Cloud:**
- ❌ No pre-receive hooks for regular repos
- ✅ Only GitHub Enterprise Server supports pre-receive hooks
- ⚠️ Best you can do: Pre-push hooks + CI/CD (but secrets still leak if pushed)

**GitLab Cloud:**
- ✅ **Push Rules** (Premium/Ultimate) can reject pushes
- ✅ GitLab Enterprise supports pre-receive hooks
- ⚠️ Free tier: Limited to CI/CD (secrets leak if pushed)

**Bitbucket Cloud:**
- ❌ No pre-receive hooks
- ✅ Bitbucket Server supports pre-receive hooks

**Recommendation for Cloud Hosts:**
If you absolutely must prevent secret leaks and you're on a cloud host without pre-receive hook support, consider:
1. Self-hosting your Git server (GitLab, Gitea, etc.)
2. Using GitLab Premium/Ultimate for Push Rules
3. Using GitHub Enterprise Server
4. Accepting the risk and relying on pre-push hooks + team discipline

