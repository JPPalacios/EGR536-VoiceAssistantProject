#!/bin/bash

update_repo() {
  echo "updating GitHub repository..."

  git add .

  echo 'enter commit message:'
  read commitMessage

  git commit -m "$commitMessage"

  branch="main"

  git push origin $branch

  echo "update done."
}

cleanup () {
  echo "cleaning up..."

  # cleanup sent/received files
  rm speech_response.mp3
  rm text_response.txt
  rm *.wav

  echo "cleanup done."
}

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
  key="$1"

  case $key in
    --update_repo)
      update_repo_only=true
      ;;
    --cleanup)
      cleanup_only=true
      ;;
    *)
      echo "Unknown option: $key"
      exit 1
      ;;
  esac
  shift
done

# If no arguments are provided, execute all parts by default
if [[ ! $update_repo_only && ! $cleanup_only ]]; then
  # update_repo
  # cleanup
  exit 0
fi

# Execute the selected parts based on command-line arguments
if [[ $update_repo_only ]]; then
  update_repo
fi

if [[ $cleanup_only ]]; then
  cleanup
fi