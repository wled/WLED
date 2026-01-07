# Git Hooks Directory

This directory contains Git hooks that are automatically configured when you clone or checkout this repository.

## Automatic Setup

The `post-checkout` hook automatically:
1. Configures `core.hooksPath` to `.githooks`
2. Makes all hooks executable
3. Runs on every `git clone` and `git checkout`

## Available Hooks

- **pre-commit**: Checks for secrets before committing
- **pre-push**: Checks for secrets before pushing
- **post-checkout**: Auto-configures hooks on clone/checkout

## Manual Installation

If automatic setup doesn't work:

```bash
git config core.hooksPath .githooks
chmod +x .githooks/*
```

## Bypassing Hooks

⚠️ **You can bypass hooks, but DON'T!**

```bash
git commit --no-verify    # Bypasses pre-commit
git push --no-verify      # Bypasses pre-push
```

**Why this is dangerous:**
- Secrets in Git history are permanent
- They're visible to anyone with repo access
- **Always rotate exposed secrets immediately!**

