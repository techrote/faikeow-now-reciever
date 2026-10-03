# Issue execution protocol

## Start of an issue

1. Reconcile live `main`.
2. Read `AGENTS.md`, `RAG.md` and all authority docs.
3. Read the complete issue body/comments.
4. Inspect open PRs and relevant branches.
5. Confirm dependencies are actually complete.
6. Create/resume one issue-owned branch.

Do not begin implementation from stale prompt-time SHAs without reconciling live state.

## During implementation

- Keep the issue boundary.
- Update authority docs if a contract changes.
- Add deterministic regression tests for bugs fixed.
- Preserve recovery paths.
- Preserve target-independent seams.
- Record uncertain hardware facts as unknown rather than guessing.

## Hardware issues

When physical evidence is required:

- record exact source commit before testing;
- record artifact hashes;
- record board/chip/host identity;
- preserve raw captures/logs outside git when bulky;
- commit concise curated evidence;
- do not generalize one failed incompatible setup into a firmware conclusion.

If hardware is unavailable, finish safe tooling/documentation work, record the exact remaining campaign, and leave the issue open.

## Pull request

The PR body should contain:

- owning FNR issue;
- scope summary;
- exact final head;
- automated validation;
- physical validation if applicable;
- explicit non-claims;
- any upstream TiltMouse contract version/commit relied upon.

## Merge

Use squash merge unless there is a concrete reason not to.

Before merging:

- exact final PR head has required green CI;
- branch is reconciled with current `main`;
- no unresolved review findings;
- every issue acceptance criterion is established.

After merging:

- verify the merge commit on `main`;
- check post-merge CI when configured;
- update tracker/evidence;
- close issue only if genuinely complete.

## Handoff

A stopped/incomplete issue should leave one concise issue comment containing:

- exact branch/head;
- what is established;
- tests/checks run;
- what remains;
- exact next command/action;
- hardware/evidence still required.

Do not claim later work will happen in the background.
