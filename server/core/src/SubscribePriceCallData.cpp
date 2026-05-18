#include <mutex>
#include <spdlog/spdlog.h>

#include "../include/SubscribePriceCallData.h"
#include "../include/SubscriberManager.h"
#include "market/v1/market.pb.h"
#include <grpcpp/completion_queue.h>

SubscribePriceCallData::SubscribePriceCallData(market::v1::MarketService::AsyncService *service,
                                               grpc::ServerCompletionQueue *completionQueue)
    : mService(service), mCompletionQueue(completionQueue), mContext{}, mRequest{}, mResponse{}
{
    mPriceWriter = std::make_unique<grpc::ServerAsyncWriter<market::v1::PriceUpdate>>(&mContext);

    mService->RequestSubscribePrices(&mContext, &mRequest, mPriceWriter.get(), mCompletionQueue, mCompletionQueue,
                                     &mCreateTag);
}

void SubscribePriceCallData::ProcessData(CallDataTag *tag, bool ok)
{
    if (ok && eCallDataAction::FINISH == tag->actionType)
    {
        spdlog::info("Context status: client {} disconnected", mContext.peer());
        SubscriberManager::Instance().RemoveSubscriber(this);

        delete this;
        return;
    }

    if (!ok)
    {
        // When ok=false, the operation was cancelled and the call is invalid.
        // Do not attempt to call Finish() as it will cause invalid pointer access.
        // Just mark as finished and let it be cleaned up naturally.
        mIsFinished.store(true);
        return;
    }

    if (eCallDataAction::CONNECT == tag->actionType)
    {

        spdlog::info("Client subscribe to {}", mRequest.symbol());

        new SubscribePriceCallData(mService, mCompletionQueue);
        SubscriberManager::Instance().AddSubscriber(this);
    }

    if (eCallDataAction::WRITE == tag->actionType)
    {
        mWriteInProgress.store(false);
        ProcessQueue();
    }
}

void SubscribePriceCallData::PushPrice(const std::string &symbol, double value)
{
    if (mIsFinished.load() || symbol != mRequest.symbol())
    {
        return;
    }

    market::v1::PriceUpdate update;
    update.set_symbol(mRequest.symbol());
    update.set_price(value);
    update.set_timestamp(time(nullptr));

    spdlog::info("PRICE: {} {} {}", update.symbol(), update.price(), update.timestamp());

    {
        std::lock_guard<std::mutex> lock(mWriteMutex);
        mUpdateQueue.push(update);
    }

    ProcessQueue();

    std::this_thread::sleep_for(std::chrono::microseconds(400)); // Just to avoid response spamming
}

void SubscribePriceCallData::ProcessQueue()
{
    std::lock_guard<std::mutex> locker(mWriteMutex);

    if (mWriteInProgress.load() || mUpdateQueue.empty())
    {
        return;
    }

    mWriteInProgress.store(true);
    mResponse = mUpdateQueue.front();
    mUpdateQueue.pop();
    mPriceWriter->Write(mResponse, &mWriteTag);
}
