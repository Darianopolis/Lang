#if 0
#pragma once

#include <lang/core.hpp>
#include <lang/lang.hpp>

#include <ankerl/unordered_dense.h>

NAMESPACE_BEGIN(lang)

#define VALUE_TYPE_LIST(DO) \
    DO(Nil, Nil) \
    DO(Boolean, Boolean) \
    DO(Number, Number) \
    DO(Namespace, Namespace) \
    DO(NamespaceAccess, NamespaceAccess) \
    DO(LocalAccess, LocalAccess) \
    DO(NativeFunction, NativeFunction) \
    DO(String, String) \
    DO(Function, Function)

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
        case ValueType::_invalid: raise_error("get_value_type_name :: invalid value type");
        VALUE_TYPE_LIST(VALUE_TYPE_SWITCH_RETURN_NAME)
    }
}

struct ValueRef
{
    ValueType type;
    void* ptr;
};

struct Nil
{};

struct Boolean
{
    bool value;
};

struct Number
{
    f64 value;
};

struct Namespace
{
    ankerl::unordered_dense::map<std::string_view, ValueRef> values;

    bool has(std::string_view key)
    {
        return values.find(key) != values.end();
    }

    ValueRef get(std::string_view key)
    {
        auto iter = values.find(key);
        if (iter == values.end()) return {ValueType::Nil};
        return iter->second;
    }

    void set(std::string_view key, ValueRef value)
    {
        if (has(key)) {
            raise_error("cannot reassign namespace value");
        }
        values.emplace(key, value);
    }
};

struct ExecutionContext;

struct Function
{
    AstFunction* node;
    struct BytecodeChunk* bytecode = nullptr;
};

struct NativeFunction
{
    std::function<void(ExecutionContext*, u32 stack_index)> function;
};

struct NamespaceAccess
{
    Namespace* _namespace;
    std::string_view field;
};

struct LocalAccess
{
    u32 local_index;
};

struct String
{
    // TODO: Temporary, will be represented as array of 8 bit unsigned integers

    std::string value;
};

inline
std::string value_to_string(ValueType type, void* data)
{
    switch (type) {
        case ValueType::Nil:
            return "Nil";
        case ValueType::Boolean: {
            auto boolean = static_cast<Boolean*>(data);
            return boolean->value ? "True" : "False";
        }
        case ValueType::Number: {
            auto num = static_cast<Number*>(data);
            return std::to_string(num->value);
        }
        case ValueType::String: {
            return (*static_cast<String**>(data))->value;
        }
        case ValueType::Function: {
            return std::format("Function({})", (void*)static_cast<Function*>(data)->node);
        }
        case ValueType::NamespaceAccess: {
            auto access = static_cast<NamespaceAccess*>(data);
            return std::format("NamespaceAccess({} in [{}])", access->field, (void*)access->_namespace);
        }
        case ValueType::LocalAccess: {
            auto access = static_cast<LocalAccess*>(data);
            return std::format("LocalAccess({})", access->local_index);
        }
        default:
            raise_error("value_to_string :: unhandled value type: {}", get_value_type_name(type));
    }
}
inline
std::string value_to_string(ValueRef ref)
{
    return value_to_string(ref.type, ref.ptr);
}

template<typename T>
consteval ValueType get_value_type()
{
#define VALUE_TYPE_TAG(Type, ...) if constexpr (std::same_as<T, Type>) { return ValueType::Type; }

    VALUE_TYPE_LIST(VALUE_TYPE_TAG)
}

constexpr
size_t get_value_type_size(ValueType type)
{
#define VALUE_TYPE_SWITCH_SIZE(name, structure, ...) case ValueType::name: return sizeof(structure);
    switch (type) {
        case ValueType::_invalid: raise_error("get_value_type_size :: invalid value type");
        VALUE_TYPE_LIST(VALUE_TYPE_SWITCH_SIZE)
    }
}

constexpr
size_t get_value_type_align(ValueType type)
{
#define VALUE_TYPE_SWITCH_ALIGN(name, structure, ...) case ValueType::name: return alignof(structure);
    switch (type) {
        case ValueType::_invalid: raise_error("get_value_type_align :: invalid value type");
        VALUE_TYPE_LIST(VALUE_TYPE_SWITCH_ALIGN)
    }
}

constexpr
ValueRef copy_to_owned(ValueRef ref)
{
    switch (ref.type) {
#define VALUE_TYPE_SWITCH_COPY(name, structure, ...) case ValueType::name: return { ValueType::name, new structure(*static_cast<structure*>(ref.ptr))};
        VALUE_TYPE_LIST(VALUE_TYPE_SWITCH_COPY)
        default: raise_error("unsupported value type: {}", get_value_type_name(ref.type));
    }
}

constexpr
void copy_to(void* target, ValueRef ref)
{
    switch (ref.type) {
#define VALUE_TYPE_SWITCH_COPY_TO(name, structure, ...) \
        case ValueType::name: { \
            std::memmove(target, ref.ptr, get_value_type_size(ref.type)); \
            break; \
        }
        VALUE_TYPE_LIST(VALUE_TYPE_SWITCH_COPY_TO)
        default: raise_error("unsupported value type: {}", get_value_type_name(ref.type));
    }
}

template<typename T>
constexpr
T align_up_pow2(T v, u64 align) noexcept
{
    return T((u64(v) + (align - 1)) &~ (align - 1));
}

// -----------------------------------------------------------------------------

struct StackEntry
{
    ValueType type;
    u32 offset;
};

struct VmStack
{
    std::vector<StackEntry> stack_entries = std::vector<StackEntry>(2<<16);
    std::vector<std::byte>  stack_data    = std::vector<std::byte>(2<<16);
    u32 top = 0;
    u32 next_offset = 0;

    template<typename T>
    void push(const T& value)
    {

        if (top >= stack_entries.size()) {
            raise_error("vm stack overflow");
        }

        constexpr auto type = get_value_type<std::remove_cvref_t<T>>();
        constexpr auto size = get_value_type_size(type);
        constexpr auto align = get_value_type_align(type);

        auto new_offset = align_up_pow2(next_offset, align);
        next_offset = new_offset + size;

        stack_entries[top] = { type, new_offset };
        std::memmove(stack_data.data() + new_offset, &value, size);
        top++;
    }

    void push(ValueRef ref)
    {
        switch (ref.type) {
#define VALUE_TYPE_SWITCH_COPY_TO_STACK(name, structure, ...) case ValueType::name: push(*static_cast<structure*>(ref.ptr)); break;
            VALUE_TYPE_LIST(VALUE_TYPE_SWITCH_COPY_TO_STACK)
            default: raise_error("push_from :: invalid value type");
        }
    }

    void replace(u32 index, ValueRef ref)
    {
        auto target = stack_entries[index];
        if (target.type != ref.type) {
            raise_error("cannot reassign type from {} to {}",
                get_value_type_name(target.type),
                get_value_type_name(ref.type));
        }

        copy_to(stack_data.data() + target.offset, ref);
    }

    void set_top(u32 index)
    {
        top = index;
        next_offset = index
            ? stack_entries[index - 1].offset + get_value_type_size(stack_entries[index - 1].type)
            : 0;
    }

    template<typename T>
    void set_top_and_push(u32 index, const T& value)
    {
        set_top(index);
        push(value);
    }

    ValueRef peek_at(u32 idx)
    {
        auto entry = stack_entries[idx];
        // log_trace("stack[{}] = {{ type = {}, value = {} }}", idx, get_value_type_name(entry.type), value_to_string(entry.type, stack_data.data() + entry.offset));
        return {
            .type = entry.type,
            .ptr = stack_data.data() + entry.offset,
        };
    }

    ValueRef peek()
    {
        if (!top) raise_error("stack empty");

        return peek_at(top - 1);
    }
};

// -----------------------------------------------------------------------------

struct CompileJobSystem;

// struct GlobalContext
// {
//     uint32_t next_identifier = 1;
//     ankerl::unordered_dense::map<std::string_view, uint32_t> identifiers;

//     uint32_t get_identifier_id(std::string_view str)
//     {
//         auto& id = identifiers[str];
//         if (!id) {
//             id = next_identifier++;
//             if (!next_identifier) raise_error("too many identifiers");
//         }
//         return id;
//     }
// };

struct FileContext
{
    // GlobalContext* global_context;
    Namespace* _namespace;
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
    ValueRef value;
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
#endif
