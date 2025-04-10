// Copyright (c) 2019 by Robert Bosch GmbH. All rights reserved.
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

#ifndef IOX_HOOFS_VM_IPC_VM_MEMORY_MAP_HPP
#define IOX_HOOFS_VM_IPC_VM_MEMORY_MAP_HPP

#include "iceoryx_platform/mman.hpp"
#include "iox/builder.hpp"
#include "iox/expected.hpp"
#include "iox/filesystem.hpp"

#include <cstdint>

namespace iox
{
namespace detail
{

enum class VMMemoryMapError : uint8_t
{
    ACCESS_FAILED,
    UNABLE_TO_LOCK,
    INVALID_FILE_DESCRIPTOR,
    MAP_OVERLAP,
    INVALID_PARAMETERS,
    OPEN_FILES_SYSTEM_LIMIT_EXCEEDED,
    FILESYSTEM_DOES_NOT_SUPPORT_MEMORY_MAPPING,
    NOT_ENOUGH_MEMORY_AVAILABLE,
    OVERFLOWING_PARAMETERS,
    PERMISSION_FAILURE,
    NO_WRITE_PERMISSION,
    UNKNOWN_ERROR
};

/// @brief Flags defining how the mapped data should be handled
// NOLINTNEXTLINE(performance-enum-size) int32_t required for POSIX API
enum class VMMemoryMapFlags : int32_t
{
    /// @brief changes are shared
    SHARE_CHANGES = MAP_SHARED,

    /// @brief changes are private
    PRIVATE_CHANGES = MAP_PRIVATE,

    /// @brief SHARED and enforce the base address hint
    // NOLINTNEXTLINE(hicpp-signed-bitwise) enum type is defined by POSIX, no logical fault
    SHARE_CHANGES_AND_FORCE_BASE_ADDRESS_HINT = MAP_SHARED | MAP_FIXED,

    /// @brief PRIVATE and enforce the base address hint
    // NOLINTNEXTLINE(hicpp-signed-bitwise) enum type is defined POSIX, no logical fault
    PRIVATE_CHANGES_AND_FORCE_BASE_ADDRESS_HINT = MAP_PRIVATE | MAP_FIXED,
};

class VMMemoryMap;
/// @brief The builder of a 'VMMemoryMap' object
class VMMemoryMapBuilder
{
    /// @brief The base address suggestion to which the memory should be mapped. But
    ///        there is no guarantee that it is really mapped at this position.
    ///        One has to verify with .getBaseAddress if the hint was accepted.
    ///        Setting it to nullptr means no suggestion
    IOX_BUILDER_PARAMETER(const void*, baseAddressHint, nullptr)

    /// @brief The length of the memory which should be mapped
    IOX_BUILDER_PARAMETER(uint64_t, length, 0U)

    /// @brief The file descriptor which should be mapped into process space
    IOX_BUILDER_PARAMETER(int32_t, fileDescriptor, 0)

    /// @brief Defines if the memory should be mapped read only or with write access.
    ///        A read only memory section will cause a segmentation fault when written to.
    IOX_BUILDER_PARAMETER(AccessMode, accessMode, AccessMode::ReadWrite)

    /// @brief Sets the flags defining how the mapped data should be handled
    IOX_BUILDER_PARAMETER(VMMemoryMapFlags, flags, VMMemoryMapFlags::SHARE_CHANGES)

    /// @brief Offset of the memory location
    IOX_BUILDER_PARAMETER(off_t, offset, 0)

  public:
    /// @brief creates a valid 'VMMemoryMap' object. If the construction failed the
    ///        expected contains an enum value describing the error.
    /// @return expected containing 'VMMemoryMap' on success otherwise 'VMMemoryMapError'
    expected<VMMemoryMap, VMMemoryMapError> create() noexcept;
};

/// @brief C++ abstraction of mmap and munmap. When a 'VMMemoryMap' object is
///        created the configured memory is mapped into the process space until
///        that object goes out of scope - then munmap is called and the memory
///        region is removed from the process space.
class VMMemoryMap
{
  public:
    /// @brief copy operations are removed since we are handling a system resource
    VMMemoryMap(const VMMemoryMap&) = delete;
    VMMemoryMap& operator=(const VMMemoryMap&) = delete;

    /// @brief move constructor
    /// @param[in] rhs the source object
    VMMemoryMap(VMMemoryMap&& rhs) noexcept;

    /// @brief move assignment operator
    /// @param[in] rhs the source object
    /// @return reference to *this
    VMMemoryMap& operator=(VMMemoryMap&& rhs) noexcept;

    /// @brief destructor, calls munmap when the underlying memory is mapped
    ~VMMemoryMap() noexcept;

    /// @brief returns the base address, if the object was moved it returns nullptr
    const void* getBaseAddress() const noexcept;

    /// @brief returns the base address, if the object was moved it returns nullptr
    void* getBaseAddress() noexcept;

    friend class VMMemoryMapBuilder;

  private:
    VMMemoryMap(void* const baseAddress, const uint64_t length) noexcept;
    bool destroy() noexcept;
    static VMMemoryMapError errnoToEnum(const int32_t errnum) noexcept;

    void* m_baseAddress{nullptr};
    uint64_t m_length{0U};
};
} // namespace detail
} // namespace iox

#endif // IOX_HOOFS_VM_IPC_VM_MEMORY_MAP_HPP
