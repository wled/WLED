Import('env')
import subprocess
import re

def get_github_repo():
    """Extract GitHub repository name from git remote URL.
    
    Uses the remote that the current branch tracks, falling back to 'origin'.
    This handles cases where repositories have multiple remotes or where the
    main remote is not named 'origin'.
    
    Returns:
        str: Repository name in 'owner/repo' format for GitHub repos,
             'unknown' for non-GitHub repos, missing git CLI, or any errors.
    """
    try:
        remote_name = 'origin'  # Default fallback
        
        # Try to get the remote for the current branch
        try:
            # Get current branch name
            branch_result = subprocess.run(['git', 'rev-parse', '--abbrev-ref', 'HEAD'], 
                                         capture_output=True, text=True, check=True)
            current_branch = branch_result.stdout.strip()
            
            # Get the remote for the current branch
            remote_result = subprocess.run(['git', 'config', f'branch.{current_branch}.remote'], 
                                         capture_output=True, text=True, check=True)
            tracked_remote = remote_result.stdout.strip()
            
            # Use the tracked remote if we found one
            if tracked_remote:
                remote_name = tracked_remote
        except subprocess.CalledProcessError:
            # If branch config lookup fails, continue with 'origin' as fallback
            pass
        
        # Get the remote URL for the determined remote
        result = subprocess.run(['git', 'remote', 'get-url', remote_name], 
                              capture_output=True, text=True, check=True)
        remote_url = result.stdout.strip()
        
        # Check if it's a GitHub URL
        if 'github.com' not in remote_url.lower():
            return 'unknown'
        
        # Parse GitHub URL patterns:
        # https://github.com/owner/repo.git
        # git@github.com:owner/repo.git
        # https://github.com/owner/repo
        
        # Remove .git suffix if present
        if remote_url.endswith('.git'):
            remote_url = remote_url[:-4]
        
        # Handle HTTPS URLs
        https_match = re.search(r'github\.com/([^/]+/[^/]+)', remote_url, re.IGNORECASE)
        if https_match:
            return https_match.group(1)
        
        # Handle SSH URLs
        ssh_match = re.search(r'github\.com:([^/]+/[^/]+)', remote_url, re.IGNORECASE)
        if ssh_match:
            return ssh_match.group(1)
        
        return 'unknown'
        
    except FileNotFoundError:
        # Git CLI is not installed or not in PATH
        return 'unknown'
    except subprocess.CalledProcessError:
        # Git command failed (e.g., not a git repo, no remote, etc.)
        return 'unknown'
    except Exception:
        # Any other unexpected error
        return 'unknown'

repo = get_github_repo()
env.Append(BUILD_FLAGS=[f'-DWLED_REPO=\\"{repo}\\"'])