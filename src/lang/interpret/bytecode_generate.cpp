#if 0
#include "bytecode.hpp"

NAMESPACE_BEGIN(lang)
struct Local {
    Token name{};
    u32 index;
};

enum class YieldsValue { No, Yes };

struct BytecodeGenerator
{
    BytecodeChunk* chunk = new BytecodeChunk{};

    std::vector<Local> locals;

    u32 stack_depth = 0;

    auto emit(Opcode opcode, i32 stack_change) -> auto&
    {
        stack_depth += stack_change;
        return chunk->bytecode.emplace_back(Instruction(opcode));
    }

    template<typename T>
    void emit_operand(T t)
    {
        static_assert(sizeof(T) <= sizeof(Instruction));
        Instruction inst{};
        std::memcpy(&inst, &t, sizeof(T));
        chunk->bytecode.emplace_back(inst);
    }

    size_t emit_jump_operand_placeholder()
    {
        auto offset = chunk->bytecode.size();
        chunk->bytecode.emplace_back();
        return offset;
    }

    template<typename T>
    void emit_operand_at(size_t offset, T t)
    {
        static_assert(sizeof(T) <= sizeof(Instruction));
        std::memcpy(chunk->bytecode.data() + offset, &t, sizeof(T));
    }

    void expect_yield(YieldsValue yields)
    {
        if (yields == YieldsValue::No) {
            raise_error("expected node to yield value");
        }
    }

    auto generate_function_body(const AstFunction& function) -> YieldsValue
    {
        std::vector<Local> parameters;
        for (auto p = function.parameters.head; p; p = p->next) {
            auto* var_decl = static_cast<AstVarDecl*>(p);
            auto* target = static_cast<AstNamedReference*>(var_decl->target);
            parameters.emplace_back(Local {
                .name = target->token,
                .index = u32(parameters.size()),
            });
        }

        generate_block_body(*static_cast<AstBlock*>(function.body), parameters);

        return YieldsValue::Yes;
    }

    auto generate_block_body(const AstBlock& block, std::span<const Local> injected_locals = {}) -> YieldsValue
    {
        u32 initial_stack_depth = stack_depth;
        u32 initial_local_depth = locals.size();
        defer { locals.resize(initial_local_depth); };

        if (!injected_locals.empty()) {
            for (auto& local : injected_locals) {
                locals.emplace_back(local);
            }
            stack_depth += injected_locals.size();
        }

        AstNode* last_nonterminal{};
        auto last_yielded = YieldsValue::No;

        auto pop_if_yielded = [&] {
            // TODO: Should we do this after every expression, or POP/PUSH_DOWN in bulk at the end of a scope?
            if (last_yielded == YieldsValue::Yes) {
                emit(Opcode::Pop, -1);
                emit_operand(1);
            }
        };

        for (auto s = block.statements.head; s; s = s->next) {
            pop_if_yielded();
            last_yielded = generate_node(s);
            last_nonterminal = s;
        }

        auto cleanup_stack = [&](YieldsValue should_yield) {
            if (should_yield == YieldsValue::No) {
                auto pop_count = stack_depth - initial_stack_depth;
                emit(Opcode::Pop, -pop_count);
                emit_operand(pop_count);
            } else {
                auto push_count = (stack_depth - initial_stack_depth) - 1;
                log_warn("pushing down");
                log_warn("  stack_depth = {}", stack_depth);
                log_warn("  initial_stack_depth = {}", initial_stack_depth);
                log_warn("  push_count = {}", push_count);
                if (push_count > 0) {
                    emit(Opcode::PushDown, -push_count);
                    emit_operand(push_count);
                }
            }
        };

        if (block.terminal_expression) {
            pop_if_yielded();
            last_yielded = generate_node(block.terminal_expression);
            cleanup_stack(last_yielded);
            return last_yielded;
        } else {
            if (is_ast_node_block_expr(last_nonterminal)) {
                cleanup_stack(last_yielded);
                return last_yielded;
            } else {
                cleanup_stack(YieldsValue::No);
                return YieldsValue::No;
            }
        }
    }

    auto generate_node(AstNode* _node, bool is_root = false) -> YieldsValue {
        return visit<YieldsValue>(overload_set {
            [&](AstVarDecl& node) -> YieldsValue {
                if (!is_root && node.target->type == AstNodeType::NamedReference) {
                    // Local variable declaration

                    if (node.init) {
                        expect_yield(generate_node(node.init));
                        emit(Opcode::LoadImmediate, 0);
                        emit_operand(1);
                    } else {
                        emit(Opcode::LoadNil, 1);
                    }

                    locals.push_back({
                        .name = static_cast<AstNamedReference*>(node.target)->token,
                        .index = stack_depth - 1,
                    });
                } else {
                    expect_yield(generate_node(node.target));
                    if (node.init) expect_yield(generate_node(node.init));
                    emit(Opcode::NamespaceVarDecl, (node.init) ? -2 : -1);
                    emit_operand(bool(node.init));
                }

                // Variable declarations never yield a value
                return YieldsValue::No;
            },
            [&](AstBlock& node) -> YieldsValue {
                return generate_block_body(node);
            },
            [&](AstFunction& node) {
                if (!node.body) {
                    raise_error("value prototypes are not currently supported");
                }

                emit(Opcode::LoadFunction, 1);
                emit_operand(&node);

                return YieldsValue::Yes;
            },
            [&](AstCall& node) -> YieldsValue {
                usz num_args = 0;
                for (auto arg = node.arguments.head; arg; arg = arg->next) {
                    expect_yield(generate_node(arg));
                    num_args++;
                }
                expect_yield(generate_node(node.callee));

                if (node.token.raw == lang_str("(")) {
                    emit(Opcode::CallFunction, -num_args);
                    emit_operand(num_args);
                } else if (node.token.raw == lang_str("[")) {
                    emit(Opcode::CallSubscript, -num_args);
                    emit_operand(num_args);
                } else {
                    raise_error("unknown call type: {}", node.token.raw);
                }

                // Functions always return some value (possibly unit)
                return YieldsValue::Yes;
            },
            // [&](const AstCallNamedArgument& node) {
            // },
            [&](AstLiteral& node) -> YieldsValue {
                switch (node.token.type) {
                    case TokenType::Number: {
                        f64 value;
                        auto raw = node.token.number.raw;
                        auto res = std::from_chars(raw.begin(), raw.end(), value);
                        if (!res) {
                            raise_error("error parsing number from token: {}", std::make_error_code(res.ec).message());
                        }
                        emit(Opcode::LoadNumber, 1);
                        emit_operand(value);
                        break;
                    }
                    case TokenType::String:
                        raise_error("TODO");
                        // emit(Opcode::LoadString, 1).token = node.token;
                        break;
                    case TokenType::Keyword:
                        if (node.token.raw == lang_str("true")) {
                            emit(Opcode::LoadBoolean, 1);
                            emit_operand(true);
                            break;
                        } else if (node.token.raw == lang_str("false")) {
                            emit(Opcode::LoadBoolean, 1);
                            emit_operand(false);
                            break;
                        }
                        [[fallthrough]];
                    default:
                        raise_error("unknown token type for literal");
                }
                return YieldsValue::Yes;
            },
            [&](AstNamedReference& node) -> YieldsValue {
                Local* i_local{};
                for (u32 i = locals.size(); i--;) {
                    // Search from back so that we find later (shadowing) locals first
                    if (locals[i].name.raw == node.token.raw) {
                        i_local = &locals[i];
                        break;
                    }
                }
                if (i_local) {
                    emit(Opcode::LoadLocal, 1);
                    emit_operand(i_local->index);
                } else {
                    emit(Opcode::LoadGlobal, 1);
                    emit_operand(node.token.raw.data());
                    emit_operand(node.token.raw.size());
                }
                return YieldsValue::Yes;
            },
            [&](AstOpExpr& node) -> YieldsValue {
                if (node.lhs) expect_yield(generate_node(node.lhs));
                if (node.rhs) expect_yield(generate_node(node.rhs));
                emit(Opcode::Operator, 1 - bool(node.lhs) - bool(node.rhs));
                emit_operand(node.op);

                // TODO: Handle operators that don't evaluate to anything in future?
                return YieldsValue::Yes;
            },
            [&](AstFieldAccess& node) -> YieldsValue {
                expect_yield(generate_node(node.lhs));
                emit(Opcode::FieldAccess, 0);
                emit_operand(node.field);
                return YieldsValue::Yes;
            },
            [&](AstIf& node) {
                expect_yield(generate_node(node.cond));

                emit(Opcode::JumpIfFalse, -1);
                auto jump_over_then_index = emit_jump_operand_placeholder();

                log_warn(" stack depth after jump_false = {}", stack_depth);

                if (node.then_branch->type != AstNodeType::Block) raise_error("expected block for then_branch");
                auto then_yields = generate_block_body(*static_cast<AstBlock*>(node.then_branch));
                log_warn(" stack depth after then_branch = {}", stack_depth);

                // Backpatch then jump
                emit_operand_at(jump_over_then_index, chunk->bytecode.size() - jump_over_then_index);

                auto else_yields = YieldsValue::No;
                if (node.else_branch) {
                    emit(Opcode::Jump, -1);
                    auto jump_over_else_index = emit_jump_operand_placeholder();

                    // Update then jump to jump past unconditional else skip jump
                    chunk->bytecode[jump_over_then_index]++;

                    if (node.then_branch->type != AstNodeType::Block) raise_error("expected block for then_branch");
                    else_yields = generate_block_body(*static_cast<AstBlock*>(node.else_branch));
                    log_warn(" stack depth after else_branch = {}", stack_depth);

                    // Backpatch else jump
                    emit_operand_at(jump_over_else_index, chunk->bytecode.size() - jump_over_else_index);
                }

                if (then_yields != else_yields) {
                    raise_error("either both or neither branch of an if/else must yield a value");
                }

                return then_yields;
            },
            // [&](const AstFor& node) {
            // },
            [&](AstWhile& node) {

                auto loop_start_ip = chunk->bytecode.size();

                expect_yield(generate_node(node.cond));

                emit(Opcode::JumpIfFalse, -1);
                auto jump_out_index = emit_jump_operand_placeholder();

                if (node.body->type != AstNodeType::Block) raise_error("expected block for while body");
                auto body_yields = generate_block_body(*static_cast<AstBlock*>(node.body));
                if (body_yields == YieldsValue::Yes) {
                    raise_error("loop body cannot directly yield value, use break expression");
                }

                auto loop_jump_index = chunk->bytecode.size();
                emit(Opcode::Jump, 0);
                emit_operand(loop_start_ip - loop_jump_index);

                // Backpatch out jump
                emit_operand_at(jump_out_index, chunk->bytecode.size() - jump_out_index);

                // TODO: support break + else
                return YieldsValue::No;
            },
            // [&](const AstDefer& node) {
            // },
            [&](auto& node) -> YieldsValue {
                raise_error("unknown node type: {} ({})", get_ast_node_type_name(node.type), std::to_underlying(node.type));
            },
        }, _node);
    }
};

BytecodeChunk* bytecode_generate(AstNode* node)
{
    BytecodeGenerator generator;
    generator.generate_node(node, true);
    generator.emit(Opcode::Halt, 0);
    return generator.chunk;
}

BytecodeChunk* bytecode_generate_function_body(AstFunction* node)
{
    BytecodeGenerator generator;
    generator.generate_function_body(*node);
    generator.emit(Opcode::Return, 0);
    return generator.chunk;
}

void bytecode_debug_print(BytecodeChunk* chunk)
{
    auto print_instr = [](std::string_view instruction, std::string_view args_fmt, auto&&... args)
    {
        auto arg_str = std::format(std::runtime_format(args_fmt), args...);
        log_debug("[{:14}] {}", instruction, arg_str);
    };

    auto end = chunk->bytecode.data() + chunk->bytecode.size();
    Instruction* ip = chunk->bytecode.data();
    while (ip < end) {
        switch (Opcode(*ip)) {
            case Opcode::LoadNil:
                print_instr("LOAD_NIL", "");
                ip++;
                break;
            case Opcode::LoadNumber:
                print_instr("LOAD_NUMBER", "{}", *(ip + 1));
                ip += 2;
                break;
            case Opcode::LoadString:
                print_instr("LOAD_STRING", "{}", instruction.token.string.value);
                break;
            case Opcode::LoadBoolean:
                print_instr("LOAD_BOOLEAN", "{}", instruction.boolean);
                break;
            case Opcode::LoadFunction:
                print_instr("LOAD_FUNCTION", "{}", (void*)instruction.ast_function);
                break;
            case Opcode::LoadGlobal:
                print_instr("LOAD_GLOBAL", "{}", instruction.token.raw);
                break;
            case Opcode::LoadLocal:
                print_instr("LOAD_LOCAL", "{}", instruction.stack_index);
                break;
            case Opcode::LoadImmediate:
                print_instr("LOAD_NOW", "{}", instruction.count);
                break;
            case Opcode::CallFunction:
                print_instr("CALL_FUNCTION", "{}", instruction.arg_count);
                break;
            case Opcode::CallSubscript:
                print_instr("CALL_SUBSCRIPT", "{}", instruction.arg_count);
                break;
            case Opcode::FieldAccess:
                print_instr("FIELD_ACCESS", "{}", instruction.token.raw);
                break;
            case Opcode::NamespaceVarDecl:
                print_instr("VAR_DECL", "has_init = {}", instruction.decl_has_init);
                break;
            case Opcode::Operator:
                print_instr("OPERATOR", "{}", get_op_name(instruction.op));
                break;
            case Opcode::Pop:
                print_instr("POP", "{}", instruction.count);
                break;
            case Opcode::PushDown:
                print_instr("PUSH_DOWN", "{}", instruction.count);
                break;
            case Opcode::JumpIfFalse:
                print_instr("JUMP_FALSE", "{}", instruction.jump_delta);
                break;
            case Opcode::Jump:
                print_instr("JUMP", "{}", instruction.jump_delta);
                break;
            case Opcode::Return:
                print_instr("RETURN", "");
                break;
            case Opcode::Halt:
                print_instr("HALT", "");
                break;
            default:
                raise_error("unknown opcode: {}", get_opcode_name(instruction.opcode));
        }
    }

    // for (auto& instruction : chunk->bytecode) {
    //     switch (instruction.opcode) {
    //         case Opcode::LoadNil:
    //             print_instr("LOAD_NIL", "");
    //             break;
    //         case Opcode::LoadNumber:
    //             print_instr("LOAD_NUMBER", "{}", instruction.number);
    //             break;
    //         case Opcode::LoadString:
    //             print_instr("LOAD_STRING", "{}", instruction.token.string.value);
    //             break;
    //         case Opcode::LoadBoolean:
    //             print_instr("LOAD_BOOLEAN", "{}", instruction.boolean);
    //             break;
    //         case Opcode::LoadFunction:
    //             print_instr("LOAD_FUNCTION", "{}", (void*)instruction.ast_function);
    //             break;
    //         case Opcode::LoadGlobal:
    //             print_instr("LOAD_GLOBAL", "{}", instruction.token.raw);
    //             break;
    //         case Opcode::LoadLocal:
    //             print_instr("LOAD_LOCAL", "{}", instruction.stack_index);
    //             break;
    //         case Opcode::LoadImmediate:
    //             print_instr("LOAD_NOW", "{}", instruction.count);
    //             break;
    //         case Opcode::CallFunction:
    //             print_instr("CALL_FUNCTION", "{}", instruction.arg_count);
    //             break;
    //         case Opcode::CallSubscript:
    //             print_instr("CALL_SUBSCRIPT", "{}", instruction.arg_count);
    //             break;
    //         case Opcode::FieldAccess:
    //             print_instr("FIELD_ACCESS", "{}", instruction.token.raw);
    //             break;
    //         case Opcode::NamespaceVarDecl:
    //             print_instr("VAR_DECL", "has_init = {}", instruction.decl_has_init);
    //             break;
    //         case Opcode::Operator:
    //             print_instr("OPERATOR", "{}", get_op_name(instruction.op));
    //             break;
    //         case Opcode::Pop:
    //             print_instr("POP", "{}", instruction.count);
    //             break;
    //         case Opcode::PushDown:
    //             print_instr("PUSH_DOWN", "{}", instruction.count);
    //             break;
    //         case Opcode::JumpIfFalse:
    //             print_instr("JUMP_FALSE", "{}", instruction.jump_delta);
    //             break;
    //         case Opcode::Jump:
    //             print_instr("JUMP", "{}", instruction.jump_delta);
    //             break;
    //         case Opcode::Return:
    //             print_instr("RETURN", "");
    //             break;
    //         case Opcode::Halt:
    //             print_instr("HALT", "");
    //             break;
    //         default:
    //             raise_error("unknown opcode: {}", get_opcode_name(instruction.opcode));
    //     }
    // }
}

NAMESPACE_END
#endif
