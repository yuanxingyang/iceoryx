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

//! [iceoryx includes]
#include "request_and_response_types.hpp"

#include "iceoryx_posh/popo/server.hpp"
#include "iceoryx_posh/runtime/posh_runtime.hpp"
#include "iox/signal_watcher.hpp"
//! [iceoryx includes]

#include <iostream>

int main(int argc, char* argv[])
{
    iox::log::Logger::setLogLevel(iox::log::LogLevel::Trace);
    std::string roudiIp(argv[1]);
    std::string ipaddress(argv[2]);
    uint32_t delay = strtoul(argv[3],nullptr,10);
    //! [initialize runtime]
    constexpr char APP_NAME[] = "iox-cpp-request-response-server-basic-eth";
    //iox::runtime::PoshRuntime::initRuntime(APP_NAME);
    iox::runtime::PoshRuntime::initRuntime(APP_NAME,iox::runtime::RoudiIpcChannelType::ETH_SOCKET, iox::string<64U>(iox::TruncateToCapacity,roudiIp.c_str(),64U), iox::string<64U>(iox::TruncateToCapacity,ipaddress.c_str(),64U));
    //! [initialize runtime]

    //! [create server]
    iox::popo::Server<DataBuffer, DataBuffer> server({"Example", "Request-Response", "Test"});
    //! [create server]

    //! [process requests in a loop]
    while (!iox::hasTerminationRequested())
    {
        //! [take request]
        server.take().and_then([&](const auto& request) {
            std::cout << APP_NAME << " Got Request size: " << sizeof(request)   << std::endl;

            //! [send response]
            server.loan(request)
                .and_then([&](auto& response) {
                    //response->sum = request->augend + request->addend;
                    std::memcpy(response->data,request->data,sizeof(request));
                    
                    response.send().or_else(
                        [&](auto& error) { std::cout << "Could not send Response! Error: " << error << std::endl; });
                })
                .or_else([&](auto& error) {
                    std::cout << APP_NAME << "Could not allocate Response! Error: " << error << std::endl;
                });
            //! [send response]
        });
        //! [take request]

        std::chrono::microseconds SLEEP_TIME{delay};
        std::this_thread::sleep_for(SLEEP_TIME);
        //constexpr std::chrono::milliseconds SLEEP_TIME{100U};
        //std::this_thread::sleep_for(SLEEP_TIME);
    }
    //! [process requests in a loop]

    return EXIT_SUCCESS;
}
