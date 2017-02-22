// ==========================================================================
//                 SeqAn - The Library for Sequence Analysis
// ==========================================================================
// Copyright (c) 2006-2016, Knut Reinert, FU Berlin
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Knut Reinert or the FU Berlin nor the names of
//       its contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL KNUT REINERT OR THE FU BERLIN BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.
//
// ==========================================================================
// Author: Rene Rahn <rene.rahn@fu-berlin.de>
// ==========================================================================

#include <seqan/align_parallel_2.h>

SEQAN_DEFINE_TEST(test_align_parallel_intermediate_dp_result_construct)
{
    using namespace seqan;

    using TIntermediate = IntermediateDPResult<DPScout_<DPCell_<int, AffineGaps>, Default>>;

    SEQAN_ASSERT(std::is_default_constructible<TIntermediate>::value);
    SEQAN_ASSERT(std::is_copy_constructible<TIntermediate>::value);
    SEQAN_ASSERT(std::is_move_constructible<TIntermediate>::value);
    SEQAN_ASSERT(std::is_copy_assignable<TIntermediate>::value);
    SEQAN_ASSERT(std::is_move_assignable<TIntermediate>::value);

    {
        TIntermediate interim;
        SEQAN_ASSERT_EQ(interim.mMaxState.first, minValue<int>());
        SEQAN_ASSERT_EQ(interim.mMaxState.second, 0u);
        SEQAN_ASSERT_EQ(interim.mTileCol, 0u);
        SEQAN_ASSERT_EQ(interim.mTileRow, 0u);
    }

    {
        TIntermediate interim{{10, 3u}};
        SEQAN_ASSERT_EQ(interim.mMaxState.first, 10);
        SEQAN_ASSERT_EQ(interim.mMaxState.second, 3u);
        SEQAN_ASSERT_EQ(interim.mTileCol, 0u);
        SEQAN_ASSERT_EQ(interim.mTileRow, 0u);
    }

    {
        TIntermediate interim{{10, 3u}, 2u, 4u};
        SEQAN_ASSERT_EQ(interim.mMaxState.first, 10);
        SEQAN_ASSERT_EQ(interim.mMaxState.second, 3u);
        SEQAN_ASSERT_EQ(interim.mTileCol, 2u);
        SEQAN_ASSERT_EQ(interim.mTileRow, 4u);
    }
}

SEQAN_DEFINE_TEST(test_align_parallel_intermediate_dp_result_update_max)
{
    using namespace seqan;

    using TIntermediate = IntermediateDPResult<DPScout_<DPCell_<int, AffineGaps>, Default>>;
    using TState = typename TIntermediate::TState;

    TIntermediate interim{{10, 3u}, 2, 4};
    updateMax(interim, TState{9, 7u}, 3, 5);
    SEQAN_ASSERT_EQ(interim.mMaxState.first, 10);
    SEQAN_ASSERT_EQ(interim.mMaxState.second, 3u);
    SEQAN_ASSERT_EQ(interim.mTileCol, 2u);
    SEQAN_ASSERT_EQ(interim.mTileRow, 4u);

    updateMax(interim, TState{11, 7u}, 3, 5);
    SEQAN_ASSERT_EQ(interim.mMaxState.first, 11);
    SEQAN_ASSERT_EQ(interim.mMaxState.second, 7u);
    SEQAN_ASSERT_EQ(interim.mTileCol, 3u);
    SEQAN_ASSERT_EQ(interim.mTileRow, 5u);

}

SEQAN_DEFINE_TEST(test_align_parallel_intermediate_dp_result_clear)
{
    using namespace seqan;

    using TIntermediate = IntermediateDPResult<DPScout_<DPCell_<int, AffineGaps>, Default>>;

    {
        TIntermediate interim{{10, 3u}, 2, 4};
        clear(interim);

        SEQAN_ASSERT_EQ(interim.mMaxState.first, minValue<int>());
        SEQAN_ASSERT_EQ(interim.mMaxState.second, 0u);
        SEQAN_ASSERT_EQ(interim.mTileCol, 0u);
        SEQAN_ASSERT_EQ(interim.mTileRow, 0u);
    }


    {
        TIntermediate interim{{10, 3u}, 2, 4};
        clear(std::move(interim));

        SEQAN_ASSERT_EQ(interim.mMaxState.first, minValue<int>());
        SEQAN_ASSERT_EQ(interim.mMaxState.second, 0u);
        SEQAN_ASSERT_EQ(interim.mTileCol, 0u);
        SEQAN_ASSERT_EQ(interim.mTileRow, 0u);
    }
}
