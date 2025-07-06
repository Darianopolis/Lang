#pragma once

#include "lang/lang.hpp"

#include <ankerl/unordered_dense.h>

#include <vector>
#include <variant>
#include <stdfloat>

NAMESPACE_BEGIN(lang::vm)

struct GlobalContext
{

};

struct Function
{
    AstFunction* function;
};

enum class ValueType
{
    Number,
    Function,
};

struct Value
{
    ValueType type;
    union {
        __float128 number;
        Function function;
    };
};

struct Namespace
{
    ankerl::unordered_dense::map<std::string_view, Value> entries;
};

struct Compiler
{
    struct Impl;

    void add_fragment(AstNode* node);
    void run();
};

NAMESPACE_END
