#include "lang.hpp"

#include <ankerl/unordered_dense.h>

LANG_NAMESPACE_BEGIN

/*
 *
 * Pratt parser
 *
 * Reference: https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html
 *
 */

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
                    raise_error("op({}, {}) = [{}] contradicts earlier assignment to [{}]", get_op_name(l), get_op_name(r), get_dir_name(dir), get_dir_name(cur_dir));
                } else {
                    log_warn("  redundant assignment of op({}, {}) to = {}", get_op_name(l), get_op_name(r), get_dir_name(dir));
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
        for (int i = 0; i < op_groups.size(); ++i) {
            for (int j = i + 1; j < op_groups.size(); ++j) {
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

    order({{ LANG_OPS_All }, { None }});

    order({
        { LANG_OPS_Postfix LANG_OPS_Prefix },
        { LANG_OPS_Arithmetic LANG_OPS_Bitwise },
        { LANG_OPS_Comparison },
        { LANG_OPS_Logical },
        { LANG_OPS_Range },
        { TypeAnnotation }
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
    assoc(LeftToRight, { { Power }, { Multiply, Divide }, { Add, Subtract } });

    // Bitwise infix

    order({{ Bitwise_And }, { Bitwise_Or }});

    // Logical infix

    order(             {{ Logical_And }, { Logical_Or }});
    assoc(LeftToRight, {{ Logical_And }, { Logical_Or }});

    // Assignment

    order({{ LANG_OPS_Prefix }, { LANG_OPS_Assign }});
    bind(Right,
        { LANG_OPS_Assign },
        {
            LANG_OPS_Postfix
            LANG_OPS_Arithmetic
            LANG_OPS_Bitwise
            LANG_OPS_Comparison
            LANG_OPS_Logical
            LANG_OPS_Range
        });
    assoc(RightToLeft, {{ LANG_OPS_Assign }});

    // Type Annotation

    bind(Left, { TypeAnnotation }, { LANG_OPS_Assign });

    // Output

    log_debug("Defined {} operator relationships for {} operators", lookup.size(), std::array { LANG_OPS_All }.size());

#undef X

    return lookup;
}();

BindingDirection get_op_bind_dir(Operator l, Operator r)
{
    auto iter = OperatorOrdering.find(make_op_pair_key(l, r));
    return iter == OperatorOrdering.end() ? BindingDirection::Invalid : iter->second;
}

#define LANG_NOISY_PARSER 1

struct Parser
{
    TokenIterator iter;

#if LANG_NOISY_PARSER
    i32 depth = -1;
#endif

    bool is(std::string_view raw)
    {
        return iter.peek().raw == raw;
    }

    bool match(std::string_view raw)
    {
        if (iter.peek().raw == raw) {
            iter.advance();
            return true;
        }
        return false;
    }

    void consume(std::string_view raw, std::string_view message)
    {
        if (iter.peek().raw != raw) raise_error("{}, got {}", message, iter.peek().raw);
        iter.advance();
    }

// -------------------------------------------------------------------------

    AstNodeList finish_block()
    {
#if LANG_NOISY_PARSER
        depth++;
        defer { depth--; };
        log_debug("{}finish_block", indent_str(depth));
#endif

        AstNodeListBuilder stmts;

        // TODO

        if (!is("}")) {
            stmts.append(expression());
        }

        consume("}", "expected '}' to terminate block");

        return stmts.to_list();
    }

    AstNode* var_decl()
    {
#if LANG_NOISY_PARSER
        depth++;
        defer { depth--; };
        log_debug("{}var_decl", indent_str(depth));
#endif

        auto ident = iter.peek();
        if (ident.type == TokenType::Identifier) {
            iter.advance();
        } else if (ident.raw == ":") {
            ident = InvalidToken;
        } else {
            raise_error("expected identifier or ':' to begin var decl");
        }

        AstNode* type_expr{};
        AstNode* init{};
        bool constant = true;

        if (match(":")) {
            if (!is("=") && !is(":")) {
                type_expr = expression(Operator::TypeAnnotation);
            }

            if (match("=")) {
                init = expression();
                constant = false;
            } else if (match(":")) {
                init = expression();
            }
        }

        return new AstVarDecl{{AstNodeType::VarDecl}, ident, type_expr, constant, init};
    }

    AstNodeList parameters(u8 end)
    {
#if LANG_NOISY_PARSER
        depth++;
        defer { depth--; };
        log_debug("{}parameters({})", indent_str(depth), char(end));
#endif

        AstNodeListBuilder params;

        for (;;) {
            if (iter.peek().raw[0] == end) break;

            params.append(var_decl());

            if (!match(",")) break;
        }

        return params.to_list();
    }

    AstNodeList arguments(u8 end)
    {
#if LANG_NOISY_PARSER
        depth++;
        defer { depth--; };
        log_debug("{}arguments({})", indent_str(depth), char(end));
#endif

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

    AstNode* expression(Operator left_op = Operator::None)
    {
#if LANG_NOISY_PARSER
        depth++;
        defer { depth--; };
        log_debug("{}expression({})", indent_str(depth), get_op_name(left_op));
#endif

        // Prefix / "Nonfix"

        auto next = iter.advance();
        AstNode* lhs{};
        switch (next.type) {
            case TokenType::Keyword:
                if (next.raw == "true" || next.raw == "false") {
                    lhs = new AstLiteral{{AstNodeType::Literal}, next};
                    break;
                } else {
                    [[fallthrough]];
                }
            case TokenType::Number:
            case TokenType::String:
                lhs = new AstLiteral{{AstNodeType::Literal}, next};
                break;
            case TokenType::Identifier:
                lhs = new AstNamedReference{{AstNodeType::NamedReference}, next};
                break;
            case TokenType::Punctuation:
                if (next.punctuation.value == "(") {
                    auto matched = iter.list->matching_pair_for(next);
                    if (matched.peek_next().raw == "->" || matched.peek_next().raw == "{") {
                        // Function
                        auto params = parameters(')');
                        consume(")", "expected ')' to follow parameter list");
                        AstNode* return_type{};
                        if (match("->")) {
                            return_type = expression(Operator::TypeAnnotation);
                        }
                        if (match("{")) {
                            // Function Implementation
                            auto body = finish_block();
                            lhs = new AstFunction{{AstNodeType::Function}, params, return_type, body};
                        } else {
                            // Function Prototype
                            lhs = new AstFunction{{AstNodeType::Function}, params, return_type, {}};
                        }
                    } else {
                        // Grouping
                        lhs = expression();
                        consume(")", "expected ')' to terminate group");
                    }
                    break;
                } else {
                    [[fallthrough]];
                }
            default:
                auto op = get_prefix_op(next.raw);
                if (op != Operator::None) {
                    auto rhs = expression(op);
                    lhs = new AstOpExpr{{AstNodeType::OpExpr}, {}, next, rhs};
                } else {
                    raise_error("expected valid lhs, got {}", next.raw);
                }
        }

        for (;;) {
            auto op_token = iter.peek();
            if (op_token.type == TokenType::Invalid) break;

            auto op = get_other_op(op_token.raw);
            if (op == Operator::None) {
                break;
            }

            auto dir = get_op_bind_dir(left_op, op);

#if LANG_NOISY_PARSER
            log_debug("{}({}, {}) = {}", indent_str(depth), get_op_name(left_op), get_op_name(op), get_dir_name(dir));
#endif
            if (dir == BindingDirection::Invalid) {
                raise_error("ambiguous operator sequence (no ordering for '{}' and '{}'), use parenthesis",
                    get_op_name(left_op), get_op_name(op));
            }
            if (dir == BindingDirection::Left) {
                break;
            }

            iter.advance();
            if (is_op_postfix(op)) {
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
                    lhs = new AstOpExpr{{AstNodeType::OpExpr}, lhs, op_token, {}};
                }
            } else {
                auto rhs = expression(op);
                lhs = new AstOpExpr{{AstNodeType::OpExpr}, lhs, op_token, rhs};
            }
        }

        return lhs;
    }
};

AstNodeList parse(TokenList& tokens)
{
    Parser parser { tokens.begin() };
    return {parser.var_decl()};
}

LANG_NAMESPACE_END
