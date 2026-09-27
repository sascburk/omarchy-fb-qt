#!/usr/bin/env bash
# Build a local pacman package without a GitHub release (developer testing).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PKGVER="$(rg -n '^pkgver=' "$ROOT/packaging/PKGBUILD" | head -1 | cut -d= -f2)"
PKGNAME=omarchy-fb-qt
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

STAGE="$WORKDIR/$PKGNAME-$PKGVER"
mkdir -p "$STAGE"

rsync -a \
  --exclude '.git/' \
  --exclude 'build/' \
  --exclude 'cmake-build-*/' \
  --exclude '.cache/' \
  --exclude '*.user' \
  "$ROOT/" "$STAGE/"

cp "$ROOT/packaging/PKGBUILD" "$WORKDIR/PKGBUILD"
cp "$ROOT/packaging/$PKGNAME.desktop" "$WORKDIR/"
cp "$ROOT/packaging/$PKGNAME.install" "$WORKDIR/"
cp "$ROOT/packaging/$PKGNAME.svg" "$WORKDIR/"

# Local source tarball + skip integrity (no remote artifact).
sed -i \
  -e "s|^source=.*|source=(\"$PKGNAME-$PKGVER.tar.gz\")|" \
  -e "s|^sha256sums=.*|sha256sums=('SKIP')|" \
  "$WORKDIR/PKGBUILD"

tar -C "$WORKDIR" -czf "$WORKDIR/$PKGNAME-$PKGVER.tar.gz" "$PKGNAME-$PKGVER"

echo "→ makepkg in $WORKDIR"
(cd "$WORKDIR" && makepkg -f --noconfirm)

OUT="$(ls -1 "$WORKDIR"/"$PKGNAME-$PKGVER"-*.pkg.tar.* | head -1)"
mkdir -p "$ROOT/packaging/out"
cp -v "$OUT" "$ROOT/packaging/out/"
echo "Package: $ROOT/packaging/out/$(basename "$OUT")"
echo "Install: sudo pacman -U $ROOT/packaging/out/$(basename "$OUT")"
