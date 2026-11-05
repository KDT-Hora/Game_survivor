#pragma once
#include <array>

#include "ECSConfig.h"

namespace ECS {

// Entity の ID の発行と回収。使い終わった ID は待ち行列の最後に戻し、すぐには再利用しない
// (破棄した Entity の古い ID を持ち続けても、別の Entity に当たりにくくするため)。
class EntityManager {
public:
    EntityManager() {
        for (EntityID i = 0; i < MAX_ENTITIES; ++i) {
            available.push(i);
            signatures[i].reset();
        }
    }

    EntityID CreateEntity() {
        assert(living < MAX_ENTITIES && "Too many entities in existence.");
        EntityID id = available.front();
        available.pop();
        alive[id] = true;
        ++living;
        return id;
    }

    // 破棄して ID を戻す。すでに破棄済み・無効な ID なら何もしない。
    void DestroyEntity(EntityID entity) {
        if (!IsAlive(entity)) return;
        signatures[entity].reset();
        alive[entity] = false;
        available.push(entity);
        --living;
    }

    bool IsAlive(EntityID entity) const { return entity < MAX_ENTITIES && alive[entity]; }

    void SetSignature(EntityID e, Signature s) { signatures[e] = s; }
    Signature GetSignature(EntityID e) const { return signatures[e]; }
    std::uint32_t LivingCount() const { return living; }

private:
    std::queue<EntityID> available;
    std::array<Signature, MAX_ENTITIES> signatures;
    std::array<bool, MAX_ENTITIES> alive{};
    std::uint32_t living = 0;
};

}  // namespace ECS
