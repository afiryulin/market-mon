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
    if (eState::CREATE == mState)
    {
        mState = eState::PROCESS;

        mService->RequestSubscribePrices(&mContext, &mRequest, mPriceWriter.get(), mCompletionQueue, mCompletionQueue, this);
        return;
    }

    if (eState::PROCESS == mState)
    {
        new SubscribePriceCallData(mService, mCompletionQueue);

        spdlog::info("Client subscribe to {}", mRequest.symbol());

        mState = eState::WRITE;
        SendPrice();

        return;
    }

    if (eState::WRITE == mState)
    {
        SendPrice();
        mState = eState::FINISH;
        return;
    }

    delete this;
}

void Print(const market::v1::PriceUpdate &response)
{
    spdlog::info("TRACE: {} {} {}", response.symbol(), response.price(), response.timestamp());
}

void SubscribePriceCallData::SendPrice()
{
    std::lock_guard<std::mutex> lock(mMutex);
    spdlog::info("SubscribePriceCallData::SendPrice");
    Print(mResponse);

    if (eState::FINISH == mState || mWriteInProgress)
    {
        return;
    }

    mResponse.set_symbol(mRequest.symbol());
    mResponse.set_price(1000 + rand() % 100);
    mResponse.set_timestamp(time(nullptr));

    Print(mResponse);
    mWriteInProgress = true;
    try
    {
        mPriceWriter->Write(mResponse, this);
    }
    catch (const std::exception &ex)
    {
        spdlog::error("Exception: {}", ex.what());
    }
}