#!/usr/bin/env bash
# Simple installer for Omarchy File Browser (Qt 6 / QML)
#
# Usage (CIFS/noexec-safe — run via bash):
#   bash install.sh              # build + install system-wide
#   bash install.sh uninstall    # remove
#   bash install.sh package      # only build a pacman package into packaging/out/
#
# Optional desktop integration (asked interactively after install, or via flags):
#   bash install.sh --default-fm --hotkey
#   bash install.sh --no-default-fm --no-hotkey
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKGNAME="omarchy-fb-qt"
DESKTOP_ID="${PKGNAME}.desktop"
PREFIX="${PREFIX:-/usr}"
BUILD_DIR="${BUILD_DIR:-/tmp/${PKGNAME}-build}"
STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/${PKGNAME}"
BINDINGS_LUA="${XDG_CONFIG_HOME:-$HOME/.config}/hypr/bindings.lua"
MARKER_BEGIN="-- OMARCHY_FB_QT_MANAGED_BEGIN"
MARKER_END="-- OMARCHY_FB_QT_MANAGED_END"

# Colors only when stdout is a TTY
if [[ -t 1 ]]; then
  BOLD=$'\033[1m'; DIM=$'\033[2m'; OK=$'\033[32m'; ERR=$'\033[31m'; RST=$'\033[0m'
else
  BOLD=; DIM=; OK=; ERR=; RST=
fi

log()  { printf '%s→%s %s\n' "$BOLD" "$RST" "$*"; }
ok()   { printf '%s✓%s %s\n' "$OK" "$RST" "$*"; }
die()  { printf '%s✗%s %s\n' "$ERR" "$RST" "$*" >&2; exit 1; }

need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing command: $1"; }

elevate() {
  if [[ "$(id -u)" -eq 0 ]]; then
    "$@"
  elif command -v sudo >/dev/null 2>&1; then
    sudo "$@"
  elif command -v pkexec >/dev/null 2>&1; then
    pkexec "$@"
  else
    die "Need root rights (sudo or pkexec) to install into ${PREFIX}"
  fi
}

# Returns 0 for yes. Default: y|n (second arg).
ask_yes_no() {
  local prompt=$1
  local default=${2:-n}
  local reply=""

  # Non-interactive: honor explicit flags / env, else default.
  if [[ ! -t 0 || ! -t 1 ]]; then
    [[ "$default" == "y" ]]
    return
  fi

  if [[ "$default" == "y" ]]; then
    read -r -p "$prompt [Y/n] " reply || true
    [[ -z "${reply}" || "${reply}" =~ ^[Yy] ]]
  else
    read -r -p "$prompt [y/N] " reply || true
    [[ "${reply}" =~ ^[Yy] ]]
  fi
}

DEPS=(cmake gcc make qt6-base qt6-declarative qt6-multimedia hicolor-icon-theme)
OPT_DEPS=(ttf-ia-writer gst-libav gst-plugins-good xdg-desktop-portal qt6-webengine)

ensure_deps() {
  need_cmd pacman
  local missing=()
  local pkg
  for pkg in "${DEPS[@]}"; do
    pacman -Q "$pkg" &>/dev/null || missing+=("$pkg")
  done
  if ((${#missing[@]})); then
    log "Installing build/runtime deps: ${missing[*]}"
    elevate pacman -S --needed --noconfirm "${missing[@]}"
  else
    ok "Dependencies present"
  fi

  local hint=()
  for pkg in "${OPT_DEPS[@]}"; do
    pacman -Q "$pkg" &>/dev/null || hint+=("$pkg")
  done
  if ((${#hint[@]})); then
    printf '%s  Optional (recommended): pacman -S %s%s\n' "$DIM" "${hint[*]}" "$RST"
  fi
}

do_build() {
  need_cmd cmake
  mkdir -p "$BUILD_DIR"
  log "Configuring in ${BUILD_DIR}"
  cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX"
  log "Building…"
  cmake --build "$BUILD_DIR" -j"$(nproc)"
  ok "Build finished: ${BUILD_DIR}/${PKGNAME}"
}

set_default_file_manager() {
  need_cmd xdg-mime
  mkdir -p "$STATE_DIR"

  local previous
  previous="$(xdg-mime query default inode/directory 2>/dev/null || true)"
  if [[ -n "$previous" && "$previous" != "$DESKTOP_ID" ]]; then
    printf '%s\n' "$previous" >"$STATE_DIR/previous-inode-directory"
  fi

  # Ensure the desktop file is visible to xdg-mime (user + system).
  if [[ ! -f "${PREFIX}/share/applications/${DESKTOP_ID}" ]]; then
    die "Desktop file missing: ${PREFIX}/share/applications/${DESKTOP_ID}"
  fi

  xdg-mime default "$DESKTOP_ID" inode/directory
  # Common companion types used by file managers / portals.
  xdg-mime default "$DESKTOP_ID" inode/mount-point 2>/dev/null || true

  local now
  now="$(xdg-mime query default inode/directory 2>/dev/null || true)"
  if [[ "$now" == "$DESKTOP_ID" ]]; then
    ok "Default browser for folders: ${DESKTOP_ID}"
  else
    die "Failed to set default browser for folders (got: ${now:-none})"
  fi
}

restore_default_file_manager() {
  command -v xdg-mime >/dev/null 2>&1 || return 0
  local previous=""
  if [[ -f "$STATE_DIR/previous-inode-directory" ]]; then
    previous="$(cat "$STATE_DIR/previous-inode-directory")"
  fi
  if [[ -z "$previous" ]]; then
    previous="org.gnome.Nautilus.desktop"
  fi
  local current
  current="$(xdg-mime query default inode/directory 2>/dev/null || true)"
  if [[ "$current" == "$DESKTOP_ID" ]]; then
    xdg-mime default "$previous" inode/directory 2>/dev/null || true
    ok "Restored previous browser for folders: ${previous}"
  fi
  rm -f "$STATE_DIR/previous-inode-directory"
}

ensure_bindings_file() {
  mkdir -p "$(dirname "$BINDINGS_LUA")"
  if [[ ! -f "$BINDINGS_LUA" ]]; then
    cat >"$BINDINGS_LUA" <<'EOF'
-- Keep only your personal keybinding overrides here.
EOF
  fi
}

remove_hotkey_block() {
  [[ -f "$BINDINGS_LUA" ]] || return 0
  grep -Fq "$MARKER_BEGIN" "$BINDINGS_LUA" 2>/dev/null || return 0
  local tmp
  tmp="$(mktemp)"
  awk -v b="$MARKER_BEGIN" -v e="$MARKER_END" '
    $0 == b { skip=1; next }
    $0 == e { skip=0; next }
    !skip { print }
  ' "$BINDINGS_LUA" >"$tmp" && mv "$tmp" "$BINDINGS_LUA"
  rm -f "$tmp"
}

set_hotkey() {
  ensure_bindings_file
  remove_hotkey_block

  cat >>"$BINDINGS_LUA" <<EOF

${MARKER_BEGIN}
-- Replaces Omarchy default (Nautilus) for SUPER+SHIFT+F. Managed by install.sh.
hl.unbind("SUPER + SHIFT + F")
o.bind("SUPER + SHIFT + F", "File manager", { launch = "${PKGNAME}" })
${MARKER_END}
EOF

  ok "Bound SUPER+SHIFT+F → ${PKGNAME}"
  printf '%s  Note: this overrides the Omarchy default (Nautilus).%s\n' "$DIM" "$RST"
  printf '%s  SUPER+ALT+SHIFT+F (file manager cwd) is unchanged.%s\n' "$DIM" "$RST"

  if command -v hyprctl >/dev/null 2>&1; then
    hyprctl reload >/dev/null 2>&1 || true
    if command -v hyprctl >/dev/null 2>&1; then
      local errs
      errs="$(hyprctl configerrors 2>/dev/null || true)"
      if [[ -n "${errs}" && "${errs}" != "ok" && "${errs}" != *"no errors"* ]]; then
        printf '%s  hyprctl configerrors:%s\n%s\n' "$DIM" "$RST" "$errs"
      fi
    fi
  fi
}

remove_hotkey() {
  if [[ -f "$BINDINGS_LUA" ]] && grep -Fq "$MARKER_BEGIN" "$BINDINGS_LUA" 2>/dev/null; then
    remove_hotkey_block
    ok "Removed SUPER+SHIFT+F override (Omarchy Nautilus default applies again)"
    if command -v hyprctl >/dev/null 2>&1; then
      hyprctl reload >/dev/null 2>&1 || true
    fi
  fi
}

maybe_configure_desktop() {
  local want_default="${OPT_DEFAULT_FM:-}"
  local want_hotkey="${OPT_HOTKEY:-}"

  printf '\n%sDesktop integration%s\n' "$BOLD" "$RST"

  if [[ -z "$want_default" ]]; then
    printf '%s  This handles folders only. Web links stay with the current web browser.%s\n' "$DIM" "$RST"
    if ask_yes_no "Set ${PKGNAME} as the default browser for folders?" n; then
      want_default=y
    else
      want_default=n
    fi
  fi
  if [[ "$want_default" == "y" ]]; then
    set_default_file_manager
  else
    log "Skipped default browser for folders"
  fi

  if [[ -z "$want_hotkey" ]]; then
    if ask_yes_no "Bind SUPER+SHIFT+F to open ${PKGNAME}? (replaces Nautilus)" n; then
      want_hotkey=y
    else
      want_hotkey=n
    fi
  fi
  if [[ "$want_hotkey" == "y" ]]; then
    set_hotkey
  else
    log "Skipped SUPER+SHIFT+F binding"
  fi
}

do_install_cmake() {
  do_build
  log "Installing to ${PREFIX}…"
  elevate cmake --install "$BUILD_DIR"
  elevate install -Dm644 "$ROOT/LICENSE" \
    "${PREFIX}/share/licenses/${PKGNAME}/LICENSE"

  if command -v update-desktop-database >/dev/null 2>&1; then
    elevate update-desktop-database -q "${PREFIX}/share/applications" || true
  fi
  if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    elevate gtk-update-icon-cache -q -t -f "${PREFIX}/share/icons/hicolor" || true
  fi

  ok "Installed ${PKGNAME}"
  maybe_configure_desktop

  printf '\n  Start:  %s%s%s\n' "$BOLD" "${PKGNAME}" "$RST"
  printf '  Or:     open from the app menu (Omarchy File Browser)\n'
  printf '  Help:   press %s?%s inside the app\n\n' "$BOLD" "$RST"
}

do_uninstall() {
  restore_default_file_manager
  remove_hotkey

  if pacman -Q "$PKGNAME" &>/dev/null; then
    log "Removing pacman package ${PKGNAME}…"
    elevate pacman -Rns --noconfirm "$PKGNAME"
    ok "Removed"
    return
  fi

  log "Removing files from ${PREFIX}…"
  elevate rm -f "${PREFIX}/bin/${PKGNAME}"
  elevate rm -f "${PREFIX}/share/applications/${DESKTOP_ID}"
  elevate rm -f "${PREFIX}/share/icons/hicolor/scalable/apps/${PKGNAME}.svg"
  elevate rm -rf "${PREFIX}/share/${PKGNAME}"
  elevate rm -rf "${PREFIX}/share/licenses/${PKGNAME}"
  if command -v update-desktop-database >/dev/null 2>&1; then
    elevate update-desktop-database -q "${PREFIX}/share/applications" || true
  fi
  ok "Uninstalled (manual install)"
}

do_package() {
  need_cmd makepkg
  need_cmd rsync
  need_cmd tar
  bash "$ROOT/packaging/makepkg-local.sh"
}

usage() {
  cat <<EOF
Omarchy File Browser — installer

  bash install.sh                 Build and install to ${PREFIX}
  bash install.sh package         Build a .pkg.tar.zst into packaging/out/
  bash install.sh uninstall       Remove the application (+ restore defaults)

Desktop integration flags (optional; otherwise asked interactively):
  --default-fm / --no-default-fm  Set/skip as default browser for folders
  --hotkey / --no-hotkey          Bind/skip SUPER+SHIFT+F

Environment:
  PREFIX=/usr          Install prefix (default: /usr)
  BUILD_DIR=/tmp/...   Out-of-tree build dir (default: /tmp/${PKGNAME}-build)
EOF
}

CMD="install"
OPT_DEFAULT_FM=""
OPT_HOTKEY=""
for arg in "$@"; do
  case "$arg" in
    --default-fm) OPT_DEFAULT_FM=y ;;
    --no-default-fm) OPT_DEFAULT_FM=n ;;
    --hotkey) OPT_HOTKEY=y ;;
    --no-hotkey) OPT_HOTKEY=n ;;
    -h|--help|help) CMD="help" ;;
    package|--package) CMD="package" ;;
    uninstall|remove|--uninstall|--remove) CMD="uninstall" ;;
    install|"") CMD="install" ;;
    *) die "Unknown argument: $arg" ;;
  esac
done

case "$CMD" in
  install)
    ensure_deps
    do_install_cmake
    ;;
  package)
    ensure_deps
    do_package
    ;;
  uninstall|remove)
    do_uninstall
    ;;
  help)
    usage
    ;;
  *)
    usage
    die "Unknown command: $CMD"
    ;;
esac
