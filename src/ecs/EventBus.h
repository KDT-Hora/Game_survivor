#pragma once
// rimrim の EventBus を流用。
#include <functional>
#include <list>

#include "ECSConfig.h"

namespace ECS {

struct Event {
    virtual ~Event() = default;
};

class EventBus {
public:
    template <typename TEvent>
    using Callback = std::function<void(const TEvent&)>;

    template <typename TEvent>
    void Subscribe(Callback<TEvent> callback) {
        subscribers[std::type_index(typeid(TEvent))].push_back(
            [callback](const Event& e) { callback(static_cast<const TEvent&>(e)); });
    }

    template <typename TEvent, typename... Args>
    void Publish(Args&&... args) {
        TEvent event(std::forward<Args>(args)...);
        auto it = subscribers.find(std::type_index(typeid(TEvent)));
        if (it == subscribers.end()) return;
        for (auto& cb : it->second) cb(event);
    }

private:
    using Handler = std::function<void(const Event&)>;
    std::unordered_map<std::type_index, std::list<Handler>> subscribers;
};

}  // namespace ECS
