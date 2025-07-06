#if 0
#include "bytecode.hpp"

NAMESPACE_BEGIN(lang)

FileContext* file_context_create()
{
    return new FileContext {
        ._namespace = new Namespace(),
    };
}

BytecodeExecutionContext* bytecode_execution_context_create(FileContext* file, BytecodeChunk* chunk)
{
    auto ctx = new BytecodeExecutionContext{};
    ctx->file = file;
    ctx->frames.push_back({
        // .chunk = chunk,
        .ip = chunk->bytecode.data(),
    });
    return ctx;
}

struct LoadResult
{
    ValueRef value;
    bool blocked = false;
};

static
LANG_FORCEINLINE
bool requires_loading(ValueType type)
{
    switch (type) {
        case ValueType::NamespaceAccess:
        case ValueType::LocalAccess:
            return true;
        default:
            return false;
    }
}

static
LANG_FORCEINLINE
LoadResult try_load_value(BytecodeExecutionContext* context, ValueRef ref)
{
    switch (ref.type) {
        case ValueType::NamespaceAccess: {
            auto namespace_access = static_cast<NamespaceAccess*>(ref.ptr);
            auto loaded = namespace_access->_namespace->get(namespace_access->field);
            if (loaded.ptr) {
                return { loaded };
            } else {
                return { ref, true };
            }
        }
        case ValueType::LocalAccess: {
            auto local_access = static_cast<LocalAccess*>(ref.ptr);
            auto val = context->stack.peek_at(local_access->local_index);
            return { val };
        }
        default:
            return { ref };
    }
}

LANG_FORCEINLINE
LoadResult try_load_values(BytecodeExecutionContext* context, ValueRef* refs, u32 first_idx, u32 num_values)
{
    for (auto i = 0; i < num_values; ++i) {
        auto[val, blocked] = try_load_value(context, context->stack.peek_at(first_idx + i));
        if (blocked) return { val, true };
        refs[i] = val;
    }

    return {};
}

#define TRY_LOAD_VALUES(refs, first_idx, num_values) do \
    if (auto res = try_load_values(context, refs, first_idx, num_values); res.blocked) { \
        return { ExecutionResultCode::Blocked, res.value }; \
    } while (0)

// static
// LANG_FORCEINLINE
// void rtcheck_expect_type(ValueType type, ValueType expected)
// {
//     if (type != expected) raise_error("expected {}, got {}", get_value_type_name(expected), get_value_type_name(type));
// }
#define rtcheck_expect_type(...)

ExecutionResult BytecodeExecutionContext::run()
{
    auto context = this;

    auto* frame = &context->frames.back();

    auto* ip = frame->ip;

    auto stack_contents_to_str = [&] {
        std::string stack_contents{};
        for (u32 i = 0; i < stack.top; ++i) {
            stack_contents += " ";
            stack_contents += value_to_string(stack.stack_entries[i].type, stack.stack_data.data() + stack.stack_entries[i].offset);
        }
        return stack_contents;
    };

#define OPCODE_COUNT(opcode, ...) + 1
#define OPCODE_LABELS(opcode, ...) [std::to_underlying(Opcode::opcode)] = &&GOTO_##opcode,
#define OPCODE_CASE(opcode, ...) GOTO_##opcode

    std::array<void*, OPCODE_LIST(OPCODE_COUNT)> jump_table {
        {OPCODE_LIST(OPCODE_LABELS)}
    };

#define DISPATCH goto *jump_table[std::to_underlying(ip->opcode)]
// #define DISPATCH log_trace("instr = {:20} stack.size = [{:2}:{:3}] stack = [{} ]", get_opcode_name(ip->opcode), stack.top, stack.next_offset, stack_contents_to_str()); \
//     goto *jump_table[std::to_underlying(ip->opcode)]

    DISPATCH;

    // auto counts = std::to_array<std::pair<Opcode, size_t>>({
    //     OPCODE_LIST(OPCODE_COUNT)
    // });

    // #define OPCODE_CASE(name) case Opcode::name
    // #define DISPATCH break

    // // for (; frame->ip < frame->chunk->bytecode.size(); frame->ip++) {
    // for (;;) {
    //     // defer { frame->ip++; };
    //     // if (frame->ip >= frame->chunk->bytecode.size()) {
    //     //     frames.pop_back();
    //     //     if (frames.empty()) {
    //     //         log_warn("no more frames, exiting!");
    //     //         break;
    //     //     } else {
    //     //         frame = &frames.back();
    //     //         // log_warn("down one frame");
    //     //         continue;
    //     //     }
    //     // }

    //     // auto& instr = frame->chunk->bytecode[frame->ip];
    //     // log_trace(">> ip = {} stack.size = [{}:{}] stack = [{} ]", frame->ip, stack.top, stack.next_offset, stack_contents_to_str());
    //     // if (instr.opcode == Opcode::Operator) {
    //     //     log_trace("   op = Operator '{}'", get_op_name(instr.op));
    //     // } else {
    //     //     log_trace("   op = {}", get_opcode_name(instr.opcode));
    //     // }

    //     // counts[size_t(instr.opcode)].second++;

    //     switch (ip->opcode) {
    OPCODE_CASE(LoadNil): {
        raise_error("unsupported");
    }
    OPCODE_CASE(LoadString): {
        raise_error("unsupported");
    }
    OPCODE_CASE(LoadNumber): {
        stack.push(Number{.value = ip->number});
        ip++;
        DISPATCH;
    }
    OPCODE_CASE(LoadBoolean): {
        stack.push(Boolean{.value = ip->boolean});
        ip++;
        DISPATCH;
    }
    OPCODE_CASE(LoadFunction): {
        stack.push(Function{.node = ip->ast_function});
        ip++;
        DISPATCH;
    }
    OPCODE_CASE(NamespaceVarDecl): {
        ValueRef init;
        if (ip->decl_has_init) TRY_LOAD_VALUES(&init, stack.top - 1, 1);

        auto target = stack.peek_at(stack.top - 2);

        rtcheck_expect_type(target.type, ValueType::NamespaceAccess);
        auto access = static_cast<NamespaceAccess*>(target.ptr);
        access->_namespace->set(access->field, copy_to_owned(init));
        context->job_system->try_unblock(access);

        stack.set_top(stack.top - 2);

        ip++;
        DISPATCH;
    }
    OPCODE_CASE(LoadGlobal): {
        stack.push(NamespaceAccess{context->file->_namespace, ip->token.raw});
        ip++;
        DISPATCH;
    }
    OPCODE_CASE(LoadLocal): {
        // log_debug("   stack_idx_offset = {}", frame->stack_idx_offset);
        // log_debug("   instr.stack_index = {}", instr.stack_index);

        auto value = stack.peek_at(ip->stack_index + frame->stack_idx_offset);
        if (value.type == ValueType::LocalAccess) {
            // log_debug("   forwarding existing local access {}", value_to_string(value));
            stack.push(value);
        } else {
            stack.push(LocalAccess{ip->stack_index + frame->stack_idx_offset});
        }

        ip++;
        DISPATCH;
    }
    OPCODE_CASE(LoadImmediate): {
        if (requires_loading(stack.peek().type)) {
            ValueRef value;
            TRY_LOAD_VALUES(&value, stack.top - 1, 1);
            stack.set_top(stack.top - 1);
            stack.push(value);
        }
        ip++;
        DISPATCH;
    }
    OPCODE_CASE(FieldAccess): {
        raise_error("unsupported");
    }
    OPCODE_CASE(Operator):
        switch (ip->op) {
            case Operator::Multiply: {
                ValueRef vals[2];
                TRY_LOAD_VALUES(vals, stack.top - 2, 2);
                double res = *static_cast<double*>(vals[0].ptr) * *static_cast<double*>(vals[1].ptr);
                stack.set_top_and_push(stack.top - 2, Number{res});
                break;
            }
            case Operator::Add: {
                ValueRef vals[2];
                TRY_LOAD_VALUES(vals, stack.top - 2, 2);
                double res = *static_cast<double*>(vals[0].ptr) + *static_cast<double*>(vals[1].ptr);
                stack.set_top_and_push(stack.top - 2, Number{res});
                break;
            }
            case Operator::Subtract: {
                ValueRef vals[2];
                TRY_LOAD_VALUES(vals, stack.top - 2, 2);
                double res = *static_cast<double*>(vals[0].ptr) - *static_cast<double*>(vals[1].ptr);
                stack.set_top_and_push(stack.top - 2, Number{res});
                break;
            }
            case Operator::Less: {
                ValueRef vals[2];
                TRY_LOAD_VALUES(vals, stack.top - 2, 2);
                bool res = *static_cast<double*>(vals[0].ptr) < *static_cast<double*>(vals[1].ptr);
                stack.set_top_and_push(stack.top - 2, Boolean{res});
                break;
            }
            case Operator::Assign: {
                ValueRef rhs;
                TRY_LOAD_VALUES(&rhs, stack.top - 1, 1);

                auto lhs = stack.peek_at(stack.top - 2);

                rtcheck_expect_type(ValueType::LocalAccess, lhs.type);

                switch (lhs.type) {
                    case ValueType::LocalAccess: {
                        auto access = static_cast<LocalAccess*>(lhs.ptr);
                        stack.replace(access->local_index, rhs);

                        break;
                    }
                    default:
                        raise_error("invalid lhs for assign: {}", get_value_type_name(lhs.type));
                }

                stack.set_top(stack.top - 1);

                break;
            }
            default:
                raise_error("currently supported operator: {}", get_op_name(ip->op));
        }
        ip++;
        DISPATCH;
    OPCODE_CASE(Pop): {
        stack.set_top(stack.top - ip->count);
        ip++;
        DISPATCH;
    }
    OPCODE_CASE(PushDown): {
        auto value = stack.peek();
        // log_debug("   instr.count = {}", instr.count);
        auto new_top = stack.top - (ip->count + 1);
        // log_debug("   top     = {}", stack.top);
        // log_debug("   new_top = {}", new_top);
        stack.set_top(new_top);
        if (value.type == ValueType::LocalAccess) {
            auto access = static_cast<LocalAccess*>(value.ptr);
            if (access->local_index >= new_top) {
                // we're moving below the target local reference, replace with local value directly
                stack.push(stack.peek_at(access->local_index));
                ip++;
                DISPATCH;
            }
        }
        stack.push(value);
        ip++;
        DISPATCH;
    }
    OPCODE_CASE(JumpIfFalse): {
        ValueRef cond;
        TRY_LOAD_VALUES(&cond, stack.top - 1, 1);

        // TODO: Truthiness function
        bool truthy = true;
        if (cond.type == ValueType::Boolean) truthy = static_cast<Boolean*>(cond.ptr)->value;
        else if (cond.type == ValueType::Nil) truthy = false;
        else if (cond.type == ValueType::Number) truthy = static_cast<Number*>(cond.ptr)->value;

        stack.set_top(stack.top - 1);

        if (!truthy) {
            ip += ip->jump_delta;
        } else {
            ip++;
        }

        DISPATCH;
    }
    OPCODE_CASE(Jump): {
        ip += ip->jump_delta;
        DISPATCH;
    }
    OPCODE_CASE(CallFunction): {
        ValueRef callee;
        TRY_LOAD_VALUES(&callee, stack.top - 1, 1);

        rtcheck_expect_type(callee.type, ValueType::Function);

        auto* function = static_cast<Function*>(callee.ptr);
        if (!function->bytecode) {
            log_warn("Generating function body bytecode");
            function->bytecode = bytecode_generate_function_body(function->node);

            log_debug("");
            log_debug("==== Fragment ====");
            ast_debug_print(function->node);
            log_debug("==== Bytecode ====");
            bytecode_debug_print(function->bytecode);
        }

        stack.set_top(stack.top - ip->arg_count);

        frame->ip = ++ip;

        frame = &frames.emplace_back(Frame {
            .ip = function->bytecode->bytecode.data(),
            .stack_idx_offset = stack.top - 1,
        });

        ip = frame->ip;
        DISPATCH;
    }
    OPCODE_CASE(CallSubscript): {
        raise_error("unsupported");
    }
    OPCODE_CASE(Return):
    {
        frames.pop_back();
        frame = &frames.back();
        ip = frame->ip;
        DISPATCH;
    }
    //         default:
    //             raise_error("currently unsupported opcode: {}", get_opcode_name(instr.opcode));
    //     }
    // }
    OPCODE_CASE(Halt):

    log_trace(">> COMPLETE: stack.size = [{}:{}] stack = [{} ]", stack.top, stack.next_offset, stack_contents_to_str());

    // std::ranges::sort(counts, std::greater{}, [](auto& pair) {
    //     return pair.second;
    // });
    // for (auto[opcode, count] : counts) {
    //     if (count) {
    //         log_info("opcode[{:20}] = {}", get_opcode_name(opcode), count);
    //     }
    // }

    return {
        .code = ExecutionResultCode::Success,
        .value = stack.top ? stack.peek() : ValueRef{ValueType::_invalid},
    };
}

NAMESPACE_END
#endif
