#include "lang.hpp"

#include <ankerl/unordered_dense.h>

LANG_NAMESPACE_USE

/*
 *
 * Pratt parser
 *
 * Reference: https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html
 *
 */

struct BindingPowerSet
{
    BindingPower prefix{}, infix{}, postfix{};
};

const static
auto BindingPowerLookup = []{
    ankerl::unordered_dense::map<std::string_view, BindingPowerSet> lookup;

    u8 binding_power = 255;
    FixType fix_type;
    Associativity associativity;

    for (auto& op : Operators) {
        std::visit(overload_set {
            [&](std::string_view str) {
                BindingPower bp = {};
                switch (fix_type) {
                    case lang::FixType::Prefix:
                        bp.right = binding_power;
                        lookup[str].prefix = bp;
                        break;
                    case lang::FixType::Infix:
                        if (associativity == Associativity::LeftToRight) {
                            bp.left = binding_power - 1;
                            bp.right = binding_power;
                        } else {
                            bp.left = binding_power;
                            bp.right = binding_power - 1;
                        }
                        lookup[str].infix = bp;
                        break;
                    case lang::FixType::Postfix:
                        bp.left = binding_power;
                        lookup[str].postfix = bp;
                        break;
                }
            },
            [&](FixType _fix_type) {
                fix_type = _fix_type;
            },
            [&](OperatorGroup group) {
                binding_power -= 2;
                associativity = group.dir;
            }
        }, op);
    }

    return lookup;
}();

const static
auto TypeExprBindingPower = BindingPowerLookup.at(TypeExprOperatorKey).prefix.right;

BindingPower prefix_binding_power(Token token)
{
    auto iter = BindingPowerLookup.find(token.raw);
    if (iter == BindingPowerLookup.end()) return {};
    return iter->second.prefix;
}

BindingPower infix_binding_power(Token token)
{
    auto iter = BindingPowerLookup.find(token.raw);
    if (iter == BindingPowerLookup.end()) return {};
    return iter->second.infix;
}

BindingPower postfix_binding_power(Token token)
{
    auto iter = BindingPowerLookup.find(token.raw);
    if (iter == BindingPowerLookup.end()) return {};
    return iter->second.postfix;
}

struct Parser
{
    TokenIterator iter;

    AstNodeList finish_block()
    {
        AstNodeListBuilder stmts;

        // TODO

        if (iter.peek().raw != "}") {
            stmts.append(expression());
        }

        if (iter.advance().raw != "}") raise_error("expected '}}' to terminate block");

        return stmts.to_list();
    }

    AstNode* var_decl()
    {
        auto ident = iter.advance();
        if (ident.type != lang::TokenType::Identifier) {
            if (ident.raw != ":") {
               raise_error("expected identifier or ':'");
            }

            auto type_expr = expression();
            return new AstVarDecl{{AstNodeType::VarDecl}, InvalidToken, type_expr, true, {}};
        }

        AstNode* type_expr{};
        AstNode* init{};
        bool constant = true;

        if (iter.peek().raw == ":") {
            iter.advance();
            if (iter.peek().raw != "=") {
                type_expr = expression(TypeExprBindingPower);
            }

            if (iter.peek().raw == "=") {
                iter.advance();
                init = expression();
                constant = false;
            } else if (iter.peek().raw == ":") {
                iter.advance();
                init = expression();
            }
        }

        return new AstVarDecl{{AstNodeType::VarDecl}, ident, type_expr, constant, init};
    }

    AstNodeList parameters(u8 end)
    {
        AstNodeListBuilder params;

        for (;;) {
            if (iter.peek().raw[0] == end) break;

            params.append(var_decl());

            if (iter.peek().raw != ",") break;
            iter.advance();
        }

        return params.to_list();
    }

    AstNodeList arguments(u8 end)
    {
        AstNodeListBuilder args;

        for (;;) {
            if (iter.peek().raw[0] == end) break;

            auto t = iter.peek();
            if (t.raw == ".") {
                iter.advance();
                auto ident = iter.advance();
                if (ident.type != lang::TokenType::Identifier) raise_error("expected parameter name");
                if (iter.advance().raw != "=") raise_error("expected '=' after named parameter");
                auto expr = expression();
                args.append(new AstCallNamedArgument{{AstNodeType::CallNamedArgument}, ident, expr});
            } else {
                auto expr = expression();
                args.append(expr);
            }

            if (iter.peek().raw != ",") break;

            iter.advance();
        }

        return args.to_list();
    }

    AstNode* expression(u32 min_bp = 0)
    {
        // Prefix / "Nonfix"

        auto next = iter.advance();
        AstNode* lhs{};
        switch (next.type) {
            case lang::TokenType::Keyword:
                if (next.raw == "true" || next.raw == "false") {
                    lhs = new AstLiteral{{AstNodeType::Literal}, next};
                    break;
                } else {
                    [[fallthrough]];
                }
            case lang::TokenType::Number:
            case lang::TokenType::String:
                lhs = new AstLiteral{{AstNodeType::Literal}, next};
                break;
            case lang::TokenType::Identifier:
                lhs = new AstNamedReference{{AstNodeType::NamedReference}, next};
                break;
            case lang::TokenType::Punctuation:
                if (next.punctuation.value == "(") {
                    auto matched = iter.list->matching_pair_for(next);
                    if (matched.peek_next().raw == "->" || matched.peek_next().raw == "{") {
                        // Function
                        auto params = parameters(')');
                        if (iter.advance().raw != ")") raise_error("expected ')' to follow parameter list, got {}", iter.peek_prev().raw);
                        AstNode* return_type{};
                        if (iter.peek().raw == "->") {
                            iter.advance();
                            return_type = expression(TypeExprBindingPower);
                        }
                        if (iter.peek().raw == "{") {
                            // Function Implementation
                            iter.advance();
                            auto body = finish_block();
                            lhs = new AstFunction{{AstNodeType::Function}, params, return_type, body};
                        } else {
                            // Function Prototype
                            lhs = new AstFunction{{AstNodeType::Function}, params, return_type, {}};
                        }
                    } else {
                        // Grouping
                        lhs = expression();
                        if (iter.advance().raw != ")") raise_error("expected ')' to terminate group");
                    }
                    break;
                } else {
                    [[fallthrough]];
                }
            default:
                if (auto bp = prefix_binding_power(next)) {
                    auto rhs = expression(bp.right);
                    lhs = new AstOpExpr{{AstNodeType::OpExpr}, {}, next, rhs};
                } else {
                    raise_error("expected valid lhs, found {}", next.raw);
                }
        }

        for (;;) {
            auto op = iter.peek();
            if (op.type == TokenType::Invalid) break;
            if (op.type != TokenType::Punctuation) raise_error("expected operator, got {}", op.raw);

            // Infix

            if (auto bp = infix_binding_power(op)) {
                if (bp.left < min_bp) break;

                iter.advance();
                auto rhs = expression(bp.right);
                lhs = new AstOpExpr{{AstNodeType::OpExpr}, lhs, op, rhs};
                continue;
            }

            // Postfix

            if (auto bp = postfix_binding_power(op)) {
                if (bp.left < min_bp) break;

                iter.advance();

                if (op.raw == ".") {
                    // Field accesss
                    auto field = iter.advance();
                    if (field.type != lang::TokenType::Identifier)
                        raise_error("expected field following field access operator '.', got {}", field.raw);
                    lhs = new AstFieldAccess{{AstNodeType::FieldAccess}, lhs, field};
                } else if (op.raw == "[") {
                    // Subscript
                    auto args = arguments(']');
                    if (auto t = iter.advance(); t.raw != "]")
                        raise_error("expected ']' to terminate subscript, got {}", t.raw);
                    lhs = new AstCall{{AstNodeType::Call}, lhs, op, args};
                } else if (op.raw == "(") {
                    // Call
                    auto args = arguments(')');
                    if (auto t = iter.advance(); t.raw != ")")
                        raise_error("expected ')' to terminate call, got {}", t.raw);
                    lhs = new AstCall{{AstNodeType::Call}, lhs, op, args};
                } else {
                    lhs = new AstOpExpr{{AstNodeType::OpExpr}, lhs, op, {}};
                }

                continue;
            }

            break;
        }

        return lhs;
    }
};

AstNodeList lang::parse(TokenList& tokens)
{
    Parser parser { tokens.begin() };
    return {parser.expression()};
}
