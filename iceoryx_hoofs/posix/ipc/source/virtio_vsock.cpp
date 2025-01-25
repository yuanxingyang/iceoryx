// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
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

#include "iox/virtio_vsock.hpp"
#include "iceoryx_platform/socket.hpp"
#include "iceoryx_platform/unistd.hpp"
#include "iox/logging.hpp"
#include "iox/posix_call.hpp"
#include "iox/scope_guard.hpp"

#include <chrono>
#include <cstdint>
#include <string>

namespace iox
{
constexpr uint64_t VirtioVSock::MAX_MESSAGE_SIZE;
constexpr uint64_t VirtioVSock::NULL_TERMINATOR_SIZE;

#define GET_VSOCK_CID(name) (atoi(name))
#define GET_VSOCK_PORT(name) (atoi(strchr(name, ':') + 1))

expected<VirtioVSock, PosixIpcChannelError> VirtioVSockBuilder::create() const noexcept
{
    if (!isValidVSockName(m_name))
    {
        return err(PosixIpcChannelError::INVALID_CHANNEL_NAME);
    }

    if (m_maxMsgSize > VirtioVSock::MAX_MESSAGE_SIZE)
    {
        return err(PosixIpcChannelError::MAX_MESSAGE_SIZE_EXCEEDED);
    }

    sockaddr_vm sockAddr{};
    // initialize the sockAddr data structure with the provided name
    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.svm_family = AF_VSOCK;
    sockAddr.svm_port = GET_VSOCK_PORT(m_name);
    sockAddr.svm_cid = GET_VSOCK_CID(m_name);

    // the mask will be applied to the permissions, we only allow users and group members to have read and write access
    // the system call always succeeds, no need to check for errors
    // NOLINTJUSTIFICATION type is defined by POSIX, no logical fault
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    mode_t umaskSaved = umask(S_IXUSR | S_IXGRP | S_IRWXO);
    // Reset to old umask when going out of scope
    ScopeGuard umaskGuard([&umaskSaved] { umask(umaskSaved); });

    auto socketCall =
        IOX_POSIX_CALL(iox_socket)(AF_VSOCK, SOCK_STREAM, 0).failureReturnValue(VirtioVSock::ERROR_CODE).evaluate();

    if (socketCall.has_error())
    {
        return err(VirtioVSock::errnoToEnum(m_name, socketCall.error().errnum));
    }
    auto sockfd = socketCall.value().value;

    if (PosixIpcChannelSide::SERVER == m_channelSide)
    {
        auto bindCall =
            // NOLINTJUSTIFICATION enforced by POSIX API
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            IOX_POSIX_CALL(iox_bind)(sockfd, reinterpret_cast<struct sockaddr*>(&sockAddr), sizeof(sockAddr))
                .failureReturnValue(VirtioVSock::ERROR_CODE)
                .evaluate();

        if (bindCall.has_error())
        {
            VirtioVSock::closeFileDescriptor(m_name, sockfd, sockAddr, m_channelSide).or_else([](auto) {
                IOX_LOG(ERROR,
                        "Unable to close socket file descriptor in error related cleanup during initialization.");
            });
            // possible errors in closeFileDescriptor() are masked and we inform the user about the actual error
            return err(VirtioVSock::errnoToEnum(m_name, bindCall.error().errnum));
        }
        return ok(VirtioVSock{m_name, m_channelSide, sockfd, sockAddr, m_maxMsgSize});
    }
    // we use a connected socket, this leads to a behavior closer to the message queue (e.g. error if client
    // is created and server not present)
    auto connectCall =
        // NOLINTJUSTIFICATION enforced by POSIX API
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        IOX_POSIX_CALL(iox_connect)(sockfd, reinterpret_cast<struct sockaddr*>(&sockAddr), sizeof(sockAddr))
            .failureReturnValue(VirtioVSock::ERROR_CODE)
            .suppressErrorMessagesForErrnos(ENOENT, ECONNREFUSED)
            .evaluate();

    if (connectCall.has_error())
    {
        VirtioVSock::closeFileDescriptor(m_name, sockfd, sockAddr, m_channelSide).or_else([](auto) {
            IOX_LOG(ERROR, "Unable to close socket file descriptor in error related cleanup during initialization.");
        });
        // possible errors in closeFileDescriptor() are masked and we inform the user about the actual error
        return err(VirtioVSock::errnoToEnum(m_name, connectCall.error().errnum));
    }

    return ok(VirtioVSock{m_name, m_channelSide, sockfd, sockAddr, m_maxMsgSize});
}

// @todo iox-#832
// NOLINTJUSTIFICATION make a struct out of arguments in #832
// NOLINT(readability-function-size, bugprone-easily-swappable-parameters)
VirtioVSock::VirtioVSock(const VSockName_t& vsockName,
                                   const PosixIpcChannelSide channelSide,
                                   const int32_t sockfd,
                                   const sockaddr_vm sockAddr,
                                   const uint64_t maxMsgSize) noexcept
    : m_name(vsockName)
    , m_channelSide(channelSide)
    , m_sockfd(sockfd)
    , m_sockAddr(sockAddr)
    , m_maxMessageSize(maxMsgSize)

{
}

VirtioVSock::~VirtioVSock() noexcept
{
    if (destroy().has_error())
    {
        IOX_LOG(ERROR, "unable to cleanup virtio vsock \"" << m_name << "\" in the destructor");
    }
}

VirtioVSock::VirtioVSock(VirtioVSock&& other) noexcept
{
    *this = std::move(other);
}

VirtioVSock& VirtioVSock::operator=(VirtioVSock&& other) noexcept
{
    if (this != &other)
    {
        if (destroy().has_error())
        {
            IOX_LOG(ERROR,
                    "Unable to cleanup virtio vsock \"" << m_name
                                                              << "\" in the move constructor/move assignment operator");
        }

        m_name = std::move(other.m_name);
        m_channelSide = other.m_channelSide;
        m_sockfd = other.m_sockfd;
        m_sockAddr = other.m_sockAddr;
        m_maxMessageSize = other.m_maxMessageSize;

        other.m_sockfd = INVALID_FD;
    }

    return *this;
}

expected<bool, PosixIpcChannelError> VirtioVSock::unlinkIfExists(const VSockName_t& name) noexcept
{
    return true;
}

expected<bool, PosixIpcChannelError> VirtioVSock::unlinkIfExists(const NoPathPrefix_t,
                                                                      const VSockName_t& name) noexcept
{
    return true;
}

expected<void, PosixIpcChannelError> VirtioVSock::closeFileDescriptor() noexcept
{
    return VirtioVSock::closeFileDescriptor(m_name, m_sockfd, m_sockAddr, m_channelSide).and_then([this] {
        m_sockfd = INVALID_FD;
    });
}

expected<void, PosixIpcChannelError> VirtioVSock::closeFileDescriptor(const VSockName_t& name,
                                                                           const int sockfd,
                                                                           const sockaddr_vm& sockAddr,
                                                                           PosixIpcChannelSide channelSide) noexcept
{
    if (sockfd != INVALID_FD)
    {
        auto closeCall = IOX_POSIX_CALL(iox_closesocket)(sockfd).failureReturnValue(ERROR_CODE).evaluate();

        if (!closeCall.has_error())
        {
            return ok();
        }
        return err(VirtioVSock::errnoToEnum(name, closeCall.error().errnum));
    }
    return ok();
}

expected<void, PosixIpcChannelError> VirtioVSock::destroy() noexcept
{
    if (m_sockfd != INVALID_FD)
    {
        return closeFileDescriptor();
    }

    return ok();
}

expected<void, PosixIpcChannelError> VirtioVSock::send(const std::string& msg) const noexcept
{
    // we also support timedSend. The setsockopt call sets the timeout for all further sendto calls, so we must set
    // it to 0 to turn the timeout off
    return timedSendImpl<char, Termination::NULL_TERMINATOR>(
        msg.c_str(), msg.size(), units::Duration::fromSeconds(0ULL));
}

expected<void, PosixIpcChannelError> VirtioVSock::timedSend(const std::string& msg,
                                                                 const units::Duration& timeout) const noexcept
{
    return timedSendImpl<char, Termination::NULL_TERMINATOR>(msg.c_str(), msg.size(), timeout);
}

expected<std::string, PosixIpcChannelError> VirtioVSock::receive() const noexcept
{
    // we also support timedReceive. The setsockopt call sets the timeout for all further recvfrom calls, so we must set
    // it to 0 to turn the timeout off
    return timedReceive(units::Duration::fromSeconds(0ULL));
}

expected<std::string, PosixIpcChannelError>
VirtioVSock::timedReceive(const units::Duration& timeout) const noexcept
{
    auto result = expected<uint64_t, PosixIpcChannelError>(in_place, uint64_t(0));
    Message_t msg;
    msg.unsafe_raw_access([&](auto* str, const auto info) -> uint64_t {
        result = this->timedReceiveImpl<char, Termination::NULL_TERMINATOR>(str, info.total_size, timeout);
        if (result.has_error())
        {
            return 0;
        }
        return result.value();
    });
    if (result.has_error())
    {
        return err(result.error());
    }
    return ok<std::string>(msg.c_str());
}

PosixIpcChannelError VirtioVSock::errnoToEnum(const int32_t errnum) const noexcept
{
    return errnoToEnum(m_name, errnum);
}

// NOLINTJUSTIFICATION the function size and cognitive complexity results from the error handling and the expanded log macro
// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
PosixIpcChannelError VirtioVSock::errnoToEnum(const VSockName_t& name, const int32_t errnum) noexcept
{
    switch (errnum)
    {
    case EACCES:
    {
        IOX_LOG(ERROR, "permission to create virtio vsock denied \"" << name << "\"");
        return PosixIpcChannelError::ACCESS_DENIED;
    }
    case EAFNOSUPPORT:
    {
        IOX_LOG(ERROR, "address family not supported for virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::INVALID_ARGUMENTS;
    }
    case EINVAL:
    {
        IOX_LOG(ERROR, "provided invalid arguments for virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::INVALID_ARGUMENTS;
    }
    case EMFILE:
    {
        IOX_LOG(ERROR, "process limit reached for virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::PROCESS_LIMIT;
    }
    case ENFILE:
    {
        IOX_LOG(ERROR, "system limit reached for virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::SYSTEM_LIMIT;
    }
    case ENOBUFS:
    {
        IOX_LOG(ERROR, "queue is full for virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::OUT_OF_MEMORY;
    }
    case ENOMEM:
    {
        IOX_LOG(ERROR, "out of memory for virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::OUT_OF_MEMORY;
    }
    case EPROTONOSUPPORT:
    {
        IOX_LOG(ERROR, "protocol type not supported for virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::INVALID_ARGUMENTS;
    }
    case EADDRINUSE:
    {
        IOX_LOG(ERROR, "virtio vsock already in use \"" << name << "\"");
        return PosixIpcChannelError::CHANNEL_ALREADY_EXISTS;
    }
    case EBADF:
    {
        IOX_LOG(ERROR, "invalid file descriptor for virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::INVALID_FILE_DESCRIPTOR;
    }
    case ENOTSOCK:
    {
        IOX_LOG(ERROR, "invalid virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::INVALID_FILE_DESCRIPTOR;
    }
    case EADDRNOTAVAIL:
    {
        IOX_LOG(ERROR, "interface or address error for virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::INVALID_CHANNEL_NAME;
    }
    case EFAULT:
    {
        IOX_LOG(ERROR, "outside address space error for virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::INVALID_CHANNEL_NAME;
    }
    case ELOOP:
    {
        IOX_LOG(ERROR, "too many symbolic links for virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::INVALID_CHANNEL_NAME;
    }
    case ENAMETOOLONG:
    {
        IOX_LOG(ERROR, "name too long for virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::INVALID_CHANNEL_NAME;
    }
    case ENOTDIR:
    {
        IOX_LOG(ERROR, "not a directory error for virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::INVALID_CHANNEL_NAME;
    }
    case ENOENT:
    {
        // no error message needed since this is a normal use case
        return PosixIpcChannelError::NO_SUCH_CHANNEL;
    }
    case EROFS:
    {
        IOX_LOG(ERROR, "read only error for virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::INVALID_CHANNEL_NAME;
    }
    case EIO:
    {
        IOX_LOG(ERROR, "I/O for virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::I_O_ERROR;
    }
    case ENOPROTOOPT:
    {
        IOX_LOG(ERROR, "invalid option for virtio vsock \"" << name << "\"");
        return PosixIpcChannelError::INVALID_ARGUMENTS;
    }
    case ECONNREFUSED:
    {
        // no error message needed since this is a normal use case
        return PosixIpcChannelError::NO_SUCH_CHANNEL;
    }
    case ECONNRESET:
    {
        IOX_LOG(ERROR, "connection was reset by peer for \"" << name << "\"");
        return PosixIpcChannelError::CONNECTION_RESET_BY_PEER;
    }
    case EWOULDBLOCK:
    {
        // no error message needed since this is a normal use case
        return PosixIpcChannelError::TIMEOUT;
    }
    default:
    {
        IOX_LOG(ERROR, "internal logic error in virtio vsock \"" << name << "\" occurred");
        return PosixIpcChannelError::INTERNAL_LOGIC_ERROR;
    }
    }
}
} // namespace iox
