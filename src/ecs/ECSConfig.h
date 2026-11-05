#pragma once
// rimrim の ECSConfig.h を流用(UTF-8 化、上限値のみ変更)。
#include <bitset>
#include <cassert>
#include <cstdint>
#include <memory>
#include <queue>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace ECS {

using EntityID = std::uint32_t;
using ComponentTypeID = std::uint8_t;

constexpr EntityID MAX_ENTITIES = 20000;
constexpr ComponentTypeID MAX_COMPONENTS = 64;
constexpr EntityID NULL_ENTITY = 0xFFFFFFFFu;

using Signature = std::bitset<MAX_COMPONENTS>;

inline ComponentTypeID GetUniqueComponentID() {
    static ComponentTypeID lastID = 0;
    assert(lastID < MAX_COMPONENTS && "Too many component types.");
    return lastID++;
}

template <typename T>
inline ComponentTypeID GetComponentTypeID() noexcept {
    static const ComponentTypeID typeID = GetUniqueComponentID();
    return typeID;
}

}  // namespace ECS
