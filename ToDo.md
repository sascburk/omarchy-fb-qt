# Omarchy File Browser — status

Native Qt 6 / QML file browser for Omarchy (keyboard-first, theme-synced).

## Done

- [x] CMake + Qt Quick Controls 2 app shell with Omarchy theme loader
- [x] File listing, navigation, parent / go-to / filter / sort / hidden files
- [x] Preview pane (text, image, video/audio, optional PDF)
- [x] File ops: open, edit, rename, trash/delete, new file/folder, copy/cut/paste
- [x] Marks, directory size, favorites, Open with… (persisted per MIME)
- [x] i18n (`en` / `de`), `language=auto` uses system locale
- [x] PKGBUILD + `bash install.sh`

## Notes

- Build out-of-tree (`/tmp/...`) when the source tree is on a noexec share.
- See `README.md` for install, config, and packaging.
