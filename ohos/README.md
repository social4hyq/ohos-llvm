# ohos-llvm

OHOS-patched LLVM, for native bootstrap on HarmonyOS aarch64 (host = target =
`aarch64-linux-ohos`; **not** a cross-compile).

This repo is a **snapshot of upstream `llvmorg-<ver>` plus the OHOS code-sign
patch** — see `code-sign-<ver>.patch`. It is *not* a live fork of
llvm-project: that history is ~20 GB and impractical to host. Each LLVM
version lands here as a disconnected root commit (snapshot + patch). `origin`
still points at `llvm/llvm-project` so new tags can be fetched.

## What the patch adds

`lld/ELF --code-sign`: at link time, lld injects a `.codesign` section holding
an fs-verity self-signature (Merkle root over the output). HarmonyOS requires
every executable ELF to carry a code-sign block, so this lets the toolchain
produce runnable binaries directly.

The patch also adds a **sync barrier** (`fsync` after `buffer->commit()`,
gated on `--code-sign`) so that an immediate `exec` of a freshly-linked binary
(cargo's build-script flow) no longer hits `ETXTBSY` (Text file busy, os 26)
from hmfs page-cache writeback being still in flight.

## Layout

- `code-sign-<ver>.patch` — the version-specific patch (canonical copy; also
  mirrored in `social4hyq/homebrew-core` tap `Patches/llvm@21/`).
- The applied patch is the top commit on each version branch
  (`git show <patch-commit>` or `git format-patch -1 <patch-commit>`).

## Upgrade

See [`UPGRADING.md`](./UPGRADING.md).

## Build

The build is driven by the Homebrew formula
`social4hyq/homebrew-core` → `Formula/l/llvm@21.rb` (cmake + ninja, clang+lld,
then compiler-rt + OHOS multiarch libc++/libc++abi/libunwind runtimes, signed
in-place via `--code-sign`). A standalone `build-llvm.sh` mirroring the formula
lives in the predecessor tree `Software/ohos-llvm-old/`.
