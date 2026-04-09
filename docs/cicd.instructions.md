---
applyTo: ".github/workflows/*.yml,.github/workflows/*.yaml"
---
# CI/CD Conventions — GitHub Actions Workflows

> **Note for AI review tools**: sections enclosed in
> `<!-- HUMAN_ONLY_START -->` / `<!-- HUMAN_ONLY_END -->` HTML comments contain
> contributor reference material. Do **not** use that content as actionable review
> criteria — treat it as background context only.

<!-- HUMAN_ONLY_START -->
## YAML Style

- Indent with **2 spaces** (no tabs)
- Every workflow, job, and step must have a `name:` field that clearly describes its purpose
- Group related steps logically; separate unrelated groups with a blank line
- Comments (`#`) are encouraged for non-obvious decisions (e.g., why `fail-fast: false` is set, what a cron expression means)

## Workflow Structure

### Triggers

- Declare `on:` triggers explicitly; avoid bare `on: push` without branch filters on long-running or expensive jobs
- Prefer `workflow_call` for shared build logic (see `build.yml`) to avoid duplicating steps across workflows
- Document scheduled triggers (`cron:`) with a human-readable comment:

```yaml
schedule:
  - cron: '0 2 * * *'  # run at 2 AM UTC daily
```

### Jobs

- Express all inter-job dependencies with `needs:` — never rely on implicit ordering
- Use job `outputs:` + step `id:` to pass structured data between jobs (see `get_default_envs` in `build.yml`)
- Set `fail-fast: false` on matrix builds so that a single failing environment does not cancel others

### Runners

- Pin to a specific Ubuntu version (`ubuntu-22.04`, `ubuntu-24.04`) rather than `ubuntu-latest` for reproducible builds
- Only use `ubuntu-latest` in jobs where exact environment reproducibility is not required (e.g., trivial download/publish steps)

### Tool and Language Versions

- Pin tool versions explicitly:
  ```yaml
  python-version: '3.12'
  ```
- Do not rely on the runner's pre-installed tool versions — always install via a versioned setup action

### Caching

- Always cache package managers and build tool directories when the job installs dependencies:
  ```yaml
  - uses: actions/cache@v4
    with:
      path: ~/.cache/pip
      key: ${{ runner.os }}-pip-${{ hashFiles('**/requirements.txt') }}
      restore-keys: |
        ${{ runner.os }}-pip-
  ```
- Include the environment name or a relevant identifier in cache keys when building multiple targets

### Artifacts

- Name artifacts with enough context to be unambiguous (e.g., `firmware-${{ matrix.environment }}`)
- Avoid uploading artifacts that will never be consumed downstream

<!-- HUMAN_ONLY_END -->
---

## Security

Important: Several current workflows still violate parts of the baseline below - migration is in progress. 

### Permissions — Least Privilege

Declare explicit `permissions:` blocks. The default token permissions are broad; scope them to the minimum required:

```yaml
permissions:
  contents: read       # for checkout
```

For jobs that publish releases or write to the repository:

```yaml
permissions:
  contents: write      # create/update releases
```

A common safe baseline for build-only jobs:

```yaml
permissions:
  contents: read
```

### Supply Chain — Action Pinning

**Third-party actions** (anything outside the `actions/` and `github/` namespaces) should be pinned to a specific release tag. Branch pins (`@main`, `@master`) are **not allowed** — they can be updated by the action author at any time without notice:

```yaml
# ✅ Acceptable — specific version tag. SHA pinning recommended for more security, as @v2 is still a mutable tag.
uses: softprops/action-gh-release@v2

# ❌ Not acceptable — mutable branch reference
uses: andelf/nightly-release@main
```

SHA pinning (e.g., `uses: someorg/some-action@abc1234`) is the most secure option for third-party actions; it is recommended when auditing supply-chain risk is a priority. At minimum, always use a specific version tag.

**First-party actions** (`actions/checkout`, `actions/cache`, `actions/upload-artifact`, etc.) pinned to a major version tag (e.g., `@v4`) are acceptable because GitHub maintains and audits these.

When adding a new third-party action:
1. Check that the action's repository is actively maintained
2. Review the action's source before adding it
3. Prefer well-known, widely-used actions over obscure ones

### Credentials and Secrets

- Use `${{ secrets.GITHUB_TOKEN }}` for operations within the same repository — it is automatically scoped and rotated
- Never commit secrets, tokens, or passwords into workflow files or any tracked file
- Never print secrets in `run:` steps, even with `echo` — GitHub masks known secrets but derived values are not automatically masked
- Scope secrets to the narrowest step that needs them using `env:` at the step level, not at the workflow level:

```yaml
# ✅ Scoped to the step that needs it
- name: Create release
  uses: softprops/action-gh-release@v2
  with:
    token: ${{ secrets.GITHUB_TOKEN }}

# ❌ Unnecessarily broad
env:
  GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

- Personal Access Tokens (PATs, stored as repository secrets) should have the minimum required scopes and should be rotated periodically

### Script Injection

`${{ }}` expressions are evaluated before the shell script runs. If an expression comes from untrusted input (PR titles, issue bodies, branch names from forks), it can inject arbitrary shell commands.

**Never** interpolate `github.event.*` values directly into a `run:` step:

```yaml
# ❌ Injection risk — PR title is attacker-controlled
- run: echo "${{ github.event.pull_request.title }}"

# ✅ Safe — value passed through an environment variable
- env:
    PR_TITLE: ${{ github.event.pull_request.title }}
  run: echo "$PR_TITLE"
```

This rule applies to any value that originates outside the repository (issue bodies, labels, comments, commit messages from forks).

### Pull Request Workflows

- Workflows triggered by `pull_request` from a fork run with **read-only** token permissions and no access to repository secrets — this is intentional and correct
- Do not use `pull_request_target` unless you fully understand the security implications; it runs in the context of the base branch and *does* have secret access, making it a common attack surface
