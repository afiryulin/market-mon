#pragma once

#include <memory>
#include <vector>

#include "ICallDataBase.h"

template <typename Service> class CallDataFactory
{
public:
    template <IsCallData<Service>... CallDataTypes>
    static void SeedQueues(Service *service,
                           const std::vector<std::unique_ptr<grpc::ServerCompletionQueue>> &queues)
    {
        for (auto &cq : queues)
        {
            (..., new CallDataTypes(service, cq.get()));
        }
    }
};
