# Fork Statistics Analysis Tool

This tool analyzes GitHub repository forks to provide insights into fork activity and health for the WLED project.

## Features

The script analyzes and reports on:

- **Branch Analysis**: Which forks have branches that do not exist in the main repo
- **Recency Analysis**: Which forks have recent versions of main vs outdated forks  
- **Contribution Analysis**: Which fork repos have been the source of PRs into the main repo
- **Activity Detection**: Which forks have active development but haven't contributed PRs
- **Owner Commit Analysis**: Statistics about commits made by fork owners to their own repositories
- **Age Statistics**: Distribution of how far behind forks are (1 month, 3 months, 6 months, 1 year, 2+ years)
- **Incremental Saving**: Automatically saves intermediate results every 10 forks to prevent data loss

## Requirements

- Python 3.7+
- `requests` library (included in WLED requirements.txt)
- GitHub personal access token (recommended for analyzing large numbers of forks)

## Usage

### Quick Demo

To see what the output looks like with sample data:

```bash
python3 tools/fork_stats.py --demo
```

### Basic Analysis (Rate Limited)

Analyze the first 10 forks without a token (uses GitHub's unauthenticated API with 60 requests/hour limit):

```bash
python3 tools/fork_stats.py --max-forks 10
```

### Full Analysis with Token

For comprehensive analysis, create a GitHub personal access token:

1. Go to GitHub Settings > Developer settings > Personal access tokens > Tokens (classic)
2. Generate a new token with `public_repo` scope
3. Set the token as an environment variable:

```bash
export GITHUB_TOKEN="your_token_here"
python3 tools/fork_stats.py
```

Or pass it directly:

```bash
python3 tools/fork_stats.py --token "your_token_here"
```

### Advanced Options

```bash
# Analyze specific repository
python3 tools/fork_stats.py --repo owner/repo

# Limit number of forks analyzed
python3 tools/fork_stats.py --max-forks 50

# Save detailed JSON results
python3 tools/fork_stats.py --output results.json

# Check what would be analyzed without making API calls
python3 tools/fork_stats.py --dry-run

# Different output format
python3 tools/fork_stats.py --format json
```

## Output

### Summary Format (Default)

The tool provides a human-readable summary including:

- Repository statistics (total forks, stars, watchers)
- Fork age distribution showing staleness
- Activity analysis showing contribution patterns
- Key insights about fork health

### JSON Format

Detailed machine-readable output including:

- Complete fork metadata for each analyzed fork
- Branch information and unique branches
- Contribution history and activity metrics
- Owner commit statistics for each fork
- Full statistical breakdown
- Intermediate results are automatically saved to `tempresults.json` every 10 forks to prevent data loss on interruption

## Rate Limits

- **Without Token**: 60 requests/hour (can analyze ~10-20 forks)
- **With Token**: 5000 requests/hour (can analyze hundreds of forks)

Each fork requires approximately 3-5 API requests to fully analyze.

## Example Output

```
============================================================
FORK ANALYSIS SUMMARY FOR wled/WLED
============================================================

Repository Details:
  - Total Forks: 1,243
  - Analyzed: 100
  - Stars: 15,500
  - Watchers: 326

Fork Age Distribution:
  - Last updated ≤ 1 month:        8 (  8.0%)
  - Last updated ≤ 3 months:      12 ( 12.0%)
  - Last updated ≤ 6 months:      15 ( 15.0%)
  - Last updated ≤ 1 year:        23 ( 23.0%)
  - Last updated ≤ 2 years:       25 ( 25.0%)
  - Last updated > 5 years:       17 ( 17.0%)

Fork Activity Analysis:
  - Forks with unique branches:             34 (34.0%)
  - Forks with recent main branch:          42 (42.0%)
  - Forks that contributed PRs:             18 (18.0%)
  - Active forks (no PR contributions):     23 (23.0%)

Owner Commit Analysis:
  - Forks with owner commits:               67 (67.0%)
  - Total commits by fork owners:         2845
  - Average commits per fork:             28.5

Key Insights:
  - Most forks are significantly behind main branch
  - Significant number of forks have custom development
  - Majority of forks show some owner development activity
```

## Use Cases

- **Project Maintenance**: Identify which forks are actively maintained
- **Community Engagement**: Find potential contributors who haven't submitted PRs
- **Code Discovery**: Locate interesting custom features in fork branches  
- **Health Assessment**: Monitor overall ecosystem health of the project
- **Outreach Planning**: Target active fork maintainers for collaboration

## Implementation Details

The script uses the GitHub REST API v3 and implements:

- Rate limiting with automatic backoff
- Error handling for private/deleted repositories
- Efficient pagination for large fork lists
- Branch comparison algorithms
- PR attribution analysis
- Commit recency detection

## Troubleshooting

- **Rate Limit Errors**: Use a GitHub token or reduce `--max-forks`
- **Permission Errors**: Ensure token has `public_repo` scope
- **Network Errors**: Check internet connection and GitHub status
- **Large Repository Timeouts**: Use `--max-forks` to limit analysis scope