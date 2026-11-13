---
name: replicate
description: Use when building or extending a module in the nexora project by copying the structure, patterns, and coding style of counter-sanding-kecil (the gold standard). Trigger on requests like "replicate", "follow counter-sanding-kecil", "build input display like counter-sanding-kecil", or when creating a new module that should match existing conventions.
---

# Replicate

Goal: build the target module (default: `input display`) so it matches the
reference module (`counter-sanding-kecil`) in structure, naming, and style,
while spending as few tokens and requests as possible.

Project layout (relative to the `nexora` root):

- `counter-sanding-kecil/` -> REFERENCE (gold standard, never modify)
- `counter-sanding-besar/` -> not involved unless the user asks
- `input display/` -> TARGET

## Golden rules (token saving)

1. NEVER read the whole reference. List first, read selectively.
2. Skip: `node_modules`, `dist`, `build`, `.git`, lockfiles, images, binaries.
3. Read each file at most once per session. Take notes instead of re-reading.
4. Cache what you learn in `BLUEPRINT.md` (see Step 1) and reuse it next time.
5. Work in small steps: one module/layer at a time, then verify.
6. Do not rewrite files that already match the pattern.

## Step 0: Check the cache

Look for `BLUEPRINT.md` in this skill folder.

- If it exists: read it, skip to Step 2.
- If it does not exist: do Step 1.

## Step 1: Learn the reference (only once)

1. Show the directory tree of `counter-sanding-kecil` only (names, no contents),
   max 3 levels deep.
2. Identify the layers/roles of each folder (for example: entry point,
   config, components, services, utils, tests). Infer from names first.
3. Read ONE representative file per layer, plus the entry point and the
   config/manifest files. Do not read more unless something is unclear.
4. Write `BLUEPRINT.md` in this skill folder containing:
   - Directory tree with a one-line purpose per folder
   - Build/run flow: how the app starts and how data moves through the layers
   - Naming conventions (files, functions, variables, constants)
   - Code style notes (formatting, error handling, comments, imports order)
   - A short "how to add a new feature" checklist, in order of files touched
     Keep it under about 150 lines. Prefer facts over prose.

## Step 2: Compare reference vs target

1. Show the directory tree of `input display` (names only).
2. List differences against `BLUEPRINT.md`:
   - Missing folders/files that the reference has
   - Things `input display` needs that the reference does not have
   - Things the reference has that `input display` does not need
3. For each difference, decide and note one line:
   `COPY` (same as reference) | `ADAPT` (same role, different content) |
   `NEW` (no equivalent, create a sensible directory) | `SKIP`.
4. For `NEW` items, choose names and locations that follow the naming
   convention and layering in `BLUEPRINT.md`. State the choice briefly.

Show this plan to the user (short list) before building if there are more
than 3 `NEW` items; otherwise proceed.

## Step 3: Build

1. Create the directory skeleton first (empty folders/files with correct names).
2. Fill files layer by layer, in the same order as the "how to add a new
   feature" checklist in `BLUEPRINT.md`.
3. For each file: open the matching reference file only if the pattern is
   not already clear from `BLUEPRINT.md`. Then write the target file.
4. Keep the same style, naming, imports order, and error handling as the
   reference. Do not introduce new libraries unless required; say so if you do.
5. Do not copy domain-specific content blindly (labels, calculations,
   constants). Adapt them to `input display`.

## Step 4: Verify

1. Run the same build/lint/test commands the reference uses (from `BLUEPRINT.md`).
2. Fix errors one at a time; do not rewrite unrelated files.
3. Report in at most 10 lines:
   - What was created
   - What was adapted, and what was newly invented (with reasons)
   - Anything that could not be verified

## Output style

- Be concise. No long recaps of files you already summarized in `BLUEPRINT.md`.
- If something is ambiguous, ask ONE short question, then continue.
