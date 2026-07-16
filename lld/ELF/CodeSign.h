/*
 * Copyright (c) 2025-2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef CODESIGN_H
#define CODESIGN_H

#include "llvm/ADT/ArrayRef.h"
#include <cstddef>
#include <cstdint>

namespace FsVerityCodeSign {

namespace FsVerityConstants {
constexpr uint8_t VERSION = 0x1;
constexpr uint8_t FS_VERITY_DESCRIPTOR_TYPE = 0x1;
constexpr uint8_t SHA256_ALGORITHM = 0x1;
constexpr uint8_t ELF_CODE_SIGN_VERSION = 0x3;

constexpr int8_t LOG_2_OF_FSVERITY_HASH_PAGE_SIZE = 12;
constexpr int FSVERITY_HASH_PAGE_SIZE = 4096;
constexpr int CHUNK_SIZE = 4096;
constexpr int DIGEST_SIZE = 32;
constexpr int ROOT_HASH_SIZE = 32;
constexpr int DESCRIPTOR_SIZE = 256;

constexpr int FLAG_SELF_SIGN = 1 << 4;
} // namespace FsVerityConstants

class CodeSign {
public:
  // Generate the Merkle tree root hash for the input buffer.
  // This implements the fs-verity Merkle tree algorithm, which recursively
  // hashes 4KB chunks of data to build a tree structure.
  std::vector<uint8_t>
  generateMerkleTreeRootHash(llvm::ArrayRef<uint8_t> inputBuffer, size_t size,
                             size_t csOffset);

private:
  std::vector<size_t> getOffsetArrays(size_t dataSize);
  std::vector<size_t> getLevelSize(size_t dataSize);
  void generateHashData(llvm::ArrayRef<uint8_t> inputBuffer,
                        llvm::MutableArrayRef<uint8_t> outputBuffer,
                        std::vector<size_t> &offsetArrays);
  std::vector<uint8_t>
  getMerkleTreeRootHash(llvm::MutableArrayRef<uint8_t> dataBuffer,
                        size_t inputDataSize);
  size_t csOffset = 0;
};

LLVM_PACKED_START
struct FsVerity {
  uint8_t version;
  uint8_t hashAlgorithm;
  uint8_t log2BlockSize;
  uint8_t saltSize;
  uint32_t signSize;
  uint64_t fileSize;
  uint8_t rawRootHash[64];
  uint8_t salt[32];
  uint32_t flag;
  uint8_t reservedAfterFlags[4];
  uint64_t merkleTreeOffset;
  uint8_t reservedAfterTreeOffset[127];
  uint8_t csVersion;
};

struct FsVerityWithSign {
  uint32_t type;
  uint32_t length;
  FsVerity fsVerity;
  uint8_t signature[32];
};
LLVM_PACKED_END

} // namespace FsVerityCodeSign
#endif // CODESIGN_H
