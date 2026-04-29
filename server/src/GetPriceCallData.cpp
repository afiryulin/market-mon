#include "../include/GetPriceCallData.h"
#include <grpcpp/support/async_unary_call.h>

GetPriceCallData::GetPriceCallData(market::v1::MarketService::AsyncService *service,
                                   grpc::ServerCompletionQueue *completionQueue)
    : mService(service), mCompletionQueue(completionQueue)
{
    mResponder =
        std::make_unique<grpc::ServerAsyncResponseWriter<market::v1::PriceResponse>>(&mContext);

    ProcessData(true);
}

void GetPriceCallData::ProcessData(bool ok)
{
    if (!ok)
    {
        delete this;
        return;
    }

    if (eState::CREATE == mState)
    {
        mState = eState::PROCESS;

        mService->RequestGetPrice(&mContext, &mRequest, mResponder.get(), mCompletionQueue,
                                  mCompletionQueue, this);

        return;
    }

    if (eState::PROCESS == mState)
    {
        new GetPriceCallData(mService, mCompletionQueue);

        mResponse.set_symbol(mRequest.symbol());
        mResponse.set_price(50000 + rand() % 1000);
        mResponse.set_timestamp(time(nullptr));

        mState = eState::FINISH;
        return;
    }

    if (eState::FINISH == mState)
    {
        delete this;
    }
}