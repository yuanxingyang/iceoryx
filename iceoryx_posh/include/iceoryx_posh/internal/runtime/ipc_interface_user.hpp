// Copyright (c) 2019 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2021 by Apex.AI Inc. All rights reserved.
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

#ifndef IOX_POSH_RUNTIME_IPC_INTERFACE_USER_HPP
#define IOX_POSH_RUNTIME_IPC_INTERFACE_USER_HPP

#include "iceoryx_posh/internal/runtime/ipc_interface_base.hpp"

namespace iox
{
namespace runtime
{
/// @brief Class for using a IPC channel
template <typename IpcChannelType = platform::IoxIpcChannelType>
class IpcInterfaceUser : public IpcInterface<IpcChannelType>
{
  public:
    /// @brief Constructs a IpcInterfaceUser and opens a IPC channel.
    ///        Therefore, isInitialized should always be called
    ///        before using this class.
    /// @param[in] name Unique identifier of the IPC channel
    /// @param[in] domainId to tie the interface to
    /// @param[in] resourceType to be used for the resource prefix
    /// @param[in] maxMessages maximum number of queued messages
    /// @param[in] message size maximum message size
    IpcInterfaceUser(const RuntimeName_t& name,
                     const DomainId domainId,
                     const ResourceType resourceType,
                     const uint64_t maxMessages = APP_MAX_MESSAGES,
                     const uint64_t messageSize = APP_MESSAGE_SIZE,
                     RoudiIpcChannelType channelType = RoudiIpcChannelType::BASE,
                     IpAdress_t ipAddress = DEFALUT_IP) noexcept
        : IpcInterface<IpcChannelType>(name, domainId, resourceType, maxMessages, messageSize, channelType, ipAddress)
    {
        IpcInterface<IpcChannelType>::openIpcChannel(PosixIpcChannelSide::CLIENT);
    }

    IpcInterfaceUser(IpcInterfaceUser&&) noexcept = default;
    IpcInterfaceUser& operator=(IpcInterfaceUser&&) noexcept = default;

    /// @brief The copy constructor and assignment operator are deleted since
    ///         this class manages a resource (IPC channel) which cannot
    ///         be copied.
    IpcInterfaceUser(const IpcInterfaceUser&) = delete;
    IpcInterfaceUser& operator=(const IpcInterfaceUser&) = delete;
};

} // namespace runtime
} // namespace iox

#endif // IOX_POSH_RUNTIME_IPC_INTERFACE_USER_HPP
