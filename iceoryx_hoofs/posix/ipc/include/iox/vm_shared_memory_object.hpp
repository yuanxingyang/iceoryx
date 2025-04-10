// Copyright (c) 2019 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2021 - 2023 by Apex.AI Inc. All rights reserved.
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

#ifndef IOX_HOOFS_VM_ICP_VM_SHARED_MEMORY_OBJECT_HPP
#define IOX_HOOFS_VM_ICP_VM_SHARED_MEMORY_OBJECT_HPP

#include "iceoryx_platform/stat.hpp"
#include "iox/builder.hpp"
#include "iox/bump_allocator.hpp"
#include "iox/detail/vm_memory_map.hpp"
#include "iox/detail/vm_shared_memory.hpp"
#include "iox/file_management_interface.hpp"
#include "iox/filesystem.hpp"
#include "iox/optional.hpp"

#include <cstdint>

namespace iox
{

enum class VMSharedMemoryObjectError : uint8_t
{
    SHARED_MEMORY_CREATION_FAILED,
    MAPPING_SHARED_MEMORY_FAILED,
    UNABLE_TO_VERIFY_MEMORY_SIZE,
    REQUESTED_SIZE_EXCEEDS_ACTUAL_SIZE,
    INTERNAL_LOGIC_FAILURE,
};

enum class VMSharedMemoryAllocationError : uint8_t
{
    REQUESTED_MEMORY_AFTER_FINALIZED_ALLOCATION,
    NOT_ENOUGH_MEMORY,
    REQUESTED_ZERO_SIZED_MEMORY

};

class VMSharedMemoryObjectBuilder;

/// @brief Creates a shared memory segment and maps it into the process space.
///        One can use optionally the allocator to acquire memory.
//class VMSharedMemoryObject : public FileManagementInterface<VMSharedMemoryObject>
class VMSharedMemoryObject
{
  public:
    using Builder = VMSharedMemoryObjectBuilder;

    static constexpr const void* const NO_ADDRESS_HINT = nullptr;
    VMSharedMemoryObject(const VMSharedMemoryObject&) = delete;
    VMSharedMemoryObject& operator=(const VMSharedMemoryObject&) = delete;
    VMSharedMemoryObject(VMSharedMemoryObject&&) noexcept;
    VMSharedMemoryObject& operator=(VMSharedMemoryObject&&) noexcept;
    ~VMSharedMemoryObject() noexcept = default;

    /// @brief Returns start- or base-address of the shared memory.
    const void* getBaseAddress() const noexcept;

    /// @brief Returns start- or base-address of the shared memory.
    void* getBaseAddress() noexcept;

    /// @brief Returns the underlying file handle of the shared memory
    shm_handle_t getFileHandle() const noexcept;

    /// @brief True if the shared memory has the ownership. False if an already
    ///        existing shared memory was opened.
    bool hasOwnership() const noexcept;

    /// @brief Returns the size of the corresponding file.
    /// @return On failure a 'FileStatError' describing the error otherwise the size.
    expected<uint64_t, FileStatError> get_size() const noexcept;

    friend class VMSharedMemoryObjectBuilder;

  private:
    VMSharedMemoryObject(detail::VMSharedMemory&& sharedMemory, detail::VMMemoryMap&& memoryMap) noexcept;

    friend struct FileManagementInterface<VMSharedMemoryObject>;
    shm_handle_t get_file_handle() const noexcept;

  private:
    detail::VMSharedMemory m_sharedMemory;
    detail::VMMemoryMap m_memoryMap;
};

class VMSharedMemoryObjectBuilder
{
    /// @brief A valid file name for the shared memory with the restriction that
    ///        no leading dot is allowed since it is not compatible with every
    ///        file system
    IOX_BUILDER_PARAMETER(detail::VMSharedMemory::Name_t, name, "")

    /// @brief Defines the size of the shared memory
    IOX_BUILDER_PARAMETER(uint64_t, memorySizeInBytes, 0U)

    /// @brief Defines if the memory should be mapped read only or with write access.
    ///        A read only memory section will cause a segmentation fault when written to.
    IOX_BUILDER_PARAMETER(AccessMode, accessMode, AccessMode::ReadOnly)

    /// @brief Defines how the shared memory is acquired
    IOX_BUILDER_PARAMETER(OpenMode, openMode, OpenMode::OpenExisting)

    /// @brief If this is set to a non null address create will try to map the shared
    ///        memory to the provided address. Since it is a hint, this mapping can
    ///        fail. The .getBaseAddress() method of the SharedMemoryObject returns
    ///        the actual mapped base address.
    IOX_BUILDER_PARAMETER(optional<const void*>, baseAddressHint, nullopt)

    /// @brief Defines the access permissions of the shared memory
    IOX_BUILDER_PARAMETER(access_rights, permissions, perms::none)

  public:
    expected<VMSharedMemoryObject, VMSharedMemoryObjectError> create() noexcept;
};
} // namespace iox

#endif // IOX_HOOFS_VM_ICP_VM_SHARED_MEMORY_OBJECT_HPP
