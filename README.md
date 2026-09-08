# NeNe Folio

A frameless Markdown notebook for Windows with a category drawer. Plain C (C23), Win32, no UI library.

> **Status (2026-09-09):** first vertical slice. The app starts, scans `data/` next to the executable,
> reconciles it with `categories.json` / `index.json`, and draws the index in the drawer of a frameless
> window. No viewer, no editing, no reordering yet. Nothing to download yet (Phase 4).

## What it will be

- A borderless window: a drawer on the left, a Markdown view/editor on the right.
- The drawer is an index of `data/<category>/*.md`. Categories fold like an accordion, carry a colour and an
  order, and notes are reordered by drag. There is no scrollbar; the drawer scrolls by wheel and keys.
- A search box at the top of the drawer searches every note.
- Colours and orders live in `data/categories.json` and `data/<category>/index.json`.

The requirements are in [SPECIFICATION.md](SPECIFICATION.md).

## How it is built

This repository follows the AYANE strict policy: **one meaning, one canonical implementation path, enforced by machines.**

- `docs/ARCHITECTURE_CONSTITUTION.md` — the rules (ARC-NNN) and how far C reaches for each of them
- `docs/CODING_RULES.md` — the approved subset of C (C-NNN)
- `docs/QUALITY_GATES.md` — what is enforced right now (active / planned / impossible), the only source of truth
- `docs/quality/phase0-results.json` — the compiler measurements the rules are based on
- `docs/adr/` — decisions, including why clang-cl and not MSVC, and why plain Win32

The only definition of done, locally and in CI:

```powershell
pwsh -NoProfile -File ./eng/check.ps1
```

It needs Visual Studio Build Tools with the pinned MSVC toolset and the bundled LLVM 19.1.5, CMake, Ninja and
Python 3.12 (`eng/tool-versions.json`). Run `pwsh -NoProfile -File ./eng/bootstrap.ps1` once after cloning.

## License

MIT. See [LICENSE](LICENSE).
