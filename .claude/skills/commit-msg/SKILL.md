---
name: commit-msg
description: >
  Enforces Conventional Commits format for all git commit messages.
  Use this skill whenever the user asks to commit changes, create a
  commit message, or when you detect a git commit operation. Ensures
  every commit follows the pattern: type(scope): description.
  Supported types: feat, fix, docs, style, refactor, test, chore, perf, ci.
  Rejects commits that don't match the format.
---

# Conventional Commit Messages

## When to Activate

Activate this skill when:

- The user asks you to commit changes
- The user asks you to write a commit message
- You are about to run `git commit` as part of a larger task
- The user asks to amend a commit message

## Commit Format

Every commit message MUST follow this pattern:

```
type(scope): short description

[optional body]

[optional footer]
```

### Allowed Types

| Type     | When to Use                                                |
| -------- | ---------------------------------------------------------- |
| feat     | A new feature visible to users                             |
| fix      | A bug fix                                                  |
| docs     | Documentation-only changes                                 |
| style    | Formatting, semicolons, whitespace — no logic change       |
| refactor | Code restructuring that doesn't fix a bug or add a feature |
| test     | Adding or correcting tests                                 |
| chore    | Build, tooling, dependency updates                         |
| perf     | Performance improvement                                    |
| ci       | CI/CD configuration changes                                |

### Rules

1. Type is required. Scope is optional but encouraged.
2. Description starts with lowercase, no period at the end.
3. Description is imperative mood: "add feature" not "added feature".
4. Body wraps at 72 characters.
5. Breaking changes MUST include `BREAKING CHANGE:` in the footer.
6. If multiple logical changes exist, suggest splitting into separate commits.

### Examples

Good:

- `feat(auth): add OAuth2 login with Google provider`
- `fix(api): handle null response from payment gateway`
- `refactor(db): extract connection pooling into shared module`

Bad:

- `updated stuff` — no type, vague description
- `Fix: Bug` — wrong format, not descriptive
- `feat: Added new feature for users.` — past tense, period, vague

## Process

1. Analyze the staged changes with `git diff --cached`
2. Determine the appropriate type based on the nature of changes
3. Identify the scope from the primary directory or module affected
4. Write a concise description in imperative mood
5. If changes are complex, add a body explaining the "why"
6. Present the commit message to the user for approval before committing
