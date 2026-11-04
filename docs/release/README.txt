NeNe Folio — Windows x64

Extract the whole product folder to a writable local NTFS folder, then start
NeNeFolio.exe. Keep fonts/ beside the executable. No installation or network
connection is required. Windows 10 version 2004 or later is required.

The first launch can start with no data/ folder. Use + beside the Categories
heading to create a category, then New note to create a note.
The app creates data/ beside the executable when writing.
Notes, category order, settings and saved history stay in the note folder.
Settings > Note storage lets you select another folder for the next launch.
Changing the location does not copy or move existing notes.

Before updating, save, close the app, and back up the entire note folder,
including hidden files and .history/. Also back up nenefolio-startup.json
beside the executable if present. Extract the new version to a new folder.
For the default location, copy data/ there. For a custom location, copy the
startup file beside the new executable; it keeps the same selected folder.
Keep the backup when downgrading: older versions may reject newer settings.
Do not overwrite an open app.

Use the gear icon for theme, language, line numbers, minimap and version.
Ctrl+E switches Read/Edit; Ctrl+M opens Actions; Ctrl+Shift+M toggles the minimap.
Markdown import/export is in category menus and Actions. ZIP export includes
saved note bodies and referenced category images/ files. Other local images
can be collected after confirmation; HTTPS URLs remain references. A single
Markdown export keeps image references without copying images. ZIP is not a
backup of settings, order or history.
Read mode displays PNG/JPEG images, basic code syntax colors, and native
Mermaid flowcharts and sequence diagrams. Diagram sources stay in Markdown;
unsupported syntax is reported. Remote images access their HTTPS host.
F1 or Ctrl+P lists commands and keys. Ctrl+S saves; Alt+F4 saves and closes.
Do not use an older binary while data/.rename.json contains unfinished work.

This application is distributed under the MIT license in LICENSE.
Third-party notices are in THIRD_PARTY_NOTICES.txt.
Bundled Noto fonts use SIL Open Font License 1.1 in fonts/LICENSE-Noto-CJK.txt.
Font origins and hashes are recorded in fonts/manifest.json.

This is an unsigned portable package.
