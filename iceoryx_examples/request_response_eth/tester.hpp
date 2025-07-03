#ifndef IOX_EXAMPLES_REQUEST_AND_RESPONSE_TESTER_HPP
#define IOX_EXAMPLES_REQUEST_AND_RESPONSE_TESTER_HPP
#include <iostream>

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

inline void incrementReceive(uint32_t size)
{
    mTotalBytesReceived += size;
    mIntervalBytesReceived += size;
    mTotalReply++;
    mIntervalReply++;
}

inline void incrementSend(uint32_t size)
{
    mTotalBytesSent += size;
    mIntervalBytesSent += size;
    mTotalRequest++;
    mIntervalRequest++;
}

inline void incrementFailure()
{
    mFailureCount++;
}

inline void resetInterval()
{
    mIntervalBytesSent = 0;
    mIntervalBytesReceived = 0;
    mIntervalRequest = 0;
    mIntervalReply = 0;
    mIntervalDelay = 0;
}

inline void doStatistic()
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

inline void doPrintInfo()
{
    while (!iox::hasTerminationRequested())
    {
        doStatistic();
        constexpr std::chrono::milliseconds SLEEP_TIME{1000};
        std::this_thread::sleep_for(SLEEP_TIME);
    }
}

#endif //IOX_EXAMPLES_REQUEST_AND_RESPONSE_TESTER_HPP
