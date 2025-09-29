#!/usr/bin/env python3
"""
Fork Statistics Analysis Tool for WLED Repository

This script analyzes the forks of the WLED repository to provide insights on:
- Which forks have branches that do not exist in the main repo
- Which forks have a recent version of main (vs outdated forks)
- Which fork repos have been the source of PRs into the main repo
- Which forks have active development but haven't contributed PRs
- Statistics on how far behind forks are (1 month, 3 months, 6 months, 1+ years)

Usage:
    python3 tools/fork_stats.py [--token GITHUB_TOKEN] [--repo owner/repo] [--output OUTPUT_FILE]

Environment Variables:
    GITHUB_TOKEN: GitHub personal access token for API access
"""

import argparse
import json
import os
import sys
import time
from datetime import datetime, timedelta, timezone
from typing import Dict, List, Optional, Set, Tuple
import requests
from dataclasses import dataclass


@dataclass
class ForkInfo:
    """Information about a repository fork."""
    name: str
    full_name: str
    owner: str
    html_url: str
    updated_at: datetime
    pushed_at: datetime
    default_branch: str
    branches: List[str]
    unique_branches: List[str]
    behind_main_by_commits: int
    behind_main_by_days: int
    has_contributed_prs: bool
    recent_commits: int
    is_active: bool
    owner_commits: int


class GitHubAPIError(Exception):
    """Custom exception for GitHub API errors."""
    pass


class ForkStatsAnalyzer:
    """Analyzes fork statistics for a GitHub repository."""
    
    def __init__(self, token: Optional[str] = None):
        self.token = token or os.getenv('GITHUB_TOKEN')
        if not self.token:
            print("Warning: No GitHub token provided. API rate limits will be severely restricted.")
        
        self.session = requests.Session()
        if self.token:
            self.session.headers.update({'Authorization': f'token {self.token}'})
        
        self.session.headers.update({
            'Accept': 'application/vnd.github.v3+json',
            'User-Agent': 'WLED-Fork-Stats-Analyzer/1.0'
        })
        
        # Rate limiting
        self.requests_made = 0
        self.rate_limit_remaining = None
        self.rate_limit_reset = None
    
    def _make_request(self, url: str, params: Optional[Dict] = None) -> Dict:
        """Make a GitHub API request with rate limiting."""
        if self.rate_limit_remaining is not None and self.rate_limit_remaining <= 5:
            if self.rate_limit_reset:
                wait_time = self.rate_limit_reset - time.time()
                if wait_time > 0:
                    print(f"Rate limit low ({self.rate_limit_remaining} remaining). Waiting {wait_time:.1f} seconds...")
                    time.sleep(wait_time + 1)
        
        try:
            response = self.session.get(url, params=params)
            
            # Update rate limit info
            self.rate_limit_remaining = int(response.headers.get('X-RateLimit-Remaining', 0))
            reset_timestamp = response.headers.get('X-RateLimit-Reset')
            if reset_timestamp:
                self.rate_limit_reset = int(reset_timestamp)
            
            self.requests_made += 1
            
            if self.requests_made % 50 == 0:
                print(f"API requests made: {self.requests_made}, remaining: {self.rate_limit_remaining}")
            
            if response.status_code == 403:
                if 'rate limit' in response.text.lower():
                    raise GitHubAPIError("Rate limit exceeded")
                else:
                    raise GitHubAPIError("API access forbidden (check token permissions)")
            
            response.raise_for_status()
            return response.json()
        
        except requests.exceptions.RequestException as e:
            raise GitHubAPIError(f"API request failed: {e}")
    
    def get_repository_info(self, repo: str) -> Dict:
        """Get basic repository information."""
        url = f"https://api.github.com/repos/{repo}"
        return self._make_request(url)
    
    def get_forks(self, repo: str, max_forks: Optional[int] = None) -> List[Dict]:
        """Get all forks of a repository."""
        forks = []
        page = 1
        per_page = 100
        
        while True:
            url = f"https://api.github.com/repos/{repo}/forks"
            params = {'page': page, 'per_page': per_page, 'sort': 'newest'}
            
            print(f"Fetching forks page {page}...")
            data = self._make_request(url, params)
            
            if not data:
                break
            
            forks.extend(data)
            
            if len(data) < per_page:
                break
            
            if max_forks and len(forks) >= max_forks:
                forks = forks[:max_forks]
                break
            
            page += 1
        
        return forks
    
    def get_branches(self, repo: str, max_branches: int = 500) -> List[str]:
        """Get all branches for a repository (with limits for performance)."""
        branches = []
        page = 1
        per_page = 100
        max_pages = max_branches // 100 + 1
        
        while page <= max_pages:
            url = f"https://api.github.com/repos/{repo}/branches"
            params = {'page': page, 'per_page': per_page}
            
            try:
                data = self._make_request(url, params)
                if not data:
                    break
                
                branches.extend([branch['name'] for branch in data])
                
                if len(data) < per_page:
                    break
                
                # Limit total branches to avoid excessive API calls
                if len(branches) >= max_branches:
                    branches = branches[:max_branches]
                    break
                
                page += 1
            
            except GitHubAPIError as e:
                if "404" in str(e):  # Repository might be empty or deleted
                    break
                raise
        
        return branches
    
    def get_pull_requests_from_fork(self, main_repo: str, fork_owner: str) -> List[Dict]:
        """Get pull requests created from a specific fork (optimized)."""
        prs = []
        page = 1
        per_page = 100
        max_pages = 10  # Limit to first 1000 PRs to avoid excessive API calls
        
        print(f"  Checking PRs from {fork_owner}...")
        
        while page <= max_pages:
            url = f"https://api.github.com/repos/{main_repo}/pulls"
            params = {
                'state': 'all',
                'head': f'{fork_owner}:',
                'page': page,
                'per_page': per_page
            }
            
            try:
                data = self._make_request(url, params)
                if not data:
                    break
                
                # Filter PRs that are actually from this fork owner
                fork_prs = [pr for pr in data if pr['head']['repo'] and 
                           pr['head']['repo']['owner']['login'] == fork_owner]
                prs.extend(fork_prs)
                
                if len(data) < per_page:
                    break
                
                page += 1
                
                # Early exit if we found PRs (for performance - we just need to know if any exist)
                if fork_prs:
                    break
            
            except GitHubAPIError:
                break  # Some API limitations or permissions issues
        
        return prs
    
    def get_commits_by_author(self, repo: str, author: str, branch: str = None, max_commits: int = 500) -> int:
        """Get number of commits by a specific author (optimized for performance)."""
        url = f"https://api.github.com/repos/{repo}/commits"
        params = {
            'author': author,
            'per_page': 100
        }
        if branch:
            params['sha'] = branch
        
        try:
            commits_count = 0
            page = 1
            max_pages = max_commits // 100 + 1  # Limit pages to avoid excessive API calls
            
            while page <= max_pages:
                params['page'] = page
                print(f"  Fetching commits by {author}, page {page}...")
                response = self.session.get(url, params=params)
                if response.status_code != 200:
                    break
                
                # Update rate limit tracking
                self.rate_limit_remaining = int(response.headers.get('X-RateLimit-Remaining', 0))
                reset_timestamp = response.headers.get('X-RateLimit-Reset')
                if reset_timestamp:
                    self.rate_limit_reset = int(reset_timestamp)
                self.requests_made += 1
                
                data = response.json()
                if not data:
                    break
                
                commits_count += len(data)
                
                # If we got less than per_page results, we're done
                if len(data) < 100:
                    break
                
                page += 1
                
                # Early exit if we hit our limit
                if commits_count >= max_commits:
                    commits_count = max_commits  # Cap at max to indicate truncation
                    break
                    
                # Add small delay between requests to be nice to API
                time.sleep(0.1)
            
            return commits_count
        
        except Exception as e:
            print(f"  Error fetching commits for {author}: {e}")
            return 0
    
    def get_commits_since_date(self, repo: str, since_date: datetime, branch: str = None) -> int:
        """Get number of commits since a specific date."""
        url = f"https://api.github.com/repos/{repo}/commits"
        params = {
            'since': since_date.isoformat(),
            'per_page': 1
        }
        if branch:
            params['sha'] = branch
        
        try:
            response = self.session.get(url, params=params)
            if response.status_code != 200:
                return 0
            
            # Get total count from Link header if available
            link_header = response.headers.get('Link')
            if link_header and 'rel="last"' in link_header:
                # Parse the last page number to estimate total commits
                import re
                match = re.search(r'page=(\d+).*rel="last"', link_header)
                if match:
                    return min(int(match.group(1)) * 30, 1000)  # Rough estimate, cap at 1000
            
            data = response.json()
            return len(data) if data else 0
        
        except:
            return 0
    
    def compare_repositories(self, base_repo: str, head_repo: str) -> Dict:
        """Compare two repositories to see how far behind head is from base."""
        url = f"https://api.github.com/repos/{base_repo}/compare/main...{head_repo}:main"
        
        try:
            return self._make_request(url)
        except GitHubAPIError:
            return {}
    
    def analyze_fork(self, fork: Dict, main_repo: str, main_branches: Set[str]) -> ForkInfo:
        """Analyze a single fork and return detailed information."""
        fork_name = fork['full_name']
        fork_owner = fork['owner']['login']
        
        start_time = time.time()
        print(f"Analyzing fork: {fork_name}")
        
        # Get fork branches
        try:
            print(f"  Fetching branches...")
            fork_branches = self.get_branches(fork_name)
            print(f"  Found {len(fork_branches)} branches")
        except GitHubAPIError as e:
            print(f"  Error fetching branches: {e}")
            fork_branches = []
        
        # Find unique branches (branches in fork but not in main repo)
        unique_branches = [branch for branch in fork_branches if branch not in main_branches]
        if unique_branches:
            print(f"  Found {len(unique_branches)} unique branches: {unique_branches[:5]}")
        
        # Check if fork has contributed PRs
        prs_from_fork = self.get_pull_requests_from_fork(main_repo, fork_owner)
        has_contributed = len(prs_from_fork) > 0
        if has_contributed:
            print(f"  Found {len(prs_from_fork)} PRs from this fork")
        
        # Compare with main repository
        print(f"  Comparing with main repository...")
        comparison = self.compare_repositories(main_repo, fork_name)
        behind_commits = comparison.get('behind_by', 0)
        
        # Calculate days behind based on last push
        pushed_at = datetime.fromisoformat(fork['pushed_at'].replace('Z', '+00:00'))
        now = datetime.now(timezone.utc)
        days_behind = (now - pushed_at).days
        print(f"  Last pushed {days_behind} days ago")
        
        # Check for recent activity
        print(f"  Checking recent activity...")
        thirty_days_ago = now - timedelta(days=30)
        recent_commits = self.get_commits_since_date(fork_name, thirty_days_ago)
        is_active = recent_commits > 0 or days_behind < 30
        
        # Get commits by fork owner
        print(f"  Analyzing owner commits...")
        owner_commits = self.get_commits_by_author(fork_name, fork_owner, max_commits=200)
        if owner_commits >= 200:
            print(f"  Owner has 200+ commits (truncated for performance)")
        else:
            print(f"  Owner has {owner_commits} commits")
        
        elapsed_time = time.time() - start_time
        print(f"  Analysis completed in {elapsed_time:.1f} seconds")
        
        return ForkInfo(
            name=fork['name'],
            full_name=fork_name,
            owner=fork_owner,
            html_url=fork['html_url'],
            updated_at=datetime.fromisoformat(fork['updated_at'].replace('Z', '+00:00')),
            pushed_at=pushed_at,
            default_branch=fork['default_branch'],
            branches=fork_branches,
            unique_branches=unique_branches,
            behind_main_by_commits=behind_commits,
            behind_main_by_days=days_behind,
            has_contributed_prs=has_contributed,
            recent_commits=recent_commits,
            is_active=is_active,
            owner_commits=owner_commits
        )
    
    def analyze_repository_forks(self, repo: str, max_forks: Optional[int] = None, fast_mode: bool = False) -> Dict:
        """Main analysis function for repository forks."""
        start_time = time.time()
        print(f"Starting fork analysis for {repo}")
        if fast_mode:
            print("Fast mode enabled: Skipping detailed analysis of forks inactive for 3+ years")
        
        # Get main repository info
        main_repo_info = self.get_repository_info(repo)
        print(f"Repository: {main_repo_info['full_name']}")
        print(f"Forks count: {main_repo_info['forks_count']}")
        
        # Get main repository branches
        print("Fetching main repository branches...")
        main_branches = set(self.get_branches(repo))
        print(f"Main repository has {len(main_branches)} branches")
        
        # Get all forks
        forks = self.get_forks(repo, max_forks)
        print(f"Found {len(forks)} forks to analyze")
        print(f"Estimated API requests needed: {len(forks) * (3 if fast_mode else 8)}")
        if self.rate_limit_remaining:
            print(f"Current rate limit: {self.rate_limit_remaining} requests remaining")
        
        if not forks:
            return {
                'main_repo': main_repo_info,
                'total_forks': 0,
                'analyzed_forks': [],
                'statistics': {}
            }
        
        # Analyze each fork
        analyzed_forks = []
        temp_results_file = "tempresults.json"
        
        for i, fork in enumerate(forks, 1):
            try:
                print(f"Progress: {i}/{len(forks)} - {fork['full_name']}")
                
                # Skip very old forks to improve performance (if fast mode enabled)
                if fast_mode:
                    pushed_at = datetime.fromisoformat(fork['pushed_at'].replace('Z', '+00:00'))
                    days_since_push = (datetime.now(timezone.utc) - pushed_at).days
                    
                    if days_since_push > 1095:  # Skip forks not updated in 3+ years
                        print(f"  Skipping fork (not updated in {days_since_push} days)")
                        # Create minimal fork info for very old forks
                        fork_info = ForkInfo(
                            name=fork['name'],
                            full_name=fork['full_name'],
                            owner=fork['owner']['login'],
                            html_url=fork['html_url'],
                            updated_at=datetime.fromisoformat(fork['updated_at'].replace('Z', '+00:00')),
                            pushed_at=pushed_at,
                            default_branch=fork['default_branch'],
                            branches=[],
                            unique_branches=[],
                            behind_main_by_commits=0,
                            behind_main_by_days=days_since_push,
                            has_contributed_prs=False,
                            recent_commits=0,
                            is_active=False,
                            owner_commits=0
                        )
                        analyzed_forks.append(fork_info)
                        continue
                
                fork_info = self.analyze_fork(fork, repo, main_branches)
                analyzed_forks.append(fork_info)
            except Exception as e:
                print(f"Error analyzing fork {fork['full_name']}: {e}")
                continue
            
            # Save intermediate results every 10 forks
            if i % 10 == 0:
                print(f"Saving intermediate results to {temp_results_file}...")
                temp_results = {
                    'main_repo': main_repo_info,
                    'total_forks': len(forks),
                    'analyzed_so_far': i,
                    'analyzed_forks': [
                        {
                            'name': fork.name,
                            'full_name': fork.full_name,
                            'owner': fork.owner,
                            'html_url': fork.html_url,
                            'updated_at': fork.updated_at.isoformat(),
                            'pushed_at': fork.pushed_at.isoformat(),
                            'default_branch': fork.default_branch,
                            'branches': fork.branches,
                            'unique_branches': fork.unique_branches,
                            'behind_main_by_commits': fork.behind_main_by_commits,
                            'behind_main_by_days': fork.behind_main_by_days,
                            'has_contributed_prs': fork.has_contributed_prs,
                            'recent_commits': fork.recent_commits,
                            'is_active': fork.is_active,
                            'owner_commits': fork.owner_commits
                        }
                        for fork in analyzed_forks
                    ],
                    'statistics': self._calculate_statistics(analyzed_forks),
                    'analysis_timestamp': datetime.now(timezone.utc).isoformat()
                }
                
                try:
                    with open(temp_results_file, 'w') as f:
                        json.dump(temp_results, f, indent=2)
                except Exception as save_error:
                    print(f"Warning: Failed to save intermediate results: {save_error}")
                
                # Be nice to the API
                time.sleep(1)
        
        # Calculate statistics
        statistics = self._calculate_statistics(analyzed_forks)
        
        # Clean up temporary results file on successful completion
        temp_results_file = "tempresults.json"
        if os.path.exists(temp_results_file):
            try:
                os.remove(temp_results_file)
                print(f"Cleaned up temporary file: {temp_results_file}")
            except Exception as e:
                print(f"Warning: Could not remove temporary file {temp_results_file}: {e}")
        
        return {
            'main_repo': main_repo_info,
            'total_forks': len(forks),
            'analyzed_forks': analyzed_forks,
            'statistics': statistics,
            'analysis_timestamp': datetime.now(timezone.utc).isoformat()
        }
    
    def _calculate_statistics(self, forks: List[ForkInfo]) -> Dict:
        """Calculate summary statistics from analyzed forks."""
        if not forks:
            return {}
        
        total_forks = len(forks)
        
        # Categorize by age
        now = datetime.now(timezone.utc)
        age_categories = {
            '1_month': 0,
            '3_months': 0,
            '6_months': 0,
            '1_year': 0,
            '2_years': 0,
            '5_plus_years': 0
        }
        
        for fork in forks:
            days_old = (now - fork.pushed_at).days
            if days_old <= 30:
                age_categories['1_month'] += 1
            elif days_old <= 90:
                age_categories['3_months'] += 1
            elif days_old <= 180:
                age_categories['6_months'] += 1
            elif days_old <= 365:
                age_categories['1_year'] += 1
            elif days_old <= 730:
                age_categories['2_years'] += 1
            else:
                age_categories['5_plus_years'] += 1
        
        # Other statistics
        forks_with_unique_branches = len([f for f in forks if f.unique_branches])
        forks_with_recent_main = len([f for f in forks if f.behind_main_by_days <= 365])
        forks_with_contributed_prs = len([f for f in forks if f.has_contributed_prs])
        active_non_contributing = len([f for f in forks if f.is_active and not f.has_contributed_prs])
        
        # Owner commit statistics
        forks_with_owner_commits = len([f for f in forks if f.owner_commits > 0])
        total_owner_commits = sum(f.owner_commits for f in forks)
        avg_owner_commits = total_owner_commits / total_forks if total_forks > 0 else 0
        
        return {
            'total_analyzed': total_forks,
            'age_distribution': age_categories,
            'forks_with_unique_branches': forks_with_unique_branches,
            'forks_with_recent_main': forks_with_recent_main,
            'forks_that_contributed_prs': forks_with_contributed_prs,
            'active_non_contributing_forks': active_non_contributing,
            'forks_with_owner_commits': forks_with_owner_commits,
            'total_owner_commits': total_owner_commits,
            'avg_owner_commits_per_fork': round(avg_owner_commits, 1),
            'percentage_with_unique_branches': (forks_with_unique_branches / total_forks) * 100,
            'percentage_with_recent_main': (forks_with_recent_main / total_forks) * 100,
            'percentage_contributed_prs': (forks_with_contributed_prs / total_forks) * 100,
            'percentage_active_non_contributing': (active_non_contributing / total_forks) * 100,
            'percentage_with_owner_commits': (forks_with_owner_commits / total_forks) * 100
        }


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(description='Analyze GitHub repository fork statistics')
    parser.add_argument('--token', help='GitHub personal access token')
    parser.add_argument('--repo', default='wled/WLED', help='Repository in owner/repo format')
    parser.add_argument('--output', help='Output file for JSON results')
    parser.add_argument('--max-forks', type=int, help='Maximum number of forks to analyze')
    parser.add_argument('--format', choices=['json', 'summary'], default='summary',
                       help='Output format')
    parser.add_argument('--demo', action='store_true',
                       help='Run with sample data for demonstration (no API calls)')
    parser.add_argument('--dry-run', action='store_true',
                       help='Show what would be analyzed without making API calls')
    parser.add_argument('--fast', action='store_true',
                       help='Fast mode: skip detailed analysis of very old forks (3+ years) for better performance')
    
    args = parser.parse_args()
    
    # Validate repository format
    if '/' not in args.repo:
        print("Error: Repository must be in 'owner/repo' format")
        sys.exit(1)
    
    # Create analyzer
    analyzer = ForkStatsAnalyzer(args.token)
    
    if args.demo:
        # Create sample data for demonstration
        print("DEMO MODE: Using sample data for demonstration")
        print("This shows what the output would look like for WLED repository analysis\n")
        
        sample_results = {
            'main_repo': {
                'full_name': 'wled/WLED',
                'forks_count': 1243,
                'stargazers_count': 15500,
                'watchers_count': 326
            },
            'total_forks': 100,  # Sample size
            'analyzed_forks': [],  # Not needed for summary
            'statistics': {
                'total_analyzed': 100,
                'age_distribution': {
                    '1_month': 8,
                    '3_months': 12,
                    '6_months': 15,
                    '1_year': 23,
                    '2_years': 25,
                    '5_plus_years': 17
                },
                'forks_with_unique_branches': 34,
                'forks_with_recent_main': 42,
                'forks_that_contributed_prs': 18,
                'active_non_contributing_forks': 23,
                'forks_with_owner_commits': 67,
                'total_owner_commits': 2845,
                'avg_owner_commits_per_fork': 28.5,
                'percentage_with_unique_branches': 34.0,
                'percentage_with_recent_main': 42.0,
                'percentage_contributed_prs': 18.0,
                'percentage_active_non_contributing': 23.0,
                'percentage_with_owner_commits': 67.0
            }
        }
        
        if args.output:
            # Save sample results to JSON for demo
            with open(args.output, 'w') as f:
                json.dump(sample_results, f, indent=2)
            print(f"Sample JSON results saved to {args.output}")
        
        if args.format == 'summary' or not args.output:
            print_summary(sample_results)
        return
    
    if args.dry_run:
        try:
            # Just get basic repository info for dry run
            print(f"DRY RUN: Analyzing repository {args.repo}")
            repo_info = analyzer.get_repository_info(args.repo)
            print(f"Repository: {repo_info['full_name']}")
            print(f"Total forks: {repo_info['forks_count']:,}")
            
            forks_to_analyze = args.max_forks or min(repo_info['forks_count'], 100)
            print(f"Would analyze: {forks_to_analyze} forks")
            print(f"Estimated API requests: {forks_to_analyze * 3 + 10}")
            print(f"Rate limit status: {analyzer.rate_limit_remaining or 'Unknown'} requests remaining")
            
            if not analyzer.token:
                print("WARNING: No token provided. Rate limit is 60 requests/hour for unauthenticated requests.")
                if repo_info['forks_count'] > 20:
                    print("Consider using a GitHub token for analyzing larger repositories.")
            else:
                print("GitHub token provided. Rate limit is 5000 requests/hour.")
            
            return
        except GitHubAPIError as e:
            print(f"Error accessing repository: {e}")
            sys.exit(1)
    
    try:
        # Run analysis
        results = analyzer.analyze_repository_forks(args.repo, args.max_forks, args.fast)
        
        if args.output:
            # Save detailed results as JSON
            with open(args.output, 'w') as f:
                # Convert ForkInfo objects to dicts for JSON serialization
                serializable_results = results.copy()
                serializable_results['analyzed_forks'] = [
                    {
                        'name': fork.name,
                        'full_name': fork.full_name,
                        'owner': fork.owner,
                        'html_url': fork.html_url,
                        'updated_at': fork.updated_at.isoformat(),
                        'pushed_at': fork.pushed_at.isoformat(),
                        'default_branch': fork.default_branch,
                        'branches': fork.branches,
                        'unique_branches': fork.unique_branches,
                        'behind_main_by_commits': fork.behind_main_by_commits,
                        'behind_main_by_days': fork.behind_main_by_days,
                        'has_contributed_prs': fork.has_contributed_prs,
                        'recent_commits': fork.recent_commits,
                        'is_active': fork.is_active,
                        'owner_commits': fork.owner_commits
                    }
                    for fork in results['analyzed_forks']
                ]
                json.dump(serializable_results, f, indent=2)
            print(f"\nDetailed results saved to {args.output}")
        
        # Print summary
        if args.format == 'summary' or not args.output:
            print_summary(results)
    
    except KeyboardInterrupt:
        print("\nAnalysis interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


def print_summary(results: Dict):
    """Print a human-readable summary of the analysis."""
    stats = results['statistics']
    main_repo = results['main_repo']
    
    print("\n" + "="*60)
    print(f"FORK ANALYSIS SUMMARY FOR {main_repo['full_name']}")
    print("="*60)
    
    print(f"\nRepository Details:")
    print(f"  - Total Forks: {main_repo['forks_count']:,}")
    print(f"  - Analyzed: {stats.get('total_analyzed', 0):,}")
    print(f"  - Stars: {main_repo['stargazers_count']:,}")
    print(f"  - Watchers: {main_repo['watchers_count']:,}")
    
    if 'age_distribution' in stats:
        print(f"\nFork Age Distribution:")
        age_dist = stats['age_distribution']
        total = stats['total_analyzed']
        print(f"  - Last updated ≤ 1 month:     {age_dist['1_month']:4d} ({age_dist['1_month']/total*100:5.1f}%)")
        print(f"  - Last updated ≤ 3 months:    {age_dist['3_months']:4d} ({age_dist['3_months']/total*100:5.1f}%)")
        print(f"  - Last updated ≤ 6 months:    {age_dist['6_months']:4d} ({age_dist['6_months']/total*100:5.1f}%)")
        print(f"  - Last updated ≤ 1 year:      {age_dist['1_year']:4d} ({age_dist['1_year']/total*100:5.1f}%)")
        print(f"  - Last updated ≤ 2 years:     {age_dist['2_years']:4d} ({age_dist['2_years']/total*100:5.1f}%)")
        print(f"  - Last updated > 5 years:     {age_dist['5_plus_years']:4d} ({age_dist['5_plus_years']/total*100:5.1f}%)")
    
    print(f"\nFork Activity Analysis:")
    print(f"  - Forks with unique branches:           {stats.get('forks_with_unique_branches', 0):4d} ({stats.get('percentage_with_unique_branches', 0):.1f}%)")
    print(f"  - Forks with recent main branch:        {stats.get('forks_with_recent_main', 0):4d} ({stats.get('percentage_with_recent_main', 0):.1f}%)")
    print(f"  - Forks that contributed PRs:           {stats.get('forks_that_contributed_prs', 0):4d} ({stats.get('percentage_contributed_prs', 0):.1f}%)")
    print(f"  - Active forks (no PR contributions):   {stats.get('active_non_contributing_forks', 0):4d} ({stats.get('percentage_active_non_contributing', 0):.1f}%)")
    
    print(f"\nOwner Commit Analysis:")
    print(f"  - Forks with owner commits:             {stats.get('forks_with_owner_commits', 0):4d} ({stats.get('percentage_with_owner_commits', 0):.1f}%)")
    print(f"  - Total commits by fork owners:         {stats.get('total_owner_commits', 0):4d}")
    print(f"  - Average commits per fork:             {stats.get('avg_owner_commits_per_fork', 0):4.1f}")
    
    print(f"\nKey Insights:")
    if stats.get('percentage_with_recent_main', 0) < 50:
        print(f"  - Most forks are significantly behind main branch")
    if stats.get('percentage_contributed_prs', 0) < 10:
        print(f"  - Very few forks have contributed back to main repository")
    if stats.get('percentage_with_unique_branches', 0) > 20:
        print(f"  - Significant number of forks have custom development")
    if stats.get('percentage_with_owner_commits', 0) > 60:
        print(f"  - Majority of forks show some owner development activity")
    
    print("\n" + "="*60)


if __name__ == '__main__':
    main()