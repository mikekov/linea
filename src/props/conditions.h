// SPDX-License-Identifier: GPL-2.0-or-later
//
// Conditions: compile-time-composable predicates over SelectionState.
//
// Each condition is a small struct with operator()(const SelectionState&) -> bool.
// Combinators (&&, ||, !) produce composite types — the entire expression is
// one type the compiler can see through and inline. Type erasure happens once
// at the Binder boundary (visibleWhen/enableWhen store a single std::function
// whose body is the inlined expression tree).
//
//   b.visibleWhen(_ui->resetOpacity,
//       Cond::hasSelection && Cond::differsFrom(Props::opacity, 1.0));
//
// Count-based primitives answer "what is selected?"; property-based primitives
// answer "what values does the selection have?". Property primitives are safe
// on empty selections: mixed_property is Unset (no items visited), so
// differsFrom/uniformEquals return false and present returns false.
//
// differsFrom is the primary primitive for reset/enable rules — it returns
// true when the value is NOT uniformly equal to the given constant, covering
// both single-with-a-different-value and mixed. uniformEquals (is_single &&
// value == X) is rarely the right choice for enablement because it excludes
// mixed mode, where editing is most useful.

#ifndef LINEA_PROPS_CONDITIONS_H
#define LINEA_PROPS_CONDITIONS_H

#include <concepts>
#include <utility>

#include "props/property-def.h"

namespace Linea::Props::Cond {

// Anything callable with (const SelectionState&) -> bool is a condition.
// This covers the structs below, raw lambdas, and std::function.
template <typename T>
concept Condition = requires(const T& t, const SelectionState& s) {
    { t(s) } -> std::same_as<bool>;
};

// --- Count / composition primitives (empty structs, zero storage) ----------

// Selection is non-empty. Hack: exclude page/svgs from the count, as almost nothing can handle it.
struct HasSelection {
    bool operator()(const SelectionState& s) const { return !s.empty() && !s.pageSelection(); }
};
inline constexpr HasSelection hasSelection{};

struct HasPageSelection {
    bool operator()(const SelectionState& s) const { return s.pageSelection(); }
};
inline constexpr HasPageSelection hasPageSelection{};

// Exactly one item selected. Hack: exclude page/svgs from the count, as almost nothing can handle it.
struct HasSingleSelection {
    bool operator()(const SelectionState& s) const { return s.element.count.items == 1 && !s.pageSelection(); }
};
inline constexpr HasSingleSelection singleSelection{};

// All selected items are of one type (panel visibility).
//   Cond::allOf<&Counts::rectangles>  -> "only rectangles selected"
template <int Counts::* Member>
struct AllOf {
    bool operator()(const SelectionState& s) const {
        const auto& c = s.element.count;
        return c.*Member > 0 && c.*Member == c.items;
    }
};
template <int Counts::* Member>
inline constexpr AllOf<Member> allOf{};

// Exactly one selected item is one of the given types.
//   Cond::single<&Counts::pages, &Counts::svgs>  -> "one page or one svg root selected"
template <int Counts::*... Members>
    requires (sizeof...(Members) >= 2)
struct Single {
    bool operator()(const SelectionState& s) const {
        const auto& c = s.element.count;
        int matches = 0;
        ((matches += c.*Members), ...);
        return matches == 1;
    }
};
template <int Counts::*... Members>
    requires (sizeof...(Members) >= 2)
inline constexpr Single<Members...> single{};

// At least one item of a type.
//   Cond::hasType<&Counts::images>  -> "has an image"
template <int Counts::* Member>
struct HasType {
    bool operator()(const SelectionState& s) const {
        return s.element.count.*Member > 0;
    }
};
template <int Counts::* Member>
inline constexpr HasType<Member> hasType{};

// All selected items belong to one of the given types (panel visibility).
//   Cond::allOfSum<&Counts::stars, &Counts::polygons>  -> "only stars or polygons"
template <int Counts::*... Members>
    requires (sizeof...(Members) >= 2)
struct AllOfSum {
    bool operator()(const SelectionState& s) const {
        const auto& c = s.element.count;
        int sum = 0;
        ((sum += c.*Members), ...);
        return sum > 0 && sum == c.items;
    }
};
template <int Counts::*... Members>
    requires (sizeof...(Members) >= 2)
inline constexpr AllOfSum<Members...> allOfSum{};

// Non-empty selection with at least one item that is NOT of the given type.
//   Cond::hasOtherThan<&Counts::images>  -> "has at least one non-image"
template <int Counts::* Member>
struct HasOtherThan {
    bool operator()(const SelectionState& s) const {
        const auto& c = s.element.count;
        return c.items > 0 && !s.pageSelection() && c.*Member < c.items;
    }
};
template <int Counts::* Member>
inline constexpr HasOtherThan<Member> hasOtherThan{};

// Text tool has no active span subselection (object-level edits are valid).
struct NoTextSubselection {
    bool operator()(const SelectionState& s) const {
        return !s.element.count.has_text_subselection;
    }
};
inline constexpr NoTextSubselection noTextSubselection{};

// Text tool is the current tool.
struct TextToolActive {
    bool operator()(const SelectionState& s) const {
        return s.element.count.text_tool_active;
    }
};
inline constexpr TextToolActive textToolActive{};

// Current tool is the given tool (panel visibility).
//   Cond::toolIs<TOOLS_TEXT>  -> "text tool is active"
// For multiple tools, combine with ||:
//   Cond::toolIs<TOOLS_SHAPES_RECT> || Cond::toolIs<TOOLS_SHAPES_ELLIPSE>
template <tools_enum Tool>
struct ToolIs {
    bool operator()(const SelectionState& s) const {
        return s.element.count.activeTool == Tool;
    }
};
template <tools_enum Tool>
inline constexpr ToolIs<Tool> toolIs{};

// --- Property primitives (store PropertyDef pointer + optional value) ------

// Property has at least one value (not unset). True for single AND mixed.
template <typename T>
struct Present {
    const PropertyDef<T>* def;
    bool operator()(const SelectionState& s) const {
        return !def->get(s).is_unset();
    }
};

// All items agree (is_single). Rarely the right primitive for enablement.
template <typename T>
struct Uniform {
    const PropertyDef<T>* def;
    bool operator()(const SelectionState& s) const {
        return def->get(s).is_single();
    }
};

// Items disagree (is_mixed).
template <typename T>
struct Mixed {
    const PropertyDef<T>* def;
    bool operator()(const SelectionState& s) const {
        return def->get(s).is_mixed();
    }
};

// All items agree AND the agreed value equals `value`.
// Returns false on empty/unset/mixed selections.
template <typename T, typename U = T>
struct UniformEquals {
    const PropertyDef<T>* def;
    U value;
    bool operator()(const SelectionState& s) const {
        const auto& p = def->get(s);
        return p.is_single() && p.value() == value;
    }
};

// NOT uniformly equal to `value`: either mixed, or single-with-a-different-value.
// Returns false on empty/unset selections.
//   Cond::differsFrom(Props::opacity, 1.0) -> "opacity is not 100% everywhere"
template <typename T, typename U = T>
struct DiffersFrom {
    const PropertyDef<T>* def;
    U value;
    bool operator()(const SelectionState& s) const {
        const auto& p = def->get(s);
        if (p.is_unset()) return false;
        if (p.is_mixed()) return true;
        return !(p.value() == value);
    }
};

// --- Combinators (aggregate by value, no allocation) -----------------------

template <Condition A, Condition B>
struct And {
    A a; B b;
    bool operator()(const SelectionState& s) const { return a(s) && b(s); }
};

template <Condition A, Condition B>
struct Or {
    A a; B b;
    bool operator()(const SelectionState& s) const { return a(s) || b(s); }
};

template <Condition A>
struct Not {
    A a;
    bool operator()(const SelectionState& s) const { return !a(s); }
};

// --- Operators: build composite types, not runtime trees -------------------

template <Condition A, Condition B>
constexpr And<std::decay_t<A>, std::decay_t<B>> operator&&(A&& a, B&& b) {
    return {std::forward<A>(a), std::forward<B>(b)};
}

template <Condition A, Condition B>
constexpr Or<std::decay_t<A>, std::decay_t<B>> operator||(A&& a, B&& b) {
    return {std::forward<A>(a), std::forward<B>(b)};
}

template <Condition A>
constexpr Not<std::decay_t<A>> operator!(A&& a) {
    return {std::forward<A>(a)};
}

// Qualified negation for use when ADL can't find operator! (e.g. on lambdas
// whose type is not in the Cond namespace): Cond::not_(myLambda).
template <Condition A>
constexpr Not<std::decay_t<A>> not_(A&& a) {
    return {std::forward<A>(a)};
}

// --- Factories for property primitives -------------------------------------

template <typename T>
constexpr Present<T> present(const PropertyDef<T>& def) {
    return {&def};
}

template <typename T>
constexpr Uniform<T> uniform(const PropertyDef<T>& def) {
    return {&def};
}

template <typename T>
constexpr Mixed<T> mixed(const PropertyDef<T>& def) {
    return {&def};
}

template <typename T, typename U>
constexpr UniformEquals<T, U> uniformEquals(const PropertyDef<T>& def, U value) {
    return {&def, std::move(value)};
}

template <typename T, typename U>
constexpr DiffersFrom<T, U> differsFrom(const PropertyDef<T>& def, U value) {
    return {&def, std::move(value)};
}

} // namespace Linea::Props::Cond

#endif // LINEA_PROPS_CONDITIONS_H
