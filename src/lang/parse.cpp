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
        auto ident = iter.peek();
        if (ident.type == lang::TokenType::Identifier) {
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
                type_expr = expression(TypeExprBindingPower);
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
        AstNodeListBuilder args;

        for (;;) {
            if (iter.peek().raw[0] == end) break;

            if (match(".")) {
                auto ident = iter.advance();
                if (ident.type != lang::TokenType::Identifier) raise_error("expected parameter name");
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
                        consume(")", "expected ')' to follow parameter list");
                        AstNode* return_type{};
                        if (match("->")) {
                            return_type = expression(TypeExprBindingPower);
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
                if (auto bp = prefix_binding_power(next)) {
                    auto rhs = expression(bp.right);
                    lhs = new AstOpExpr{{AstNodeType::OpExpr}, {}, next, rhs};
                } else {
                    raise_error("expected valid lhs, got {}", next.raw);
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
                    consume("]", "expected ']' to terminate subscript");
                    lhs = new AstCall{{AstNodeType::Call}, lhs, op, args};
                } else if (op.raw == "(") {
                    // Call
                    auto args = arguments(')');
                    consume(")", "expected ')' to terminate call");
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
