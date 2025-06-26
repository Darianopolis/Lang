#pragma once

#include <lang/core.hpp>
#include <lang/lang.hpp>

#include <ankerl/unordered_dense.h>

NAMESPACE_BEGIN(lang)

#define OPCODE_LIST(DO) \
    DO(LoadNumber) \
    DO(LoadString) \
    DO(LoadNamedReference) \
    \
    DO(CallFunction) \
    DO(CallSubscript) \
    \
    DO(FieldAccess) \
    DO(VarDecl) \
    \
    DO(Operator)

#define OPCODE_DECLARE(op, ...) op,

enum class Opcode
{
    OPCODE_LIST(OPCODE_DECLARE)
};

constexpr
std::string_view get_opcode_name(Opcode opcode)
{
#define OPCODE_SWITCH_RETURN_NAME(op, ...) case Opcode::op: return #op;

    switch (opcode) {
        OPCODE_LIST(OPCODE_SWITCH_RETURN_NAME)
    }
}

struct Instruction
{
    Opcode opcode;
    union {
        Token token = {};
        Operator op;
        u32 arg_count;
        bool decl_has_init;
    };
};

struct BytecodeChunk
{
    std::vector<Instruction> bytecode;
};

BytecodeChunk* bytecode_generate(AstNode* node);
void bytecode_debug_print(BytecodeChunk*);

// -----------------------------------------------------------------------------

#define VALUE_TYPE_LIST(DO) \
    DO(Number) \
    DO(Collection) \
    DO(CollectionAccess)

#define VALUE_TYPE_DECLARE(name, ...) name,

enum class ValueType
{
    VALUE_TYPE_LIST(VALUE_TYPE_DECLARE)
};

constexpr
std::string_view get_value_type_name(ValueType type)
{
#define VALUE_TYPE_SWITCH_RETURN_NAME(name, ...) case ValueType::name: return #name;

    switch (type) {
        VALUE_TYPE_LIST(VALUE_TYPE_SWITCH_RETURN_NAME)
    }
}

struct Value
{
    ValueType type;
};

struct Number : Value
{
    double value;
};

struct Collection : Value
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
            raise_error("cannot reassign collection value");
        }
        values.emplace(key, value);
    }
};

struct CollectionAccess : Value
{
    Collection* collection;
    Token field;
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
    switch (value->type) {
        case ValueType::Number: {
            auto num = static_cast<Number*>(value);
            return std::to_string(num->value);
        }
        default:
            raise_error("value_to_string :: unsupported value type: {}", get_value_type_name(value->type));
    }
}

// -----------------------------------------------------------------------------

struct CompileJobSystem;

struct FileContext
{
    Collection* global;
};

struct ExecutionContext
{
    CompileJobSystem* job_system;
    FileContext* file;
    BytecodeChunk* chunk;
    std::vector<Value*> stack;
    u32 ip;
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

// -----------------------------------------------------------------------------

FileContext* file_context_create();
ExecutionContext* execution_context_create(FileContext* file, BytecodeChunk* chunk);
ExecutionResult execution_context_run(ExecutionContext* context);

// -----------------------------------------------------------------------------

struct CompileJobSystem
{
    std::vector<std::pair<ExecutionContext*, CollectionAccess*>> blocked;
    std::vector<ExecutionContext*> unblocked;
    std::vector<std::pair<ExecutionContext*, ExecutionResult>> completed;

    void add_fragment(AstNode* node, FileContext* file_context);
    void try_unblock(CollectionAccess* access);
    void compile();
};

NAMESPACE_END
