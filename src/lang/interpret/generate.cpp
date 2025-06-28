#include "interpret.hpp"

NAMESPACE_BEGIN(lang)

BytecodeChunk* bytecode_generate(AstNode* node)
{
    auto chunk = new BytecodeChunk{};

    struct Local {
        Token name{};
        u32 index;
    };

    std::vector<Local> locals;

    u32 stack_depth = 0;

    // "yield" in this context refers to whether a node leaves a value on the stack
    //   to be consumed in a subsequent operation

    // TODO: Handle multiple yields
    enum class YieldsValue { No, Yes };

    auto generate_root = [&](this auto&& generate_node, AstNode* _node, bool is_root = false) -> YieldsValue {

        auto emit = [&](Opcode opcode, i32 stack_change) -> auto& {
            stack_depth += stack_change;
            return chunk->bytecode.emplace_back(opcode);
        };

        auto generate_block_body = [&](const AstBlock& block) -> YieldsValue {
            u32 initial_stack_depth = stack_depth;
            u32 initial_local_depth = locals.size();
            defer { locals.resize(initial_local_depth); };

            AstNode* last_nonterminal{};
            auto last_yielded = YieldsValue::No;

            auto pop_if_yielded = [&] {
                // TODO: Should we do this after every expression, or POP/PUSH_DOWN in bulk at the end of a scope?
                if (last_yielded == YieldsValue::Yes) {
                    emit(Opcode::Pop, -1).count = 1;
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
                    emit(Opcode::Pop, -pop_count).count = pop_count;
                } else {
                    auto push_count = (stack_depth - initial_stack_depth) - 1;
                    if (push_count > 0) {
                        emit(Opcode::PushDown, -push_count).count = push_count;
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
        };

        auto expect_yield = [&](YieldsValue yields) {
            if (yields == YieldsValue::No) {
                raise_error("expected node to yield value");
            }
        };

        return visit<YieldsValue>(overload_set {
            [&](const AstVarDecl& node) -> YieldsValue {
                if (!is_root && node.target->type == AstNodeType::NamedReference) {
                    // Local variable declaration

                    if (node.init) {
                        expect_yield(generate_node(node.init));
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
                    emit(Opcode::NamespaceVarDecl, (node.init) ? -2 : -1).decl_has_init = bool(node.init);
                }

                // Variable declarations never yield a value
                return YieldsValue::No;
            },
            [&](const AstBlock& node) -> YieldsValue {
                return generate_block_body(node);
            },
            // [&](const AstFunction& node) {
            // },
            [&](const AstCall& node) -> YieldsValue {
                expect_yield(generate_node(node.callee));
                usz num_args = 0;
                for (auto arg = node.arguments.head; arg; arg = arg->next) {
                    expect_yield(generate_node(arg));
                    num_args++;
                }

                if (node.token.raw == lang_str("(")) {
                    emit(Opcode::CallFunction, -num_args).arg_count = num_args;
                } else if (node.token.raw == lang_str("[")) {
                    emit(Opcode::CallSubscript, -num_args).arg_count = num_args;
                } else {
                    raise_error("unknown call type: {}", node.token.raw);
                }

                // Functions always return some value (possibly unit)
                return YieldsValue::Yes;
            },
            // [&](const AstCallNamedArgument& node) {
            // },
            [&](const AstLiteral& node) -> YieldsValue {
                switch (node.token.type) {
                    case TokenType::Number:
                        emit(Opcode::LoadNumber, 1).token = node.token;
                        break;
                    case TokenType::String:
                        emit(Opcode::LoadString, 1).token = node.token;
                        break;
                    default:
                        raise_error("unknown token type for literal");
                }
                return YieldsValue::Yes;
            },
            [&](const AstNamedReference& node) -> YieldsValue {
                Local* i_local{};
                for (u32 i = locals.size(); i--;) {
                    // Search from back so that we find later (shadowing) locals first
                    if (locals[i].name.raw == node.token.raw) {
                        i_local = &locals[i];
                        break;
                    }
                }
                if (i_local) {
                    emit(Opcode::LoadLocal, 1).stack_index = i_local->index;
                } else {
                    emit(Opcode::LoadGlobal, 1).token = node.token;
                }
                return YieldsValue::Yes;
            },
            [&](const AstOpExpr& node) -> YieldsValue {
                if (node.lhs) expect_yield(generate_node(node.lhs));
                if (node.rhs) expect_yield(generate_node(node.rhs));
                emit(Opcode::Operator, 1 - bool(node.lhs) - bool(node.rhs)).op = node.op;

                // TODO: Handle operators that don't evaluate to anything in future?
                return YieldsValue::Yes;
            },
            [&](const AstFieldAccess& node) -> YieldsValue {
                expect_yield(generate_node(node.lhs));
                emit(Opcode::FieldAccess, 0).token = node.field;
                return YieldsValue::Yes;
            },
            [&](const AstIf& node) {
                expect_yield(generate_node(node.cond));

                auto jump_over_then_index = chunk->bytecode.size();
                emit(Opcode::JumpIfFalse, -1);

                if (node.then_branch->type != AstNodeType::Block) raise_error("expected block for then_branch");
                auto then_yields = generate_block_body(*static_cast<AstBlock*>(node.then_branch));

                // Backpatch then jump
                chunk->bytecode[jump_over_then_index].target_instruction = chunk->bytecode.size();

                auto else_yields = YieldsValue::No;
                if (node.else_branch) {
                    auto jump_over_else_index = chunk->bytecode.size();
                    emit(Opcode::Jump, 0);

                    // Update then jump to jump past unconditional else skip jump
                    chunk->bytecode[jump_over_then_index].target_instruction++;

                    if (node.then_branch->type != AstNodeType::Block) raise_error("expected block for then_branch");
                    else_yields = generate_block_body(*static_cast<AstBlock*>(node.else_branch));

                    // Backpatch else jump
                    chunk->bytecode[jump_over_else_index].target_instruction = chunk->bytecode.size();
                }

                if (then_yields != else_yields) {
                    raise_error("either both or neither branch of an if/else must yield a value");
                }

                return then_yields;
            },
            // [&](const AstFor& node) {
            // },
            [&](const AstWhile& node) {

                auto loop_start_ip = chunk->bytecode.size();

                expect_yield(generate_node(node.cond));

                auto jump_out_index = chunk->bytecode.size();
                emit(Opcode::JumpIfFalse, -1);

                if (node.body->type != AstNodeType::Block) raise_error("expected block for while body");
                auto body_yields = generate_block_body(*static_cast<AstBlock*>(node.body));
                if (body_yields == YieldsValue::Yes) {
                    raise_error("loop body cannot directly yield value, use break expression");
                }

                emit(Opcode::Jump, 0).target_instruction = loop_start_ip;

                // Backpatch out jump
                chunk->bytecode[jump_out_index].target_instruction = chunk->bytecode.size();

                // TODO: support break + else
                return YieldsValue::No;
            },
            // [&](const AstDefer& node) {
            // },
            [&](const auto& node) -> YieldsValue {
                raise_error("unknown node type: {} ({})", get_ast_node_type_name(node.type), std::to_underlying(node.type));
            },
        }, _node);
    };

    generate_root(node, true);

    return chunk;
}

void bytecode_debug_print(BytecodeChunk* chunk)
{
    auto print_instr = [](std::string_view instruction, std::string_view args_fmt, auto&&... args)
    {
        auto arg_str = std::format(std::runtime_format(args_fmt), args...);
        log_debug("[{:14}] {}", instruction, arg_str);
    };

    for (auto& instruction : chunk->bytecode) {
        switch (instruction.opcode) {
            case Opcode::LoadNil:
                print_instr("LOAD_NIL", "");
                break;
            case Opcode::LoadNumber:
                print_instr("LOAD_NUMBER", "{}", instruction.token.raw);
                break;
            case Opcode::LoadString:
                print_instr("LOAD_STRING", "{}", instruction.token.string.value);
                break;
            case Opcode::LoadGlobal:
                print_instr("LOAD_GLOBAL", "{}", instruction.token.raw);
                break;
            case Opcode::LoadLocal:
                print_instr("LOAD_LOCAL", "{}", instruction.stack_index);
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
                print_instr("JUMP_FALSE", "{}", instruction.target_instruction);
                break;
            case Opcode::Jump:
                print_instr("JUMP", "{}", instruction.target_instruction);
                break;
            default:
                raise_error("unknown opcode: {}", get_opcode_name(instruction.opcode));
        }
    }
}

NAMESPACE_END
