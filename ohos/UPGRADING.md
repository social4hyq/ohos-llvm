# Upgrading to a new LLVM version

The OHOS code-sign patch is **version-specific**. `CodeSign.cpp` / `CodeSign.h`
are new files that apply cleanly to any version, but the modifications to
`Writer.cpp` (the `buffer->commit()` site + `writeSelfSign()` machinery),
`Config.h`, `Driver.cpp`, `Options.td`, and `SyntheticSections.*` target lines
that **drift between LLVM releases** — expect to re-point those hunks by hand
on a new version.

## Procedure

```bash
NEWVER=22.1.7
TAG=llvmorg-$NEWVER

# 1. Fetch the new version (shallow — llvm-project full history is ~20 GB)
git clone --depth 1 --branch "$TAG" \
  https://github.com/llvm/llvm-project.git "ohos-llvm-$NEWVER"
cd "ohos-llvm-$NEWVER"
git remote add github https://github.com/social4hyq/ohos-llvm.git

# 2. Try the current patch (from this repo's ohos/ or the tap)
patch -p1 < ohos/code-sign-<OLDVER>.patch
```

- **If it applies cleanly** → done with the patch.
- **If `Writer.cpp` hunks reject** → open `lld/ELF/Writer.cpp`, find
  `Writer<ELFT>::run()`'s tail (the `if (!ctx.e.disableOutput) { if (auto e =
  buffer->commit()) ... }` block), re-apply the `else if (ctx.arg.codeSign) {
  fsync ... }` block and the `writeSelfSign()` call/decl/definition. Then
  regenerate the patch (see below). The `CodeSign.*` / `SyntheticSections` /
  `Config.h` / `Driver.cpp` / `Options.td` parts usually still apply.

### Regenerate the patch after manual re-application

```bash
# pristine copy of the new version's lld/ELF (e.g. from the tarball or a 2nd clone)
diff -ruN --label a/lld/ELF --label b/lld/ELF \
  /path/to/pristine-$NEWVER/lld/ELF lld/ELF > ohos/code-sign-$NEWVER.patch
# verify it round-trips:
cp /path/to/pristine-$NEWVER/lld/ELF/Writer.cpp /tmp/W.cpp
( cd /path/to/pristine-$NEWVER && patch -p1 < ohos/code-sign-$NEWVER.patch --dry-run )
```
Mirror the new patch into the tap
(`homebrew-core/Patches/llvm@<MAJOR>/code-sign.patch`) and bump the formula
`url`/`sha256`.

### Commit + push as a new version branch

```bash
git checkout -b "llvm-$NEWVER"
git add -A
git commit -m "Apply OHOS code-sign patch on $TAG"
# IMPORTANT: a shallow clone's snapshot commit has a missing parent, so a
# plain `git push` fails with "remote: fatal: did not receive expected object".
# Rewrite the snapshot as a root commit, then push:
SNAP=$(git rev-parse HEAD~1)   # the upstream snapshot, one below the patch commit
TREE=$(git rev-parse "$SNAP^{tree}")
ROOT=$(git log -1 --format='%B' "$SNAP" | \
  GIT_AUTHOR_NAME="$(git log -1 --format='%an' "$SNAP")" \
  GIT_AUTHOR_EMAIL="$(git log -1 --format='%ae' "$SNAP")" \
  GIT_AUTHOR_DATE="$(git log -1 --format='%aI' "$SNAP")" \
  GIT_COMMITTER_NAME="$(git log -1 --format='%cn' "$SNAP")" \
  GIT_COMMITTER_DATE="$(git log -1 --format='%cI' "$SNAP")" \
  git commit-tree "$TREE")
git rebase --onto "$ROOT" "$SNAP" "llvm-$NEWVER"
git push github "llvm-$NEWVER"
```

## Version branch convention

- `main` → latest patched LLVM version (the default branch).
- `llvm-<ver>` → each specific version (e.g. `llvm-21.1.8`, `llvm-22.1.7`),
  each a disconnected snapshot-root + patch commit.

This keeps multiple coexisting versions addressable while staying within
GitHub's practical repo-size limits (each branch is a shallow snapshot, not the
full upstream DAG).
