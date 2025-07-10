#pragma once

#include <lang/core.hpp>

#include <vector>
#include <variant>
#include <algorithm>
#include <cstring>
#include <any>

#include <ankerl/unordered_dense.h>

NAMESPACE_BEGIN(lang)

// -----------------------------------------------------------------------------

template<typename T>
struct Dedup
{
    ankerl::unordered_dense::map<T, u32> lookup;
    std::vector<T> cache{};

    usz total_lookups = 0;

    usz unique_count()
    {
        return cache.size();
    }

    u32 get_id(T item)
    {
        total_lookups++;
        auto& id = lookup[item];
        if (!id) {
            cache.emplace_back(item);
            id = cache.size();
        }
        return id;
    }

    T from_id(u32 id)
    {
        return cache[id - 1];
    }
};

struct Constants
{
    Dedup<std::string_view> identifiers;
    Dedup<std::string_view> strings;
    Dedup<std::string_view> numbers;
};

// -----------------------------------------------------------------------------
//     Tokens
// -----------------------------------------------------------------------------

constexpr std::array Keywords
{
    "if", "else",
    "for", "while", "match",
    "break", "continue",
    "return", "defer",
    "true", "false",
    "struct", "enum",
    "and", "or",
    "undefined", "null",

    "@as", "@typeOf",
};

constexpr auto PunctuationSequences = std::to_array<std::string_view>({
    "^",    // dereference

    "+",    // arithmetic identity / add
    "+=",   // add assign
    "++",   // pre/post increment
    "-",    // arithmetic negation / subtract
    "-=",   // subtract assign
    "--",   // pre/post decrement
    "->",   // return type introducer
    "*",    // multiply
    "*=",   // multiply assign
    "**",   // power
    "**=",  // power assign
    "/",    // divide
    "/=",   // divide assign
    "%",    // modulo
    "%=",   // modulo assign
    "<",    // less than
    "<=",   // less greater
    "<<",   // shift left
    "<<=",  // shift left assign
    ">",    // greater
    ">=",   // greater equal
    ">>",   // shift right
    ">>=",  // shift right assign
    "&",    // bitwise and
    "&=",   // bitwise and assign
    "|",    // bitwise or
    "|=",   // bitwise or assign
    "=",    // assign
    "==",   // equal
    "!",    // logical negate
    "!=",   // not equal
    "~",    // bitwise negate
    "~|",   // xor
    "~|=",  // xor assign

    "(", ")",   // parentheses
    "{", "}",   // braces
    "[", "]",   // brackets

    "${",       // token sequence introducer
    "\\{",      // code splice
    "\\id{",    // identifier splice

    ".",    // field access
    "..",   // range exclusive
    "..=",  // range inclusive
    ":",    // declaration / type annotation introducer / const init

    ";",    // statement delimiter
    ",",    // expression delimiter

    "?",    // *reserved
});

enum class TokenType : u8 {};

namespace TokenTypes {
    constexpr TokenType Invalid      = TokenType(0);
    constexpr TokenType Eof          = TokenType(1);
    constexpr TokenType Identifier   = TokenType(2);
    constexpr TokenType String       = TokenType(3);
    constexpr TokenType Number       = TokenType(4);

    constexpr TokenType Keyword_First = TokenType(5);
    constexpr TokenType Keyword_Last  = TokenType(5 + Keywords.size() - 1);
    constexpr TokenType Punc_First    = TokenType(5 + Keywords.size());
    constexpr TokenType Punc_Last     = TokenType(5 + Keywords.size() + PunctuationSequences.size() - 1);
};

inline
std::string_view get_punc_str(TokenType type)
{
    return PunctuationSequences[std::to_underlying(type) - std::to_underlying(TokenTypes::Punc_First)];
}

inline
std::string_view get_keyword_str(TokenType type)
{
    return Keywords[std::to_underlying(type) - std::to_underlying(TokenTypes::Keyword_First)];
}

inline
std::string_view get_token_type_name(TokenType type)
{
    switch (type) {
        case TokenTypes::Identifier:
            return "Identifier";
        case TokenTypes::String:
            return "String";
        case TokenTypes::Number:
            return "Number";
        case TokenTypes::Keyword_First...TokenTypes::Keyword_Last:
            return get_keyword_str(type);
        case TokenTypes::Punc_First...TokenTypes::Punc_Last:
            return get_punc_str(type);
        default:
            raise_error("");
    }
}

consteval TokenType keyword(std::string_view str)
{
    for (usz i = 0; i < Keywords.size(); ++i) {
        if (Keywords[i] == str) {
            return TokenType(std::to_underlying(TokenTypes::Keyword_First) + i);
        }
    }
    raise_error("Expected valid keyword, got '{}'", str);
}

consteval TokenType punc(std::string_view str)
{
    for (usz i = 0; i < PunctuationSequences.size(); ++i) {
        if (PunctuationSequences[i] == str) {
            return TokenType(std::to_underlying(TokenTypes::Punc_First) + i);
        }
    }
    raise_error("Expected valid keyword, got '{}'", str);
}

// TODO: use unique AST nodes for undefined/nodes instead of identifiers
#define TOKEN_IsIdentifierLikeCases \
    case TokenTypes::Identifier: \
    case keyword("undefined"): \
    case keyword("null"): \
    case keyword("@as"): \
    case keyword("@typeOf") \

inline
bool token_type_is_identifier_like(TokenType type)
{
    switch (type) {
        TOKEN_IsIdentifierLikeCases:
            return true;
        default:
            return false;
    }
}

// -----------------------------------------------------------------------------

constexpr auto InvalidTokenIndex = ~0u;

struct Token
{
    TokenType type = TokenTypes::Invalid;
    u32 value;
};

inline
std::string token_to_string(Constants& constants, Token token)
{
    switch (token.type) {
        case TokenTypes::Eof:
            return "Eof";
        case TokenTypes::Identifier:
            return std::format("Identifier({})", constants.identifiers.from_id(token.value));
        case TokenTypes::String:
            return std::format("String({})", constants.strings.from_id(token.value));
        case TokenTypes::Number:
            return std::format("Number({})", constants.numbers.from_id(token.value));
        case TokenTypes::Keyword_First...TokenTypes::Keyword_Last:
            return std::format("Keyword({})", get_keyword_str(token.type));
        case TokenTypes::Punc_First...TokenTypes::Punc_Last:
            return std::format("Punc({})", get_punc_str(token.type));
        default:
            raise_error("invalid token type ({})", std::to_underlying(token.type));
    }
}

constexpr auto InvalidToken = Token { .type = TokenTypes::Invalid };
constexpr auto EofToken = Token { .type = TokenTypes::Eof };

inline
bool token_has_value(TokenType type)
{
    switch (type) {
        case TokenTypes::Identifier:
        case TokenTypes::String:
        case TokenTypes::Number:
        case punc("["):
        case punc("("):
        case punc("{"):
        case punc("\\{"):
        case punc("\\id{"):
            return true;
        default:
            return false;
    }
}

struct TokenList
{
    Constants* constants;
    std::vector<u32> tokens;

    usz size_in_bytes()
    {
        return tokens.size();
    }

    void append(Token token)
    {
        if (token_has_value(token.type)) {
            auto offset = tokens.size();
            tokens.resize(offset + 5);
            tokens[offset] = std::to_underlying(token.type);
            std::memcpy(tokens.data() + offset + 1, &token.value, 4);
        } else {
            tokens.emplace_back(std::to_underlying(token.type));
        }
    }

    struct TokenIterator begin() const;

    TokenIterator matching_pair_for(Token token) const;
};

#define LANG_NOISY_TOKEN_ITERATOR 0

struct TokenIterator
{
    const TokenList* list;
    u32 current;

#if LANG_NOISY_TOKEN_ITERATOR
    mutable bool _peeked = false;
#endif

    bool is_at_end() const { return current >= list->tokens.size();            }
    u32      index() const { return is_at_end() ? InvalidTokenIndex : current; }

    Token get_token_at(u32 offset) const
    {
        TokenType type = TokenType(list->tokens[offset]);
        if (token_has_value(type)) {
            Token token{ .type = type };
            std::memcpy(&token.value, list->tokens.data() + offset + 1, 4);
            return token;
        } else {
            return Token{ .type = type };
        }
    }

    u32 next_offset() const
    {
        TokenType type = TokenType(list->tokens[current]);
        if (token_has_value(type)) {
            return current + 5;
        } else {
            return current + 1;
        }
    }

    Token advance()
    {
#if LANG_NOISY_TOKEN_ITERATOR
        if (!_peeked) {
            peek();
        }
        _peeked = is_at_end();
#endif
        auto token = is_at_end() ? EofToken : get_token_at(current);
        current = next_offset();
        return token;
    }

    Token peek() const
    {
        auto token = is_at_end() ? EofToken : get_token_at(current);
#if LANG_NOISY_TOKEN_ITERATOR
        if (!_peeked) {
            log_trace("peek {}", token_to_string(*list->constants, token));
            _peeked = true;
        }
#endif
        return token;
    }

    Token peek_next() const
    {
        auto next = next_offset();
        auto token = next >= list->tokens.size() ? EofToken : get_token_at(next);
#if LANG_NOISY_TOKEN_ITERATOR
        log_trace("peek_next {}", token_to_string(*list->constants, token));
#endif
        return token;
    }
};

inline
TokenIterator TokenList::matching_pair_for(Token token) const
{
    switch (token.type) {
        case punc("("):
        case punc("["):
        case punc("{"):
        case punc("\\{"):
        case punc("\\id{"):
        {
            auto i = token.value;
            return TokenIterator{this, i};
        }
        default:
            return TokenIterator{this, InvalidTokenIndex};
    }
}

inline
TokenIterator TokenList::begin() const
{
    return TokenIterator{this, 0};
}

constexpr bool is_end_of_expression_token(Token token)
{
    switch (token.type) {
        case punc(")"):
        case punc("}"):
        case punc("]"):
        case punc(";"):
        case punc(","):
            return true;
        default:
            return false;
    }
}

void scan(TokenList& out, Constants& constants, std::string_view source);

// -----------------------------------------------------------------------------
//     Operators
// -----------------------------------------------------------------------------

#define OPS_Postfix \
    X(Call) X(Subscript) X(FieldAccess) X(Postincrement) X(Postdecrement) X(Dereference)

#define OPS_Prefix \
    X(Preincrement) X(Predecrement) X(Arithmetic_Identity) \
    X(Arithmetic_Negate) X(Bitwise_Negate) X(Logical_Negate) X(AddressOf)

#define OPS_Arithmetic \
    X(Add) X(Subtract) X(Multiply) X(Divide) X(Modulo) X(Power)

#define OPS_Bitwise \
    X(ShiftLeft) X(ShiftRight) X(Xor) X(Bitwise_And) X(Bitwise_Or)

#define OPS_Logical \
    X(Logical_And) X(Logical_Or)

#define OPS_Comparison \
    X(Equal) X(NotEqual) X(Greater) X(GreaterEqual) X(Less) X(LessEqual)

#define OPS_Range \
    X(Range_Exclusive) X(Range_Inclusive)

#define OPS_VarDecl \
    X(VarDecl)

#define OPS_Assign \
    X(Assign) \
    X(Assign_Add) X(Assign_Subtract) X(Assign_Multiply) X(Assign_Divide) X(Assign_Modulo) X(Assign_Exponentiate) \
    X(Assign_ShiftLeft) X(Assign_ShiftRight) X(Assign_Xor) X(Assign_Bitwise_And) X(Assign_Bitwise_Or)

#define OPS_ControlFlow \
    X(Break) X(Return) X(Continue) \

#define OPS_All \
    OPS_Postfix \
    OPS_Prefix \
    OPS_Arithmetic \
    OPS_Bitwise \
    OPS_Logical \
    OPS_Comparison \
    OPS_Range \
    OPS_VarDecl \
    OPS_Assign \
    OPS_ControlFlow

enum class Operator : u16
{
    None,

#define X(i, ...) i,
    OPS_All
#undef X
};

constexpr
std::string_view get_op_name(Operator op)
{
    switch (op) {
        case Operator::None: return "None";

#define X(o, ...) case Operator::o: return #o;
        OPS_All
#undef X
    }
}

enum class ExpressionMode
{
    Normal,
    TypeAnnotation,
};

constexpr std::string_view get_expr_mode_name(ExpressionMode mode)
{
    switch (mode) {
        case ExpressionMode::Normal: return "Normal";
        case ExpressionMode::TypeAnnotation: return "TypeAnnotation";
    }
}

constexpr Operator get_prefix_op(TokenType type, ExpressionMode mode)
{
    switch (type) {
        case keyword("break"): return Operator::Break;
        case keyword("return"): return Operator::Return;
        case keyword("continue"): return Operator::Continue;

        case punc("&"): return Operator::AddressOf;

        case punc("++"): return Operator::Preincrement;
        case punc("--"): return Operator::Predecrement;

        case punc("+"): return Operator::Arithmetic_Identity;
        case punc("-"): return Operator::Arithmetic_Negate;
        case punc("~"): return Operator::Bitwise_Negate;
        case punc("!"): return Operator::Logical_Negate;
    }
    return Operator::None;
}

constexpr Operator get_other_op(TokenType type, ExpressionMode mode)
{
    switch (type) {
        case punc("^"):   return Operator::Dereference;
        case punc("++"):  return Operator::Postincrement;
        case punc("--"):  return Operator::Postdecrement;
        case punc("("):   return Operator::Call;
        case punc("["):   return Operator::Subscript;
        case punc("."):   return Operator::FieldAccess;

        case punc("+"):   return Operator::Add;
        case punc("-"):   return Operator::Subtract;
        case punc("*"):   return Operator::Multiply;
        case punc("/"):   return Operator::Divide;
        case punc("%"):   return Operator::Modulo;
        case punc("**"):  return Operator::Power;

        case punc("<<"):  return Operator::ShiftLeft;
        case punc(">>"):  return Operator::ShiftRight;
        case punc("~|"):  return Operator::Xor;
        case punc("&"):   return Operator::Bitwise_And;
        case punc("|"):   return Operator::Bitwise_Or;

        case keyword("and"):  return Operator::Logical_And;
        case keyword("or"):  return Operator::Logical_Or;

        case punc("=="):  return Operator::Equal;
        case punc("!="):  return Operator::NotEqual;
        case punc(">"):   return Operator::Greater;
        case punc(">="):  return Operator::GreaterEqual;
        case punc("<"):   return Operator::Less;
        case punc("<="):  return Operator::LessEqual;

        case punc(".."):  return Operator::Range_Exclusive;
        case punc("..="): return Operator::Range_Inclusive;

        case punc("="):   return (mode == ExpressionMode::TypeAnnotation) ? Operator::None : Operator::Assign;
        case punc(":"):   return (mode == ExpressionMode::TypeAnnotation) ? Operator::None : Operator::VarDecl;

        case punc("+="):  return Operator::Assign_Add;
        case punc("-="):  return Operator::Assign_Subtract;
        case punc("*="):  return Operator::Assign_Multiply;
        case punc("/="):  return Operator::Assign_Divide;
        case punc("%="):  return Operator::Assign_Modulo;
        case punc("**="): return Operator::Assign_Exponentiate;
        case punc("<<="): return Operator::Assign_ShiftLeft;
        case punc(">>="): return Operator::Assign_ShiftRight;
        case punc("~|="): return Operator::Assign_Xor;
        case punc("&="):  return Operator::Assign_Bitwise_And;
        case punc("|="):  return Operator::Assign_Bitwise_Or;
    }
    return Operator::None;
}

constexpr bool is_op_postfix(Operator op)
{
    switch (op) {
        case Operator::Dereference:
        case Operator::Postincrement:
        case Operator::Postdecrement:
        case Operator::Call:
        case Operator::Subscript:
        case Operator::FieldAccess:
            return true;
        default:
            return false;
    }
}

NAMESPACE_END
