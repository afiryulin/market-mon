#include <memory>

#include "../include/TradeCallData.h"
#include "spdlog/spdlog.h"

TradeCallData::TradeCallData(MarketService::AsyncService *service,
                             ServerCompletionQueue *completionQueue)
    : mService(service), mCompletionQueue(completionQueue), mContext{}, mRequest{}, mResponse{}
{
    mStream = std::make_unique<ServerAsyncReaderWriter<TradeEvent, TradeRequest>>(&mContext);

    ProcessData(true);
}

void TradeCallData::ProcessData(bool ok)
{
    if (!ok)
    {

        Finish();
        return;
    }

    if (eState::CREATE == mState)
    {
        mState = eState::CONNECTED;
        mService->RequestTradeStream(&mContext, mStream.get(), mCompletionQueue, mCompletionQueue,
                                     this);

        return;
    }

    if (eState::CONNECTED == mState)
    {
        spdlog::info("TradeStream connected: {}", mContext.peer());
        new TradeCallData(mService, mCompletionQueue);

        StartRead();
        return;
    }

    if (eState::READ == mState)
    {
        spdlog::info("Trade Order: {} {} {}", mRequest.symbol(), mRequest.quantity(),
                     mRequest.is_buy() ? "BUY" : "SELL");

        TradeEvent response{};
        response.set_symbol(mRequest.symbol());
        response.set_price(1000 + rand() % 100);
        response.set_status("ACCEPTED");

        EnqueueResponse(response);
        StartRead();
        return;
    }

    if (eState::WRITE == mState)
    {
        {
            std::lock_guard<std::mutex> lockWriter(mWriteMutex);
            mIsWriting.store(false);
        }

        TryWriteNext();
        return;
    }

    if (eState::FINISH == mState)
    {
        delete this;
        return;
    }
}
void TradeCallData::StartRead()
{
    if (mIsFinished.load())
        return;

    mState = eState::READ;
    mStream->Read(&mRequest, this);
}

void TradeCallData::EnqueueResponse(const TradeEvent &response)
{
    {
        std::lock_guard<std::mutex> lock(mWriteMutex);
        mWriteQueue.push(response);
    }

    TryWriteNext();
}

void TradeCallData::TryWriteNext()
{
    if (mIsFinished.load())
        return;

    std::lock_guard<std::mutex> locker(mWriteMutex);

    if (mIsWriting.load())
        return;

    if (mWriteQueue.empty())
        return;

    mResponse = mWriteQueue.front();
    mWriteQueue.pop();

    mIsWriting.store(true);
    mState = eState::WRITE;

    mStream->Write(mResponse, this);
}

void TradeCallData::Finish()
{
    if (mIsFinished.load())
        return;

    spdlog::info("TradeStream disconnected: {}", mContext.peer());

    mState = eState::FINISH;
    mStream->Finish(Status::OK, this);
}
