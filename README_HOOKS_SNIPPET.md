# README Snippet: Git Hooks Setup

Add this section to your README.md:

```markdown
## 🔒 Git Hooks Setup (Automatic!)

This repository **automatically configures Git hooks** when you clone or checkout. Hooks are enabled by default to prevent accidentally committing secrets or sensitive information.

### Automatic Setup ✅

**No action required!** Hooks are automatically configured via a post-checkout hook that runs after:
- `git clone`
- `git checkout`

The hooks will:
- ✅ Automatically configure `core.hooksPath` to `.githooks`
- ✅ Scan for passwords, API keys, secrets, and tokens
- ✅ Prevent accidental commits with sensitive information

### Manual Setup (If Needed)

If automatic setup doesn't work, run:

```bash
./setup-git-hooks.sh
```

Or manually:

```bash
# Configure Git to use repository hooks
git config core.hooksPath .githooks

# Or install pre-commit framework
pip install pre-commit
pre-commit install
pre-commit install --hook-type pre-push
```

### Verify Setup

Check if hooks are configured:

```bash
git config core.hooksPath
# Should output: .githooks
```

### ⚠️ Important Notes

- **Never use `--no-verify` flags** - This bypasses security checks
- **Never commit secrets** - Use environment variables or secrets managers
- Hooks will check for common patterns like passwords, API keys, tokens, etc.

### What Gets Checked

The hooks scan for:
- Passwords (`password = "..."`)
- API keys (`api_key = "..."`)
- Secrets (`secret = "..."`)
- Tokens (`token = "..."`)

### Troubleshooting

**Hooks not running?**
- Check: `git config core.hooksPath` (should be `.githooks`)
- Verify hooks are executable: `ls -la .githooks/`

**Need to bypass for legitimate reason?**
- Contact the repository maintainer first
- Use `--no-verify` only as a last resort
- Remember: secrets in Git history are permanent!

### Why This Matters

Once secrets are pushed to Git:
- They're in history forever (even if deleted)
- They're visible to anyone with repo access
- They may be in forks, clones, and backups
- **Always rotate exposed secrets immediately!**
```

