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

    mService->RequestSubscribePrices(&mContext, &mRequest, mPriceWriter.get(), mCompletionQueue,
                                     mCompletionQueue, &mCreateTag);
}

void SubscribePriceCallData::ProcessData(CallDataTag *tag, bool ok)
{
    if (!ok)
    {
        if (!mIsFinished.load())
        {
            mIsFinished.store(true);
            SubscriberManager::Instance().RemoveSubscriber(this);
            mPriceWriter->Finish(grpc::Status::OK, &mFinishTag);
        }
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

    if (eCallDataAction::FINISH == tag->actionType)
    {
        spdlog::info("Context status: client {} disconnected", mContext.peer());
        delete this;
    }
}

void SubscribePriceCallData::PushPrice(const std::string &symbol, double value)
{
    if (symbol != mRequest.symbol())
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
