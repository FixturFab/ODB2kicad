#!/bin/bash
set -e

export NODE_OPTIONS="--max-old-space-size=8192"

if [ -z "$1" ]; then
  echo "Usage: $0 <iterations>"
  exit 1
fi

for ((i=1; i<=$1; i++)); do
  echo "=== Ralph iteration $i / $1 ==="
  result=$(timeout 1800 claude --max-turns 40 --permission-mode acceptEdits --allowedTools "Bash Edit Write Read Glob Grep Task WebFetch WebSearch NotebookEdit" -p "@PRD-wasm.md @progress.txt \
  DO NOT enter plan mode or call EnterPlanMode. Work directly on implementation. \
  BEFORE starting: read progress.txt to see what previous iterations accomplished. Pick up where they left off. \
  1. Find the highest-priority incomplete stage and work on it. The stages are 1, 2, 3, 4, 5, 6, 7 as defined in the PRD. \
  2. GRANULAR PROGRESS: After completing each meaningful piece of work, IMMEDIATELY append a one-line summary to progress.txt describing what you changed. Do NOT wait until the end. \
  3. When the current stage compiles and passes its acceptance criteria, commit your changes. \
  4. Document what doesn't work in what-doesnt-work.md. \
  ONLY WORK ON A SINGLE STAGE per iteration. \
  IF more than 75% of the context is used, append your current status to progress.txt and exit. \
  IF something is going horribly wrong, document it in progress.txt and exit. \
  If the PRD is complete (all stages 1-7 done), output <promise>COMPLETE</promise>.") || {
    exit_code=$?
    if [ $exit_code -eq 124 ]; then
      echo "WARNING: Iteration $i timed out after 1800s, continuing to next iteration"
      continue
    else
      echo "ERROR: Iteration $i failed with exit code $exit_code"
      exit $exit_code
    fi
  }

  echo "$result"

  if [[ "$result" == *"<promise>COMPLETE</promise>"* ]]; then
    echo "PRD complete after $i iterations."
    exit 0
  fi
done
