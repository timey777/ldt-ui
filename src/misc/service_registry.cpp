#include "service_registry.h"
#include "engine/engine.h"
#include <algorithm>
#include <climits>
void ServiceRegistry::registerService(AbstractEngine *service)
{
    services_[service->type()] = service;
    pipeline_.push_back(service);
}

void ServiceRegistry::order(const std::vector<std::type_index>& pipelineOrder)
{
    std::unordered_map<std::type_index, int> order;
    for (int i = 0; i < pipelineOrder.size(); ++i)
        order[pipelineOrder[i]] = i;

    std::stable_sort(pipeline_.begin(), pipeline_.end(),
        [&](AbstractEngine* a, AbstractEngine* b) {
            auto ia = order.count(typeid(*a)) ? order[typeid(*a)] : INT_MAX;
            auto ib = order.count(typeid(*b)) ? order[typeid(*b)] : INT_MAX;
            return ia < ib;
        }
    );
}
