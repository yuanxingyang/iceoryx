// Copyright (c) 2019 by Robert Bosch GmbH. All rights reserved.
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

#ifndef IOX_POSH_RUNTIME_IPC_INTERFACE_HPP
#define IOX_POSH_RUNTIME_IPC_INTERFACE_HPP

#include "iceoryx_posh/internal/runtime/ipc_message.hpp"
#include "iceoryx_posh/iceoryx_posh_types.hpp"
#include "iceoryx_platform/unistd.hpp"
#include "iox/duration.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>

namespace iox
{
namespace runtime
{

enum class RoudiIpcChannelType
{
    BASE,
    ETH_SOCKET,
};

/// @brief Class should never be used by the end-user.
///     Handles the common properties and methods for the IpcChannelType. The handling of
///     the IPC channels must be done by the children.
/// @tparam IpcChannelType the type of ipc channel, supported types are MessageQueue, NamedPipe and UnixDomainSocket
/// @note This class won't uniquely identify if another object is using the same IPC channel
class IIpcInterface
{
  public:

    virtual ~IIpcInterface() noexcept = default;

    /// @brief Receives a message from the IPC channel and stores it in
    ///         answer.
    /// @param[out] answer If a message is received it is stored there.
    /// @return If the call failed or an invalid message was
    ///             received it returns false, otherwise true.
    virtual bool receive(IpcMessage& answer) const noexcept = 0;

    /// @brief Tries to receive a message from the IPC channel within a
    ///         specified timeout. It stores the message in answer.
    /// @param[in] timeout for receiving a message.
    /// @param[in] answer The answer of the IPC channel. If timedReceive
    ///         failed the content of answer is undefined.
    /// @return If a valid message was received before the timeout occures
    ///             it returns true, otherwise false.
    ///         It also returns false if clock_gettime() failed
    virtual bool timedReceive(const units::Duration timeout, IpcMessage& answer) const noexcept = 0;

    /// @brief Tries to send the message specified in msg.
    /// @param[in] msg Must be a valid message, if its an invalid message
    ///                 send will return false
    /// @return If a valid message was send it returns true,
    ///             otherwise if the message was invalid it will return false.
    virtual bool send(const IpcMessage& msg) const noexcept = 0;

    /// @brief Tries to send the message specified in msg to the message
    ///        queue within a specified timeout.
    /// @param[in] msg Must be a valid message, if its an invalid message
    ///                 send will return false
    /// @param[in] timeout specifies the duration to wait for sending.
    /// @return If a valid message was send it returns true,
    ///             otherwise if the message was invalid it will return false.
    virtual bool timedSend(const IpcMessage& msg, const units::Duration timeout) const noexcept = 0;

    /// @brief Returns the interface name, the unique char string which
    ///         explicitly identifies the IPC channel.
    /// @return name of the IPC channel
    virtual const RuntimeName_t& getRuntimeName() const noexcept = 0;

    /// @brief If the IPC channel could not be opened or linked in the
    ///         constructor it will return false, otherwise true. This is
    ///         needed since the constructor is not allowed to throw an
    ///         exception.
    ///         You should always check a IPC channel with isInitialized
    ///         before using it, since all other methods will fail and
    ///         return false if a message could not be successfully
    ///         initialized.
    /// @return initialization state
    virtual bool isInitialized() const noexcept = 0;

    /// @brief Since there might be an outdated IPC channel due to an unclean temination
    ///        this function closes the IPC channel if it's existing.
    /// @param[in] name of the IPC channel to clean up
    //virtual void cleanupOutdatedIpcChannel(const InterfaceName_t& name) noexcept = 0;

    friend class IpcRuntimeInterface;
  protected:
    /// @brief Closes and opens an existing IPC channel using the same parameters as before.
    ///        If the queue was not open, it is just openened.
    /// @return true if successfully reopened, false if not
    virtual bool reopen() noexcept = 0;

    /// @brief Checks if the IPC channel has its counterpart in the file system
    /// @return If the IPC channel, which corresponds to a descriptor,
    ///         is still availabe in the file system it returns true,
    ///         otherwise it was deleted or the IPC channel was not open and returns false
    virtual bool ipcChannelMapsToFile() noexcept = 0;

};

} // namespace runtime
} // namespace iox

#endif // IOX_POSH_RUNTIME_IPC_INTERFACE_HPP
