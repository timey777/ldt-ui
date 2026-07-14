#pragma once
#include <typeindex>
#include <unordered_map>
#include <vector>

class AbstractEngine;
class ServiceRegistry {
public:
    void registerService(AbstractEngine* service);

    template<typename T>
    T* get() const {
        auto it = services_.find(std::type_index(typeid(T)));
        if (it == services_.end()) return nullptr;
        return reinterpret_cast<T*>(it->second);
    }

    template<typename T>
    bool has() const {
        return services_.count(std::type_index(typeid(T))) != 0;
    }
    template<typename Fn>
    void forEach(Fn&& fn) {
        for (auto& services_ : pipeline_) {
            fn(services_);
        }
    }
    template<typename Fn>
    void forEach(const std::vector<std::type_index>& pipelineOrder, Fn&& fn) {
        for (auto& indx : pipelineOrder) {
            fn(services_[indx]);
        }
    }
    void order(const std::vector<std::type_index>& pipelineOrder);
private:
    std::unordered_map<std::type_index, AbstractEngine*> services_;
    std::vector<AbstractEngine*> pipeline_;
};
