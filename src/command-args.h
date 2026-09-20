// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Declarative argument specs for console commands.
 *
 * Spec factories return Boost.Parser parsers that record named values
 * into a ParsedArgs carried in the parser's globals. Specs compose with
 * the usual operators: `-p` (optional), `p1 >> p2` (sequence),
 * `p1 | p2` (alternative), `+p` (repetition), and `>` (expectation point
 * — reports "expected ..." instead of a bare failure). spec() type-erases
 * a composed parser for storage in command tables.
 */

#ifndef LINEA_COMMAND_ARGS_H
#define LINEA_COMMAND_ARGS_H

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <boost/parser/parser.hpp>

#include "colors/color.h"
#include "id-clash.h"

namespace Linea {

// Largest sane SVG-plane coordinate — same bound the canvas tools use
// (see in_svg_plane, src/ui/tools/pencil-tool.cpp).
inline constexpr double kMaxCoord = 1e18;

/// A value captured by an argument spec.
using ArgValue = std::variant<std::string, std::vector<double>>;

/// Named values produced by a command's argument spec, plus the raw
/// remainder for spec-less commands.
class ParsedArgs {
public:
    std::vector<std::string> tokens; ///< tokens after the command name (quotes decoded)
    std::string rest;                ///< raw text after the command name

    bool has(std::string_view name) const;
    double num(std::string_view name, double dflt = 0.0) const;
    const std::string& str(std::string_view name) const;
    std::vector<double> nums(std::string_view name) const;

    // called by spec semantic actions
    void put(std::string name, std::string value);
    void putNum(const std::string& name, double value); // repeated captures accumulate

private:
    std::map<std::string, ArgValue, std::less<>> _values;
};

/// Type-erased spec: parse `input`, fill `args`, set `error` on failure.
using ArgSpec = std::function<bool(std::string_view input, ParsedArgs& args, std::string& error)>;

namespace args {

namespace bp = boost::parser;

namespace detail {

inline std::string toString(const std::string& s) { return s; }
inline std::string toString(std::string_view s) { return std::string(s); }

template<typename T>
std::string toString(const T& v) {
    return std::visit([](const auto& x) { return toString(x); }, v);
}

// Reports `message` at the current position and fails this parser.
template<typename Context>
void fail(Context const& ctx, const std::string& message) {
    bp::_report_error(ctx, message);
    bp::_pass(ctx) = false;
}

/// Spaceless arithmetic expression: number literals, unary + -, binary
/// + - * / and parentheses. The whole expression is lexeme'd, so
/// `1 -2` stays two arguments rather than 1 + (-2).
bp::rule<struct arith_tag, double> const arith = "arithmetic expression";
bp::rule<struct arith_term_tag, double> const arith_term = "term";
bp::rule<struct arith_fact_tag, double> const arith_fact = "number";

// fact = [sign] (number | '(' expr ')') — the sign is an optional element
// rather than a recursive alternative; actions attached to a rule's
// recursive self-invocation see a none attribute in this bp version.
auto const arith_fact_def =
    (-bp::char_("+-") >>
     (bp::double_ | '(' >> arith >> ')'))[([](auto& ctx) {
        const auto& [sign, v] = bp::_attr(ctx);
        bp::_val(ctx) = (sign && *sign == '-') ? -v : v;
    })];

auto const arith_term_def =
    (arith_fact >> *(bp::char_("*/") >> arith_fact))[([](auto& ctx) {
        double v = std::get<0>(bp::_attr(ctx));
        for (auto const& [op, rhs] : std::get<1>(bp::_attr(ctx))) {
            v = (op == '*') ? v * rhs : v / rhs;
        }
        bp::_val(ctx) = v;
    })];

auto const arith_def = bp::lexeme[
    (arith_term >> *(bp::char_("+-") >> arith_term))[([](auto& ctx) {
        double v = std::get<0>(bp::_attr(ctx));
        for (auto const& [op, rhs] : std::get<1>(bp::_attr(ctx))) {
            v = (op == '+') ? v + rhs : v - rhs;
        }
        bp::_val(ctx) = v;
    })]];

BOOST_PARSER_DEFINE_RULES(arith, arith_term, arith_fact);

/// Quoted string: "..." or '...', backslash escapes; produces a decoded
/// std::string. The single definition of quoting for this layer — the
/// command-line splitter, tokenizer and string specs all use it.
inline auto const quoted_str = bp::quoted_string('"') | bp::quoted_string('\'');

} // namespace detail

/// Arithmetic expression, finite, |v| <= kMaxCoord; recorded under `name`.
inline auto num(std::string name) {
    return detail::arith[([name = std::move(name)](auto& ctx) {
        const double v = bp::_attr(ctx);
        if (!std::isfinite(v) || std::abs(v) > kMaxCoord) {
            detail::fail(ctx, name + ": not a valid coordinate");
            return;
        }
        bp::_globals(ctx).putNum(name, v);
    })];
}

/// Integer, |v| <= kMaxCoord; recorded under `name`.
inline auto integer(std::string name) {
    return bp::int_[([name = std::move(name)](auto& ctx) {
        bp::_globals(ctx).putNum(name, static_cast<double>(bp::_attr(ctx)));
    })];
}

/// Positive arithmetic expression (radius, width, height); recorded under `name`.
inline auto pos(std::string name) {
    return detail::arith[([name = std::move(name)](auto& ctx) {
        const double v = bp::_attr(ctx);
        if (!std::isfinite(v) || v <= 0 || v > kMaxCoord) {
            detail::fail(ctx, name + " must be a positive number");
            return;
        }
        bp::_globals(ctx).putNum(name, v);
    })];
}

/// One of `words` as a bare token; the match is recorded under `name`.
inline auto kw(std::string name, std::initializer_list<const char*> words) {
    const std::vector<std::string> list(words.begin(), words.end());
    std::string expect = "one of ";
    for (size_t i = 0; i < list.size(); ++i) {
        if (i) {
            expect += '|';
        }
        expect += list[i];
    }
    return bp::string_view[bp::lexeme[+(bp::char_ - bp::char_(" \t\"'"))]]
        [([name = std::move(name), list, expect](auto& ctx) {
            const std::string_view v = bp::_attr(ctx);
            if (std::find(list.begin(), list.end(), v) == list.end()) {
                detail::fail(ctx, expect);
                return;
            }
            bp::_globals(ctx).put(name, std::string(v));
        })];
}

inline bool isSpace(char c) {
    return std::isspace(static_cast<unsigned char>(c));
}

/// `text` with leading whitespace removed.
inline std::string_view ltrim(std::string_view text) {
    const auto b = text.find_first_not_of(" \t\n\r");
    return b == std::string_view::npos ? std::string_view() : text.substr(b);
}

/// `text` with surrounding whitespace removed.
inline std::string_view trim(std::string_view text) {
    text = ltrim(text);
    const auto e = text.find_last_not_of(" \t\n\r");
    return e == std::string_view::npos ? std::string_view() : text.substr(0, e + 1);
}

/// The first whitespace-delimited word of `text` and the remainder
/// (still carrying its leading whitespace), after ltrim.
inline std::pair<std::string_view, std::string_view> splitFirst(std::string_view text) {
    text = ltrim(text);
    const size_t e = text.find_first_of(" \t\n\r");
    if (e == std::string_view::npos) {
        return {text, {}};
    }
    return {text.substr(0, e), text.substr(e)};
}

/// Quoted "..." string with backslash escapes; recorded under `name`.
inline auto qstr(std::string name) {
    return detail::quoted_str[([name = std::move(name)](auto& ctx) {
        bp::_globals(ctx).put(name, bp::_attr(ctx));
    })];
}

/// Quoted, validated SVG id (see is_object_id_valid); named "id" by default.
inline auto id(std::string name = "id") {
    return detail::quoted_str[([name = std::move(name)](auto& ctx) {
        std::string v = bp::_attr(ctx);
        auto [valid, message] = is_object_id_valid(v);
        if (!valid) {
            detail::fail(ctx, "id: " + message.raw());
            return;
        }
        bp::_globals(ctx).put(name, std::move(v));
    })];
}

/// Paint value: none | inherit | N% | any CSS color (validated via
/// Inkscape::Colors::Color::parse — #hex, names, rgb()/hsl()/etc.).
/// Quote the value if it contains spaces ("rgb(1, 2, 3)").
inline auto paint(std::string name) {
    return (detail::quoted_str |
            bp::string_view[bp::lexeme[+(bp::char_ - bp::char_(" \t"))]])
        [([name = std::move(name)](auto& ctx) {
            std::string v = detail::toString(bp::_attr(ctx));
            bool ok = v == "none" || v == "inherit";
            if (!ok && v.ends_with('%')) {
                char* end = nullptr;
                const double p = std::strtod(v.c_str(), &end);
                ok = end == v.c_str() + v.size() - 1 && 0.0 <= p && p <= 100.0;
            }
            if (!ok) {
                ok = Inkscape::Colors::Color::parse(v).has_value();
            }
            if (!ok) {
                detail::fail(ctx, name + ": expected none|inherit|NN%|css-color");
                return;
            }
            bp::_globals(ctx).put(name, std::move(v));
        })];
}

/// Stroke width: 'hairline' or a non-negative expression (0 == hairline).
/// 'hairline' is recorded as the literal string, numbers as doubles.
inline auto strokeWidth(std::string name) {
    return bp::string("hairline")[([name](auto& ctx) {
               bp::_globals(ctx).put(name, detail::toString(bp::_attr(ctx)));
           })]
           | detail::arith[([name = std::move(name)](auto& ctx) {
                 const double d = bp::_attr(ctx);
                 if (!std::isfinite(d) || d < 0.0 || d > kMaxCoord) {
                     detail::fail(ctx, name + ": expected hairline|NN>=0");
                     return;
                 }
                 bp::_globals(ctx).putNum(name, d);
             })];
}

/// Opacity: expression yielding 0..1 or 0%..100%; recorded under `name`
/// as a fraction 0..1.
inline auto opacity(std::string name) {
    return bp::lexeme[detail::arith >> -bp::char_('%')]
        [([name = std::move(name)](auto& ctx) {
            const auto& t = bp::_attr(ctx);
            const double d = std::get<0>(t) * (std::get<1>(t) ? 0.01 : 1.0);
            if (!std::isfinite(d) || d < 0.0 || d > 1.0) {
                detail::fail(ctx, name + ": expected 0..1|NN%");
                return;
            }
            bp::_globals(ctx).putNum(name, d);
        })];
}

/// Bare token or quoted string; recorded under `name`.
inline auto str(std::string name) {
    return (detail::quoted_str |
            bp::string_view[bp::lexeme[+(bp::char_ - bp::char_(" \t"))]])
        [([name = std::move(name)](auto& ctx) {
            bp::_globals(ctx).put(name, detail::toString(bp::_attr(ctx)));
        })];
}

/// File path argument (quoted or bare token); recorded under `name`.
inline auto filearg(std::string name) { return str(std::move(name)); }

/// The remainder of the line verbatim, trimmed; recorded under `name`.
inline auto rest(std::string name) {
    return bp::string_view[bp::lexeme[*bp::char_]]
        [([name = std::move(name)](auto& ctx) {
            bp::_globals(ctx).put(name, std::string(trim(bp::_attr(ctx))));
        })];
}

/// Split a command line on unquoted ';'. Quoted sections (detail::quoted_str)
/// and backslash escapes are honored; an unterminated quote does not protect
/// a following ';'. Segments are returned raw — quoting is decoded by each
/// command's own spec.
inline std::vector<std::string_view> splitCommands(std::string_view line) {
    auto const piece = bp::string_view[detail::quoted_str]
                     | ('\\' >> bp::char_)
                     | (bp::char_ - ';');
    auto const segment = bp::string_view[*piece];
    std::vector<std::string_view> segs;
    bp::callback_error_handler quiet{[](std::string const&) {}};
    if (line.empty() ||
        !bp::parse(line, bp::with_error_handler(segment % ';', quiet), segs)) {
        segs = {line};
    }
    return segs;
}

/// Tokenize on whitespace; quoted sections (detail::quoted_str) decode
/// escapes and group whitespace; `\x` outside quotes escapes any char.
inline std::vector<std::string> splitTokens(std::string_view text) {
    std::vector<std::string> out;
    auto const push = [](auto& ctx) {
        const auto& a = bp::_attr(ctx);
        auto& v = bp::_globals(ctx);
        if constexpr (std::is_same_v<std::decay_t<decltype(a)>, std::string>) {
            v.push_back(a);
        } else if constexpr (std::is_same_v<std::decay_t<decltype(a)>,
                                            std::vector<char>>) {
            v.emplace_back(a.begin(), a.end());
        } else {
            v.emplace_back(a.data(), a.size()); // string_view
        }
    };
    auto const bare = bp::lexeme[
        +(bp::lit('\\') >> bp::char_ | (bp::char_ - bp::char_(" \t\n")))];
    auto const tok = detail::quoted_str[push] | bare[push];
    bp::parse(text, bp::with_globals(*tok, out), bp::ws);
    return out;
}

/// Wrap a composed spec in a callable that fully parses `input`.
template<typename P>
ArgSpec spec(bp::parser_interface<P> const& p) {
    return [p](std::string_view input, ParsedArgs& args, std::string& error) {
        std::string message;
        bp::callback_error_handler handler{[&message](std::string const& s) {
            if (message.empty()) {
                message = s;
            }
        }};
        auto parser =
            bp::with_error_handler(bp::with_globals(p, args), handler);
        if (bp::parse(input, parser, bp::ws)) {
            return true;
        }
        error = std::move(message);
        return false;
    };
}

} // namespace args

} // namespace Linea

#endif // LINEA_COMMAND_ARGS_H
