#include <mutex>
#include <spdlog/spdlog.h>

#include "../include/SubscribePriceCallData.h"
#include "market/v1/market.pb.h"
#include <grpcpp/completion_queue.h>
#include "../include/SubscriberManager.h"

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
        spdlog::info("NOK RESULT Context status: client {} disconnected", mContext.peer());

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
        spdlog::info("Client subscribe to {}", mRequest.symbol());

        new SubscribePriceCallData(mService, mCompletionQueue);
        SubscriberManager::Instance().AddSubscriber(this);

        mState = eState::WRITE;
        SendPrice();
        return;
    }

    if (eState::WRITE == mState)
    {
        std::lock_guard<std::mutex> locker(mWriteMutex);
        if (!mUpdateQueue.empty())
        {
            mUpdateQueue.pop();
            mPriceWriter->Write(mUpdateQueue.front(), this);
        }
        else
        {
            mState = eState::FINISH;
            mWriteInProgress.store(false);
        }

        return;
    }

    if (eState::FINISH == mState && !ok)
    {
        spdlog::info("Context status: client {} disconnected", mContext.peer());

        SubscriberManager::Instance().RemoveSubscriber(this);
        mPriceWriter->Finish(grpc::Status::OK, this);
        delete this;
        return;
    }
}

void SubscribePriceCallData::PushPrice(const std::string &symbol, double value)
{
    if (symbol != mRequest.symbol())
    {
        return;
    }

    market::v1::PriceUpdate update;
    update.set_symbol(symbol);
    update.set_price(value);
    update.set_timestamp(time(nullptr));

    std::lock_guard<std::mutex> locker(mWriteMutex);

    mUpdateQueue.push(update);

    if (!mWriteInProgress.load())
    {
        mWriteInProgress.store(true);
        mPriceWriter->Write(mUpdateQueue.front(), this);
    }
}

void Print(const market::v1::PriceUpdate &response)
{
    spdlog::trace("TRACE: {} {} {}", response.symbol(), response.price(), response.timestamp());
}

void SubscribePriceCallData::SendPrice()
{
    std::lock_guard<std::mutex> lock(mWriteMutex);
    spdlog::info("SubscribePriceCallData::SendPrice");
    Print(mResponse);

    if (eState::FINISH == mState)
    {
        return;
    }

    mResponse.set_symbol(mRequest.symbol());
    mResponse.set_price(1000 + rand() % 100);
    mResponse.set_timestamp(time(nullptr));

    Print(mResponse);
    std::this_thread::sleep_for(std::chrono::microseconds(400));
    mPriceWriter->Write(mResponse, this);
}