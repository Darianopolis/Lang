#include "lang.hpp"

#include <ankerl/unordered_dense.h>

#include <source_location>

NAMESPACE_BEGIN(lang)

enum class BindingDirection : u8
{
    Invalid,
    Left,
    Right,
};

constexpr
std::string_view get_dir_name(BindingDirection dir)
{
    if (dir == BindingDirection::Left) return "Left";
    if (dir == BindingDirection::Right) return "Right";
    return "None";
}

constexpr auto make_op_pair_key(Operator l, Operator r) {
    return u32(l) | u32(r) << 16;
};

const static
auto OperatorOrdering = []{
    ankerl::unordered_dense::map<u32, BindingDirection> lookup;

    auto bind = [&](BindingDirection dir, std::initializer_list<Operator> _l, std::initializer_list<Operator> _r) {
        for (auto l : _l) {
            for (auto r : _r) {
                auto& cur_dir = lookup[make_op_pair_key(l, r)];
                if (cur_dir == BindingDirection::Invalid) {
                    // log_trace("op({}, {}) = {}", get_op_name(l), get_op_name(r), get_dir_name(dir));
                } else if (cur_dir != dir) {
                    raise_error("op({}, {}) = [{}] contradicts earlier assignment to [{}]",
                        get_op_name(l), get_op_name(r), get_dir_name(dir), get_dir_name(cur_dir));
                } else {
                    log_warn("  redundant assignment of op({}, {}) to = {}",
                        get_op_name(l), get_op_name(r), get_dir_name(dir));
                }
                cur_dir = dir;
            }
        }
    };

    enum class Associativity
    {
        LeftToRight,
        RightToLeft,
    };

    using enum Associativity;
    using enum BindingDirection;
    using enum Operator;

    auto order = [&](std::span<const std::initializer_list<Operator>> op_groups) {
        for (u32 i = 0; i < op_groups.size(); ++i) {
            for (u32 j = i + 1; j < op_groups.size(); ++j) {
                auto l = op_groups.begin()[i];
                auto r = op_groups.begin()[j];
                bind(Left, l, r);
                bind(Right, r, l);
            }
        }
    };

    auto assoc = [&](Associativity assoc, std::span<const std::initializer_list<Operator>> op_groups) {
        for (auto ops : op_groups) {
            switch (assoc) {
                case Associativity::LeftToRight:
                    bind(Left, ops, ops);
                    break;
                case Associativity::RightToLeft:
                    bind(Right, ops, ops);
            }
        }
    };

#define X(i) i,

    // General operator strata

    order({{ OPS_All }, { None }});

    order({
        { OPS_Postfix OPS_Prefix },
        { OPS_Arithmetic OPS_Bitwise },
        { OPS_Comparison },
        { OPS_Logical },
        { OPS_Range },
        { OPS_ControlFlow },
    });

    // Post-/Pre-fix

    order({
        { Call, Subscript, FieldAccess },
        { Preincrement, Predecrement, Postincrement, Postdecrement },
        { Dereference },
        { AddressOf },
        { Arithmetic_Identity, Arithmetic_Negate, Bitwise_Negate, Logical_Negate }
    });

    // Arithmetic infix

    order(             { { Power }, { Multiply, Divide }, { Add, Subtract } });
    assoc(LeftToRight, {            { Multiply, Divide }, { Add, Subtract } });

    // Bitwise infix

    order({{ Bitwise_And }, { Bitwise_Or }});

    // Logical infix

    order(             {{ Logical_And }, { Logical_Or }});
    assoc(LeftToRight, {{ Logical_And }, { Logical_Or }});

    // Assignment / Variable Declaration

    order({{ OPS_Prefix }, { OPS_Assign }});
    bind(Right,
        {
            OPS_Assign
            OPS_VarDecl
        },
        {
            OPS_Postfix
            OPS_Arithmetic
            OPS_Bitwise
            OPS_Comparison
            OPS_Logical
            OPS_Range
        });
    assoc(RightToLeft, {{ OPS_Assign }});
    bind(Right, { OPS_VarDecl }, { OPS_Assign });

    // Control Flow

    bind(Right, { OPS_VarDecl }, { OPS_ControlFlow });
    bind(Right, { OPS_ControlFlow }, { OPS_Assign });

    // Output

    log_debug("Defined {} operator relationships for {} operators", lookup.size(), std::array { OPS_All }.size());

#undef X

    return lookup;
}();

BindingDirection get_op_bind_dir(Operator l, Operator r)
{
    auto iter = OperatorOrdering.find(make_op_pair_key(l, r));
    return iter == OperatorOrdering.end() ? BindingDirection::Invalid : iter->second;
}

#define LANG_NOISY_PARSER 1

#if LANG_NOISY_PARSER
#define LANG_PARSER_TRACE_FUNC(fmt, ...) \
    auto _func_name = __func__; \
    log_debug("{}{}(" fmt ")", indent_str(_depth, ". "), _func_name __VA_OPT__(,) __VA_ARGS__); \
    _depth++; \
    defer { \
        _depth--; \
        log_trace(text_ansi_color_reset "{}" text_ansi_color_begin(90) "{}", indent_str(_depth, ". "), _func_name); \
    }
#else
#define LANG_PARSER_TRACE_FUNC(...)
#endif

struct Parser
{
    TokenIterator iter;

#if LANG_NOISY_PARSER
    i32 _depth = 0;
#endif

    bool is(StaticLangString raw)
    {
        return iter.peek().raw == raw.str;
    }

    bool match(StaticLangString raw)
    {
        if (iter.peek().raw == raw.str) {
            iter.advance();
            return true;
        }
        return false;
    }

    void consume(StaticLangString raw, std::string_view message)
    {
        if (iter.peek().raw != raw.str) raise_error("{}, got {}", message, iter.peek().raw);
        iter.advance();
    }

    Token consume_identifier(std::string_view message)
    {
        auto token = iter.advance();
        if (token.type != TokenType::Identifier) raise_error("{}, got {}", message, token.raw);
        return token;
    }

// -------------------------------------------------------------------------

    AstNode* maybe_else()
    {
        LANG_PARSER_TRACE_FUNC("");

        return match("else") ? block_expr() : nullptr;
    }

    AstIf* finish_if()
    {
        LANG_PARSER_TRACE_FUNC("");

        auto expr = expression();
        auto then_branch = block_expr();
        auto else_branch = maybe_else();

        return new AstIf{{AstNodeType::If}, expr, then_branch, else_branch};
    }

    AstFor* finish_for()
    {
        LANG_PARSER_TRACE_FUNC("");

        auto range_expr = expression();

        consume("->", "expected '->' after for range expression");

        consume("(", "expected parameter list in for");
        auto params = parameters(')');
        consume(")", "expected ')' to follow parameter list");

        auto body = block();
        auto else_expr = maybe_else();

        return new AstFor{{AstNodeType::For}, range_expr, params, body, else_expr};
    }

    AstWhile* finish_while()
    {
        LANG_PARSER_TRACE_FUNC("");

        auto cond = expression();
        auto body = block();
        auto else_expr = maybe_else();

        return new AstWhile{{AstNodeType::While}, cond, body, else_expr};
    }

    AstNode* maybe_block_expr()
    {
        LANG_PARSER_TRACE_FUNC("");

        if (match("if")) {
            return finish_if();
        } else if (match("for")) {
            return finish_for();
        } else if (match("while")) {
            return finish_while();
        } else if (match("{")) {
            return finish_block();
        }

        return nullptr;
    }

    AstNode* block_expr()
    {
        LANG_PARSER_TRACE_FUNC("");

        auto expr = maybe_block_expr();
        if (!expr) raise_error("expected block expression, got {}", iter.peek().raw);
        return expr;
    }

    AstDefer* maybe_defer()
    {
        LANG_PARSER_TRACE_FUNC("");

        if (match("defer")) {
            auto body = block();
            return new AstDefer{{AstNodeType::Defer}, body};
        }
        return nullptr;
    }

    std::pair<AstNode*, bool> statement()
    {
        LANG_PARSER_TRACE_FUNC("");

        if (match(";") || iter.is_at_end()) return {nullptr, false};

        // Block expression

        if (auto stmt = maybe_block_expr()) return {stmt, false};

        // Defer

        if (auto stmt = maybe_defer()) return {stmt, false};

        // Expression statement

        auto expr = expression();
        return {expr, !match(";")};
    }

    AstBlock* finish_block()
    {
        LANG_PARSER_TRACE_FUNC("");

        AstNodeListBuilder stmts;
        AstNode* terminal{};

        while (!is("}")) {
            auto[stmt, is_terminal] = statement();
            if (!stmt) continue;
            if (is_terminal) {
                terminal = stmt;
                break;
            }
            stmts.append(stmt);
        }

        consume("}", "expected '}' to terminate block");

        return new AstBlock{{AstNodeType::Block}, stmts.to_list(), terminal};
    }

    AstBlock* block()
    {
        LANG_PARSER_TRACE_FUNC("");

        consume("{", "expected block");
        return finish_block();
    }

    AstNodeList file()
    {
        LANG_PARSER_TRACE_FUNC("");

        AstNodeListBuilder stmts;

        while (iter.peek().type != TokenType::Eof) {
            auto[stmt, is_terminal] = statement();
            if (!stmt) continue;
            stmts.append(stmt);
            if (is_terminal) {
                break;
                // raise_error("terminal expressions not allowed at file scope");
            }
        }

        if (iter.peek().type != TokenType::Eof) {
            raise_error("expected end of file, got {}", iter.peek().raw);
        }

        return stmts.to_list();
    }

    AstVarDecl* parameter()
    {
        LANG_PARSER_TRACE_FUNC("");

        AstNode* target{};
        auto ident = iter.peek();
        if (ident.type == TokenType::Identifier) {
            iter.advance();
            target = new AstNamedReference{{AstNodeType::NamedReference}, ident};
        } else if (ident.raw != ":") {
            raise_error("expected identifier or ':' to begin parameter");
        }

        if (match(":")) {
            return finish_var_decl(target);
        } else {
            return new AstVarDecl{{AstNodeType::VarDecl}, target, nullptr, false, nullptr};
        }
    }

    AstNodeList parameters(u8 end)
    {
        LANG_PARSER_TRACE_FUNC("closing = '{}'", char(end));

        AstNodeListBuilder params;

        for (;;) {
            if (iter.peek().raw[0] == end) break;

            params.append(parameter());

            if (!match(",")) break;
        }

        return params.to_list();
    }

    AstNodeList arguments(u8 end)
    {
        LANG_PARSER_TRACE_FUNC("closing = '{}'", char(end));

        AstNodeListBuilder args;

        for (;;) {
            if (iter.peek().raw[0] == end) break;

            if (match(".")) {
                auto ident = iter.advance();
                if (ident.type != TokenType::Identifier) raise_error("expected parameter name");
                if (iter.advance().raw != "=") raise_error("expected '=' after named parameter");
                auto expr = expression();
                args.append(new AstCallNamedArgument{{AstNodeType::CallNamedArgument}, ident, expr});
            } else {
                args.append(expression());
            }

            if (!match(",")) break;
        }

        return args.to_list();
    }

    LANG_FORCEINLINE
    AstNode* maybe_function()
    {
        LANG_PARSER_TRACE_FUNC("");

        if (iter.peek().raw != "(") return nullptr;

        auto matched = iter.list->matching_pair_for(iter.peek());
        if (matched.peek_next().raw != "->" && matched.peek_next().raw != "{") return nullptr;

        iter.advance();

        // Function
        auto params = parameters(')');
        consume(")", "expected ')' to follow parameter list");
        AstNode* return_type{};
        if (match("->")) {
            return_type = expression(Operator::None, ExpressionMode::TypeAnnotation);
        }
        if (match("{")) {
            // Function Implementation
            auto body = finish_block();
            return new AstFunction{{AstNodeType::Function}, params, return_type, body};
        } else {
            // Function Prototype
            return new AstFunction{{AstNodeType::Function}, params, return_type, {}};
        }
    }

    LANG_FORCEINLINE
    AstStruct* finish_struct()
    {
        consume("{", "expected '{' following struct type introduction");

        AstNodeListBuilder stmts;

        while (!iter.is_at_end() && iter.peek().raw != lang_str("}")) {
            auto[stmt, is_terminal] = statement();
            if (!stmt) continue;
            stmts.append(stmt);
            if (is_terminal) {
                raise_error("terminal expressions not allowed at file scope");
            }
            if (stmt->type != AstNodeType::VarDecl) {
                raise_error("struct declaration body may only continue declarations");
            }
        }

        consume("}", "expected '}' to close struct body");

        return new AstStruct{{AstNodeType::Struct}, stmts.to_list()};
    }

    LANG_FORCEINLINE
    AstEnumValue* enum_value()
    {
        auto ident = consume_identifier("expected identifier in enum value");

        AstNode* init{};
        if (match("=")) {
            init = expression(Operator::None);
        }

        return new AstEnumValue{{AstNodeType::EnumValue}, ident, init};
    }

    LANG_FORCEINLINE
    AstEnum* finish_enum()
    {
        AstNode* type{};
        if (match(":")) {
            type = expression(Operator::None, ExpressionMode::TypeAnnotation);
        }

        consume("{", "expected '{' following enum type introduction");

        AstNodeListBuilder values;

        for (;;) {
            if (iter.is_at_end() || is("}")) break;

            values.append(enum_value());

            if (!match(",")) break;
        }

        consume("}", "expected '}' following enum declaration body");

        return new AstEnum{{AstNodeType::Enum}, type, values.to_list()};
    }

    LANG_FORCEINLINE
    AstNode* finish_match()
    {
        auto* expr = expression();
        consume("{", "expected '{' followed match expression");

        AstNodeListBuilder arms;

        for (;;) {
            if (iter.is_at_end() || is("}")) break;

            auto pattern = expression();
            consume("->", "expected '->' following match expression");
            auto body = block();

            arms.append(new AstMatchArm{{AstNodeType::MatchArm}, pattern, body});
        }

        consume("}", "expected '}' following match body");

        return new AstMatch{{AstNodeType::Match}, expr, arms.to_list()};
    }

    LANG_FORCEINLINE
    AstNode* named_identifier()
    {
        auto token = consume_identifier("expected identifier");
        return new AstNamedReference{{AstNodeType::NamedReference}, token};
    }

    LANG_FORCEINLINE
    AstNode* maybe_non_recursive_expression()
    {
        LANG_PARSER_TRACE_FUNC("");

        if (auto expr = maybe_block_expr()) {
            return expr;
        }

        auto next = iter.peek();
        switch (next.type) {
            case TokenType::Keyword:
                if (match("true") || match("false")) {
                    return new AstLiteral{{AstNodeType::Literal}, next};
                }  if (match("enum")) {
                    return finish_enum();
                } else if (match("struct")) {
                    return finish_struct();
                } else if (match("match")) {
                    return finish_match();
                }
                break;
            case TokenType::Number:
            case TokenType::String:
                iter.advance();
                return new AstLiteral{{AstNodeType::Literal}, next};
            case TokenType::Identifier:
                return named_identifier();
            case TokenType::Punctuation:
                if (next.punctuation.value == "(") {
                    if (auto func = maybe_function()) {
                        return func;
                    } else {
                        // Grouping
                        iter.advance();
                        auto expr = expression();
                        consume(")", "expected ')' to terminate group");
                        return expr;
                    }

                    break;
                }
                break;
            default:
        }

        return nullptr;
    }

    AstVarDecl* finish_var_decl(AstNode* target)
    {
        LANG_PARSER_TRACE_FUNC("");

        AstNode* type_expr{};
        bool constant = true;

        if (!is("=") && !is(":")) {
            type_expr = expression(Operator::None, ExpressionMode::TypeAnnotation);
        }

        bool has_init = false;
        if (match("=")) {
            has_init = true;
            constant = false;
        } else if (match(":")) {
            has_init = true;
        } else {
            constant = false;
        }

        if (has_init) {
            auto rhs = expression(Operator::VarDecl);
            return new AstVarDecl{{AstNodeType::VarDecl}, target, type_expr, constant, rhs};
        } else {
            return new AstVarDecl{{AstNodeType::VarDecl}, target, type_expr, constant, nullptr};
        }
    }

    AstNode* expression(Operator left_op = Operator::None, ExpressionMode mode = ExpressionMode::Normal)
    {
        LANG_PARSER_TRACE_FUNC("left_op = '{}', mode = '{}'", get_op_name(left_op), get_expr_mode_name(mode));

        auto lhs = maybe_non_recursive_expression();
        if (!lhs) {
            auto op_token = iter.advance();
            auto op = get_prefix_op(op_token.raw, mode);
            if (op == Operator::None) raise_error("expected valid lhs or prefix operator, got {}", op_token.raw);

            if ((op == Operator::Break || op == Operator::Return) && is_end_of_expression_token(iter.peek())) {
                // TODO: Generalize this for all mixed prefix/nonfix operators?
                lhs = new AstOpExpr{{AstNodeType::OpExpr}, op_token, op, {}, {}};
            } else {
                auto rhs = expression(op, mode);
                lhs = new AstOpExpr{{AstNodeType::OpExpr}, op_token, op, {}, rhs};
            }
        }

        for (;;) {
            auto op_token = iter.peek();

            auto op = get_other_op(op_token.raw, mode);
            if (op == Operator::None) {
// #if LANG_NOISY_PARSER
//                 log_debug("{}unrecognised operator '{}', breaking", indent_str(_depth), op_token.raw);
// #endif
                break;
            }

            auto dir = get_op_bind_dir(left_op, op);

// #if LANG_NOISY_PARSER
//             log_debug("{}({}, {}) = {}", indent_str(_depth), get_op_name(left_op), get_op_name(op), get_dir_name(dir));
// #endif
            if (dir == BindingDirection::Invalid) {
                raise_error("ambiguous operator sequence (no ordering for '{}' and '{}'), use parenthesis",
                    get_op_name(left_op), get_op_name(op));
            }
            if (dir == BindingDirection::Left) {
                break;
            }

            iter.advance();
            if (op == Operator::VarDecl) {
                // Variable Declaration
                lhs = finish_var_decl(lhs);

            } else  if (is_op_postfix(op)) {
                if (op_token.raw == ".") {
                    // Field accesss
                    auto field = iter.advance();
                    if (field.type != TokenType::Identifier)
                        raise_error("expected field following field access operator '.', got {}", field.raw);
                    lhs = new AstFieldAccess{{AstNodeType::FieldAccess}, lhs, field};
                } else if (op_token.raw == "[") {
                    // Subscript
                    auto args = arguments(']');
                    consume("]", "expected ']' to terminate subscript");
                    lhs = new AstCall{{AstNodeType::Call}, lhs, op_token, args};
                } else if (op_token.raw == "(") {
                    // Call
                    auto args = arguments(')');
                    consume(")", "expected ')' to terminate call");
                    lhs = new AstCall{{AstNodeType::Call}, lhs, op_token, args};
                } else {
                    lhs = new AstOpExpr{{AstNodeType::OpExpr}, op_token, op, lhs, {}};
                }
            } else {
                auto rhs = expression(op, mode);
                lhs = new AstOpExpr{{AstNodeType::OpExpr}, op_token, op, lhs, rhs};
            }
        }

        return lhs;
    }
};

AstNodeList parse(TokenList& tokens)
{
    Parser parser { tokens.begin() };
    return parser.file();
}

NAMESPACE_END
