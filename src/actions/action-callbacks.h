#ifndef LINEA_ACTION_CALLBACKS_H
#define LINEA_ACTION_CALLBACKS_H

#include "action-meta.h"

// Callback type aliases are defined in action-meta.h
// This header exists for backward compatibility and explicit inclusion

namespace Inkscape {
namespace Actions {

// Helper to cast lambdas to the appropriate callback type
template<typename Func>
constexpr auto makeAppCallback(Func&& f) {
    return AppCallback(f);
}

template<typename Func>
constexpr auto makeWindowCallback(Func&& f) {
    return WindowCallback(f);
}

template<typename Func>
constexpr auto makeDocCallback(Func&& f) {
    return DocCallback(f);
}

template<typename Func>
constexpr auto makeSelCallback(Func&& f) {
    return SelCallback(f);
}

template<typename Func>
constexpr auto makeBoolCallback(Func&& f) {
    return BoolCallback(f);
}

template<typename Func>
constexpr auto makeStringParamCallback(Func&& f) {
    return StringParamCallback(f);
}

template<typename Func>
constexpr auto makeIntParamCallback(Func&& f) {
    return IntParamCallback(f);
}

template<typename Func>
constexpr auto makeDoubleParamCallback(Func&& f) {
    return DoubleParamCallback(f);
}

} // namespace Actions
} // namespace Inkscape

#endif
