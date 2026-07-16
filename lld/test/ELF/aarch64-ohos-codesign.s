# REQUIRES: aarch64
# RUN: llvm-mc -filetype=obj -triple=aarch64-linux-ohos %s -o %t.o

# Test basic code signing functionality
# RUN: ld.lld %t.o --code-sign -o %t

# Verify section header information
# RUN: llvm-readelf -S %t | FileCheck --check-prefix=SECTION %s

# Verify section content (hex dump)
# RUN: llvm-readelf -x .codesign %t | FileCheck --check-prefix=CONTENT %s

# SECTION: .codesign
# SECTION-SAME: PROGBITS
# The following checks .codesign section size: 4096
# SECTION-SAME: {{[0-9a-f]+}} {{[0-9a-f]+}} 001000

# Verify FsVerityWithSign structure in hex dump
# CONTENT: Hex dump of section '.codesign':
# - type: 0x1 (FS_VERITY_DESCRIPTOR_TYPE)
# - length: 0x120
# - version: 0x1
# - hashAlgorithm: 0x1 (SHA256)
# - log2BlockSize: 0x0c (12, meaning 4096 bytes)
# - signSize: 0x20
# CONTENT-NEXT: 0x{{[0-9a-f]+}} 01000000 20010000 01010c00 20000000

.globl _start
_start:
  ret
