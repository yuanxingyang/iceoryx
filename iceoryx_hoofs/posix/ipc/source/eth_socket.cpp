// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2021 - 2023 by Apex.AI Inc. All rights reserved.
// Copyright (c) 2023 by ekxide IO GmbH. All rights reserved.
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

#include "iox/eth_socket.hpp"
#include "iceoryx_platform/socket.hpp"
#include "iceoryx_platform/unistd.hpp"
#include "iox/logging.hpp"
#include "iox/posix_call.hpp"
#include "iox/scope_guard.hpp"
extern "C" {
    #include <arpa/inet.h>
}

#include <chrono>
#include <cstdint>
#include <string>

namespace iox
{
constexpr uint64_t EthSocket::MAX_MESSAGE_SIZE;
constexpr uint64_t EthSocket::NULL_TERMINATOR_SIZE;

#define GET_ETHSOCK_IP(name)     (name.substr(0, name.find(":").value()).value().c_str())
#define GET_ETHSOCK_PORT(name)   htons(static_cast<in_port_t>(std::stoi(name.substr(name.find(":").value() + 1).value().c_str())))

expected<EthSocket, PosixIpcChannelError> EthSocketBuilder::create() const noexcept
{
    if (!isValidIPAddress(m_name.c_str()))
    {
        return err(PosixIpcChannelError::INVALID_CHANNEL_NAME);
    }

    if (m_maxMsgSize > EthSocket::MAX_MESSAGE_SIZE)
    {
        return err(PosixIpcChannelError::MAX_MESSAGE_SIZE_EXCEEDED);
    }

    sockaddr_in sockAddr{};
    // initialize the sockAddr data structure with the provided name
    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = GET_ETHSOCK_PORT(m_name);
    if (inet_pton(AF_INET, GET_ETHSOCK_IP(m_name), &sockAddr.sin_addr.s_addr) != 1)
    {
        return err(PosixIpcChannelError::INVALID_CHANNEL_NAME);
    }

    // the mask will be applied to the permissions, we only allow users and group members to have read and write access
    // the system call always succeeds, no need to check for errors
    // NOLINTJUSTIFICATION type is defined by POSIX, no logical fault
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    mode_t umaskSaved = umask(S_IXUSR | S_IXGRP | S_IRWXO);
    // Reset to old umask when going out of scope
    ScopeGuard umaskGuard([&umaskSaved] { umask(umaskSaved); });

    auto socketCall =
        IOX_POSIX_CALL(iox_socket)(AF_INET, SOCK_DGRAM, 0).failureReturnValue(EthSocket::ERROR_CODE).evaluate();

    if (socketCall.has_error())
    {
        return err(EthSocket::errnoToEnum(m_name, socketCall.error().errnum));
    }
    auto sockfd = socketCall.value().value;

    if (PosixIpcChannelSide::SERVER == m_channelSide)
    {
        auto bindCall =
            // NOLINTJUSTIFICATION enforced by POSIX API
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            IOX_POSIX_CALL(iox_bind)(sockfd, reinterpret_cast<struct sockaddr*>(&sockAddr), sizeof(sockAddr))
                .failureReturnValue(EthSocket::ERROR_CODE)
                .evaluate();

        if (bindCall.has_error())
        {
            EthSocket::closeFileDescriptor(m_name, sockfd, sockAddr, m_channelSide).or_else([](auto) {
                IOX_LOG(Error,
                        "Unable to close socket file descriptor in error related cleanup during initialization.");
            });
            // possible errors in closeFileDescriptor() are masked and we inform the user about the actual error
            return err(EthSocket::errnoToEnum(m_name, bindCall.error().errnum));
        }
        return ok(EthSocket{m_name, m_channelSide, sockfd, sockAddr, m_maxMsgSize});
    }
    // we use a connected socket, this leads to a behavior closer to the message queue (e.g. error if client
    // is created and server not present)
    auto connectCall =
        // NOLINTJUSTIFICATION enforced by POSIX API
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        IOX_POSIX_CALL(iox_connect)(sockfd, reinterpret_cast<struct sockaddr*>(&sockAddr), sizeof(sockAddr))
            .failureReturnValue(EthSocket::ERROR_CODE)
            .suppressErrorMessagesForErrnos(ENOENT, ECONNREFUSED)
            .evaluate();

    if (connectCall.has_error())
    {
        EthSocket::closeFileDescriptor(m_name, sockfd, sockAddr, m_channelSide).or_else([](auto) {
            IOX_LOG(Error, "Unable to close socket file descriptor in error related cleanup during initialization.");
        });
        // possible errors in closeFileDescriptor() are masked and we inform the user about the actual error
        return err(EthSocket::errnoToEnum(m_name, connectCall.error().errnum));
    }

    return ok(EthSocket{m_name, m_channelSide, sockfd, sockAddr, m_maxMsgSize});
}

// @todo iox-#832
// NOLINTJUSTIFICATION make a struct out of arguments in #832
// NOLINT(readability-function-size, bugprone-easily-swappable-parameters)
EthSocket::EthSocket(const EthSocketName_t& vsockName,
                                   const PosixIpcChannelSide channelSide,
                                   const int32_t sockfd,
                                   const sockaddr_in sockAddr,
                                   const uint64_t maxMsgSize) noexcept
    : m_name(vsockName)
    , m_channelSide(channelSide)
    , m_sockfd(sockfd)
    , m_sockAddr(sockAddr)
    , m_maxMessageSize(maxMsgSize)

{
}

EthSocket::~EthSocket() noexcept
{
    if (destroy().has_error())
    {
        IOX_LOG(Error, "unable to cleanup eth socket \"" << m_name << "\" in the destructor");
    }
}

EthSocket::EthSocket(EthSocket&& other) noexcept
{
    *this = std::move(other);
}

EthSocket& EthSocket::operator=(EthSocket&& other) noexcept
{
    if (this != &other)
    {
        if (destroy().has_error())
        {
            IOX_LOG(Error,
                    "Unable to cleanup eth socket \"" << m_name
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

expected<bool, PosixIpcChannelError> EthSocket::unlinkIfExists(const EthSocketName_t& name) noexcept
{
    // Ignore unused variable warning
    (void)name;
    return ok(true);
}

expected<bool, PosixIpcChannelError> EthSocket::unlinkIfExists(const NoPathPrefix_t,
                                                                      const EthSocketName_t& name) noexcept
{
    // Ignore unused variable warning
    (void)name;
    return ok(true);
}

expected<void, PosixIpcChannelError> EthSocket::closeFileDescriptor() noexcept
{
    return EthSocket::closeFileDescriptor(m_name, m_sockfd, m_sockAddr, m_channelSide).and_then([this] {
        m_sockfd = INVALID_FD;
    });
}

expected<void, PosixIpcChannelError> EthSocket::closeFileDescriptor(const EthSocketName_t& name,
                                                                           const int sockfd,
                                                                           const sockaddr_in& sockAddr,
                                                                           PosixIpcChannelSide channelSide) noexcept
{
    if (sockfd != INVALID_FD)
    {
        auto closeCall = IOX_POSIX_CALL(iox_closesocket)(sockfd).failureReturnValue(ERROR_CODE).evaluate();

        if (!closeCall.has_error())
        {
            return ok();
        }
        // Ignore unused variable warning
        (void)sockAddr;
        (void)channelSide;
        return err(EthSocket::errnoToEnum(name, closeCall.error().errnum));
    }
    return ok();
}

expected<void, PosixIpcChannelError> EthSocket::destroy() noexcept
{
    if (m_sockfd != INVALID_FD)
    {
        return closeFileDescriptor();
    }

    return ok();
}

expected<void, PosixIpcChannelError> EthSocket::send(const std::string& msg) const noexcept
{
    // we also support timedSend. The setsockopt call sets the timeout for all further sendto calls, so we must set
    // it to 0 to turn the timeout off
    return timedSendImpl<char, Termination::NULL_TERMINATOR>(
        msg.c_str(), msg.size(), units::Duration::fromSeconds(0ULL));
}

expected<void, PosixIpcChannelError> EthSocket::timedSend(const std::string& msg,
                                                                 const units::Duration& timeout) const noexcept
{
    return timedSendImpl<char, Termination::NULL_TERMINATOR>(msg.c_str(), msg.size(), timeout);
}

expected<std::string, PosixIpcChannelError> EthSocket::receive() const noexcept
{
    // we also support timedReceive. The setsockopt call sets the timeout for all further recvfrom calls, so we must set
    // it to 0 to turn the timeout off
    return timedReceive(units::Duration::fromSeconds(0ULL));
}

expected<std::string, PosixIpcChannelError>
EthSocket::timedReceive(const units::Duration& timeout) const noexcept
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

PosixIpcChannelError EthSocket::errnoToEnum(const int32_t errnum) const noexcept
{
    return errnoToEnum(m_name, errnum);
}

// NOLINTJUSTIFICATION the function size and cognitive complexity results from the error handling and the expanded log macro
// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
PosixIpcChannelError EthSocket::errnoToEnum(const EthSocketName_t& name, const int32_t errnum) noexcept
{
    switch (errnum)
    {
    case EACCES:
    {
        IOX_LOG(Error, "permission to create eth socket denied \"" << name << "\"");
        return PosixIpcChannelError::ACCESS_DENIED;
    }
    case EAFNOSUPPORT:
    {
        IOX_LOG(Error, "address family not supported for eth socket \"" << name << "\"");
        return PosixIpcChannelError::INVALID_ARGUMENTS;
    }
    case EINVAL:
    {
        IOX_LOG(Error, "provided invalid arguments for eth socket \"" << name << "\"");
        return PosixIpcChannelError::INVALID_ARGUMENTS;
    }
    case EMFILE:
    {
        IOX_LOG(Error, "process limit reached for eth socket \"" << name << "\"");
        return PosixIpcChannelError::PROCESS_LIMIT;
    }
    case ENFILE:
    {
        IOX_LOG(Error, "system limit reached for eth socket \"" << name << "\"");
        return PosixIpcChannelError::SYSTEM_LIMIT;
    }
    case ENOBUFS:
    {
        IOX_LOG(Error, "queue is full for eth socket \"" << name << "\"");
        return PosixIpcChannelError::OUT_OF_MEMORY;
    }
    case ENOMEM:
    {
        IOX_LOG(Error, "out of memory for eth socket \"" << name << "\"");
        return PosixIpcChannelError::OUT_OF_MEMORY;
    }
    case EPROTONOSUPPORT:
    {
        IOX_LOG(Error, "protocol type not supported for eth socket \"" << name << "\"");
        return PosixIpcChannelError::INVALID_ARGUMENTS;
    }
    case EADDRINUSE:
    {
        IOX_LOG(Error, "eth socket already in use \"" << name << "\"");
        return PosixIpcChannelError::CHANNEL_ALREADY_EXISTS;
    }
    case EBADF:
    {
        IOX_LOG(Error, "invalid file descriptor for eth socket \"" << name << "\"");
        return PosixIpcChannelError::INVALID_FILE_DESCRIPTOR;
    }
    case ENOTSOCK:
    {
        IOX_LOG(Error, "invalid eth socket \"" << name << "\"");
        return PosixIpcChannelError::INVALID_FILE_DESCRIPTOR;
    }
    case EADDRNOTAVAIL:
    {
        IOX_LOG(Error, "interface or address error for eth socket \"" << name << "\"");
        return PosixIpcChannelError::INVALID_CHANNEL_NAME;
    }
    case EFAULT:
    {
        IOX_LOG(Error, "outside address space error for eth socket \"" << name << "\"");
        return PosixIpcChannelError::INVALID_CHANNEL_NAME;
    }
    case ELOOP:
    {
        IOX_LOG(Error, "too many symbolic links for eth socket \"" << name << "\"");
        return PosixIpcChannelError::INVALID_CHANNEL_NAME;
    }
    case ENAMETOOLONG:
    {
        IOX_LOG(Error, "name too long for eth socket \"" << name << "\"");
        return PosixIpcChannelError::INVALID_CHANNEL_NAME;
    }
    case ENOTDIR:
    {
        IOX_LOG(Error, "not a directory error for eth socket \"" << name << "\"");
        return PosixIpcChannelError::INVALID_CHANNEL_NAME;
    }
    case ENOENT:
    {
        // no error message needed since this is a normal use case
        return PosixIpcChannelError::NO_SUCH_CHANNEL;
    }
    case EROFS:
    {
        IOX_LOG(Error, "read only error for eth socket \"" << name << "\"");
        return PosixIpcChannelError::INVALID_CHANNEL_NAME;
    }
    case EIO:
    {
        IOX_LOG(Error, "I/O for eth socket \"" << name << "\"");
        return PosixIpcChannelError::I_O_ERROR;
    }
    case ENOPROTOOPT:
    {
        IOX_LOG(Error, "invalid option for eth socket \"" << name << "\"");
        return PosixIpcChannelError::INVALID_ARGUMENTS;
    }
    case ECONNREFUSED:
    {
        // no error message needed since this is a normal use case
        return PosixIpcChannelError::NO_SUCH_CHANNEL;
    }
    case ECONNRESET:
    {
        IOX_LOG(Error, "connection was reset by peer for \"" << name << "\"");
        return PosixIpcChannelError::CONNECTION_RESET_BY_PEER;
    }
    case EWOULDBLOCK:
    {
        // no error message needed since this is a normal use case
        return PosixIpcChannelError::TIMEOUT;
    }
    default:
    {
        IOX_LOG(Error, "internal logic error in eth socket \"" << name << "\" occurred");
        return PosixIpcChannelError::INTERNAL_LOGIC_ERROR;
    }
    }
}
} // namespace iox
