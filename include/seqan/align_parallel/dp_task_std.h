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

#ifndef INCLUDE_SEQAN_ALIGN_PARALLEL_DP_TASK_STD_H_
#define INCLUDE_SEQAN_ALIGN_PARALLEL_DP_TASK_STD_H_

namespace seqan
{

// ============================================================================
// Forwards
// ============================================================================

// ============================================================================
// Tags, Classes, Enums
// ============================================================================

template <typename TTaskConfig, typename TThreadLocalStorage, typename TVecExecPolicy>
class DPTaskImpl<TTaskConfig, TThreadLocalStorage, TVecExecPolicy, ParallelExecutionPolicyNative> :
    public DPTaskBase<DPTaskImpl<TTaskConfig, TThreadLocalStorage, TVecExecPolicy, ParallelExecutionPolicyNative> >
{
public:

    using TSize = typename TTaskConfig::TSize;
    using TBase = DPTaskBase<DPTaskImpl<TTaskConfig, TThreadLocalStorage, TVecExecPolicy, ParallelExecutionPolicyNative> >;

    // ============================================================================
    // Member variables.

    std::atomic<unsigned>   refCount;
    TThreadLocalStorage*    localDpContext = nullptr;
    bool                    mIsLastTask = false;

    // ============================================================================
    // Constructor.

    DPTaskImpl(TSize pCol, TSize pRow, TTaskConfig & pConfig) :
        TBase(pCol, pRow, pConfig, *this)
    {}

    // ============================================================================
    // Member functions.

    inline void
    setRefCount(unsigned const n)
    {
        refCount.store(n, std::memory_order_relaxed);
    }

    inline unsigned
    decrementRefCount()
    {
        return --refCount;
    }

    inline unsigned
    incrementRefCount()
    {
        return ++refCount;
    }

    template <typename TQueue>
    inline void
    updateAndSpawn(TQueue & pWorkQueue)
    {
        for(auto& task : TBase::successor)
        {
            if (task != nullptr && task->decrementRefCount() == 0)
                appendValue(pWorkQueue, task);
        }
    }

    template <typename TVec, typename TQueue>
    inline SEQAN_FUNC_ENABLE_IF(Not<IsVectorExecutionPolicy<TVec> >, void)
    executeImpl(TQueue& pWorkQueue)
    {
        SEQAN_ASSERT(localDpContext != nullptr);
        TBase::runScalar(*localDpContext);
        updateAndSpawn(pWorkQueue);
        return nullptr;
    }

    template <typename TVec, typename TQueue>
    inline SEQAN_FUNC_ENABLE_IF(IsVectorExecutionPolicy<TVec>, void)
    executeImpl(TQueue& pWorkQueue)
    {
        SEQAN_ASSERT(localDpContext != nullptr);

        String<DPTaskImpl*> tasks;
        {  // Acquire scoped lock.
            std::lock_guard<decltype(TBase::_taskContext.mLock)> scopedLock(TBase::_taskContext.mLock);

            appendValue(tasks, this);
            if (length(pWorkQueue) >= TBase::_taskContext.mSimdLength - 1)
                for (auto i = 0; i < TBase::_taskContext.mSimdLength - 1; ++i)
                    appendValue(tasks, popFront(pWorkQueue));
        }   // Release scoped lock.

        SEQAN_ASSERT_GT(length(tasks), 0u);

        if (length(tasks) == 1)
        {
            front(tasks)->TBase::runScalar(*localDpContext);
        }
        else
        {
            SEQAN_ASSERT_EQ(length(tasks), TBase::_taskContext.mSimdLength);
            // Make to simd version!
            TBase::runSimd(tasks, *localDpContext);
        }
        // Update and spawn sucessors.
        for (auto& task : tasks)
        {
            task->updateAndSpawn(pWorkQueue);
        }
    }

    template <typename TQueue>
    inline DPTaskImpl *
    execute(TQueue& pWorkQueue)
    {
        executeImpl<TVecExecPolicy>(pWorkQueue);
        if (mIsLastTask)
        {
            {
                std::lock_guard<std::mutex> lk(TBase::_taskContext.mLockEvent);
                TBase::_taskContext.mReady = true;
            }
            TBase::_taskContext.mReadyEvent.notify_all();
        }
        return nullptr;
    }
};

// ============================================================================
// Metafunctions
// ============================================================================

template <typename TTaskContext, typename TThreadLocalStorage, typename TVecExecPolicy>
struct IsDPTask<DPTaskImpl<TTaskContext, TThreadLocalStorage, TVecExecPolicy, ParallelExecutionPolicyNative> > : True
{};

template <typename TTaskContext, typename TThreadLocalStorage, typename TVecExecPolicy>
struct Pointer_<DPTaskImpl<TTaskContext, TThreadLocalStorage, TVecExecPolicy, ParallelExecutionPolicyNative> >
{
    using TTask_ = DPTaskImpl<TTaskContext, TThreadLocalStorage, TVecExecPolicy, ParallelExecutionPolicyNative>;
    using Type  = std::unique_ptr<TTask_>;
};

// ============================================================================
// Functions
// ============================================================================

template <typename TTaskContext, typename TVecExecPolicy>
inline auto
createGraph(TTaskContext & context,
            TVecExecPolicy const & /*vecExecPolicy*/,
            ParallelExecutionPolicyNative const & /*taskImplTag*/)
{
    using TThreadLocalStorage = typename TTaskContext::TDPContext;
    using TDagTask = DPTaskImpl<TTaskContext, TThreadLocalStorage, TVecExecPolicy, ParallelExecutionPolicyNative>;

    DPTaskGraph<TDagTask> graph;

    resize(graph.get(), length(context.getSeqH()));
    for (int i = length(context.getSeqH()); --i >= 0;)
    {
        resize(graph[i], length(context.getSeqV()));
        for (int j = length(context.getSeqV()); --j >= 0;)
        {
            graph[i][j].reset(new TDagTask(i, j, context));
            graph[i][j]->successor[0] = (i + 1 < length(context.getSeqH())) ? graph[i+1][j].get() : nullptr;
            graph[i][j]->successor[1] = (j + 1 < length(context.getSeqV())) ? graph[i][j+1].get() : nullptr;
            graph[i][j]->setRefCount(((i > 0) ? 1 : 0) + ((j > 0) ? 1 : 0));
        }
    }
    lastTask(graph)->mIsLastTask = true;
    return graph;
}

template <typename TTaskContext, typename TThreadLocalStorage, typename TVecExecPolicy>
inline void
invoke(DPTaskGraph<DPTaskImpl<TTaskContext, TThreadLocalStorage, TVecExecPolicy, ParallelExecutionPolicyNative> > & graph)
{
    using TTask = DPTaskImpl<TTaskContext, TThreadLocalStorage, TVecExecPolicy, ParallelExecutionPolicyNative>;
    using TWorkQueue = ConcurrentQueue<TTask *>;

    struct DPThread
    {
        TWorkQueue*         workQueuePtr = nullptr;
        TThreadLocalStorage tls;

        inline void operator()()
        {
            lockWriting(*workQueuePtr);

            while (true)
            {
                TTask* task = nullptr;
                if (!popFront(task, *workQueuePtr))
                    return;

                SEQAN_ASSERT(task != nullptr);
                task->localDpContext = &tls;
                task->template execute(*workQueuePtr);
            }
        }
    };

    TWorkQueue queue;
    std::vector<std::thread> workerThreads;
    for (unsigned jobId = 0; jobId < std::thread::hardware_concurrency(); ++jobId)
        workerThreads.emplace_back(std::thread(DPThread{&queue, TThreadLocalStorage()}));

    waitForWriters(queue, std::thread::hardware_concurrency());
    appendValue(queue, firstTask(graph).get());  // Kick off execution.

    // Wait for threads to finish.
    {
        std::unique_lock<std::mutex> lk(lastTask(graph)->_taskContext.mLockEvent);
        lastTask(graph)->_taskContext.mReadyEvent.wait(lk, [&graph]{return lastTask(graph)->_taskContext.mReady;});
    }

    for (unsigned jobId = 0; jobId < length(workerThreads); ++jobId)
        unlockWriting(queue);

    SEQAN_ASSERT(empty(queue));
    // Barrier for waiting on the jobs.
    for (auto& worker : workerThreads)
        worker.join();
}
}  // namespace seqan

#endif  // #ifndef INCLUDE_SEQAN_ALIGN_PARALLEL_DP_TASK_STD_H_
