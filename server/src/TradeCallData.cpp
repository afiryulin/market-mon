#include <memory>

#include "../include/TradeCallData.h"
#include "spdlog/spdlog.h"

TradeCallData::TradeCallData(market::v1::MarketService::AsyncService *service, grpc::ServerCompletionQueue *completionQueue)
    : mService(service),
      mCompletionQueue(completionQueue),
      mContext{},
      mRequest{},
      mResponse{}
{
    mStream = std::make_unique<grpc::ServerAsyncReaderWriter<
        market::v1::TradeEvent,
        market::v1::TradeRequest>>(&mContext);

    ProcessData(true);
}

void TradeCallData::ProcessData(bool ok)
{
    if (!ok)
    {

        if (eState::FINISH == mState)
        {
            delete this;
        }
        else
        {
            mState = eState::FINISH;
            mStream->Finish(grpc::Status::OK, this);
        }
        return;
    }

    if (eState::CREATE == mState)
    {
        mState = eState::CONNECTED;
        mService->RequestTradeStream(&mContext,
                                     mStream.get(),
                                     mCompletionQueue,
                                     mCompletionQueue,
                                     this);

        return;
    }

    if (eState::CONNECTED == mState)
    {
        new TradeCallData(mService, mCompletionQueue);

        mStream->Read(&mRequest, this);
        mState = eState::WAIT_READ;
        return;
    }

    if (eState::WAIT_READ == mState)
    {
        spdlog::info("Trade Order: {} {} {}", mRequest.symbol(), mRequest.quantity(), mRequest.is_buy());

        mResponse.set_symbol(mRequest.symbol());
        mResponse.set_price(1000 + rand() % 100);
        mResponse.set_status("FILLED");

        mState = eState::WAIT_WRITE;
        mStream->Write(mResponse, this);
        return;
    }

    if (eState::WAIT_WRITE == mState)
    {
        mState = eState::WAIT_READ;
        mStream->Read(&mRequest, this);
        return;
    }
}