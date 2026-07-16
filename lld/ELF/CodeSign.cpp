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
#include "CodeSign.h"

#include "lld/Common/ErrorHandler.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Parallel.h"
#include "llvm/Support/SHA256.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace llvm;
using namespace FsVerityCodeSign::FsVerityConstants;

namespace {
size_t getChunkCount(size_t dataSize, size_t divisor) {
  return (size_t)std::ceil((double)dataSize / (double)divisor);
}

size_t getFullChunkSize(size_t dataSize, size_t divisor, size_t multiplier) {
  return getChunkCount(dataSize, divisor) * multiplier;
}

// Reads CHUNK_SIZE(4096) bytes at data and writes the 32-byte SHA256 checksum
// to `output`. If len < CHUNK_SIZE, padding is required where insufficient.
void sha256(const uint8_t *data, size_t len, uint8_t *output) {
  ArrayRef<uint8_t> block;
  std::array<uint8_t, CHUNK_SIZE> tmp{};

  if (len < CHUNK_SIZE) {
    memcpy(tmp.data(), data, len);
    block = ArrayRef<uint8_t>(tmp.data(), CHUNK_SIZE);
  } else {
    block = ArrayRef<uint8_t>(data, CHUNK_SIZE);
  }

  std::array<uint8_t, 32> hash = SHA256::hash(block);
  static_assert(hash.size() == DIGEST_SIZE, "");
  memcpy(output, hash.data(), hash.size());
}
} // namespace

namespace FsVerityCodeSign {

std::vector<uint8_t>
CodeSign::generateMerkleTreeRootHash(ArrayRef<uint8_t> inputBuffer, size_t size,
                                     size_t offset) {
  csOffset = offset;

  // Calculate the offset array for each level of the Merkle tree.
  // Each level contains hashes of the level below it.
  std::vector<size_t> offsetArrays = getOffsetArrays(size);
  assert(offsetArrays.size() >= 2);

  // Allocate buffer to store all intermediate hash values.
  // The size is determined by the total space needed for all tree levels.
  size_t hashBufSize = offsetArrays[offsetArrays.size() - 1];
  auto hashBuffer = std::make_unique<uint8_t[]>(hashBufSize);
  MutableArrayRef<uint8_t> output(hashBuffer.get(), hashBufSize);

  generateHashData(inputBuffer, output, offsetArrays);

  return getMerkleTreeRootHash(output, size);
}

// Calculate the offset of each level in the hash buffer.
std::vector<size_t> CodeSign::getOffsetArrays(size_t dataSize) {
  std::vector<size_t> levelSize = getLevelSize(dataSize);

  // Convert level sizes to cumulative offsets.
  // levelOffset[i] = sum of all level sizes from bottom to level i-1
  std::vector<size_t> levelOffset(levelSize.size() + 1);
  levelOffset[0] = 0;
  for (size_t i = 0; i < levelSize.size(); i++) {
    // Note: levelSize is stored in reverse order (top to bottom),
    // so we reverse the index when accessing it.
    levelOffset[i + 1] = levelOffset[i] + levelSize[levelSize.size() - i - 1];
  }
  return levelOffset;
}

// Calculate the buffer size needed for each level of the Merkle tree.
std::vector<size_t> CodeSign::getLevelSize(size_t dataSize) {
  std::vector<size_t> levelSize;

  size_t fullChunkSize = 0;
  size_t originalDataSize = dataSize;

  do {
    // Calculate how many hashes are needed for this level.
    fullChunkSize = getFullChunkSize(originalDataSize, CHUNK_SIZE, DIGEST_SIZE);
    // Calculate the buffer size needed to store all hashes for this level.
    int size = getFullChunkSize(fullChunkSize, CHUNK_SIZE, CHUNK_SIZE);
    levelSize.push_back(size);
    // Move up one level.
    originalDataSize = fullChunkSize;
  } while (fullChunkSize > CHUNK_SIZE);

  assert(levelSize.size() >= 1);

  return levelSize;
}

// Generate hash data for all levels of the Merkle tree.
void CodeSign::generateHashData(ArrayRef<uint8_t> inputBuffer,
                                MutableArrayRef<uint8_t> outputBuffer,
                                std::vector<size_t> &offsetArrays) {

  const size_t codeSignIndex = getChunkCount(csOffset, CHUNK_SIZE);

  const int lastIndex = offsetArrays.size() - 2;
  for (int i = lastIndex; i >= 0; --i) {
    auto generateHashBuffer =
        outputBuffer.slice(offsetArrays[i], offsetArrays[i + 1] - offsetArrays[i]);

    ArrayRef<uint8_t> originalHashBuffer =
        (i == lastIndex)
            ? inputBuffer
            : outputBuffer.slice(offsetArrays[i + 1], offsetArrays[i + 2] - offsetArrays[i + 1]);

    size_t size = originalHashBuffer.size();

    parallelFor(0, getChunkCount(size, CHUNK_SIZE), [&](size_t i) {
      if (codeSignIndex == i)
        memset(generateHashBuffer.data() + i * DIGEST_SIZE, 0x0, DIGEST_SIZE);
      else
        sha256(originalHashBuffer.data() + i * CHUNK_SIZE,
               std::min(static_cast<size_t>(size - i * CHUNK_SIZE),
                        static_cast<size_t>(CHUNK_SIZE)),
               generateHashBuffer.data() + i * DIGEST_SIZE);
    });
  }
}

// Extract the root hash from the completed Merkle tree.
std::vector<uint8_t>
CodeSign::getMerkleTreeRootHash(MutableArrayRef<uint8_t> dataBuffer,
                                size_t inputDataSize) {
  std::vector<uint8_t> rootHash(DIGEST_SIZE);
  if (inputDataSize <= FSVERITY_HASH_PAGE_SIZE) {
    assert(dataBuffer.size() == DIGEST_SIZE);
    llvm::copy(dataBuffer.slice(0, DIGEST_SIZE), rootHash.begin());
  } else {
    std::array<uint8_t, 32> hash =
        SHA256::hash(dataBuffer.slice(0, FSVERITY_HASH_PAGE_SIZE));
    llvm::copy(hash, rootHash.begin());
  }
  return rootHash;
}
} // namespace FsVerityCodeSign
