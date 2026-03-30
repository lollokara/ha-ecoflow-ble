import os
import sys
import subprocess
from pathlib import Path
import json
from generate_rag_index import main as generate_main

def get_git_diff_files():
    try:
        # Get files changed in the working tree
        result = subprocess.run(["git", "diff", "--name-only"], capture_output=True, text=True, check=True)
        # Get files that are staged
        result_staged = subprocess.run(["git", "diff", "--cached", "--name-only"], capture_output=True, text=True, check=True)
        # Get untracked files
        result_untracked = subprocess.run(["git", "ls-files", "--others", "--exclude-standard"], capture_output=True, text=True, check=True)

        files = result.stdout.splitlines() + result_staged.stdout.splitlines() + result_untracked.stdout.splitlines()
        return list(set(files))
    except subprocess.CalledProcessError:
        print("Not a git repository or git error.")
        return []

def main():
    changed_files = get_git_diff_files()
    if not changed_files:
        print("No changed files to update.")
        return

    print(f"Found {len(changed_files)} changed files. Re-running full index generation for simplicity and correctness.")
    # For a real scalable system, we would just update the specific files in the index,
    # but for this script, re-running is safer to maintain consistency across catalog, summaries, and embeddings.
    generate_main()

if __name__ == "__main__":
    main()
