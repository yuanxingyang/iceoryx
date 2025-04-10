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

#include "iox/detail/vm_shared_memory.hpp"
#include "iceoryx_platform/fcntl.hpp"
#include "iceoryx_platform/mman.hpp"
#include "iceoryx_platform/stat.hpp"
#include "iceoryx_platform/types.hpp"
#include "iceoryx_platform/unistd.hpp"
#include "iox/filesystem.hpp"
#include "iox/logging.hpp"
#include "iox/posix_call.hpp"
#include "iox/scope_guard.hpp"

#include <cassert>

namespace iox
{
namespace detail
{

static string<VMSharedMemory::Name_t::capacity() + 1> addLeadingSlash(const VMSharedMemory::Name_t& name) noexcept
{
    string<VMSharedMemory::Name_t::capacity() + 1> nameWithLeadingSlash = "/dev/";
    nameWithLeadingSlash.append(TruncateToCapacity, name);
    return nameWithLeadingSlash;
}
// NOLINTJUSTIFICATION the function size and cognitive complexity results from the error handling and the expanded log macro
// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
expected<VMSharedMemory, VMSharedMemoryError> VMSharedMemoryBuilder::create() noexcept
{
    shm_handle_t sharedMemoryFileHandle = VMSharedMemory::INVALID_HANDLE;
#ifndef __ANDROID_VM_SHM__
    bool hasOwnership = (m_openMode == OpenMode::ExclusiveCreate || m_openMode == OpenMode::PurgeAndCreate
                         || m_openMode == OpenMode::OpenOrCreate);
#else
    auto printError = [this] {
        IOX_LOG(Error,
                "Unable to create shared memory with the following properties [ name = "
                    << m_name << ", access mode = " << asStringLiteral(m_accessMode)
                    << ", open mode = " << asStringLiteral(m_openMode)
                    << ", mode = " << iox::log::oct(m_filePermissions.value()) << ", sizeInBytes = " << m_size << " ]");
    };


    // on qnx the current working directory will be added to the /dev/shmem path if the leading slash is missing
    if (m_name.empty())
    {
        IOX_LOG(Error, "No shared memory name specified!");
        return err(VMSharedMemoryError::EMPTY_NAME);
    }

    if (!isValidFileName(m_name))
    {
        IOX_LOG(Error,
                "Shared memory requires a valid file name (not path) as name and \"" << m_name
                                                                                     << "\" is not a valid file name");
        return err(VMSharedMemoryError::INVALID_FILE_NAME);
    }

    auto nameWithLeadingSlash = addLeadingSlash(m_name);

    bool hasOwnership = (m_openMode == OpenMode::ExclusiveCreate || m_openMode == OpenMode::PurgeAndCreate
                         || m_openMode == OpenMode::OpenOrCreate);

    if (hasOwnership && (m_accessMode == AccessMode::ReadOnly))
    {
        IOX_LOG(Error,
                "Cannot create shared-memory file \"" << m_name << "\" in read-only mode. "
                                                      << "Initializing a new file requires write access");
        return err(VMSharedMemoryError::INCOMPATIBLE_OPEN_AND_ACCESS_MODE);
    }

    // the mask will be applied to the permissions, therefore we need to set it to 0
    mode_t umaskSaved = umask(0U);
    {
        ScopeGuard umaskGuard([&] { umask(umaskSaved); });

        if (m_openMode == OpenMode::PurgeAndCreate)
        {
            IOX_DISCARD_RESULT(IOX_POSIX_CALL(iox_shm_unlink)(nameWithLeadingSlash.c_str())
                                   .failureReturnValue(VMSharedMemory::INVALID_HANDLE)
                                   .ignoreErrnos(ENOENT)
                                   .evaluate());
        }

        auto result =
            IOX_POSIX_CALL(iox_shm_open)(
                nameWithLeadingSlash.c_str(),
                convertToOflags(m_accessMode,
                                (m_openMode == OpenMode::OpenOrCreate) ? OpenMode::ExclusiveCreate : m_openMode),
                m_filePermissions.value())
                .failureReturnValue(VMSharedMemory::INVALID_HANDLE)
                .suppressErrorMessagesForErrnos((m_openMode == OpenMode::OpenOrCreate) ? EEXIST : 0)
                .evaluate();
        if (result.has_error())
        {
            // if it was not possible to create the shm exclusively someone else has the
            // ownership and we just try to open it
            if (m_openMode == OpenMode::OpenOrCreate && result.error().errnum == EEXIST)
            {
                hasOwnership = false;
                result = IOX_POSIX_CALL(iox_shm_open)(nameWithLeadingSlash.c_str(),
                                                      convertToOflags(m_accessMode, OpenMode::OpenExisting),
                                                      m_filePermissions.value())
                             .failureReturnValue(VMSharedMemory::INVALID_HANDLE)
                             .evaluate();
            }

            // Check again, as the if-block above may have changed 'result'
            if (result.has_error())
            {
                printError();
                return err(VMSharedMemory::errnoToEnum(result.error().errnum));
            }
        }
        sharedMemoryFileHandle = result->value;
    }

    if (hasOwnership)
    {
        auto result = IOX_POSIX_CALL(iox_ftruncate)(sharedMemoryFileHandle, static_cast<off_t>(m_size))
                          .failureReturnValue(VMSharedMemory::INVALID_HANDLE)
                          .evaluate();
        if (result.has_error())
        {
            printError();

            IOX_POSIX_CALL(iox_shm_close)
            (sharedMemoryFileHandle)
                .failureReturnValue(VMSharedMemory::INVALID_HANDLE)
                .evaluate()
                .or_else([&](auto& r) {
                    IOX_LOG(Error,
                            "Unable to close filedescriptor (close failed) : "
                                << r.getHumanReadableErrnum() << " for SharedMemory \"" << m_name << "\"");
                });

            IOX_POSIX_CALL(iox_shm_unlink)
            (nameWithLeadingSlash.c_str())
                .failureReturnValue(VMSharedMemory::INVALID_HANDLE)
                .evaluate()
                .or_else([&](auto&) {
                    IOX_LOG(Error,
                            "Unable to remove previously created SharedMemory \""
                                << m_name << "\". This may be a SharedMemory leak.");
                });

            return err(VMSharedMemory::errnoToEnum(result.error().errnum));
        }
    }
#endif
    return ok(VMSharedMemory(m_name, sharedMemoryFileHandle, hasOwnership, m_size));
}

VMSharedMemory::VMSharedMemory(const Name_t& name, const shm_handle_t handle, const bool hasOwnership, uint64_t size) noexcept
    : m_name{name}
    , m_handle{handle}
    , m_hasOwnership{hasOwnership}
    , m_size(size)
{
}

VMSharedMemory::~VMSharedMemory() noexcept
{
    destroy();
}

void VMSharedMemory::destroy() noexcept
{
    close();
    unlink();
}

void VMSharedMemory::reset() noexcept
{
    m_hasOwnership = false;
    m_name = Name_t();
    m_handle = INVALID_HANDLE;
    m_size = 0;
}

VMSharedMemory::VMSharedMemory(VMSharedMemory&& rhs) noexcept
{
    *this = std::move(rhs);
}

VMSharedMemory& VMSharedMemory::operator=(VMSharedMemory&& rhs) noexcept
{
    if (this != &rhs)
    {
        destroy();

        m_name = rhs.m_name;
        m_hasOwnership = rhs.m_hasOwnership;
        m_handle = rhs.m_handle;
        m_size = rhs.m_size;

        rhs.reset();
    }
    return *this;
}

shm_handle_t VMSharedMemory::getHandle() const noexcept
{
    return m_handle;
}

expected<uint64_t, FileStatError> VMSharedMemory::get_size() const noexcept
{
    return ok(m_size);
}

shm_handle_t VMSharedMemory::get_file_handle() const noexcept
{
    return m_handle;
}

bool VMSharedMemory::hasOwnership() const noexcept
{
    return m_hasOwnership;
}

expected<bool, VMSharedMemoryError> VMSharedMemory::unlinkIfExist(const Name_t& name) noexcept
{
    return ok(true);
}

bool VMSharedMemory::unlink() noexcept
{
    return true;
}

bool VMSharedMemory::close() noexcept
{
    return true;
}

// NOLINTJUSTIFICATION the function size and cognitive complexity results from the error handling and the expanded log macro
// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
VMSharedMemoryError VMSharedMemory::errnoToEnum(const int32_t errnum) noexcept
{
    switch (errnum)
    {
    case EACCES:
        IOX_LOG(Error, "No permission to modify, truncate or access the shared memory!");
        return VMSharedMemoryError::INSUFFICIENT_PERMISSIONS;
    case EPERM:
        IOX_LOG(Error, "Resizing a file beyond its current size is not supported by the filesystem!");
        return VMSharedMemoryError::NO_RESIZE_SUPPORT;
    case EFBIG:
        IOX_LOG(Error, "Requested Shared Memory is larger then the maximum file size.");
        return VMSharedMemoryError::REQUESTED_MEMORY_EXCEEDS_MAXIMUM_FILE_SIZE;
    case EINVAL:
        IOX_LOG(Error,
                "Requested Shared Memory is larger then the maximum file size or the filedescriptor does not "
                "belong to a regular file.");
        return VMSharedMemoryError::REQUESTED_MEMORY_EXCEEDS_MAXIMUM_FILE_SIZE;
    case EBADF:
        IOX_LOG(Error, "Provided filedescriptor is not a valid filedescriptor.");
        return VMSharedMemoryError::INVALID_FILEDESCRIPTOR;
    case EEXIST:
        IOX_LOG(Error, "A Shared Memory with the given name already exists.");
        return VMSharedMemoryError::DOES_EXIST;
    case EISDIR:
        IOX_LOG(Error, "The requested Shared Memory file is a directory.");
        return VMSharedMemoryError::PATH_IS_A_DIRECTORY;
    case ELOOP:
        IOX_LOG(Error, "Too many symbolic links encountered while traversing the path.");
        return VMSharedMemoryError::TOO_MANY_SYMBOLIC_LINKS;
    case EMFILE:
        IOX_LOG(Error, "Process limit of maximum open files reached.");
        return VMSharedMemoryError::PROCESS_LIMIT_OF_OPEN_FILES_REACHED;
    case ENFILE:
        IOX_LOG(Error, "System limit of maximum open files reached.");
        return VMSharedMemoryError::SYSTEM_LIMIT_OF_OPEN_FILES_REACHED;
    case ENOENT:
        IOX_LOG(Error, "Shared Memory does not exist.");
        return VMSharedMemoryError::DOES_NOT_EXIST;
    case ENOMEM:
        IOX_LOG(Error, "Not enough memory available to create shared memory.");
        return VMSharedMemoryError::NOT_ENOUGH_MEMORY_AVAILABLE;
    default:
        IOX_LOG(Error, "This should never happen! An unknown error occurred!");
        return VMSharedMemoryError::UNKNOWN_ERROR;
    }
}

} // namespace detail
} // namespace iox
