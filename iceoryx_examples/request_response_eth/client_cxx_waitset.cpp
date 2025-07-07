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

//! [iceoryx includes]
#include "request_and_response_types.hpp"

#include "iceoryx_posh/popo/client.hpp"
#include "iceoryx_posh/popo/wait_set.hpp"
#include "iceoryx_posh/runtime/posh_runtime.hpp"
#include "iox/signal_handler.hpp"
#include "iox/signal_watcher.hpp"
#include "tester.hpp"
//! [iceoryx includes]

#include <iostream>

constexpr char APP_NAME[] = "iox-cpp-request-response-client-waitset";
volatile bool keepRunning = {true};

using WaitSet = iox::popo::WaitSet<>;
volatile WaitSet* waitsetSigHandlerAccess{nullptr};

//! [context data to store Fibonacci numbers and sequence ids]
struct ContextData
{
    uint64_t fibonacciLast = 0;
    uint64_t fibonacciCurrent = 1;
    int64_t requestSequenceId = 0;
    int64_t expectedResponseSequenceId = requestSequenceId;
};
//! [context data to store Fibonacci numbers and sequence ids]

static void signalHandler(int sig [[maybe_unused]])
{
    keepRunning = false;
    if (waitsetSigHandlerAccess)
    {
        waitsetSigHandlerAccess->markForDestruction();
    }
}

int main(int argc, char** argv)
{
    int32_t delay = 0;
    if (argc == 2)
    {
        delay = strtoul(argv[1],nullptr,10);
    }
    iox::log::Logger::setLogLevel(iox::log::LogLevel::Trace);
    auto sigTermGuard =
        iox::registerSignalHandler(iox::PosixSignal::TERM, signalHandler).expect("failed to register SIGTERM");
    auto sigIntGuard =
        iox::registerSignalHandler(iox::PosixSignal::INT, signalHandler).expect("failed to register SIGINT");

    std::thread t(doPrintInfo);
    //! [initialize runtime]
    iox::runtime::PoshRuntime::initRuntime(APP_NAME);
    //! [initialize runtime]

    ContextData ctx;

    //! [create waitset]
    iox::popo::ConditionVariableData tmp(APP_NAME);
    WaitSet waitset;
    waitsetSigHandlerAccess = &waitset;

    //! [create client]
    iox::popo::ClientOptions options;
    options.responseQueueCapacity = 2U;
    iox::popo::Client<DataBuffer, DataBuffer> client({"Example", "Request-Response", "Test"}, options);
    //! [create client]

    // attach client to waitset
    waitset.attachState(client, iox::popo::ClientState::HAS_RESPONSE).or_else([](auto) {
        std::cerr << "failed to attach client" << std::endl;
        std::exit(EXIT_FAILURE);
    });
    //! [create waitset]

    //! [mainloop]
    while (!iox::hasTerminationRequested())
    {
        //! [send request]
        client.loan()
            .and_then([&](auto& request) {
                request.getRequestHeader().setSequenceId(ctx.requestSequenceId);
                ctx.expectedResponseSequenceId = ctx.requestSequenceId;
                ctx.requestSequenceId += 1;
                //request->augend = ctx.fibonacciLast;
                //request->addend = ctx.fibonacciCurrent;
                memset(request->data,1,sizeof(request->data));
                incrementSend(sizeof(request->data));
                request.send().or_else(
                    [&](auto& error) { std::cout << "Could not send Request! Error: " << error << std::endl; });
            })
            .or_else([](auto& error) { std::cout << "Could not allocate Request! Error: " << error << std::endl; });
        //! [send request]


        // We block and wait for samples to arrive, when the time is up we send the request again
        //! [wait and check if the client triggered]
        auto notificationVector = waitset.timedWait(iox::units::Duration::fromSeconds(5));

        for (auto& notification : notificationVector)
        {
            if (notification->doesOriginateFrom(&client))
            {
                //! [take response]
                bool has_response = false;
                uint32_t data_size = 0;
                while (client.take().and_then([&](const auto& response) {
                    auto receivedSequenceId = response.getResponseHeader().getSequenceId();
                    if (receivedSequenceId == ctx.expectedResponseSequenceId)
                    {
                        //ctx.fibonacciLast = ctx.fibonacciCurrent;
                        //ctx.fibonacciCurrent = response->sum;
                        //std::cout << APP_NAME << " Got Response : " << ctx.fibonacciCurrent << std::endl;
                        //incrementReceive(sizeof(response->data));
                        has_response = true;
                        data_size = sizeof(response->data);
                    }
                    else
                    {
                        //incrementFailure();
                        std::cout << "Got Response with outdated sequence ID! Expected = "
                                  << ctx.expectedResponseSequenceId << "; Actual = " << receivedSequenceId
                                  << "! -> skip" << std::endl;
                    }
                }))
                {
                }
                if(has_response)
                {
                    incrementReceive(data_size);
                }
                else
                {
                    incrementFailure();
                }
                //! [take response]
                break;
            }
        }
        //! [wait and check if the client triggered]
        //constexpr std::chrono::milliseconds SLEEP_TIME{950U};
        //std::this_thread::sleep_for(SLEEP_TIME);
        if(delay)
        {
            std::this_thread::sleep_for(std::chrono::nanoseconds(delay));
        }
    }
    //! [mainloop]

    std::cout << "shutting down" << std::endl;

    waitsetSigHandlerAccess = nullptr; // invalidate for signal handler

    return (EXIT_SUCCESS);
}
