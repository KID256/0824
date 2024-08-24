#!/bin/bash

# Name of the script file and files/directories to exclude
SCRIPT_NAME=$(basename "$0")
EXCLUDE_FILES=("log.txt" "$SCRIPT_NAME")
FULLY_EXCLUDED_DIRS=("exclude_this_dir")
PARTIALLY_EXCLUDED_DIRS=("node_modules")

# Function to check if an item should be fully excluded
function is_fully_excluded {
  local item="$1"
  local basename=$(basename "$item")
  
  for exclude in "${FULLY_EXCLUDED_DIRS[@]}"; do
    if [ "$basename" == "$exclude" ]; then
      return 0
    fi
  done

  return 1
}

# Function to check if an item should be partially excluded
function is_partially_excluded {
  local item="$1"
  local basename=$(basename "$item")
  
  for exclude in "${PARTIALLY_EXCLUDED_DIRS[@]}"; do
    if [ "$basename" == "$exclude" ]; then
      return 0
    fi
  done

  return 1
}

# Function to check if an item should be excluded
function is_excluded {
  local item="$1"
  local basename=$(basename "$item")
  
  for exclude in "${EXCLUDE_FILES[@]}"; do
    if [ "$basename" == "$exclude" ]; then
      return 0
    fi
  done
  
  return 1
}

# Function to print directory tree
function print_tree {
  local dir="$1"
  local prefix="$2"
  local is_last=0

  # Determine the number of items in the directory
  local items=()
  for item in "$dir"/*; do
    if is_excluded "$item"; then
      continue
    fi
    items+=("$item")
  done

  local item_count=${#items[@]}
  local index=0

  # Loop through items in the directory
  for item in "${items[@]}"; do
    ((index++))
    if is_excluded "$item"; then
      continue
    fi

    local is_last=0
    if [ "$index" -eq "$item_count" ]; then
      is_last=1
    fi

    if [ -d "$item" ]; then
      if is_fully_excluded "$item"; then
        continue
      elif is_partially_excluded "$item"; then
        # Print partially excluded directories without their content
        if [ $is_last -eq 1 ]; then
          echo "${prefix}└── $(basename "$item")/"
        else
          echo "${prefix}├── $(basename "$item")/"
        fi
        # Do not recurse for partially excluded directories
      else
        # Print directory and recurse into it
        if [ $is_last -eq 1 ]; then
          echo "${prefix}└── $(basename "$item")/"
        else
          echo "${prefix}├── $(basename "$item")/"
        fi
        print_tree "$item" "${prefix}│   "
      fi
    else
      # Print files
      if [ $is_last -eq 1 ]; then
        echo "${prefix}└── $(basename "$item")"
      else
        echo "${prefix}├── $(basename "$item")"
      fi
    fi
  done

  # Adjust the final line if there are no subdirectories
  if [ $item_count -eq 0 ] && [ "$prefix" != "" ]; then
    echo "${prefix}└──"
  fi
}

# Start from the current directory
print_tree "." "" ""

