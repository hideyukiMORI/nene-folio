# NeNe Folio — Agent Guide

Short English entry point. The authoritative handbook is [CLAUDE.md](CLAUDE.md) (Japanese);
the normative documents live in [docs/](docs/).

## Project identity

- Product: NeNe Folio — a frameless Markdown notebook for Windows with a category drawer, written in plain C
- Language / UI: C (C23, clang-cl) / 素の Win32
- Package root: `nenefolio`
- Governing principle: **one meaning, one canonical implementation path, enforced by machines**

## Current instructions and state

Work directly as one agent by default. Use a read-only sub-agent only for a necessary independent review.
An explicit role split from hide applies only within its stated project/task scope; do not inherit old model assignments.
Start with [docs/todo/current.md](docs/todo/current.md), then verify the relevant Issue/PR and checkout state.
Old memories, reports, and handoffs are dated evidence, not fresh permission or current stop/resume instructions.
Operational policy: [ADR 0043](docs/adr/0043-current-instructions-and-single-agent.md), replacing ADR 0034.

## Required reading before changing production code

1. `docs/ARCHITECTURE_CONSTITUTION.md`
2. `docs/PROJECT_LAYOUT.md`
3. `docs/CODING_RULES.md`
4. `docs/QUALITY_GATES.md`
5. `docs/DEVELOPMENT_WORKFLOW.md`
6. `docs/COMMIT_CONVENTIONS.md`
7. `docs/GLOSSARY.md`

Then the active issue, the relevant accepted ADRs, and any active waivers.
Reuse already-read, unchanged rules during the same work. For documentation-only work, read the affected policy and references;
do not load the entire project history. Keep current state in `docs/todo/current.md`, not in this entry point.

## Agent rules

- Do not invent a second implementation path because it is locally convenient.
- Do not weaken a gate to make a change pass. Fix the code instead.
- Do not mark a rule `active` in `docs/QUALITY_GATES.md` before its enforcement exists.
- Do not read the current time, randomness, locale, or environment outside `adapters/win32`.
- Do not write `default` / `else` / `_` in a branch over a closed set; it disables exhaustiveness checking.
- Do not add suppressions, lint baselines, or tool exclusions without an active waiver (or at all, where the language forbids them).
- Do not claim a command passed unless it was actually executed.
- Prefer the smallest change that fully follows the canonical path.

## The only definition of done

```bash
pwsh -NoProfile -File ./eng/check.ps1
```

Local and CI run exactly this. Use narrow checks while iterating; run the full gate before
moving the PR from Draft to Ready.

## Required completion report

Issue and rule IDs, files and behavior changed, verification commands and results,
documentation or schema changes, active waiver IDs (or `none`), remaining risks.

Investigation-only requests do not authorize editing, committing, pushing, or opening PRs.
