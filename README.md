# NeNe Folio

A frameless Markdown notebook for Windows with a category drawer. Plain C (C23), Win32, no UI library.

> **Status (2026-10-12):** the notebook is usable from source. It views and edits notes, keeps a short
> history of every save, searches, filters, replaces, and manages categories, with settings for theme,
> language and line numbers. Version 0.1.0. There is no packaged download yet (Phase 4); build it with the
> command below and run `build/NeNeFolio.exe` with a `data/` folder next to it.

## What it does

- **A borderless window** with a drawer on the left and a Markdown view/editor on the right. It follows
  the OS light/dark theme (or a fixed one) and Per-Monitor v2 DPI.
- **The drawer is an index** of `data/<category>/*.md`. Categories fold like an accordion, carry a colour
  and an order, and rows are reordered by drag (notes can move between categories). There is no scrollbar;
  the drawer scrolls by wheel and keys. A search box at the top filters every note by name and body.
- **View and edit.** Clicking a note renders its Markdown. Edit mode shows the plain text in the same
  pane; Ctrl+S writes the file back atomically, keeps the original line endings, and writes nothing when
  the text is unchanged. The body just before each save is kept as `data/.history/<category>/<note>/1.md`
  … `5.md`, and any version can be restored into the editor as an undoable edit.
- **Notes**: new untitled note with a first save, save as, rename (recoverable across restarts through
  `data/.rename.json`), move to the Windows Recycle Bin together with its history.
- **Categories**: create, rename, change colour, delete when empty, all from the right-click menu,
  the **Actions** list, the command palette, or Ex commands.
- **Find**: search inside the open note (Ctrl+F, `/`, `?`, `n`, `N`, F3), filter all notes
  (Ctrl+Shift+F), and regular-expression replace in the edited body (a replace field or `:%s/…/…/g`).
- **Keyboard first.** The index takes Vim-like keys (`j` `k` `gg` `G` `h` `l` Enter, Esc from the body),
  `:` opens an Ex line (`:w` `:q` `:wq` `:q!` `:set …` and the commands above), and Ctrl+P / F1 / `:h`
  open a command palette that also lists the key bindings of each pane. The editor accepts
  Ctrl+h/j/k/l as arrows.
- **Settings** (`data/settings.json`): line numbers for the edited text, theme `system` / `light` / `dark`,
  and UI language 日本語 / English / 简体中文. Noto Sans JP and Noto Sans SC are bundled under `fonts/`.
- **Limits**: a note body is at most 16 MiB and may not contain NUL; both are refused rather than truncated.
- Failures (unreadable ledgers, unwritable files, a second instance on the same `data/`) are reported in
  the window, never silently ignored.

The requirements, one FR per row, are in [SPECIFICATION.md](SPECIFICATION.md).
Every key and command is listed in [キーバインドとコマンド](docs/KEY_BINDINGS.md) (Japanese).

## Where the data lives

Everything is in the `data/` folder next to the executable; the app never writes anywhere else.

| Path | Meaning |
| --- | --- |
| `data/categories.json` | category order, colour and folded/expanded state |
| `data/<category>/index.json` | note order inside the category |
| `data/<category>/<note>.md` | the note itself, written back as-is |
| `data/.history/<category>/<note>/1.md` … `5.md` | the last five bodies before a save (1 is newest) |
| `data/settings.json` | line numbers, theme, language (version 3; older versions are read) |
| `data/.rename.json` | an in-flight rename, replayed on the next start |
| `data/.nenefolio.lock` | refuses a second instance on the same `data/` |

Folders whose name starts with `.` are not scanned as categories. Use a local NTFS folder; sync-folder
placeholders are read as ordinary files, but symbolic links and junctions are refused for renames.

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

## Move a note to Recycle Bin

Right-click a note row and choose **Move to Recycle Bin** to move that note and its history.
**Actions**, the command palette, and Ex `:trashnote` / `:trash` target the open named note.
An edited target is saved first. There is no confirmation dialog.

The bottom line reports **Removed from the list.**, including when the original file was already absent.
To restore a moved note, restore the note and its history folder separately from Windows Recycle Bin,
then restart the app.

## Delete an empty category

Right-click a category row and choose **Delete category**. **Actions**, the command palette,
and Ex `:deletecategory` / `:delcat` use the cursor's category, or the open document's category
when there is no cursor. Select a category first if neither exists.

A category containing notes or an unsaved document is refused. Other files in its folder and
active index filtering also prevent deletion. Orphaned history goes to Recycle Bin before the
empty folder is removed. There is no confirmation dialog or success notice.
Deleting another category preserves the open document, its edits and Undo, and search or replace fields.
If the category ledger cannot be saved after deletion, the list updates and an error is shown.

## Rename recovery

Note and category renames use one plan and `data/.rename.json`.
New records use version 2 with a `note` or `category` kind; version 1 note records remain readable.
Recovery completes before scanning notes and categories.
A version 2 record must be recovered with this version before using an older binary.

## Rename a category

Right-click a category row and choose **Rename category…**. **Actions**, the command palette,
and Ex `:renamecategory name` / `:rencat name` use the cursor's category, or the open document's
category when there is no cursor. With no name argument, the current category name is selected
in the name form. Category names follow the same rules as category creation; `.md` is retained.

The category folder, its history, and the category ledger move through one recoverable rename.
Order, colour, expansion, the open body, mode and Undo are preserved. Only an edited named note
in the target category is saved first; an unsaved document and other categories are preserved.
Index filtering prevents category renaming. A replace preview for the target category expires
after completion, while its input fields remain; type again to refresh it.

If an earlier rename needs recovery, its own name form opens first. **Retry** finishes that rename
before the requested save or rename begins. **Close** leaves the pending intent and does not start
the requested action. The read-only old → new field selects the new name; Home/End and selection
let long names be read or copied. **Rename** also opens this recovery form from an unsaved document.
Search, replace, settings and valid history remain open after a rename; Ex and the palette close
on completion. History closes if the required save clears its rows. There is no success notice.

## License

MIT. See [LICENSE](LICENSE).
