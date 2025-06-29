#pragma once

#include <lang/core.hpp>
#include <lang/lang.hpp>

#include <ankerl/unordered_dense.h>

NAMESPACE_BEGIN(lang)

#define VALUE_TYPE_LIST(DO) \
    DO(Number) \
    DO(Namespace) \
    DO(NamespaceAccess) \
    DO(LocalAccess) \
    DO(NativeFunction) \
    DO(String)

#define VALUE_TYPE_DECLARE(name, ...) name,

enum class ValueType
{
    _invalid,

    VALUE_TYPE_LIST(VALUE_TYPE_DECLARE)
};

constexpr
std::string_view get_value_type_name(ValueType type)
{
#define VALUE_TYPE_SWITCH_RETURN_NAME(name, ...) case ValueType::name: return #name;

    switch (type) {
        case ValueType::_invalid: raise_error("invalid value type");
        VALUE_TYPE_LIST(VALUE_TYPE_SWITCH_RETURN_NAME)
    }
}

struct Value
{
    ValueType type;
};

struct Number : Value
{
    f64 value;
};

struct Namespace : Value
{
    ankerl::unordered_dense::map<std::string_view, Value*> values;

    bool has(std::string_view key)
    {
        return values.find(key) != values.end();
    }

    Value* get(std::string_view key)
    {
        auto iter = values.find(key);
        if (iter == values.end()) return nullptr;
        return iter->second;
    }

    void set(std::string_view key, Value* value)
    {
        if (has(key)) {
            raise_error("cannot reassign namespace value");
        }
        values.emplace(key, value);
    }
};

struct ExecutionContext;

struct NativeFunction : Value
{
    std::function<Value*(ExecutionContext*, std::span<Value* const> args)> function;
};

struct NamespaceAccess : Value
{
    Namespace* _namespace;
    Token field;
};

struct LocalAccess : Value
{
    u32 local_index;
};

struct String : Value
{
    // TODO: Temporary, will be represented as array of 8 bit unsigned integers

    std::string value;
};

template<typename T>
T* tag_value(T* value)
{
#define VALUE_TYPE_TAG(Type, ...) if constexpr (std::same_as<T, Type>) { value->type = ValueType::Type; }

    VALUE_TYPE_LIST(VALUE_TYPE_TAG)

    return value;
}

inline
std::string value_to_string(Value* value)
{
    if (!value) {
        return "Nil";
    }
    switch (value->type) {
        case ValueType::Number: {
            auto num = static_cast<Number*>(value);
            return std::to_string(num->value);
        }
        case ValueType::String: {
            return static_cast<String*>(value)->value;
        }
        case ValueType::NamespaceAccess: {
            auto access = static_cast<NamespaceAccess*>(value);
            return std::format("NamespaceAccess({} in [{}])", access->field.raw, (void*)access->_namespace);
        }
        case ValueType::LocalAccess: {
            auto access = static_cast<LocalAccess*>(value);
            return std::format("LocalAccess({})", access->local_index);
        }
        default:
            raise_error("value_to_string :: unhandled value type: {}", get_value_type_name(value->type));
    }
}

// -----------------------------------------------------------------------------

struct CompileJobSystem;

struct FileContext
{
    Namespace* global;
};

enum class ExecutionResultCode
{
    Success,
    Error,
    Blocked,
};

struct ExecutionResult
{
    ExecutionResultCode code;
    Value* value;
};

struct ExecutionContext
{
    CompileJobSystem* job_system;
    FileContext* file;

    virtual ~ExecutionContext() = default;
    virtual ExecutionResult run() = 0;
};

// -----------------------------------------------------------------------------

FileContext* file_context_create();

// -----------------------------------------------------------------------------

struct CompileJobSystem
{
    std::vector<std::pair<ExecutionContext*, NamespaceAccess*>> blocked;
    std::vector<ExecutionContext*> unblocked;
    std::vector<std::pair<ExecutionContext*, ExecutionResult>> completed;

    void add_fragment(AstNode* node, FileContext* file_context);
    void try_unblock(NamespaceAccess* access);
    void compile();
};

NAMESPACE_END
