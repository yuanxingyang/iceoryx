// Copyright (c) 2019, 2021 by  Robert Bosch GmbH. All rights reserved.
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

#include "iceoryx_posh/internal/mepoo/memory_manager.hpp"
#include "iceoryx_posh/internal/mepoo/shared_chunk.hpp"
#include "iceoryx_posh/mepoo/chunk_header.hpp"
#include "iceoryx_utils/internal/posix_wrapper/shared_memory_object/allocator.hpp"
#include "test.hpp"

using namespace ::testing;

using namespace iox::mepoo;

class SharedChunk_Test : public Test
{
  public:
    void SetUp() override
    {
    }
    void TearDown() override
    {
    }

    ChunkManagement* GetChunkManagement(void* memoryChunk)
    {
        ChunkManagement* v = static_cast<ChunkManagement*>(chunkMgmtPool.getChunk());
        auto chunkSettingsResult = ChunkSettings::create(USER_PAYLOAD_SIZE, iox::CHUNK_DEFAULT_USER_PAYLOAD_ALIGNMENT);
        EXPECT_FALSE(chunkSettingsResult.has_error());
        if (chunkSettingsResult.has_error())
        {
            return nullptr;
        }
        auto& chunkSettings = chunkSettingsResult.value();
        ChunkHeader* chunkHeader = new (memoryChunk) ChunkHeader(mempool.getChunkSize(), chunkSettings);

        new (v) ChunkManagement{chunkHeader, &mempool, &chunkMgmtPool};
        return v;
    }

    static constexpr uint32_t CHUNK_SIZE{64U};
    static constexpr uint32_t NUMBER_OF_CHUNKS{10U};
    static constexpr uint32_t USER_PAYLOAD_SIZE{64U};

    char memory[4096U];
    iox::posix::Allocator allocator{memory, 4096U};
    MemPool mempool{sizeof(ChunkHeader) + USER_PAYLOAD_SIZE, 10U, allocator, allocator};
    MemPool chunkMgmtPool{64U, 10U, allocator, allocator};
    void* memoryChunk{mempool.getChunk()};
    ChunkManagement* chunkManagement = GetChunkManagement(memoryChunk);
    SharedChunk sut{chunkManagement};
};

TEST_F(SharedChunk_Test, SharedChunkObjectUpOnInitilizationSetsTheChunkHeaderToNullPointer)
{
    SharedChunk sut;

    EXPECT_THAT(sut.getChunkHeader(), Eq(nullptr));
}

TEST_F(SharedChunk_Test, VerifyCopyConstructorOfSharedChunk)
{
    SharedChunk sut1{chunkManagement};

    SharedChunk sut2(sut1);

    EXPECT_EQ((sut2.getChunkHeader())->chunkSize(), (sut1.getChunkHeader())->chunkSize());
}

TEST_F(SharedChunk_Test, VerifyMoveConstructorOfSharedChunk)
{
    SharedChunk sut1{chunkManagement};
    ChunkHeader* header = sut1.getChunkHeader();

    SharedChunk sut2(std::move(sut1));

    ASSERT_EQ(sut1.getChunkHeader(), nullptr);
    ASSERT_EQ(sut2.getChunkHeader(), header);
    EXPECT_EQ((sut2.getChunkHeader())->chunkSize(), (sizeof(ChunkHeader) + USER_PAYLOAD_SIZE));
}

TEST_F(SharedChunk_Test, VerifiyCopyAssigmentWithSharedChunk)
{
    SharedChunk sut1(chunkManagement);
    SharedChunk sut2;

    sut2 = sut1;

    EXPECT_EQ((sut2.getChunkHeader())->chunkSize(), (sut1.getChunkHeader())->chunkSize());
}

TEST_F(SharedChunk_Test, VerifiyMoveAssigmentForSharedChunk)
{
    SharedChunk sut1(chunkManagement);
    SharedChunk sut2;
    ChunkHeader* header = sut1.getChunkHeader();

    sut2 = std::move(sut1);

    ASSERT_EQ(sut1.getChunkHeader(), nullptr);
    ASSERT_EQ(sut2.getChunkHeader(), header);
    EXPECT_EQ((sut2.getChunkHeader())->chunkSize(), (sizeof(ChunkHeader) + USER_PAYLOAD_SIZE));
}

TEST_F(SharedChunk_Test, CompareWithSameMemoryChunkComparesToUserPayload)
{
    EXPECT_THAT(sut == sut.getUserPayload(), Eq(true));
}

TEST_F(SharedChunk_Test, GetChunkHeaderMethodReturnsNullPointerWhenSharedChunkObjectIsInitialisedWithNullPointer)
{
    SharedChunk sut(nullptr);

    EXPECT_THAT(sut.getChunkHeader(), Eq(nullptr));
}

TEST_F(SharedChunk_Test, GetChunkHeaderMethodReturnsValidPointerWhenSharedChunkObjectIsInitialisedWithAValidPointer)
{
    void* newChunk = mempool.getChunk();
    SharedChunk sut(GetChunkManagement(newChunk));

    EXPECT_THAT(sut.getChunkHeader(), Eq(newChunk));
}

TEST_F(SharedChunk_Test, EqualityOperatorOnTwoSharedChunkWithTheSameContentReturnsTrue)
{
    SharedChunk sut1{chunkManagement};
    SharedChunk sut2{chunkManagement};

    EXPECT_TRUE(sut2 == sut1);
}

TEST_F(SharedChunk_Test, EqualityOperatorOnTwoSharedChunkWithDifferentContentReturnsFalse)
{
    SharedChunk sut1{chunkManagement};
    SharedChunk sut2(nullptr);

    EXPECT_FALSE(sut1 == sut2);
}

TEST_F(SharedChunk_Test, EqualityOperatorOnSharedChunkAndSharedChunkPayloadWithDifferentChunkManagementsReturnFalse)
{
    SharedChunk sut1{chunkManagement};
    SharedChunk sut2{nullptr};

    EXPECT_FALSE(sut1 == sut2.getUserPayload());
}

TEST_F(SharedChunk_Test, EqualityOperatorOnSharedChunkAndSharedChunkPayloadWithSameChunkManagementsReturnTrue)
{
    SharedChunk sut1{chunkManagement};
    SharedChunk sut2{chunkManagement};

    EXPECT_TRUE(sut1 == sut2.getUserPayload());
}

TEST_F(SharedChunk_Test, BoolOperatorOnValidSharedChunkReturnsTrue)
{
    EXPECT_TRUE(sut);
}

TEST_F(SharedChunk_Test, BoolOperatorOnSharedChunkWithChunkManagementAsNullPointerReturnsFalse)
{
    SharedChunk sut(nullptr);

    EXPECT_FALSE(sut);
}

TEST_F(SharedChunk_Test, getUserPayloadWhenInvalidResultsInNullptr)
{
    SharedChunk sut2(nullptr);
    EXPECT_THAT(sut2.getUserPayload(), Eq(nullptr));
}

TEST_F(SharedChunk_Test, GetPayloadMethodReturnsNullPointerWhen_m_chunkmanagmentIsInvalid)
{
    SharedChunk sut1(nullptr);

    EXPECT_THAT(sut1.getUserPayload(), Eq(nullptr));
}

TEST_F(SharedChunk_Test, GetPayloadMethodReturnsValidPointerWhen_m_chunkmanagmentIsValid)
{
    using DATA_TYPE = uint32_t;
    constexpr DATA_TYPE USER_DATA{7337U};
    ChunkHeader* newChunk = static_cast<ChunkHeader*>(mempool.getChunk());

    auto chunkSettingsResult = ChunkSettings::create(sizeof(DATA_TYPE), alignof(DATA_TYPE));
    ASSERT_FALSE(chunkSettingsResult.has_error());
    auto& chunkSettings = chunkSettingsResult.value();

    new (newChunk) ChunkHeader(mempool.getChunkSize(), chunkSettings);
    new (static_cast<DATA_TYPE*>(newChunk->userPayload())) DATA_TYPE{USER_DATA};

    iox::mepoo::SharedChunk sut1(GetChunkManagement(newChunk));
    EXPECT_THAT(*static_cast<DATA_TYPE*>(sut1.getUserPayload()), Eq(USER_DATA));
}

TEST_F(SharedChunk_Test, MultipleSharedChunksCleanup)
{
    {
        SharedChunk sut3{nullptr}, sut4{nullptr}, sut5{nullptr};
        {
            {
                SharedChunk sut6{nullptr}, sut7{nullptr}, sut8{nullptr};
                {
                    iox::mepoo::SharedChunk sut2(GetChunkManagement(mempool.getChunk()));

                    sut3 = sut2;
                    sut4 = sut2;
                    sut5 = sut3;
                    sut6 = sut5;
                    sut7 = sut4;
                    sut8 = sut2;

                    EXPECT_THAT(chunkMgmtPool.getUsedChunks(), Eq(2));
                    EXPECT_THAT(mempool.getUsedChunks(), Eq(2));
                }
                EXPECT_THAT(chunkMgmtPool.getUsedChunks(), Eq(2));
                EXPECT_THAT(mempool.getUsedChunks(), Eq(2));
            }
            EXPECT_THAT(mempool.getUsedChunks(), Eq(2));
            EXPECT_THAT(chunkMgmtPool.getUsedChunks(), Eq(2));
        }
        EXPECT_THAT(mempool.getUsedChunks(), Eq(2));
        EXPECT_THAT(chunkMgmtPool.getUsedChunks(), Eq(2));
    }
    EXPECT_THAT(mempool.getUsedChunks(), Eq(1));
    EXPECT_THAT(chunkMgmtPool.getUsedChunks(), Eq(1));
}


TEST_F(SharedChunk_Test, MultipleChunksCleanup)
{
    {
        iox::mepoo::SharedChunk sut2(GetChunkManagement(mempool.getChunk()));
        {
            iox::mepoo::SharedChunk sut2(GetChunkManagement(mempool.getChunk()));
            {
                iox::mepoo::SharedChunk sut2(GetChunkManagement(mempool.getChunk()));
                iox::mepoo::SharedChunk sut4(GetChunkManagement(mempool.getChunk()));
                {
                    iox::mepoo::SharedChunk sut2(GetChunkManagement(mempool.getChunk()));
                    iox::mepoo::SharedChunk sut4(GetChunkManagement(mempool.getChunk()));
                    {
                        iox::mepoo::SharedChunk sut2(GetChunkManagement(mempool.getChunk()));
                        iox::mepoo::SharedChunk sut4(GetChunkManagement(mempool.getChunk()));
                        EXPECT_THAT(mempool.getUsedChunks(), Eq(9));
                        EXPECT_THAT(chunkMgmtPool.getUsedChunks(), Eq(9));
                    }
                    EXPECT_THAT(mempool.getUsedChunks(), Eq(7));
                    EXPECT_THAT(chunkMgmtPool.getUsedChunks(), Eq(7));
                }
                EXPECT_THAT(mempool.getUsedChunks(), Eq(5));
                EXPECT_THAT(chunkMgmtPool.getUsedChunks(), Eq(5));
            }
            EXPECT_THAT(mempool.getUsedChunks(), Eq(3));
            EXPECT_THAT(chunkMgmtPool.getUsedChunks(), Eq(3));
        }
        EXPECT_THAT(mempool.getUsedChunks(), Eq(2));
        EXPECT_THAT(chunkMgmtPool.getUsedChunks(), Eq(2));
    }
    EXPECT_THAT(mempool.getUsedChunks(), Eq(1));
    EXPECT_THAT(chunkMgmtPool.getUsedChunks(), Eq(1));
}

TEST_F(SharedChunk_Test, NonEqualityOperatorOnTwoSharedChunkWithDifferentContentReturnsTrue)
{
    SharedChunk sut1{chunkManagement};
    SharedChunk sut2;

    EXPECT_TRUE(sut1 != sut2);
}

TEST_F(SharedChunk_Test, NonEqualityOperatorOnTwoSharedChunkWithSameContentReturnsFalse)
{
    SharedChunk sut1{chunkManagement};
    SharedChunk sut2(chunkManagement);

    EXPECT_FALSE(sut1 != sut2);
}

TEST_F(SharedChunk_Test, NonEqualityOperatorOnSharedChunkAndSharedChunkPayloadWithDifferentChunkManagementsReturnTrue)
{
    SharedChunk sut1{chunkManagement};
    SharedChunk sut2(nullptr);

    EXPECT_TRUE(sut1 != sut2.getUserPayload());
}

TEST_F(SharedChunk_Test, NonEqualityOperatorOnSharedChunkAndSharedChunkPayloadWithSameChunkManagementsReturnFalse)
{
    SharedChunk sut1{chunkManagement};
    SharedChunk sut2(chunkManagement);

    EXPECT_FALSE(sut1 != sut2.getUserPayload());
}

TEST_F(SharedChunk_Test,
       ReleaseMethodReturnsChunkManagementPointerOfSharedChunkObjectAndSetsTheChunkManagementRelativePointerToNull)
{
    ChunkManagement* returnValue = sut.release();

    EXPECT_THAT(returnValue->m_mempool->getChunkSize(), sizeof(ChunkHeader) + USER_PAYLOAD_SIZE);
    EXPECT_THAT(returnValue->m_mempool->getChunkCount(), NUMBER_OF_CHUNKS);
    EXPECT_FALSE(sut);
}