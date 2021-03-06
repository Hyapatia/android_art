/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_RUNTIME_BIT_MEMORY_REGION_H_
#define ART_RUNTIME_BIT_MEMORY_REGION_H_

#include "memory_region.h"

#include "base/bit_utils.h"

namespace art {

// Bit memory region is a bit offset subregion of a normal memoryregion. This is useful for
// abstracting away the bit start offset to avoid needing passing as an argument everywhere.
class BitMemoryRegion FINAL : public ValueObject {
 public:
  BitMemoryRegion() = default;
  ALWAYS_INLINE explicit BitMemoryRegion(MemoryRegion region)
    : data_(AlignDown(reinterpret_cast<uintptr_t*>(region.pointer()), sizeof(uintptr_t))),
      bit_start_(8 * (reinterpret_cast<uintptr_t>(region.pointer()) % sizeof(uintptr_t))),
      bit_size_(region.size_in_bits()) {
  }
  ALWAYS_INLINE BitMemoryRegion(MemoryRegion region, size_t bit_offset, size_t bit_length)
    : BitMemoryRegion(region) {
    DCHECK_LE(bit_offset + bit_length, bit_size_);
    bit_start_ += bit_offset;
    bit_size_ = bit_length;
  }

  ALWAYS_INLINE bool IsValid() const { return data_ != nullptr; }

  size_t size_in_bits() const {
    return bit_size_;
  }

  ALWAYS_INLINE BitMemoryRegion Subregion(size_t bit_offset, size_t bit_length) const {
    DCHECK_LE(bit_offset + bit_length, bit_size_);
    BitMemoryRegion result = *this;
    result.bit_start_ += bit_offset;
    result.bit_size_ = bit_length;
    return result;
  }

  // Load a single bit in the region. The bit at offset 0 is the least
  // significant bit in the first byte.
  ALWAYS_INLINE bool LoadBit(uintptr_t bit_offset) const {
    DCHECK_LT(bit_offset, bit_size_);
    size_t index = (bit_start_ + bit_offset) / kBitsPerIntPtrT;
    size_t shift = (bit_start_ + bit_offset) % kBitsPerIntPtrT;
    return ((data_[index] >> shift) & 1) != 0;
  }

  ALWAYS_INLINE void StoreBit(uintptr_t bit_offset, bool value) const {
    DCHECK_LT(bit_offset, bit_size_);
    size_t index = (bit_start_ + bit_offset) / kBitsPerIntPtrT;
    size_t shift = (bit_start_ + bit_offset) % kBitsPerIntPtrT;
    data_[index] &= ~(static_cast<uintptr_t>(1) << shift);  // Clear bit.
    data_[index] |= static_cast<uintptr_t>(value ? 1 : 0) << shift;  // Set bit.
    DCHECK_EQ(value, LoadBit(bit_offset));
  }

  // Load `bit_length` bits from `data` starting at given `bit_offset`.
  // The least significant bit is stored in the smallest memory offset.
  ALWAYS_INLINE uintptr_t LoadBits(size_t bit_offset, size_t bit_length) const {
    DCHECK(IsAligned<sizeof(uintptr_t)>(data_));
    DCHECK_LE(bit_length, BitSizeOf<uintptr_t>());
    DCHECK_LE(bit_offset + bit_length, bit_size_);
    if (bit_length == 0) {
      return 0;
    }
    uintptr_t mask = std::numeric_limits<uintptr_t>::max() >> (kBitsPerIntPtrT - bit_length);
    size_t index = (bit_start_ + bit_offset) / kBitsPerIntPtrT;
    size_t shift = (bit_start_ + bit_offset) % kBitsPerIntPtrT;
    uintptr_t value = data_[index] >> shift;
    size_t finished_bits = kBitsPerIntPtrT - shift;
    if (finished_bits < bit_length) {
      value |= data_[index + 1] << finished_bits;
    }
    return value & mask;
  }

  // Store `bit_length` bits in `data` starting at given `bit_offset`.
  // The least significant bit is stored in the smallest memory offset.
  void StoreBits(size_t bit_offset, uintptr_t value, size_t bit_length) {
    DCHECK(IsAligned<sizeof(uintptr_t)>(data_));
    DCHECK_LE(bit_length, BitSizeOf<uintptr_t>());
    DCHECK_LE(bit_offset + bit_length, bit_size_);
    DCHECK_LE(value, MaxInt<uintptr_t>(bit_length));
    if (bit_length == 0) {
      return;
    }
    uintptr_t mask = std::numeric_limits<uintptr_t>::max() >> (kBitsPerIntPtrT - bit_length);
    size_t index = (bit_start_ + bit_offset) / kBitsPerIntPtrT;
    size_t shift = (bit_start_ + bit_offset) % kBitsPerIntPtrT;
    data_[index] &= ~(mask << shift);  // Clear bits.
    data_[index] |= (value << shift);  // Set bits.
    size_t finished_bits = kBitsPerIntPtrT - shift;
    if (finished_bits < bit_length) {
      data_[index + 1] &= ~(mask >> finished_bits);  // Clear bits.
      data_[index + 1] |= (value >> finished_bits);  // Set bits.
    }
    DCHECK_EQ(value, LoadBits(bit_offset, bit_length));
  }

  ALWAYS_INLINE bool Equals(const BitMemoryRegion& other) const {
    return data_ == other.data_ &&
           bit_start_ == other.bit_start_ &&
           bit_size_ == other.bit_size_;
  }

 private:
  // The data pointer must be naturally aligned. This makes loading code faster.
  uintptr_t* data_ = nullptr;
  size_t bit_start_ = 0;
  size_t bit_size_ = 0;
};

}  // namespace art

#endif  // ART_RUNTIME_BIT_MEMORY_REGION_H_
