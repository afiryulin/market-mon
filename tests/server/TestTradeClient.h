#pragma once

#include "market/v1/market.grpc.pb.h"
#include <grpcpp/grpcpp.h>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

class TestTradeClient
{
public:
    explicit TestTradeClient(const std::string &address)
    {
        auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());

        mStub = market::v1::MarketService::NewStub(channel);
        mStream = mStub->TradeStream(&mContext);

        mReader = std::jthread([this](std::stop_token st) { ReadLoop(st); });
    }

    ~TestTradeClient() { Finish(); }

    bool Write(const market::v1::TradeRequest &request) { return mStream && mStream->Write(request); }

    std::optional<market::v1::TradeEvent> ReadOne(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))
    {
        std::unique_lock<std::mutex> lock(mMutex);

        const bool hasEvent = mCv.wait_for(lock, timeout, [this] { return !mEvents.empty() || mFinished; });

        if (!hasEvent || mEvents.empty())
        {
            return std::nullopt;
        }

        auto event = std::move(mEvents.front());
        mEvents.pop_front();
        return event;
    }

    void Finish()
    {
        if (mClosed.exchange(true))
            return;

        if (mStream)
        {
            mStream->WritesDone();
        }

        mContext.TryCancel();

        if (mReader.joinable())
        {
            mReader.request_stop();
            mReader.join();
        }

        if (mStream)
        {
            grpc::Status status = mStream->Finish();
            if (!status.ok() && status.error_code() != grpc::StatusCode::CANCELLED)
            {
                ADD_FAILURE() << "gRPC Stream failed with code " << status.error_code() << ": "
                              << status.error_message();
            }
        }
    }

private:
    void ReadLoop(std::stop_token st)
    {
        while (!st.stop_requested())
        {
            market::v1::TradeEvent event;

            if (!mStream->Read(&event))
            {
                break;
            }

            {
                std::lock_guard<std::mutex> lock(mMutex);
                mEvents.push_back(std::move(event));
            }

            mCv.notify_one();
        }

        {
            std::lock_guard<std::mutex> lock(mMutex);
            mFinished = true;
        }

        mCv.notify_all();
    }

private:
    grpc::ClientContext mContext;

    std::unique_ptr<market::v1::MarketService::Stub> mStub;

    std::unique_ptr<grpc::ClientReaderWriter<market::v1::TradeRequest, market::v1::TradeEvent>> mStream;

    std::jthread mReader;

    std::mutex mMutex;
    std::condition_variable mCv;
    std::deque<market::v1::TradeEvent> mEvents;

    bool mFinished{false};
    std::atomic<bool> mClosed{false};
};
