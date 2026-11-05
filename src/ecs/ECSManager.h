#pragma once
#include "ComponentManager.h"
#include "ECSConfig.h"
#include "EntityManager.h"
#include "EventBus.h"
#include "QueueManager.h"
#include "SystemManager.h"

namespace ECS {

// rimrim の ECSManager から SFML/SystemContext 依存を外した版。
class ECSManager {
public:
    ECSManager() = default;

    EntityID CreateEntity() { return entities.CreateEntity(); }
    // 破棄の予約(安全): Flush で実際に破棄する。多重予約は無害。System ループ中はこちらを使う。
    void DestroyLater(EntityID e) {
        if (!entities.IsAlive(e)) return;
        pendingDestroy.push_back(e);
    }
    bool IsAlive(EntityID e) const { return entities.IsAlive(e); }
    std::uint32_t LivingCount() const { return entities.LivingCount(); }

    template <typename T>
    void RegisterComponent() { components.RegisterComponent<T>(); }

    template <typename T>
    void AddComponent(EntityID e, T c) {
        assert(entities.IsAlive(e));
        components.AddComponent<T>(e, std::move(c));
        auto sig = entities.GetSignature(e);
        sig.set(GetComponentTypeID<T>(), true);
        entities.SetSignature(e, sig);
        systems.EntitySignatureChanged(e, sig);
    }
    template <typename T>
    void RemoveComponent(EntityID e) {
        components.RemoveComponent<T>(e);
        auto sig = entities.GetSignature(e);
        sig.set(GetComponentTypeID<T>(), false);
        entities.SetSignature(e, sig);
        systems.EntitySignatureChanged(e, sig);
    }
    template <typename T>
    T& GetComponent(EntityID e) {
        assert(entities.IsAlive(e));
        return components.GetComponent<T>(e);
    }
    template <typename T>
    bool HasComponent(EntityID e) {
        return entities.IsAlive(e) && components.HasComponent<T>(e);
    }
    // 生存していて、かつ持っていれば非 null。
    template <typename T>
    T* TryGet(EntityID e) { return HasComponent<T>(e) ? &components.GetComponent<T>(e) : nullptr; }

    template <typename... Ts>
    static Signature MakeSignature() {
        Signature s;
        (s.set(GetComponentTypeID<Ts>(), true), ...);
        return s;
    }

    template <typename T, typename... Args>
    std::shared_ptr<T> RegisterSystem(SystemManager::PhaseID phase, Signature sig, Args&&... args) {
        return systems.RegisterSystem<T>(phase, sig, std::forward<Args>(args)...);
    }

    // フェーズ番号の昇順に実行する。同一フェーズ内の順序は不定(下のシャッフルで検証できる)。
    void Update(float dt) {
        for (auto& ph : systems.phases) {
            if (!shuffle) {
                for (auto& s : ph.second) s->Update(*this, dt);
                continue;
            }
            std::vector<std::shared_ptr<System>> order = ph.second;   // フェーズ内を毎フレーム並べ替える
            for (size_t i = order.size(); i > 1; --i) {
                shuffleState ^= shuffleState << 13; shuffleState ^= shuffleState >> 17; shuffleState ^= shuffleState << 5;
                std::swap(order[i - 1], order[shuffleState % i]);
            }
            for (auto& s : order) s->Update(*this, dt);
        }
    }
    // テスト用: フェーズ内の順序依存がないことを確かめるため、フェーズ内の実行順を毎フレーム乱す。
    void EnableIntraPhaseShuffle(std::uint32_t seed) { shuffle = true; shuffleState = seed ? seed : 1; }
    // フレーム末尾: コマンドキュー実行 → 破棄予約を反映。
    void Flush() {
        queue.Execute();
        for (size_t i = 0; i < pendingDestroy.size(); ++i) {
            EntityID e = pendingDestroy[i];
            if (!entities.IsAlive(e)) continue;
            components.EntityDestroyed(e);
            systems.EntityDestroyed(e);
            entities.DestroyEntity(e);
        }
        pendingDestroy.clear();
    }

    QueueManager& Queue() { return queue; }
    EventBus& Events() { return events; }

private:
    ComponentManager components;
    EntityManager entities;
    SystemManager systems;
    QueueManager queue;
    EventBus events;
    std::vector<EntityID> pendingDestroy;
    bool shuffle = false;
    std::uint32_t shuffleState = 1;
};

}  // namespace ECS
