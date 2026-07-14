#pragma once
#include <unordered_map>
#include <typeindex>
#include <memory>
#include "capability.h"
#include <utility>

class CapabilityHost {
public:
    CapabilityHost() = default;
    virtual ~CapabilityHost() = default;

    CapabilityHost(const CapabilityHost&) = delete;
    CapabilityHost& operator=(const CapabilityHost&) = delete;

    CapabilityHost(CapabilityHost&&) = default;
    CapabilityHost& operator=(CapabilityHost&&) = default;

    template<typename T>
    bool has() const {
        return caps_.count(typeid(T)) > 0;
    }

    template<typename T>
    T* get() {
        auto it = caps_.find(typeid(T));
        return it == caps_.end()
            ? nullptr
            : static_cast<T*>(it->second.get());
    }
    template<typename T, typename... Args>
    T& emplace(Args&&... args) {

        //static_assert(
        //    std::is_constructible_v<T, Args...>,
        //    "Capability type T is not constructible with given arguments"
        //    );

        auto cap = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *cap;
        caps_[typeid(T)] = std::move(cap);
        return ref;
    }

    template<typename T>
    void remove() {
        caps_.erase(typeid(T));
    }

private:
    std::unordered_map<std::type_index, std::unique_ptr<Capability>> caps_;
};
