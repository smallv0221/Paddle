/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserve.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#pragma once

#include "paddle/memory/detail/meta_cache.h"
#include "paddle/memory/detail/meta_data.h"
#include "paddle/memory/detail/system_allocator.h"
#include "paddle/platform/assert.h"
#include "paddle/platform/cpu_info.h"
#include "paddle/platform/gpu_info.h"

#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>

namespace paddle {
namespace memory {
namespace detail {

class BuddyAllocator {
 public:
  BuddyAllocator(SystemAllocator* system_allocator, size_t min_chunk_size,
                 size_t max_chunk_size);

  ~BuddyAllocator();

 public:
  void* Alloc(size_t unaligned_size);
  void Free(void*);
  size_t Used();

 public:
  // Disable copy and assignment.
  BuddyAllocator(const BuddyAllocator&) = delete;
  BuddyAllocator& operator=(const BuddyAllocator&) = delete;

 private:
  // Tuple type: allocator index, memory size, memory address
  using IndexSizeAddress = std::tuple<size_t, size_t, void*>;
  using PoolSet = std::set<IndexSizeAddress>;

  /*! \brief Allocate fixed-size memory from system */
  void* SystemAlloc(size_t size);

  /*! \brief If existing chunks are not suitable, refill pool */
  PoolSet::iterator RefillPool();

  /**
   *  \brief Find the suitable chunk from existing pool
   *
   *  \param it   pool iterator which contains suitable block.
   *  \param size the size of allocation.
   */
  void* SplitToAlloc(PoolSet::iterator it, size_t size);

  /*! \brief Find the existing chunk which used to allocation  */
  PoolSet::iterator FindExistChunk(size_t size);

 private:
  size_t total_used_ = 0;  // the total size of used memory
  size_t total_free_ = 0;  // the total size of free memory

  size_t min_chunk_size_;  // the minimum size of each chunk
  size_t max_chunk_size_;  // the maximum size of each chunk

 private:
  PoolSet pool_;

 private:
  // Unify the metadata format between GPU and CPU allocations
  MetadataCache cache_;

 private:
  SystemAllocator* system_allocator_;
  std::mutex mutex_;
};

BuddyAllocator* GetCPUBuddyAllocator() {
  static BuddyAllocator* a = nullptr;
  if (a == nullptr) {
    a = new BuddyAllocator(new CPUAllocator, platform::CpuMinChunkSize(),
                           platform::CpuMaxChunkSize());
  }
  return a;
}

#ifndef PADDLE_ONLY_CPU  // The following code are for CUDA.

BuddyAllocator* GetGPUBuddyAllocator(int gpu_id) {
  static BuddyAllocator** as = NULL;
  if (as == NULL) {
    int gpu_num = platform::GpuDeviceCount();
    as = new BuddyAllocator*[gpu_num];
    for (int gpu = 0; gpu < gpu_num; gpu++) {
      as[gpu] =
          new BuddyAllocator(new GPUAllocator, platform::GpuMinChunkSize(),
                             platform::GpuMaxChunkSize());
    }
  }
  return as[gpu_id];
}

#endif  // PADDLE_ONLY_CPU

}  // namespace detail
}  // namespace memory
}  // namespace paddle
