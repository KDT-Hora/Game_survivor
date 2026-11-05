#pragma once
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "ECSConfig.h"

namespace ECS {

class ECSManager;

class System {
public:
    std::set<EntityID> mEntities;  // シグネチャに合致した Entity(ID 昇順で決定的)
    Signature required;
    virtual ~System() = default;
    virtual void Update(ECSManager& ecs, float dt) = 0;
};

// System は「フェーズ」に所属する。フェーズ間は番号順に実行し、同一フェーズ内の実行順は規定しない
// (どの順で動いても結果が変わらない System だけを同じフェーズに入れる)。
// rimrim 版は hash_code を uint8 に切り詰めた unordered_map で、衝突で System が消え、順序も不定だった。
class SystemManager {
public:
    using PhaseID = int;

    template <typename T, typename... Args>
    std::shared_ptr<T> RegisterSystem(PhaseID phase, Signature signature, Args&&... args) {
        auto s = std::make_shared<T>(std::forward<Args>(args)...);
        s->required = signature;
        all.push_back(s);
        phases[phase].push_back(s);
        return s;
    }

    void EntityDestroyed(EntityID e) {
        for (auto& s : all) s->mEntities.erase(e);
    }

    void EntitySignatureChanged(EntityID e, Signature sig) {
        for (auto& s : all) {
            if ((sig & s->required) == s->required)
                s->mEntities.insert(e);
            else
                s->mEntities.erase(e);
        }
    }

    std::vector<std::shared_ptr<System>> all;
    std::map<PhaseID, std::vector<std::shared_ptr<System>>> phases;  // キー昇順 = フェーズ順
};

}  // namespace ECS
