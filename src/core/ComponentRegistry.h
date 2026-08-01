#pragma once
#include <cstdint>

using ComponentTypeID = uint32_t;
using ComponentMask   = uint64_t;

inline ComponentTypeID g_next_component_id = 0;

template<typename T>
ComponentTypeID get_component_type_id() {
    static ComponentTypeID id = g_next_component_id++;
    return id;
}

template<typename T>
ComponentMask get_component_mask() {
    return 1ULL << get_component_type_id<T>();
}

template<typename... Args>
ComponentMask make_mask() {
    return (get_component_mask<Args>() | ...);
}