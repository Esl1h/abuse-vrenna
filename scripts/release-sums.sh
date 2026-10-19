#!/bin/sh
# SHA256SUMS for a GitHub Release.
#
#   scripts/release-sums.sh TAG            sum what is attached to the release
#   scripts/release-sums.sh TAG --upload   and attach the result as SHA256SUMS
#   scripts/release-sums.sh --dir DIR      sum the files in DIR, downloading nothing
#
# Run it last. The Linux packages are uploaded by hand and the Windows zip is
# attached by CI, and this sums whatever is attached at the moment it runs: a
# file added afterwards is not covered until it runs again, and running it
# again replaces the previous SHA256SUMS. The file is plain `sha256sum` output,
# so a download is checked with
#
#     sha256sum -c SHA256SUMS --ignore-missing
set -eu

usage() {
    echo "usage: $0 TAG [--upload] | --dir DIR" >&2
    exit 2
}

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM

tag=""
dir=""
upload=0
while [ $# -gt 0 ]; do
    case $1 in
        --upload) upload=1 ;;
        --dir) shift; [ $# -gt 0 ] || usage; dir=$1 ;;
        -*) usage ;;
        *) [ -z "$tag" ] || usage; tag=$1 ;;
    esac
    shift
done
[ -n "$tag" ] || [ -n "$dir" ] || usage
[ -z "$dir" ] || [ "$upload" -eq 0 ] || { echo "release-sums: --dir cannot upload" >&2; exit 2; }

# gh picks the upstream repository in a fork, which is not the one to sum.
repo=$(git remote get-url origin | sed -E 's#^(git@github\.com:|https://github\.com/)##; s#\.git$##')

if [ -n "$dir" ]; then
    src=$dir
else
    src=$tmp/assets
    mkdir "$src"
    gh release download "$tag" -R "$repo" -D "$src" --pattern '*'
fi

mkdir "$tmp/out"
(
    cd "$src"
    for f in *; do
        [ -f "$f" ] || continue
        [ "$f" != SHA256SUMS ] || continue
        sha256sum -- "$f"
    done
) > "$tmp/out/SHA256SUMS"

[ -s "$tmp/out/SHA256SUMS" ] || { echo "release-sums: nothing to sum" >&2; exit 1; }
cat "$tmp/out/SHA256SUMS"

if [ "$upload" -eq 1 ]; then
    gh release upload "$tag" -R "$repo" "$tmp/out/SHA256SUMS" --clobber
fi
