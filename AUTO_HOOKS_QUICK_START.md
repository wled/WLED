# Quick Start: Automatic Git Hooks on Clone

## The Solution

**Yes! You can automatically enable hooks on clone.** Here's how it works:

### How It Works

1. **Post-checkout hook** (`.githooks/post-checkout`) is version controlled
2. **Bootstrap installation** copies it to `.git/hooks/post-checkout` on first setup
3. **Auto-configuration** runs after every `git clone` or `git checkout`
4. **Sets `core.hooksPath`** to `.githooks` automatically

### Result

✅ **Hooks are enabled automatically on clone**  
✅ **No manual setup required**  
⚠️ **Users can still disable, but prevents accidents**

## Setup for Your Repo

### Step 1: Add the Files

1. Copy `.githooks/` directory to your repo
2. Copy `setup-git-hooks.sh` to your repo
3. Copy `install-hooks-bootstrap.sh` to your repo (optional)

### Step 2: First-Time Setup (One-Time)

For existing clones, run once:

```bash
./setup-git-hooks.sh
```

This installs the bootstrap post-checkout hook.

### Step 3: New Clones (Automatic!)

For new clones, hooks are automatically configured:
- User runs `git clone <repo>`
- Post-checkout hook runs automatically
- `core.hooksPath` is set to `.githooks`
- Hooks are ready to use!

## How Users Experience It

### New Clone (Automatic)

```bash
$ git clone https://github.com/yourorg/yourrepo
Cloning into 'yourrepo'...
✅ Git hooks automatically configured!
   Hooks directory: .githooks
   Note: You can still bypass with --no-verify, but please don't!
```

### Existing Clone (One-Time Setup)

```bash
$ ./setup-git-hooks.sh
Setting up Git hooks...
Configuring Git to use repository hooks directory...
✅ Git hooks directory configured (.githooks)
✅ Bootstrap hook installed (auto-configures on checkout)
```

### Verification

```bash
$ git config core.hooksPath
.githooks

$ git commit -m "test"
Running pre-commit checks...
✅ Pre-commit checks passed
```

## What Gets Checked

The hooks automatically scan for:
- `password = "..."` or `password: "..."`
- `api_key = "..."` or `api_key: "..."`
- `secret = "..."` or `secret: "..."`
- `token = "..."` or `token: "..."`

## Limitations

⚠️ **Users can still bypass:**

```bash
git commit --no-verify    # Bypasses pre-commit
git push --no-verify      # Bypasses pre-push
git config --unset core.hooksPath  # Disables hooks
```

**But this prevents accidents!** Most users won't bypass unless they really need to.

## Files You Need

1. **`.githooks/post-checkout`** - Auto-configures hooks on clone/checkout
2. **`.githooks/pre-commit`** - Checks for secrets before commit
3. **`.githooks/pre-push`** - Checks for secrets before push
4. **`setup-git-hooks.sh`** - Manual setup script (optional but recommended)

## Testing

Test that it works:

```bash
# Clone a fresh copy
cd /tmp
git clone <your-repo> test-clone
cd test-clone

# Check if hooks are configured
git config core.hooksPath
# Should output: .githooks

# Try committing a secret (should be rejected)
echo 'password = "test123"' > test.txt
git add test.txt
git commit -m "test"
# Should be rejected!
```

## Integration

Add to your README:

```markdown
## Setup

This repository automatically configures Git hooks on clone.
No manual setup required! Hooks prevent accidentally committing secrets.

If hooks aren't working, run: `./setup-git-hooks.sh`
```

## Summary

✅ **Automatic on clone** - Post-checkout hook configures everything  
✅ **Prevents accidents** - Most users won't bypass  
⚠️ **Not foolproof** - Determined users can still bypass  
✅ **Best effort** - Much better than requiring manual setup!

This is the best you can do without self-hosting or premium licenses!

