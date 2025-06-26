
#include "interpret.hpp"

NAMESPACE_BEGIN(lang)

FileContext* file_context_create()
{
    return new FileContext {
        .global = tag_value(new Collection()),
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

enum class LoadResult
{
    Success,
    Failure,
    Block,
};

static Value* load_value(Value* value, LoadResult& load_res)
{
    switch (value->type) {
        case ValueType::CollectionAccess: {
            auto collection_access = static_cast<CollectionAccess*>(value);
            log_trace("attempting to access field [{}] in collection [{}]", collection_access->field.raw, (void*)collection_access->collection);
            auto loaded = collection_access->collection->get(collection_access->field.raw);
            if (loaded) {
                log_trace("  found");
                load_res = LoadResult::Success;
                return loaded;
            } else {
                log_trace("  not found");
                load_res = LoadResult::Block;
                return nullptr;
            }
        }
        default:
            load_res = LoadResult::Success;
            return value;
    }
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

    LoadResult load_res;
#define LOAD_VALUE(val) load_value((val), load_res); \
    if (load_res == LoadResult::Block) return ExecutionResult { .code = ExecutionResultCode::Blocked }

#define EXPECT_NUMBER(val) static_cast<Number*>(val); if (val->type != ValueType::Number) { \
    log_error("expected number, got: {}", get_value_type_name(val->type)); \
    return ExecutionResult { .code = ExecutionResultCode::Error }; \
}

#define BINARY_NUMBER_OP(op_sym, op_name) \
    case Operator::op_name: { \
        auto rhs = LOAD_VALUE(pop()); \
        auto lhs = LOAD_VALUE(pop()); \
        \
        auto lhs_num = EXPECT_NUMBER(lhs); \
        auto rhs_num = EXPECT_NUMBER(rhs); \
        \
        push(tag_value(new Number{ .value = lhs_num->value op_sym rhs_num->value })); \
        \
        break; \
    }

    for (; ip < bc_size; ip++) {
        auto& instr = bytecode[ip];
        switch (instr.opcode) {
            case Opcode::LoadNumber: {
                double value;
                auto raw = instr.token.number.raw;
                auto res = std::from_chars(raw.begin(), raw.end(), value);
                if (!res) {
                    log_error("error parsing number from token: {}", std::make_error_code(res.ec).message());
                    return ExecutionResult { .code = ExecutionResultCode::Error };
                }
                stack.emplace_back(tag_value(new Number{ .value = value }));
                break;
            }
            case Opcode::LoadNamedReference: {
                push(tag_value(new CollectionAccess{
                    .collection = context->file->global,
                    .field = instr.token,
                }));
                break;
            }
            case Opcode::FieldAccess: {
                auto lhs = LOAD_VALUE(pop());
                switch (lhs->type) {
                    case ValueType::Collection: {
                        auto collection = static_cast<Collection*>(lhs);
                        push(tag_value(new CollectionAccess {
                            .collection = collection,
                            .field = instr.token,
                        }));
                        break;
                    }
                    default:
                        raise_error("invalid lhs for field access");
                }
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
