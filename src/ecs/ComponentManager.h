#pragma once
#include "ECSConfig.h"

namespace ECS {

class IComponentArray {
public:
    virtual ~IComponentArray() = default;
    virtual void EntityDestroyed(EntityID entity) = 0;
};

// 型ごとの密配列。entity index -> dense index の疎配列で O(1) アクセス。
// (rimrim 版は unordered_map 2枚+範囲外アクセス無検査だったため置き換え)
template <typename T>
class ComponentArray : public IComponentArray {
public:
    ComponentArray() : sparse(MAX_ENTITIES, kNone) {}

    bool Has(EntityID e) const { return sparse[e] != kNone; }

    void InsertData(EntityID e, T component) {
        const EntityID idx = e;
        assert(sparse[idx] == kNone && "Component added twice to the same entity.");
        sparse[idx] = static_cast<std::uint32_t>(dense.size());
        dense.push_back(std::move(component));
        owners.push_back(e);
    }

    void RemoveData(EntityID e) {
        const EntityID idx = e;
        const std::uint32_t d = sparse[idx];
        if (d == kNone) return;
        const std::uint32_t last = static_cast<std::uint32_t>(dense.size() - 1);
        if (d != last) {
            dense[d] = std::move(dense[last]);
            owners[d] = owners[last];
            sparse[owners[d]] = d;
        }
        dense.pop_back();
        owners.pop_back();
        sparse[idx] = kNone;
    }

    T& GetData(EntityID e) {
        const std::uint32_t d = sparse[e];
        assert(d != kNone && "Entity does not have this component.");
        return dense[d];
    }

    void EntityDestroyed(EntityID e) override { RemoveData(e); }

private:
    static constexpr std::uint32_t kNone = 0xFFFFFFFFu;
    std::vector<T> dense;
    std::vector<EntityID> owners;
    std::vector<std::uint32_t> sparse;
};

class ComponentManager {
public:
    template <typename T>
    void RegisterComponent() {
        arrays[GetComponentTypeID<T>()] = std::make_shared<ComponentArray<T>>();
    }
    template <typename T>
    void AddComponent(EntityID e, T c) { Array<T>().InsertData(e, std::move(c)); }
    template <typename T>
    void RemoveComponent(EntityID e) { Array<T>().RemoveData(e); }
    template <typename T>
    T& GetComponent(EntityID e) { return Array<T>().GetData(e); }
    template <typename T>
    bool HasComponent(EntityID e) { return Array<T>().Has(e); }

    void EntityDestroyed(EntityID e) {
        for (auto& p : arrays) p.second->EntityDestroyed(e);
    }

private:
    template <typename T>
    ComponentArray<T>& Array() {
        auto it = arrays.find(GetComponentTypeID<T>());
        assert(it != arrays.end() && "Component type not registered.");
        return *static_cast<ComponentArray<T>*>(it->second.get());
    }
    std::unordered_map<ComponentTypeID, std::shared_ptr<IComponentArray>> arrays;
};

}  // namespace ECS
