#include <mutex>
#include <spdlog/spdlog.h>

#include "../include/SubscribePriceCallData.h"
#include "market/v1/market.pb.h"
#include <grpcpp/completion_queue.h>

SubscribePriceCallData::SubscribePriceCallData(
    market::v1::MarketService::AsyncService *service,
    grpc::ServerCompletionQueue *completionQueue)
    : mService(service),
      mCompletionQueue(completionQueue),
      mContext{},
      mRequest{},
      mResponse{}
{
    mPriceWriter = std::make_unique<grpc::ServerAsyncWriter<market::v1::PriceUpdate>>(&mContext);
    ProcessData(true);
}

void SubscribePriceCallData::ProcessData(bool ok)
{
    if (!ok)
    {
        spdlog::info("Context status: client {} disconnected", mContext.peer());

        SubscriberManager::Instance().RemoveSubscriber(this);
        delete this;
        return;
    }

    if (eState::CREATE == mState)
    {
        mState = eState::PROCESS;
        mService->RequestSubscribePrices(&mContext,
                                         &mRequest,
                                         mPriceWriter.get(),
                                         mCompletionQueue,
                                         mCompletionQueue,
                                         this);
        return;
    }

    if (eState::PROCESS == mState)
    {
        new SubscribePriceCallData(mService, mCompletionQueue);

        spdlog::info("Client subscribe to {}", mRequest.symbol());

        mState = eState::WRITE;
        return;
    }

    if (eState::WRITE == mState)
    {
        std::lock_guard<std::mutex> locker(mWriteMutex);
        mWriteInProgress.store(false);
        ProcessQueue();
        return;
    }

    if (eState::FINISH == mState || !ok)
    {
        mPriceWriter->Finish(grpc::Status::OK, this);
        delete this;
        return;
    }
}

void SubscribePriceCallData::PushPrice(const std::string &symbol, double value)
{
    if (eState::FINISH == mState || symbol != mRequest.symbol())
    {
        return;
    }

    market::v1::PriceUpdate update;
    update.set_symbol(mRequest.symbol());
    update.set_price(value);
    update.set_timestamp(time(nullptr));

    spdlog::info("TRACE: {} {} {}", update.symbol(), update.price(), update.timestamp());

    {
        std::lock_guard<std::mutex> lock(mWriteMutex);
        mUpdateQueue.push(update);
    }

    ProcessQueue();

    std::this_thread::sleep_for(std::chrono::microseconds(400)); // Just to avoid response spamming
}

void SubscribePriceCallData::ProcessQueue()
{
    if (mWriteInProgress.load() || mUpdateQueue.empty())
    {
        return;
    }

    mWriteInProgress.store(true);

    mResponse = mUpdateQueue.front();
    mUpdateQueue.pop();
    mPriceWriter->Write(mResponse, this);
}