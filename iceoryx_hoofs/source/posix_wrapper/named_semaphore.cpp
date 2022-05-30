// Copyright (c) 2022 by Apex.AI Inc. All rights reserved.
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

#include "iceoryx_hoofs/posix_wrapper/named_semaphore.hpp"
#include "iceoryx_hoofs/cxx/helplets.hpp"
#include "iceoryx_hoofs/internal/log/hoofs_logging.hpp"
#include "iceoryx_hoofs/posix_wrapper/posix_call.hpp"

namespace iox
{
namespace posix
{
static cxx::expected<SemaphoreError> unlink(const NamedSemaphore::Name_t& name) noexcept
{
    auto result = posixCall(iox_sem_unlink)(name.c_str()).failureReturnValue(-1).ignoreErrnos(ENOENT).evaluate();
    if (result.has_error())
    {
        switch (result.get_error().errnum)
        {
        case EACCES:
            LogError() << "You don't have permission to remove to remove the semaphore \"" << name << "\"";
            return cxx::error<SemaphoreError>(SemaphoreError::PERMISSION_DENIED);
        default:
            LogError() << "This should never happen. An unknown error occurred while creating the semaphore \"" << name
                       << "\".";
            return cxx::error<SemaphoreError>(SemaphoreError::UNDEFINED);
        }
    }
    return cxx::success<>();
}

cxx::expected<SemaphoreError>
NamedSemaphoreBuilder::create(cxx::optional<NamedSemaphore>& uninitializedSemaphore) noexcept
{
    if (!cxx::isValidFilePath(m_name))
    {
        LogError() << "The name \"" << m_name << "\" is not a valid semaphore name.";
        return cxx::error<SemaphoreError>(SemaphoreError::INVALID_NAME);
    }

    if (m_initialValue > SEM_VALUE_MAX)
    {
        LogError() << "The semaphores \"" << m_name << "\" initial value of " << m_initialValue
                   << " exceeds the maximum semaphore value " << SEM_VALUE_MAX;
        return cxx::error<SemaphoreError>(SemaphoreError::SEMAPHORE_OVERFLOW);
    }

    if (m_openMode == OpenMode::PURGE_AND_CREATE)
    {
        auto result = unlink(m_name);
        if (result.has_error())
        {
            return result;
        }
    }

    auto result = posixCall(iox_sem_open_ext)(m_name.c_str(),
                                              convertToOflags(m_openMode),
                                              static_cast<mode_t>(m_permissions),
                                              static_cast<unsigned int>(m_initialValue))
                      .failureReturnValue(SEM_FAILED)
                      .evaluate();

    if (result.has_error())
    {
        switch (result.get_error().errnum)
        {
        case EEXIST:
            LogError() << "A semaphore with the name \"" << m_name << "\" does already exist.";
            return cxx::error<SemaphoreError>(SemaphoreError::ALREADY_EXIST);
        case EMFILE:
            LogError() << "The per-process limit of file descriptor exceeded while creating the semaphore \"" << m_name
                       << "\"";
            return cxx::error<SemaphoreError>(SemaphoreError::FILE_DESCRIPTOR_LIMIT_REACHED);
        case ENFILE:
            LogError() << "The system wide limit of file descriptor exceeded while creating the semaphore \"" << m_name
                       << "\"";
            return cxx::error<SemaphoreError>(SemaphoreError::FILE_DESCRIPTOR_LIMIT_REACHED);
        case ENOENT:
            LogError() << "Unable to open semaphore since no semaphore with the name \"" << m_name << "\" exists.";
            return cxx::error<SemaphoreError>(SemaphoreError::NO_SEMAPHORE_WITH_THAT_NAME_EXISTS);
        case ENOMEM:
            LogError() << "Insufficient memory to create the semaphore \"" << m_name << "\".";
            return cxx::error<SemaphoreError>(SemaphoreError::OUT_OF_MEMORY);
        default:
            LogError() << "This should never happen. An unknown error occurred while creating the semaphore \""
                       << m_name << "\".";
            return cxx::error<SemaphoreError>(SemaphoreError::UNDEFINED);
        }
    }

    uninitializedSemaphore.emplace(result.value().value, m_name);
    return cxx::success<>();
}

NamedSemaphore::NamedSemaphore(iox_sem_t* handle, const Name_t& name) noexcept
    : m_handle{handle}
    , m_name{name}
{
}

NamedSemaphore::~NamedSemaphore() noexcept
{
    IOX_DISCARD_RESULT(unlink(m_name));
}

iox_sem_t* NamedSemaphore::getHandle() noexcept
{
    return m_handle;
}


} // namespace posix
} // namespace iox
