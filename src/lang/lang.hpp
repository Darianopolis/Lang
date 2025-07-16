#pragma once

#include "core.hpp"
#include "strings.hpp"
#include "lang.hpp"

#include <vector>
#include <variant>
#include <algorithm>
#include <cstring>
#include <any>

#include <ankerl/unordered_dense.h>

NAMESPACE_BEGIN(lang)

struct PaddedStringView
{
    const char* data;
    usz count;

    usz size()
    {
        return count;
    }

    std::string_view substr(usz first, usz len)
    {
        return std::string_view(data + first, len);
    }

    LANG_forceinline
    constexpr u8 operator[](usz index) const noexcept
    {
        return data[index];
    }
};

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//     Error Reporting
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

struct Error
{
    u32 line;
    u32 head_file_offset;
    u32 tail_file_offset;
    std::string message;

    void print(std::string_view source);
};

struct ErrorList
{
    std::vector<Error> errors;

    void clear() { errors.clear(); }
    bool any_errors() const { return !errors.empty(); }
    auto begin() { return errors.begin(); }
    auto end() { return errors.end(); }
};

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//     Constants
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

template<typename T>
struct Dedup
{
    ankerl::unordered_dense::map<T, u32> lookup;
    std::vector<T> cache{};

    usz total_lookups = 0;

    void clear()
    {
        total_lookups = 0;
        lookup.clear();
        cache.clear();
    }

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
        auto value = cache[id - 1];
        return value;
    }
};

struct Constants
{
    Dedup<std::string_view> identifiers;
    Dedup<std::string_view> strings;
    Dedup<std::string_view> numbers;

    void clear()
    {
        identifiers.clear();
        strings.clear();
        numbers.clear();
    }
};

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//     Tokens
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
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
    "fn",

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
        case TokenTypes::Eof:
            return "Eof";
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
            raise_error("invalid token type: {}", std::to_underlying(type));
    }
}

constexpr TokenType keyword_runtime(std::string_view str)
{
    for (usz i = 0; i < Keywords.size(); ++i) {
        if (Keywords[i] == str) {
            return TokenType(std::to_underlying(TokenTypes::Keyword_First) + i);
        }
    }
    raise_error("Expected valid keyword, got '{}'", str);
}

consteval TokenType keyword(std::string_view str)
{
    return keyword_runtime(str);
}

constexpr TokenType punc_runtime(std::string_view str)
{
    for (usz i = 0; i < PunctuationSequences.size(); ++i) {
        if (PunctuationSequences[i] == str) {
            return TokenType(std::to_underlying(TokenTypes::Punc_First) + i);
        }
    }
    raise_error("Expected valid punctuation, got '{}'", str);
}

consteval TokenType punc(std::string_view str)
{
    return punc_runtime(str);
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
    u32 line;
    u32 file_offset;

    struct Identifier {
        std::string_view value;

        constexpr bool operator==(const Identifier&) const noexcept = default;
    };

    struct String {
        std::string_view value;
        bool has_escapes;

        constexpr bool operator==(const String&) const noexcept = default;
    };

    struct Number {
        std::string_view value;
        u8 base;

        constexpr bool operator==(const Number&) const noexcept = default;
    };

    union {
        Identifier identifier = {};
        String string;
        Number number;
    };

    operator bool() const noexcept
    {
        return type != TokenTypes::Eof && type != TokenTypes::Invalid;
    }
};

#define TOKEN_CloseBracketCases \
    case punc(")"): \
    case punc("]"): \
    case punc("}") \

#define TOKEN_OpenBracketCases \
    case punc("["): \
    case punc("("): \
    case punc("{"): \
    case punc("${"): \
    case punc("\\{"): \
    case punc("\\id{") \

#define TOKEN_HasValueCases \
    case TokenTypes::Identifier: \
    case TokenTypes::String: \
    case TokenTypes::Number

inline
bool token_has_value(TokenType type)
{
    switch (type) {
        TOKEN_HasValueCases:
            return true;
        default:
            return false;
    }
}

inline
std::string token_to_string(Token token)
{
    switch (token.type) {
        case TokenTypes::Eof:
            return "Eof";
        case TokenTypes::Identifier:
            return std::format("Identifier({})", token.identifier.value);
        case TokenTypes::String:
            if (token.string.has_escapes) {
                return std::format("String<escaped>({})", token.string.value);
            } else {
                return std::format("String({})", token.string.value);
            }
        case TokenTypes::Number:
            return std::format("Number<{}>({})", token.number.base, token.number.value);
        case TokenTypes::Keyword_First...TokenTypes::Keyword_Last:
            return std::format("Keyword({})", get_keyword_str(token.type));
        case TokenTypes::Punc_First...TokenTypes::Punc_Last:
                return std::format("Punc({})", get_punc_str(token.type));
        default:
            raise_error("invalid token type ({})", std::to_underlying(token.type));
    }
}

constexpr bool is_end_of_expression_token(Token token)
{
    switch (token.type) {
        TOKEN_CloseBracketCases:
        case punc(";"):
        case punc(","):
            return true;
        default:
            return false;
    }
}

constexpr auto InvalidToken = Token { .type = TokenTypes::Invalid };
constexpr auto EofToken = Token { .type = TokenTypes::Eof };

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//     Operators
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
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
        default:
            raise_error("unrecognized op: {}", std::to_underlying(op));
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
        default: raise_error("invalid expr mode enum value: {}", std::to_underlying(mode));
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

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//     Abstract Syntax Tree
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

constexpr u32 AstHeader_Start = 0xFFFF'0000;

#define AST_Headers(DO) \
    DO(Literal_Number, = AstHeader_Start) \
    DO(Literal_String) \
    DO(Literal_True) \
    DO(Literal_False) \
    DO(Identifier) \
    DO(VarDecl) \
    DO(VarDecl_Type) \
    DO(VarDecl_ConstInit) \
    DO(VarDecl_MutableInit) \
    DO(VarDecl_TypeConstInit) \
    DO(VarDecl_TypeMutableInit) \
    DO(While) \
    DO(WhileElse) \
    DO(For) \
    DO(ForElse) \
    DO(Block) \
    DO(File) \
    DO(If) \
    DO(IfElse) \
    DO(Function_Definition) \
    DO(Function_Prototype) \
    DO(Defer) \

enum class AstHeader : u32
{
#define AST_Headers_DECLARE(name, ...) name __VA_ARGS__,
    AST_Headers(AST_Headers_DECLARE)
#define X(name, ...) name,
    OPS_Prefix
    OPS_Postfix
    OPS_Arithmetic
    OPS_Bitwise
    OPS_Logical
    OPS_Comparison
    OPS_Range
    OPS_Assign
#undef X
#define X(name, ...) name, name##_Empty,
    OPS_ControlFlow
#undef X
    End  = ~0u - 0,
};

inline
AstHeader get_vardecl_header(bool constant, bool type, bool init)
{
    if (init) {
        if (type) {
            return constant
                ? AstHeader::VarDecl_TypeConstInit
                : AstHeader::VarDecl_TypeMutableInit;
        } else {
            return constant
                ? AstHeader::VarDecl_ConstInit
                : AstHeader::VarDecl_MutableInit;
        }
    } else {
        return type ? AstHeader::VarDecl_Type : AstHeader::VarDecl;
    }
}

inline
bool ast_vardecl_header_is_constant(AstHeader header)
{
    return header == AstHeader::VarDecl_ConstInit
        || header == AstHeader::VarDecl_TypeConstInit;
}

inline
bool ast_vardecl_header_has_type(AstHeader header)
{
    return header == AstHeader::VarDecl_Type
        || header == AstHeader::VarDecl_TypeConstInit
        || header == AstHeader::VarDecl_TypeMutableInit;
}

inline
bool ast_vardecl_header_has_init(AstHeader header)
{
    return header == AstHeader::VarDecl_ConstInit
        || header == AstHeader::VarDecl_MutableInit
        || header == AstHeader::VarDecl_TypeConstInit
        || header == AstHeader::VarDecl_TypeMutableInit;
}

inline
std::string_view get_ast_header_name(AstHeader header)
{
    switch (header)
    {
    #define AST_Headers_ToString(name, ...) case AstHeader::name : return #name;
        AST_Headers(AST_Headers_ToString)
    #define X(name, ...) case AstHeader::name: return #name;
        OPS_Prefix
        OPS_Postfix
        OPS_Arithmetic
        OPS_Bitwise
        OPS_Logical
        OPS_Comparison
        OPS_Range
        OPS_Assign
    #undef X
    #define X(name, ...) case AstHeader::name:         return #name; \
                         case AstHeader::name##_Empty: return #name "_Empty";
        OPS_ControlFlow
    #undef X
        case AstHeader::End: return "End";
        default: raise_error("unknown header: {}", std::to_underlying(header));
    }
}

struct AstOffset
{
    static constexpr u32 Invalid = ~0u;

    u32 value = Invalid;

    AstOffset() noexcept = default;
    AstOffset(u32 bytecode_value) noexcept: value(bytecode_value) {}

    explicit operator bool() const noexcept
    {
        return value != Invalid;
    }

    AstOffset operator+(i32 delta) const noexcept
    {
        return {value + delta};
    }
};

struct Ast
{
    std::vector<u32> code;

    AstOffset offset()
    {
        return {u32(code.size())};
    }

    u32 at(AstOffset offset) const noexcept
    {
        return code[offset.value];
    }

    u32* pointer_to(AstOffset offset)
    {
        return code.data() + offset.value;
    }

    AstOffset write(AstHeader header) {
        auto _offset = offset();
        code.emplace_back(std::to_underlying(header));
        return _offset;
    }

    template<typename T>
    void write(T t)
    {
        code.emplace_back(std::bit_cast<u32>(t));
    }

    template<typename T>
    void write_at(AstOffset offset, T t)
    {
        code[offset.value] = std::bit_cast<u32>(t);
    }

    template<typename T>
    void insert_at(AstOffset offset, T t)
    {
        code.insert(code.begin() + offset.value, std::bit_cast<u32>(t));
    }
};
struct AstListBuilder
{
    Ast* ast;
    AstOffset first;
    AstOffset last;

    AstListBuilder(Ast& _ast): ast(&_ast) {}

    void add(AstOffset entry)
    {
        if (last) ast->write_at(last, entry);
        else first = entry;
        last = entry;
        ast->insert_at(last, AstOffset{});
    }

    AstOffset to_list()
    {
        return first;
    }
};

AstOffset parse(Ast& out, struct Scanner* scanner, Constants& constants);

void ast_print_flat(Constants& constants, Ast& ast);
void ast_compact(Constants& constants, Ast& ast, AstOffset offset, Ast& compacted);
void ast_print_tree(Constants& constants, Ast& ast, AstOffset offset);
void ast_print_details(Constants& constants, Ast& ast, AstOffset offset, std::string_view source);

NAMESPACE_END
