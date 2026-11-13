#!/bin/bash
# Validates a commit message against Conventional Commits format.
# Usage: ./validate.sh "feat(auth): add login"
# Exit 0 = valid, Exit 1 = invalid with error message.

MSG="$1"

if [ -z "$MSG" ]; then
  echo "Error: No commit message provided."
  exit 1
fi

PATTERN='^(feat|fix|docs|style|refactor|test|chore|perf|ci)(\([a-z0-9-]+\))?: .+$'

FIRST_LINE=$(echo "$MSG" | head -n1)

if ! echo "$FIRST_LINE" | grep -qE "$PATTERN"; then
  echo "Invalid commit message: '$FIRST_LINE'"
  echo ""
  echo "Expected format: type(scope): description"
  echo "Allowed types: feat, fix, docs, style, refactor, test, chore, perf, ci"
  exit 1
fi

if echo "$FIRST_LINE" | grep -qE '\.$'; then
  echo "Invalid: description should not end with a period."
  exit 1
fi

echo "Valid commit message."
exit 0