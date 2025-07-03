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
#include "iceoryx_posh/runtime/posh_runtime.hpp"
#include "iox/signal_watcher.hpp"
//! [iceoryx includes]

#include <iostream>
#include <ctime>
#include <thread>
static uint64_t interval_s = 0;
static uint64_t total_s = 0;
using clock_type = std::chrono::steady_clock;
static clock_type::time_point interval_pre_clock = clock_type::now();
static clock_type::time_point interval_current_clock = clock_type::now();
static clock_type::time_point total_begin_clock = clock_type::now();

static uint64_t mTotalBytesSent = 0;
static uint64_t mTotalBytesReceived = 0;
static uint64_t mTotalRequest = 0;
static uint64_t mTotalReply = 0;
static uint64_t mIntervalBytesSent = 0;
static uint64_t mIntervalBytesReceived = 0;
static uint64_t mIntervalRequest = 0;
static uint64_t mIntervalReply = 0;
static uint64_t mMaxDelay = 0;
static uint64_t mMinDelay = 0;
static uint64_t mTotalDelay = 0;
static uint64_t mIntervalDelay = 0;
static uint64_t mFailureCount = 0;

void incrementReceive(uint32_t size)
{
    mTotalBytesReceived += size;
    mIntervalBytesReceived += size;
    mTotalReply++;
    mIntervalReply++;
}

void incrementSend(uint32_t size)
{
    mTotalBytesSent += size;
    mIntervalBytesSent += size;
    mTotalRequest++;
    mIntervalRequest++;
}

void resetInterval()
{
    mIntervalBytesSent = 0;
    mIntervalBytesReceived = 0;
    mIntervalRequest = 0;
    mIntervalReply = 0;
    mIntervalDelay = 0;
}

void doStatistic()
{
    static bool title_printed = false;
    if (!title_printed)
    {
        printf("  |%12s| |%12s|    |%8s|   |%8s||%8s||%6s/%-6s||%10s||%10s|\n",
               "Avg Data Rate", "Inst Data Rate", "Avg Trans", "Inst Trans", "Pending", "Total", "Failure", "Avg Delay", "Max Delay");
        title_printed = true;
    }

    interval_current_clock = clock_type::now();
    uint64_t interval_s = std::chrono::duration_cast<std::chrono::seconds>(interval_current_clock - interval_pre_clock).count();
    interval_pre_clock = interval_current_clock;
    uint64_t total_s = std::chrono::duration_cast<std::chrono::seconds>(interval_current_clock - total_begin_clock).count();
    if (interval_s <= 0) interval_s = 1;
    if (total_s <= 0) total_s = 1;

    uint64_t avg_data_rate = mTotalBytesReceived / total_s;
    uint64_t inst_data_rate = mIntervalBytesReceived / interval_s;
    uint64_t avg_trans_rate = mTotalRequest / total_s;
    uint64_t inst_trans_rate = mIntervalRequest / interval_s;
    uint64_t pending_req = 0;
    if (mTotalRequest > mTotalReply)
    {
        pending_req = mTotalRequest - mTotalReply;
    }
    uint64_t avg_delay = mTotalReply ? mTotalDelay / mTotalReply : 0;

    printf("%12u B/s %12u B/s %8u Req/s %8u Req/s %8u %12u/%-4u %8u us %8u us\n",
            (uint32_t)avg_data_rate, (uint32_t)inst_data_rate, (uint32_t)avg_trans_rate,
            (uint32_t)inst_trans_rate, (uint32_t)pending_req, (uint32_t)mTotalRequest,
            (uint32_t)mFailureCount, (uint32_t)avg_delay, (uint32_t)mMaxDelay);
    resetInterval();
}

void doPrintInfo()
{
    while (!iox::hasTerminationRequested())
    {
        doStatistic();
        constexpr std::chrono::milliseconds SLEEP_TIME{1000};
        std::this_thread::sleep_for(SLEEP_TIME);
    }
}

int main(int argc, char** argv)
{
    //uint32_t block_size = strtoul(argv[1],nullptr,10);
    //uint32_t burst_size = strtoul(argv[2],nullptr,10);
    //uint32_t delay = strtoul(argv[1],nullptr,10);
    iox::log::Logger::setLogLevel(iox::log::LogLevel::Trace);

    std::thread t(doPrintInfo);
    //! [initialize runtime]
    constexpr char APP_NAME[] = "iox-cpp-request-response-client-basic";
    iox::runtime::PoshRuntime::initRuntime(APP_NAME);
    //! [initialize runtime]

    //! [create client]
    iox::popo::Client<DataBuffer, DataBuffer> client({"Example", "Request-Response", "Test"});
    //! [create client]

    //! [send requests in a loop]
    //uint64_t fibonacciLast = 0;
    //uint64_t fibonacciCurrent = 1;
    int64_t requestSequenceId = 0;
    int64_t expectedResponseSequenceId = requestSequenceId;
    std::cout<< "Databuffer size: " << sizeof(DataBuffer) << std::endl;
    while (!iox::hasTerminationRequested())
    {
        //! [send request]
        client.loan()
            .and_then([&](auto& request) {
                request.getRequestHeader().setSequenceId(requestSequenceId);
                expectedResponseSequenceId = requestSequenceId;
                requestSequenceId += 1;
                //auto current_clock = clock_type::now();
                memset(request->data,1,sizeof(request->data));
                //auto end_clock = clock_type::now();
                //uint64_t interval = std::chrono::duration_cast<std::chrono::nanoseconds>(current_clock - end_clock).count();
                //std::cout << "memset cost :" << interval << "microseconds" << std::endl;
                //request->addend = fibonacciCurrent;
                //std::cout << APP_NAME << " Send Request: " << fibonacciLast << " + " << fibonacciCurrent << std::endl;
                incrementSend(sizeof(request->data));
                request.send().or_else(
                    [&](auto& error) { std::cout << "Could not send Request! Error: " << error << std::endl; });
            })
            .or_else([](auto& error) { std::cout << "Could not allocate Request! Error: " << error << std::endl; });
        //! [send request]

        // the client polls with an interval of 150ms
        //constexpr std::chrono::milliseconds DELAY_TIME{150U};
        //std::this_thread::sleep_for(DELAY_TIME);
        /*
        if(delay)
        {
            //constexpr std::chrono::milliseconds SLEEP_TIME{950U};
            std::chrono::microseconds SLEEP_TIME{delay};
            std::this_thread::sleep_for(SLEEP_TIME);
        }
        */

        //! [take response]
        while (client.take().and_then([&](const auto& response) {
            auto receivedSequenceId = response.getResponseHeader().getSequenceId();
            if (receivedSequenceId == expectedResponseSequenceId)
            {
                //fibonacciLast = fibonacciCurrent;
                //fibonacciCurrent = response->sum;
                //std::cout << APP_NAME << " Got Response : " << fibonacciCurrent << std::endl;
                //std::cout << APP_NAME << " Got Response  size: " << sizeof(response) << std::endl;
                incrementReceive(sizeof(response->data));
            }
            else
            {
                mFailureCount++;
                std::cout << "Got Response with outdated sequence ID! Expected = " << expectedResponseSequenceId
                          << "; Actual = " << receivedSequenceId << "! -> skip" << std::endl;
            }
        }))
        {
        };

        //constexpr std::chrono::milliseconds SLEEP_TIME{950U};
        //std::this_thread::sleep_for(SLEEP_TIME);
    }
    //! [send requests in a loop]
    t.join();

    return EXIT_SUCCESS;
}
