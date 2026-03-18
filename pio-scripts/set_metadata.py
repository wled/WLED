Import('env')
import subprocess
import json
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
            return None
        
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
        
        return None
        
    except FileNotFoundError:
        # Git CLI is not installed or not in PATH
        return None
    except subprocess.CalledProcessError:
        # Git command failed (e.g., not a git repo, no remote, etc.)
        return None
    except Exception:
        # Any other unexpected error
        return None

# WLED version is managed by package.json; this is picked up in several places
# - It's integrated in to the UI code
# - Here, for wled_metadata.cpp
# - The output_bins script
# We always take it from package.json to ensure consistency
with open("package.json", "r") as package:
    WLED_VERSION = json.load(package)["version"]

def has_def(cppdefs, name):
    """ Returns true if a given name is set in a CPPDEFINES collection """
    for f in cppdefs:
        if isinstance(f, tuple):
            f = f[0]
        if f == name:
            return True
    return False


def add_wled_metadata_flags(env, node):    
    cdefs = env["CPPDEFINES"].copy()

    if not has_def(cdefs, "WLED_REPO"):
        repo = get_github_repo()
        if repo:
            cdefs.append(("WLED_REPO", f"\\\"{repo}\\\""))

    cdefs.append(("WLED_VERSION", WLED_VERSION))

    # This transforms the node in to a Builder; it cannot be modified again
    return env.Object(
        node,
        CPPDEFINES=cdefs
    )
   
env.AddBuildMiddleware(
    add_wled_metadata_flags,
    "*/wled_metadata.cpp"
)
