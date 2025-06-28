
#include "interpret.hpp"

NAMESPACE_BEGIN(lang)

FileContext* file_context_create()
{
    return new FileContext {
        .global = tag_value(new Namespace()),
    };
}

ExecutionContext* execution_context_create(FileContext* file, BytecodeChunk* chunk)
{
    return new ExecutionContext {
        .file = file,
        .chunk = chunk,
        .ip = 0,
    };
}

struct LoadResult
{
    bool blocked = false;
    Value* blocking_access = nullptr;
};

static Value* load_value(Value* value, LoadResult& load_res)
{
    switch (value->type) {
        case ValueType::NamespaceAccess: {
            auto namespace_access = static_cast<NamespaceAccess*>(value);
            // log_trace("attempting to access field [{}] in namespace [{}]", namespace_access->field.raw, (void*)namespace_access->_namespace);
            auto loaded = namespace_access->_namespace->get(namespace_access->field.raw);
            if (loaded) {
                // log_trace("  found");
                load_res = { false };
                return loaded;
            } else {
                // log_trace("  not found");
                log_trace("   waiting on field [{}] in namespace [{}]...", namespace_access->field.raw, (void*)namespace_access->_namespace);
                load_res = { true, value };
                return nullptr;
            }
        }
        default:
            load_res = { false };
            return value;
    }
}

LoadResult try_pop_and_load_values(ExecutionContext* context, Value** out_values, u32 num_values)
{
    for (u32 i = 0; i < num_values; ++i) {
        auto preload = context->stack.back();
        context->stack.pop_back();

        LoadResult res;
        auto loaded = load_value(preload, res);
        if (res.blocked) {
            context->stack.push_back(res.blocking_access);
            if (i > 0) {
                for (u32 j = i; j--;) {
                    context->stack.push_back(out_values[j]);
                }
            }
            return { true, preload };
        }

        out_values[i] = loaded;
    }

    return { false };
}

ExecutionResult execution_context_run(ExecutionContext* context)
{
    auto& bytecode = context->chunk->bytecode;
    auto& stack = context->stack;

    auto bc_size = bytecode.size();
    auto& ip = context->ip;

    auto pop = [&] -> Value* {
        auto top = stack.back();
        stack.pop_back();
        return top;
    };

    auto push = [&](Value* v) {
        stack.push_back(v);
    };

#define EXPECT_NUMBER(val) static_cast<Number*>(val); if (val->type != ValueType::Number) { \
    raise_error("expected number, got: {}", get_value_type_name(val->type)); \
}

#define BINARY_NUMBER_OP(op_sym, op_name) \
    case Operator::op_name: { \
        Value* values[2]; \
        auto res = try_pop_and_load_values(context, values, 2); \
        if (res.blocked) return { .code = ExecutionResultCode::Blocked, .value = res.blocking_access }; \
        \
        auto lhs_num = EXPECT_NUMBER(values[1]); \
        auto rhs_num = EXPECT_NUMBER(values[0]); \
        log_trace("{} {} {} = {}", lhs_num->value, #op_sym, rhs_num->value, lhs_num->value op_sym rhs_num->value); \
        \
        push(tag_value(new Number{ .value = lhs_num->value op_sym rhs_num->value })); \
        \
        break; \
    }

    for (; ip < bc_size; ip++) {

        std::string stack_contents{};
        for (auto& v : stack) {
            stack_contents += " ";
            stack_contents += value_to_string(v);
        }
        log_trace(">> ip = {} stack.size = {} stack = [{} ]", ip, stack.size(), stack_contents);

        auto& instr = bytecode[ip];
        log_trace("   op = {}", get_opcode_name(instr.opcode));
        switch (instr.opcode) {
            case Opcode::LoadNil: {
                stack.emplace_back(nullptr);
                break;
            }
            case Opcode::LoadNumber: {
                f64 value;
                auto raw = instr.token.number.raw;
                log_trace("   value = {}", raw);
                auto res = std::from_chars(raw.begin(), raw.end(), value);
                if (!res) {
                    raise_error("error parsing number from token: {}", std::make_error_code(res.ec).message());
                }
                stack.emplace_back(tag_value(new Number{ .value = value }));
                break;
            }
            case Opcode::LoadGlobal: {
                // TODO: Handle locals
                push(tag_value(new NamespaceAccess{
                    ._namespace = context->file->global,
                    .field = instr.token,
                }));
                break;
            }
            case Opcode::LoadLocal: {
                push(stack.at(instr.stack_index));
                break;
            }
            case Opcode::FieldAccess: {
                Value* lhs;
                if (auto res = try_pop_and_load_values(context, &lhs, 1); res.blocked) {
                    return { ExecutionResultCode::Blocked, res.blocking_access };
                }

                switch (lhs->type) {
                    case ValueType::Namespace: {
                        auto _namespace = static_cast<Namespace*>(lhs);
                        push(tag_value(new NamespaceAccess {
                            ._namespace = _namespace,
                            .field = instr.token,
                        }));
                        break;
                    }
                    default:
                        raise_error("invalid lhs for field access");
                }
                break;
            }
            case Opcode::NamespaceVarDecl: {
                Value* init = nullptr;
                if (instr.decl_has_init) {
                    if (auto res = try_pop_and_load_values(context, &init, 1); res.blocked) {
                        return { ExecutionResultCode::Blocked, res.blocking_access };
                    }
                }

                // TODO: Temp
                if (!instr.decl_has_init) raise_error("var decl must have init (for now)");

                auto target = pop();

                if (target->type != ValueType::NamespaceAccess) raise_error("target of a variable declaration must be a namespace access");
                auto access = static_cast<NamespaceAccess*>(target);
                if (access->_namespace->has(access->field.raw)) raise_error("target of variable declaration must not already exist");

                access->_namespace->set(access->field.raw, init);
                context->job_system->try_unblock(access);

                break;
            }
            case Opcode::Operator: {
                switch (instr.op) {
                    BINARY_NUMBER_OP(+, Add)
                    BINARY_NUMBER_OP(-, Subtract)
                    BINARY_NUMBER_OP(*, Multiply)
                    BINARY_NUMBER_OP(/, Divide)

                    default:
                        raise_error("unsupported operator: {}", get_op_name(instr.op));
                }
                break;
            }
            case Opcode::Pop: {
                for (u32 i = 0; i < instr.count; ++i) {
                    pop();
                }
                break;
            }
            case Opcode::PushDown: {
                auto v = pop();
                for (u32 i = 0; i < instr.count; ++i) {
                    pop();
                }
                push(v);
                break;
            }
            case Opcode::JumpIfFalse: {
                Value* v;
                if (auto res = try_pop_and_load_values(context, &v, 1); res.blocked) {
                    return { ExecutionResultCode::Blocked, res.blocking_access };
                }

                log_trace("   cond = {}", value_to_string(v));

                bool truthy = false;
                if (v) {
                    if (v->type == ValueType::Number) {
                        if (static_cast<Number*>(v)->value) {
                            truthy = true;
                        }
                    }
                }

                log_trace("   truthy = {}", truthy);

                if (!truthy) {
                    ip = instr.target_instruction - 1; // -1 because we'll advance to target_instruction on the next loop
                }

                break;
            }
            case Opcode::Jump: {
                ip = instr.target_instruction - 1; // -1 because we'll advance to target_instruction on the next loop
                break;
            }
            default:
                raise_error("opcode not yet supported for execution: {}", get_opcode_name(instr.opcode));
        }
    }

    return {
        .code = ExecutionResultCode::Success,
        .value = stack.empty() ? nullptr : stack.back(),
    };
}

NAMESPACE_END
