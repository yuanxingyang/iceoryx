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

#ifndef IOX_HOOFS_CONTAINERS_UNINITIALIZED_ARRAY_HPP
#define IOX_HOOFS_CONTAINERS_UNINITIALIZED_ARRAY_HPP

#include <cstdint>

namespace iox
{
namespace containers
{
// remark: we can add more functionality (policies for cache line size, redzoning)

template <typename ElementType, uint64_t Capacity, typename index_t = uint64_t>
class UnitializedArray
{
  public:
    UnitializedArray() noexcept = default;
    ~UnitializedArray() noexcept = default;

    UnitializedArray(const UnitializedArray&) = delete;
    UnitializedArray(UnitializedArray&&) = delete;
    UnitializedArray& operator=(const UnitializedArray&) = delete;
    UnitializedArray& operator=(UnitializedArray&&) = delete;

    ElementType& operator[](const index_t index) noexcept;

    const ElementType& operator[](const index_t index) const noexcept;

    ElementType* ptr(const index_t index) noexcept;

    const ElementType* ptr(const index_t index) const noexcept;

    uint64_t capacity() const noexcept;

  private:
    using byte_t = uint8_t;

    // NOLINTJUSTIFICATION required by low level UnitializedArray building block and encapsulated in abstraction
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays, hicpp-avoid-c-arrays)
    alignas(ElementType) byte_t m_buffer[Capacity * sizeof(ElementType)];

    ElementType* toPtr(index_t index) const noexcept;
};

} // namespace containers
} // namespace iox

#include "iceoryx_hoofs/internal/containers/uninitialized_array.inl"

#endif // IOX_HOOFS_CONTAINERS_UNINITIALIZED_ARRAY_HPP
