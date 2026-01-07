# Automatic Git Hooks Setup on Clone

This repository automatically configures Git hooks when you clone or checkout. This helps prevent accidentally committing secrets or sensitive information.

## How It Works

1. **Post-checkout hook**: Automatically runs after `git clone` or `git checkout`
2. **Auto-configuration**: Sets `core.hooksPath` to `.githooks` automatically
3. **No manual setup required**: Hooks are enabled by default

## What Gets Checked

The hooks automatically scan for:
- Passwords (`password = "..."`)
- API keys (`api_key = "..."`)
- Secrets (`secret = "..."`)
- Tokens (`token = "..."`)

## Manual Setup (If Needed)

If automatic setup doesn't work, run:

```bash
./setup-git-hooks.sh
```

Or manually:

```bash
git config core.hooksPath .githooks
```

## Verification

Check if hooks are configured:

```bash
git config core.hooksPath
# Should output: .githooks
```

Test the hooks:

```bash
# Try committing a test file with a password
echo 'password = "test123"' > test.txt
git add test.txt
git commit -m "test"
# Should be rejected by pre-commit hook
```

## Bypassing Hooks (Not Recommended!)

⚠️ **You can still bypass hooks, but DON'T!**

```bash
git commit --no-verify    # Bypasses pre-commit
git push --no-verify      # Bypasses pre-push
```

**Why this is dangerous:**
- Secrets in Git history are permanent
- They're visible to anyone with repo access
- They may be in forks, clones, and backups
- **Always rotate exposed secrets immediately!**

## How the Auto-Setup Works

### Method 1: Post-Checkout Hook (Automatic)

The `.githooks/post-checkout` hook:
1. Runs automatically after clone/checkout
2. Checks if `core.hooksPath` is configured
3. If not, configures it automatically
4. Makes all hooks executable

### Method 2: Bootstrap Script (One-Time)

For first-time setup, you can run:

```bash
./install-hooks-bootstrap.sh
```

This installs the post-checkout hook in `.git/hooks/` which then auto-configures everything.

### Method 3: Setup Script (Manual)

Run the full setup script:

```bash
./setup-git-hooks.sh
```

This:
- Configures `core.hooksPath`
- Installs bootstrap hooks
- Sets up pre-commit framework (if available)

## Troubleshooting

### Hooks not running?

1. **Check configuration:**
   ```bash
   git config core.hooksPath
   # Should output: .githooks
   ```

2. **Check if hooks exist:**
   ```bash
   ls -la .githooks/
   # Should show pre-commit, pre-push, post-checkout
   ```

3. **Check if hooks are executable:**
   ```bash
   ls -la .githooks/pre-commit
   # Should show -rwxr-xr-x (executable)
   ```

4. **Re-run setup:**
   ```bash
   ./setup-git-hooks.sh
   ```

### Post-checkout hook not running?

The post-checkout hook needs to be in `.git/hooks/` to run. It should be automatically installed, but you can manually install it:

```bash
./install-hooks-bootstrap.sh
```

Or manually:

```bash
cp .githooks/post-checkout .git/hooks/post-checkout
chmod +x .git/hooks/post-checkout
```

### Git version too old?

If you're using Git < 2.9, `core.hooksPath` isn't supported. The setup script will fall back to copying hooks to `.git/hooks/`, but they won't be automatically updated from the repository.

**Solution:** Upgrade Git to 2.9+ for full support.

## Technical Details

### Why Post-Checkout?

- Runs automatically on `git clone` and `git checkout`
- Can configure hooksPath before other hooks run
- Works for both new clones and existing checkouts

### Limitations

- Users can still bypass with `--no-verify`
- Users can delete hooks locally
- Users can unset `core.hooksPath`
- **Not foolproof, but prevents accidents**

### Best Practices

1. **Never commit secrets** - Use environment variables or secrets managers
2. **Don't bypass hooks** - If you need to, there's probably a problem
3. **Run setup script** - After cloning, run `./setup-git-hooks.sh` to be sure
4. **Verify configuration** - Check `git config core.hooksPath` periodically

## Integration with CI/CD

Even with automatic hooks setup, you should still:
- Use CI/CD to catch secrets (as backup)
- Scan Git history regularly
- Use branch protection rules
- Require PR reviews

See `FORCE_PRECOMMIT_GUIDE.md` for more details.

