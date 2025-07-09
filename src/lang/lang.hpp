#pragma once

#include <lang/core.hpp>

#include <vector>
#include <variant>
#include <algorithm>
#include <cstring>
#include <any>

#include <ankerl/unordered_dense.h>
#include <zstd.h>

NAMESPACE_BEGIN(lang)

// -----------------------------------------------------------------------------

constexpr std::array Keywords
{
    "if", "else",
    "for", "while", "match",
    "break", "continue",
    "return", "defer",
    "true", "false",
    "struct", "enum",
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
    "%=",   // moduloe assign
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
    "&&",   // logical and
    "|",    // bitwise or
    "|=",   // bitwise or assign
    "||",   // logical or
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

consteval std::string_view lang_str(std::string_view str)
{
    for (auto key : Keywords) if (str == key) return str;
    for (auto punc : PunctuationSequences) if (str == punc) return str;

    raise_error("Expected valid language string, got '{}'", str);
}

struct StaticLangString
{
    std::string_view str;

    template<size_t Size>
    consteval StaticLangString(const char (&str)[Size])
        : str(lang_str(std::string_view(str)))
    {}

    consteval StaticLangString(std::string_view _str)
        : str(lang_str(_str))
    {}
};

struct RuntimeLangString
{
    std::string_view str;

    explicit RuntimeLangString(std::string_view _str): str(_str) {}
};

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

#define X(i) i,
    OPS_All
#undef X
};

constexpr
std::string_view get_op_name(Operator op)
{
    switch (op) {
        case Operator::None: return "None";

#define X(o) case Operator::o: return #o;
        OPS_All
#undef X
    }
}

enum class OperatorString : u32 { };

constexpr OperatorString make_opstr(const char* op, u32 len)
{
    u32 hash {};
    for (u32 i = 0; i < len; ++i) {
        hash |= u32(op[i]) << (i * 8);
    }

    return OperatorString(hash);
}

constexpr OperatorString make_opstr(RuntimeLangString op)
{
    return make_opstr(op.str.data(), u32(op.str.size()));
}

consteval OperatorString make_opstr(StaticLangString op)
{
    return make_opstr(op.str.data(), u32(op.str.size()));
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

constexpr Operator get_prefix_op(std::string_view op, ExpressionMode mode)
{
    if (op == lang_str("break")) return Operator::Break;
    if (op == lang_str("return")) return Operator::Return;
    if (op == lang_str("continue")) return Operator::Continue;

    switch (make_opstr(RuntimeLangString(op))) {
        case make_opstr("&"): return Operator::AddressOf;

        case make_opstr("++"): return Operator::Preincrement;
        case make_opstr("--"): return Operator::Predecrement;

        case make_opstr("+"): return Operator::Arithmetic_Identity;
        case make_opstr("-"): return Operator::Arithmetic_Negate;
        case make_opstr("~"): return Operator::Bitwise_Negate;
        case make_opstr("!"): return Operator::Logical_Negate;
    }
    return Operator::None;
}

constexpr Operator get_other_op(std::string_view op, ExpressionMode mode)
{
    switch (make_opstr(RuntimeLangString(op))) {
        case make_opstr("^"):   return Operator::Dereference;
        case make_opstr("++"):  return Operator::Postincrement;
        case make_opstr("--"):  return Operator::Postdecrement;
        case make_opstr("("):   return Operator::Call;
        case make_opstr("["):   return Operator::Subscript;
        case make_opstr("."):   return Operator::FieldAccess;

        case make_opstr("+"):   return Operator::Add;
        case make_opstr("-"):   return Operator::Subtract;
        case make_opstr("*"):   return Operator::Multiply;
        case make_opstr("/"):   return Operator::Divide;
        case make_opstr("%"):   return Operator::Modulo;
        case make_opstr("**"):  return Operator::Power;

        case make_opstr("<<"):  return Operator::ShiftLeft;
        case make_opstr(">>"):  return Operator::ShiftRight;
        case make_opstr("~|"):  return Operator::Xor;
        case make_opstr("&"):   return Operator::Bitwise_And;
        case make_opstr("|"):   return Operator::Bitwise_Or;

        case make_opstr("&&"):  return Operator::Logical_And;
        case make_opstr("||"):  return Operator::Logical_Or;

        case make_opstr("=="):  return Operator::Equal;
        case make_opstr("!="):  return Operator::NotEqual;
        case make_opstr(">"):   return Operator::Greater;
        case make_opstr(">="):  return Operator::GreaterEqual;
        case make_opstr("<"):   return Operator::Less;
        case make_opstr("<="):  return Operator::LessEqual;

        case make_opstr(".."):  return Operator::Range_Exclusive;
        case make_opstr("..="): return Operator::Range_Inclusive;

        case make_opstr("="):   return (mode == ExpressionMode::TypeAnnotation) ? Operator::None : Operator::Assign;
        case make_opstr(":"):   return (mode == ExpressionMode::TypeAnnotation) ? Operator::None : Operator::VarDecl;

        case make_opstr("+="):  return Operator::Assign_Add;
        case make_opstr("-="):  return Operator::Assign_Subtract;
        case make_opstr("*="):  return Operator::Assign_Multiply;
        case make_opstr("/="):  return Operator::Assign_Divide;
        case make_opstr("%="):  return Operator::Assign_Modulo;
        case make_opstr("**="): return Operator::Assign_Exponentiate;
        case make_opstr("<<="): return Operator::Assign_ShiftLeft;
        case make_opstr(">>="): return Operator::Assign_ShiftRight;
        case make_opstr("~|="): return Operator::Assign_Xor;
        case make_opstr("&="):  return Operator::Assign_Bitwise_And;
        case make_opstr("|="):  return Operator::Assign_Bitwise_Or;
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

#define TOKEN_TYPE_LIST(DO) \
    DO(Eof, = 1) \
    \
    DO(Keyword) \
    \
    DO(Identifier) \
    DO(String) \
    DO(Number) \
    DO(Punctuation)

#define TOKEN_TYPE_DECLARE(type, ...) type __VA_ARGS__,

enum class TokenType {
    Invalid = 0,

    TOKEN_TYPE_LIST(TOKEN_TYPE_DECLARE)
};

constexpr std::string_view get_token_type_name(TokenType type)
{
    switch (type)
    {
#define TOKEN_TYPE_TO_STRING_SWITCH(type, ...) case TokenType::type: return #type;
        TOKEN_TYPE_LIST(TOKEN_TYPE_TO_STRING_SWITCH)
        default:
            raise_error("invalid token type");
    }
}

constexpr auto InvalidTokenIndex = ~0u;

struct Token
{
    TokenType type = TokenType::Invalid;
    std::string_view raw;
    union {
        struct {
            std::string_view value;
        } keyword;
        struct {
            bool is_at;
            std::string_view value;
        } identifier;
        struct {
            bool is_raw;
            std::string_view value;
        } string;
        struct {
            u32 base;
            std::string_view raw;
        } number;
        struct {
            std::string_view value;
            u32 matching = InvalidTokenIndex;
        } punctuation;
    };
};

constexpr auto InvalidToken = Token { .type = TokenType::Invalid };
constexpr auto EofToken = Token { .type = TokenType::Eof };

struct TokenList
{
    std::vector<Token> tokens;

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

    void decrement()
    {
        if (current > 0) {
            current--;

#if LANG_NOISY_TOKEN_ITERATOR
            if (_peeked) {
                log_trace("token.decrement");
                _peeked = false;
            }
#endif
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
        return is_at_end() ? EofToken : list->tokens[current++];
    }

    Token peek() const
    {
        auto token = is_at_end() ? EofToken : list->tokens[current];
#if LANG_NOISY_TOKEN_ITERATOR
        if (!_peeked) {
            log_trace("token.peek '{}'", token.raw);
            _peeked = true;
        }
#endif
        return token;
    }

    Token peek_next() const
    {
        auto next = current + 1;
        auto token = next >= list->tokens.size() ? EofToken : list->tokens[next];
#if LANG_NOISY_TOKEN_ITERATOR
        log_trace("token.peek_next '{}'", token.raw);
#endif
        return token;
    }

    Token peek_prev() const
    {
        auto token = current == 0 ? EofToken : TokenIterator(list, current - 1).peek();
#if LANG_NOISY_TOKEN_ITERATOR
        log_trace("token.peek_prev '{}'", token.raw);
#endif
        return token;
    }

    union Internal {
        std::monostate _{};
        Token token;
        Internal(): _{} { }
    } _internal;
          Token    operator *()      { return is_at_end() ? EofToken : list->tokens[current]; }
    const Token*   operator->()      { _internal.token = **this; return &_internal.token; }
    TokenIterator& operator++()      { current++; return *this; }
    TokenIterator& operator--()      { current--; return *this; };
    TokenIterator  operator++(int)   { auto copy = *this; current++; return copy; }
    TokenIterator  operator--(int)   { auto copy = *this; current--; return copy; }
    TokenIterator  operator +(i32 o) { return {list, current + u32(o)}; };
    TokenIterator  operator -(i32 o) { return {list, current - u32(o)}; };

    bool operator==(const TokenIterator& other) const { return index() == other.index(); };
    operator bool() const { return !is_at_end(); }
};

inline
TokenIterator TokenList::matching_pair_for(Token token) const
    {
        if (token.type != TokenType::Punctuation) return TokenIterator{this, InvalidTokenIndex};
        auto i = token.punctuation.matching;
        return TokenIterator{this, i};
    }

inline
TokenIterator TokenList::begin() const
{
    return TokenIterator{this, 0};
}

constexpr bool is_end_of_expression_token(Token token)
{
    switch (make_opstr(RuntimeLangString(token.raw))) {
        case make_opstr(")"):
        case make_opstr("}"):
        case make_opstr("]"):
        case make_opstr(";"):
        case make_opstr(","):
            return true;
        default:
            return false;
    }
}

TokenList scan(std::string_view);

// -----------------------------------------------------------------------------

template<typename T>
struct Dedup
{
    ankerl::unordered_dense::map<T, u32> lookup;
    std::vector<T> cache;

    usz total_lookups = 0;

    u32 get_id(T item)
    {
        total_lookups++;
        auto iter = lookup.find(item);
        if (iter == lookup.end()) {
            auto index = cache.size();
            cache.emplace_back(item);
            lookup[item] = index;
            return index;
        } else {
            return iter->second;
        }
    }

    T from_id(u32 id)
    {
        return cache[id];
    }
};

struct Constants
{
    Dedup<std::string_view> strings;
    Dedup<f64> numbers;
};

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
    #define X(name, ...) case AstHeader::name:     return #name; \
                     case AstHeader::name##_Empty: return #name "_Empty";
        OPS_ControlFlow
    #undef X
        case AstHeader::End: return "End";
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
        if constexpr (sizeof(T) <= 4) {
            u32 v = {};
            std::memcpy(&v, &t, sizeof(T));
            code.emplace_back(v);
        } else {
            static_assert(sizeof(T) % 4 == 0);
            auto offset = code.size();
            code.resize(offset + sizeof(T) / 4);
            std::memcpy(code.data() + offset, &t, sizeof(T));
        }
    }

    template<typename T>
    void write_at(AstOffset offset, T t)
    {
        if constexpr (sizeof(T) <= 4) {
            std::memcpy(code.data() + offset.value, &t, sizeof(T));
        } else {
            raise_error("TODO");
        }
    }

    template<typename T>
    void insert_at(AstOffset offset, T t)
    {
        if constexpr (sizeof(T) <= 4) {
            u32 v = {};
            std::memcpy(&v, &t, sizeof(T));
            code.insert(code.begin() + offset.value, v);
        } else {
            raise_error("TODO");
        }
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

struct ParseResult
{
    Ast ast;
    AstOffset offset;
};

ParseResult parse(TokenList& tokens, Constants& constants);

inline
void ast_print_flat(Constants& constants, Ast& ast)
{
    u32 offset = 0;
    for (auto v : ast.code) {
        std::print("[{:3}] ", offset++);
        if (v >= AstHeader_Start) {
            std::println("{}", get_ast_header_name(AstHeader(v)));
        } else {
            std::println("{}", v);
        }
    }
}

inline
void ast_compact(Constants& constants, Ast& ast, AstOffset offset, Ast& compacted)
{
    auto compact_node = [&](this auto&& compact_node, AstOffset offset) -> void {

        auto* ip = ast.pointer_to(offset);

        auto compact_list = [&](AstOffset next) {
            while (next) {
                compact_node(next + 1);
                next = *ast.pointer_to(AstOffset{next});
            }
            compacted.write(AstOffset{});
        };
        auto header = AstHeader(*ip);
        compacted.write(header);
        switch (header) {
            case AstHeader::File:
            case AstHeader::Block:
                compact_list(ip[1]);
                break;
            case AstHeader::Literal_Number:
            case AstHeader::Literal_String:
            case AstHeader::Identifier:
                compacted.write(ip[1]);
                break;
            case AstHeader::FieldAccess:
                compacted.write(ip[1]);
                compact_node(ip[2]);
                break;
            case AstHeader::Dereference:
            case AstHeader::Postincrement:
            case AstHeader::Postdecrement:
            case AstHeader::Break:
            case AstHeader::Continue:
            case AstHeader::Return:
            #define X(name, ...) case AstHeader::name:
                OPS_Prefix
            #undef X
                compact_node(ip[1]);
                break;
            #define X(name, ...) case AstHeader::name:
                OPS_Arithmetic
                OPS_Bitwise
                OPS_Logical
                OPS_Comparison
                OPS_Range
                OPS_Assign
            #undef X
                compact_node(ip[1]);
                compact_node(ip[2]);
                break;
            case AstHeader::VarDecl:
            case AstHeader::VarDecl_Type:
            case AstHeader::VarDecl_ConstInit:
            case AstHeader::VarDecl_MutableInit:
            case AstHeader::VarDecl_TypeConstInit:
            case AstHeader::VarDecl_TypeMutableInit:
                compact_node(ip[1]);
                ip += 2;
                if (ast_vardecl_header_has_type(header)) {
                    compact_node(*ip);
                    ip += 1;
                }
                if (ast_vardecl_header_has_init(header)) {
                    compact_node(*ip);
                }
                break;
            case AstHeader::Function_Definition:
            case AstHeader::Function_Prototype:
                compact_list(ip[1]);
                compact_node(ip[2]);
                if (header == AstHeader::Function_Definition) {
                    compact_list(ip[3]);
                }
                break;
            case AstHeader::Defer:
                compact_list(ip[1]);
                break;
            case AstHeader::Call:
            case AstHeader::Subscript:
                compact_node(ip[1]);
                compact_list(ip[2]);
                break;
            case AstHeader::While:
            case AstHeader::WhileElse:
            case AstHeader::If:
            case AstHeader::IfElse:
                compact_node(ip[1]);
                compact_list(ip[2]);
                if (header == AstHeader::WhileElse || header == AstHeader::IfElse) {
                    compact_list(ip[3]);
                }
                break;
            case AstHeader::For:
            case AstHeader::ForElse:
                compact_node(ip[1]);
                compact_list(ip[2]);
                compact_list(ip[3]);
                if (header == AstHeader::ForElse) {
                    compact_list(ip[4]);
                }
                break;
            default:
                ;
        }
    };

    compact_node(offset);
}

inline
void ast_print_tree(Constants& constants, Ast& ast, AstOffset offset)
{
    auto print_node_root = [&](this auto&& print_node, AstOffset offset, u64 depth) -> void {

        u32* ip = ast.pointer_to(offset);

        // #define PRINT(...)
        #define PRINT(fmt, ...) std::println("{}" fmt, indent_str(depth) __VA_OPT__(,) __VA_ARGS__)

        auto print_list = [&](AstOffset next, auto depth) {

            while (next) {
                print_node(next + 1, depth);
                next = *ast.pointer_to(next);
            }
            // compacted.write(AstOffset{});
        };

        auto header = AstHeader(*ip);

        // compacted.write(header);
        switch (header) {
            case AstHeader::File:
                print_list(ip[1], depth);
                break;
            case AstHeader::Block:
                PRINT("Block");
                print_list(ip[1], depth + 1);
                break;
            case AstHeader::Literal_Number:
                PRINT("Literal_Number({})", constants.numbers.from_id(ip[1]));
                // compacted.write(ip[1]);
                break;
            case AstHeader::Literal_String:
                PRINT("Literal_String({})", constants.strings.from_id(ip[1]));
                // compacted.write(ip[1]);
                break;
            case AstHeader::Literal_True:
            case AstHeader::Literal_False:
            case AstHeader::Break_Empty:
            case AstHeader::Continue_Empty:
            case AstHeader::Return_Empty:
                PRINT("{}", get_ast_header_name(header));
                break;
            case AstHeader::FieldAccess:
                PRINT("FieldAccess({})", constants.strings.from_id(ip[1]));
                print_node(ip[2], depth + 1);
                // compacted.write(ip[1]);
                break;
            case AstHeader::Identifier:
                PRINT("Identifier({})", constants.strings.from_id(ip[1]));
                // compacted.write(ip[1]);
                break;
// -----------------------------------------------------------------------------
            case AstHeader::Dereference:
            case AstHeader::Postincrement:
            case AstHeader::Postdecrement:
            case AstHeader::Break:
            case AstHeader::Continue:
            case AstHeader::Return:
            #define X(name, ...) case AstHeader::name:
                OPS_Prefix
            #undef X
                PRINT("{}", get_ast_header_name(header));
                print_node(ip[1], depth + 1);
                break;
// -----------------------------------------------------------------------------
            #define X(name, ...) case AstHeader::name:
                OPS_Arithmetic
                OPS_Bitwise
                OPS_Logical
                OPS_Comparison
                OPS_Range
                OPS_Assign
            #undef X
                PRINT("{}", get_ast_header_name(header));
                print_node(ip[1], depth + 1);
                print_node(ip[2], depth + 1);
                break;
// -----------------------------------------------------------------------------
            case AstHeader::VarDecl:
            case AstHeader::VarDecl_Type:
            case AstHeader::VarDecl_ConstInit:
            case AstHeader::VarDecl_MutableInit:
            case AstHeader::VarDecl_TypeConstInit:
            case AstHeader::VarDecl_TypeMutableInit:
                PRINT("VarDecl");
                print_node(ip[1], depth + 1);
                ip += 2;
                if (ast_vardecl_header_has_type(header)) {
                    PRINT(".TypeExpr");
                    print_node(*ip, depth + 1);
                    ip += 1;
                }
                if (ast_vardecl_header_has_init(header)) {
                    PRINT(".{}", ast_vardecl_header_is_constant(header) ? "ConstInit" : "MutableInit");
                    print_node(*ip, depth + 1);
                }
                break;
            case AstHeader::Function_Definition:
            case AstHeader::Function_Prototype:
                PRINT("{}", get_ast_header_name(header));
                PRINT(".Params");
                print_list(ip[1], depth + 1);
                PRINT(".ReturnType");
                print_node(ip[2], depth + 1);
                if (header == AstHeader::Function_Definition) {
                    PRINT(".Body");
                    print_list(ip[3], depth + 1);
                }
                break;
            case AstHeader::Defer:
                PRINT("Defer");
                print_list(ip[1], depth + 1);
                break;
            case AstHeader::Call:
            case AstHeader::Subscript:
                PRINT("{}", get_ast_header_name(header));
                print_node(ip[1], depth + 1);
                PRINT(".Args");
                print_list(ip[2], depth + 1);
                break;
            case AstHeader::While:
            case AstHeader::WhileElse:
                PRINT("While");
                print_node(ip[1], depth + 1);
                PRINT(".Body");
                print_list(ip[2], depth + 1);
                if (header == AstHeader::WhileElse) {
                    PRINT(".Else");
                    print_list(ip[3], depth + 1);
                }
                break;
            case AstHeader::For:
            case AstHeader::ForElse:
                PRINT("For");
                print_node(ip[1], depth + 1);
                PRINT(".Params");
                print_list(ip[2], depth + 1);
                PRINT(".Body");
                print_list(ip[3], depth + 1);
                if (header == AstHeader::ForElse) {
                    PRINT(".Else");
                    print_list(ip[4], depth + 1);
                }
                break;
            case AstHeader::If:
            case AstHeader::IfElse:
                PRINT("If");
                print_node(ip[1], depth + 1);
                PRINT(".Then");
                print_list(ip[2], depth + 1);
                if (header == AstHeader::IfElse) {
                    PRINT(".Else");
                    print_list(ip[3], depth + 1);
                }
                break;
            default:
                raise_error("unhandled ast header: {}", get_ast_header_name(header));
        }

        #undef PRINT
    };

    print_node_root(offset, 0);
}

// -----------------------------------------------------------------------------

inline
void ast_print_details(Constants& constants, Ast& ast, AstOffset offset, std::string_view source)
{
    Ast compacted;
    ast_compact(constants, ast, offset, compacted);

#if 1
    log_debug("Linked");
    ast_print_flat(constants, ast);

    log_debug("Compacted");
    ast_print_flat(constants, compacted);
#endif

    log_debug("Tree");
    ast_print_tree(constants, ast, offset);

    static std::vector<std::byte> storage;
    auto test_calculate_compressed_size = [&](const void* data, size_t size)
    {
        storage.resize(ZSTD_compressBound(size));
        return ZSTD_compress(storage.data(), storage.size(), data, size, 0);
    };

    auto compress_size = test_calculate_compressed_size(source.data(), source.size());

    log_debug("unique strings = {} / {} ({:.2f}x)", constants.strings.cache.size(), constants.strings.total_lookups, f64(constants.strings.total_lookups) / constants.strings.cache.size());
    log_debug("unique numbers = {} / {} ({:.2f}x)", constants.numbers.cache.size(), constants.numbers.total_lookups, f64(constants.numbers.total_lookups) / constants.numbers.cache.size());

    log_trace("            source = {}", byte_size_to_string(source.size()));
    log_trace(" compressed source = {}", byte_size_to_string(compress_size));

    auto comparison_size = source.size();

    compress_size = test_calculate_compressed_size(compacted.code.data(), compacted.code.size() * 4);
    log_trace("    compressed AST = {}", byte_size_to_string(compress_size));

	log_debug(" ast size = {:9} ({:5.2f}x)",
        byte_size_to_string(ast.code.size() * 4),
        f64(ast.code.size() * 4) / comparison_size);

    log_debug("compacted = {:9} ({:5.2f}x)",
        byte_size_to_string(compacted.code.size() * 4),
        f64(compacted.code.size() * 4) / comparison_size);
}

NAMESPACE_END
