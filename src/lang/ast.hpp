#pragma once

#include "core.hpp"
#include "tokens.hpp"

NAMESPACE_BEGIN(lang)

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

AstOffset parse(Ast& out, TokenList& tokens, Constants& constants);

void ast_print_flat(Constants& constants, Ast& ast);
void ast_compact(Constants& constants, Ast& ast, AstOffset offset, Ast& compacted);
void ast_print_tree(Constants& constants, Ast& ast, AstOffset offset);
void ast_print_details(Constants& constants, Ast& ast, AstOffset offset, std::string_view source);

NAMESPACE_END
