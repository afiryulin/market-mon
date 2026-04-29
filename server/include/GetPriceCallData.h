#include "ICallDataBase.h"
#include "market/v1/market.grpc.pb.h"

class GetPriceCallData final : public ICallDataBase
{
public:
    GetPriceCallData(market::v1::MarketService::AsyncService *service,
                     grpc::ServerCompletionQueue *completionQueue);
    void ProcessData(bool ok) override;

private:
    enum class eState
    {
        CREATE,
        PROCESS,
        FINISH
    };

private:
    market::v1::MarketService::AsyncService *mService;
    grpc::ServerCompletionQueue *mCompletionQueue;

    eState mState = eState::FINISH;

    grpc::ServerContext mContext;

    market::v1::PriceRequest mRequest;
    market::v1::PriceResponse mResponse;

    std::unique_ptr<grpc::ServerAsyncResponseWriter<market::v1::PriceResponse>> mResponder;
};