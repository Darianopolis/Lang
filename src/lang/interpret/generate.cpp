#include "interpret.hpp"

NAMESPACE_BEGIN(lang)

BytecodeChunk* bytecode_generate(AstNode* node)
{
    auto chunk = new BytecodeChunk{};

    auto generate_root = [&](this auto&& generate_node, AstNode* _node) -> void {
        auto emit = [&](Opcode opcode) -> auto& {
            return chunk->bytecode.emplace_back(opcode);
        };

        visit(overload_set {
            [&](const AstVarDecl& node) {
                generate_node(node.target);
                if (node.init) generate_node(node.init);
                emit(Opcode::VarDecl).decl_has_init = bool(node.init);
            },
            // [&](const AstBlock& node) {
            // },
            // [&](const AstFunction& node) {
            // },
            [&](const AstCall& node) {
                generate_node(node.callee);
                usz num_args = 0;
                for (auto arg = node.arguments.head; arg; arg = arg->next) {
                    generate_node(arg);
                    num_args++;
                }

                if (node.token.raw == lang_str("(")) {
                    emit(Opcode::CallFunction).arg_count = num_args;
                } else if (node.token.raw == lang_str("[")) {
                    emit(Opcode::CallSubscript).arg_count = num_args;
                } else {
                    log_error("unknown call type: {}", node.token.raw);
                }
            },
            // [&](const AstCallNamedArgument& node) {
            // },
            [&](const AstLiteral& node) {
                switch (node.token.type) {
                    case TokenType::Number:
                        emit(Opcode::LoadNumber).token = node.token;
                        break;
                    case TokenType::String:
                        emit(Opcode::LoadString).token = node.token;
                        break;
                    default:
                        log_error("unknown token type for literal");
                }
            },
            [&](const AstNamedReference& node) {
                emit(Opcode::LoadNamedReference).token = node.token;
            },
            [&](const AstOpExpr& node) {
                if (node.lhs) generate_node(node.lhs);
                if (node.rhs) generate_node(node.rhs);
                emit(Opcode::Operator).op = node.op;
            },
            [&](const AstFieldAccess& node) {
                generate_node(node.lhs);
                emit(Opcode::FieldAccess).token = node.field;
            },
            // [&](const AstIf& node) {
            // },
            // [&](const AstFor& node) {
            // },
            // [&](const AstWhile& node) {
            // },
            // [&](const AstDefer& node) {
            // },
            [&](const auto& node) {
                log_error("unknown node type: {} ({})", get_ast_node_type_name(node.type), std::to_underlying(node.type));
            },
        }, _node);
    };

    generate_root(node);

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
            case Opcode::LoadNumber:
                print_instr("LOAD_NUMBER", "{}", instruction.token.raw);
                break;
            case Opcode::LoadString:
                print_instr("LOAD_STRING", "{}", instruction.token.string.value);
                break;
            case Opcode::LoadNamedReference:
                print_instr("LOAD_NAMED", "{}", instruction.token.raw);
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
            case Opcode::VarDecl:
                print_instr("VAR_DECL", "has_init = {}", instruction.decl_has_init);
                break;
            case Opcode::Operator:
                print_instr("OPERATOR", "{}", get_op_name(instruction.op));
                break;
            default:
                raise_error("unknown opcode: {}", get_opcode_name(instruction.opcode));
        }
    }
}

NAMESPACE_END
