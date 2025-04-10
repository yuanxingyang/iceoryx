// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2021 - 2022 by Apex.AI Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef IOX_POSH_ROUDI_MEMORY_MEMPOOL_SEGMENT_MANAGER_INL
#define IOX_POSH_ROUDI_MEMORY_MEMPOOL_SEGMENT_MANAGER_INL

#include "iceoryx_posh/internal/roudi/memory/mempool_segment_manager.hpp"

#include "iox/assertions.hpp"
#include "iox/bump_allocator.hpp"
#include "iox/memory.hpp"

namespace iox
{
namespace roudi
{

template <typename SegmentType>
inline MemPoolSegmentManager<SegmentType>::MemPoolSegmentManager(const mepoo::SegmentConfig& segmentConfig,
                                                                   const DomainId domainId) noexcept
    : m_segmentConfig(segmentConfig)
    , m_domainId(domainId)
{
}

template <typename SegmentType>
inline MemPoolSegmentManager<SegmentType>::~MemPoolSegmentManager() noexcept
{
    destroy();
}

template <typename SegmentType>
inline uint64_t MemPoolSegmentManager<SegmentType>::size() const noexcept
{
    const uint64_t segmentManagerSize = sizeof(mepoo::SegmentManager<SegmentType>);
    return align(segmentManagerSize, mepoo::MemPool::CHUNK_MEMORY_ALIGNMENT)
           + mepoo::SegmentManager<SegmentType>::requiredManagementMemorySize(m_segmentConfig);
}

template <typename SegmentType>
inline uint64_t MemPoolSegmentManager<SegmentType>::alignment() const noexcept
{
    const uint64_t segmentManagerAlignment = alignof(mepoo::SegmentManager<SegmentType>);
    return algorithm::maxVal(segmentManagerAlignment, mepoo::MemPool::CHUNK_MEMORY_ALIGNMENT);
}

template <typename SegmentType>
inline void MemPoolSegmentManager<SegmentType>::onMemoryAvailable(not_null<void*> memory) noexcept
{
    BumpAllocator allocator(memory, size());
    auto* segmentManager = allocator.allocate(sizeof(mepoo::SegmentManager<SegmentType>), alignof(mepoo::SegmentManager<SegmentType>))
                               .expect("There should be enough memory for the 'SegmentManager'");
    m_segmentManager = new (segmentManager) mepoo::SegmentManager<SegmentType>(m_segmentConfig, m_domainId, &allocator);
}

template <typename SegmentType>
inline void MemPoolSegmentManager<SegmentType>::destroy() noexcept
{
    if (m_segmentManager)
    {
        m_segmentManager->~SegmentManager();
        m_segmentManager = nullptr;
    }
}

template <typename SegmentType>
inline optional<mepoo::SegmentManager<SegmentType>*> MemPoolSegmentManager<SegmentType>::segmentManager() const noexcept
{
    return m_segmentManager ? make_optional<mepoo::SegmentManager<SegmentType>*>(m_segmentManager) : nullopt_t();
}

} // namespace roudi
} // namespace iox

#endif //IOX_POSH_ROUDI_MEMORY_MEMPOOL_SEGMENT_MANAGER_INL
