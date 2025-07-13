#include "lang.hpp"
#include "scan.hpp"

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

#define X(i, ...) i,

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

#define LANG_NOISY_PARSER 0

#if LANG_NOISY_PARSER
#define LANG_PARSER_TRACE_FUNC(fmt, ...) \
    auto _func_name = __func__; \
    log_debug("{}{}(" fmt ")", indent_str(_depth, ". "), _func_name __VA_OPT__(,) __VA_ARGS__); \
    _depth++; \
    defer { \
        _depth--; \
        log_trace(VT_color_reset "{}" VT_color_begin(90) "{}", indent_str(_depth, ". "), _func_name); \
    }
#else
#define LANG_PARSER_TRACE_FUNC(...)
#endif

struct Parser
{
    Constants* constants;
    Scanner* scanner;
    Ast& ast;

    Token token;
    bool need_advance = true;

#if LANG_NOISY_PARSER
    i32 _depth = 0;
#endif

// -------------------------------------------------------------------------

    Token peek()
    {
        if (need_advance) {
            token = scanner->next();
#if LANG_NOISY_PARSER
            log_trace("{}{}", indent_str(_depth, ". "), token_to_string(token));
#endif
            need_advance = false;
        }
        return token;
    }

    bool is_at_end()
    {
        return peek().type == TokenTypes::Eof;
    }

    Token advance()
    {
        auto value = peek();
        need_advance = true;
        return value;
    }

// -------------------------------------------------------------------------

    bool is(TokenType type)
    {
        return peek().type == type;
    }

    bool match(TokenType type)
    {
        if (is(type)) {
            advance();
            return true;
        }
        return false;
    }

    void consume(TokenType type, std::string_view message)
    {
        auto token = advance();
        if (token.type != type) raise_error("{}, got {}", message, get_token_type_name(token.type));
    }

    Token consume_identifier(std::string_view message)
    {
        auto token = advance();
        if (!token_type_is_identifier_like(token.type)) raise_error("{}, got {}", message, get_token_type_name(token.type));
        return token;
    }

// -------------------------------------------------------------------------

    AstOffset maybe_else()
    {
        LANG_PARSER_TRACE_FUNC("");

        if (match(keyword("else"))) {
            if (match(punc("{"))) {
                return finish_block_contents();
            } else {
                AstListBuilder builder{ast};
                builder.add(block_expr());
                return builder.to_list();
            }
        }

        return {};
    }

    AstOffset finish_if()
    {
        LANG_PARSER_TRACE_FUNC("");

        auto expr = expression();
        consume(punc("{"), "expected '{' after if conditional");
        auto then_branch = finish_block_contents();
        auto else_branch = maybe_else();

        auto offset = ast.write(else_branch ? AstHeader::IfElse : AstHeader::If);
        ast.write(expr);
        ast.write(then_branch);
        if (else_branch) {
            ast.write(else_branch);
        }

        return offset;
    }

    AstOffset finish_for()
    {
        LANG_PARSER_TRACE_FUNC("");

        auto range_expr = expression();

        consume(punc("->"), "expected '->' after for range expression");

        consume(punc("("), "expected parameter list in for");
        auto params = parameters(punc(")"));
        consume(punc(")"), "expected ')' to follow parameter list");

        consume(punc("{"), "expected '{' to follow parameter list");
        auto body = finish_block_contents();

        auto else_expr = maybe_else();

        auto offset = ast.write(else_expr ? AstHeader::ForElse : AstHeader::For);
        ast.write(range_expr);
        ast.write(params);
        ast.write(body);
        if (else_expr) {
            ast.write(else_expr);
        }

        return offset;
    }

    AstOffset finish_while()
    {
        LANG_PARSER_TRACE_FUNC("");

        auto cond = expression();
        consume(punc("{"), "expected '{' after while condition");
        auto body = finish_block_contents();
        auto else_expr = maybe_else();

        auto offset = ast.write(else_expr ? AstHeader::WhileElse : AstHeader::While);
        ast.write(cond);
        ast.write(body);
        if (else_expr) {
            ast.write(else_expr);
        }

        return offset;
    }

#define PARSE_MaybeBlockExpr \
    case keyword("if"):    advance(); return finish_if(); \
    case keyword("for"):   advance(); return finish_for(); \
    case keyword("while"): advance(); return finish_while(); \
    case punc("{"):        advance(); return finish_block(); \
    case punc("\\{"): \
        raise_error("TODO"); \
        // return splice();

    AstOffset maybe_block_expr()
    {
        LANG_PARSER_TRACE_FUNC("");

        switch (peek().type) {
            PARSE_MaybeBlockExpr
        }

        return {};
    }

    AstOffset block_expr()
    {
        LANG_PARSER_TRACE_FUNC("");

        auto expr = maybe_block_expr();
        if (!expr) raise_error("expected block expression, got {}", get_token_type_name(peek().type));
        return expr;
    }

    AstOffset maybe_defer()
    {
        LANG_PARSER_TRACE_FUNC("");

        if (match(keyword("defer"))) {
            consume(punc("{"), "expected '{' to start defer body");
            auto body = finish_block_contents();
            auto offset = ast.write(AstHeader::Defer);
            ast.write(body);
            return offset;
        }
        return {};
    }

    std::pair<AstOffset, bool> statement()
    {
        LANG_PARSER_TRACE_FUNC("");

        if (is_at_end()) return {{}, true};
        if (match(punc(";"))) return {{}, false};

        // Block expression

        if (auto stmt = maybe_block_expr()) return {stmt, false};

        // Defer

        if (auto stmt = maybe_defer()) return {stmt, false};

        // Expression statement

        auto expr = expression();
        return {expr, !match(punc(";"))};
    }

    AstOffset finish_block_contents()
    {
        LANG_PARSER_TRACE_FUNC("");

        AstListBuilder builder{ast};

        while (!is(punc("}"))) {
            auto[stmt, is_terminal] = statement();
            if (!stmt) {
                if (is_terminal) break;
                else continue;
            }

            builder.add(stmt);

            if (is_terminal) {
                // terminal = stmt;
                break;
            }
        }

        consume(punc("}"), "expected '}' to terminate block");

        return builder.to_list();
    }

    AstOffset finish_block()
    {
        LANG_PARSER_TRACE_FUNC("");

        auto first = finish_block_contents();

        auto offset = ast.write(AstHeader::Block);
        ast.write(first);

        return offset;
    }

    AstOffset block()
    {
        LANG_PARSER_TRACE_FUNC("");

        consume(punc("{"), "expected block");
        return finish_block();
    }

    AstOffset file()
    {
        LANG_PARSER_TRACE_FUNC("");

        AstListBuilder builder{ast};

        while (peek().type != TokenTypes::Eof) {
            auto[stmt, is_terminal] = statement();
            if (!stmt) {
                if (is_terminal) break;
                else continue;
            }

            builder.add(stmt);

            if (is_terminal) {
                break;
                // raise_error("terminal expressions not allowed at file scope");
            }
        }

        if (peek().type != TokenTypes::Eof) {
            raise_error("expected end of file, got {}", get_token_type_name(peek().type));
        }

        auto offset = ast.write(AstHeader::File);
        ast.write(builder.to_list());

        return offset;
    }

    AstOffset parameter()
    {
        LANG_PARSER_TRACE_FUNC("");

        AstOffset target{};
        auto ident = peek();
        if (ident.type == TokenTypes::Identifier) {
            advance();
            target = ast.write(AstHeader::Identifier);
            ast.write(constants->identifiers.get_id(ident.identifier.value));
        } else if (ident.type != punc(":")) {
            raise_error("expected identifier or ':' to begin parameter");
        }

        if (match(punc(":"))) {
            return finish_var_decl(target);
        } else {
            auto offset = ast.write(get_vardecl_header(true, false, false));
            ast.write(target);
            return offset;
        }
    }

    AstOffset parameters(TokenType end)
    {
        LANG_PARSER_TRACE_FUNC("closing = '{}'", char(end));

        AstListBuilder builder{ast};

        for (;;) {
            if (peek().type == end) break;

            builder.add(parameter());

            if (!match(punc(","))) break;
        }

        return builder.to_list();
    }

    AstOffset arguments(TokenType end)
    {
        LANG_PARSER_TRACE_FUNC("closing = '{}'", char(end));

        AstListBuilder builder{ast};

        for (;;) {
            if (peek().type == end) break;

            AstOffset arg;
            if (match(punc("."))) {
                raise_error("TODO");
                // auto ident = advance();
                // if (ident.type != TokenType::Identifier) raise_error("expected parameter name");
                // if (advance().raw != punc("=")) raise_error("expected '=' after named parameter");
                // auto expr = expression();
                // args.append(new AstCallNamedArgument{{AstNodeType::CallNamedArgument}, ident, expr});
            } else {
                arg = expression();
            }

            builder.add(arg);

            if (!match(punc(","))) break;
        }

        return builder.to_list();
    }

    LANG_forceinline
    AstOffset finish_function()
    {
        LANG_PARSER_TRACE_FUNC("");

        consume(punc("("), "expected '(' to begin function parameters");

        // Function
        auto params = parameters(punc(")"));
        consume(punc(")"), "expected ')' to follow parameter list");
        AstOffset return_type{};
        if (match(punc("->"))) {
            return_type = expression(Operator::None, ExpressionMode::TypeAnnotation);
        }
        if (match(punc("{"))) {
            auto body = finish_block_contents();
            auto offset = ast.write(AstHeader::Function_Definition);
            ast.write(params);
            ast.write(return_type);
            ast.write(body);
            return offset;
        } else {
            auto offset = ast.write(AstHeader::Function_Prototype);
            ast.write(params);
            ast.write(return_type);
            return offset;
        }
    }

    // LANG_forceinline
    // AstStruct* finish_struct()
    // {
    //     consume(punc("{"), "expected '{' following struct type introduction");

    //     AstNodeListBuilder stmts;

    //     while (!iter.is_at_end() && peek().raw != lang_str(punc("}"))) {
    //         auto[stmt, is_terminal] = statement();
    //         if (!stmt) continue;
    //         stmts.append(stmt);
    //         if (is_terminal) {
    //             raise_error("terminal expressions not allowed at file scope");
    //         }
    //         if (stmt->type != AstNodeType::VarDecl && stmt->type != AstNodeType::Splice) {
    //             raise_error("struct declaration body may only continue declarations and splices");
    //         }
    //     }

    //     consume(punc("}"), "expected '}' to close struct body");

    //     return new AstStruct{{AstNodeType::Struct}, stmts.to_list()};
    // }

    // LANG_forceinline
    // AstEnumValue* enum_value()
    // {
    //     auto ident = consume_identifier("expected identifier in enum value");

    //     AstNode* init{};
    //     if (match(punc("="))) {
    //         init = expression(Operator::None);
    //     }

    //     return new AstEnumValue{{AstNodeType::EnumValue}, ident, init};
    // }

    // LANG_forceinline
    // AstEnum* finish_enum()
    // {
    //     AstNode* type{};
    //     if (match(punc(":"))) {
    //         type = expression(Operator::None, ExpressionMode::TypeAnnotation);
    //     }

    //     consume(punc("{"), "expected '{' following enum type introduction");

    //     AstNodeListBuilder values;

    //     for (;;) {
    //         if (iter.is_at_end() || is(punc("}"))) break;

    //         values.append(enum_value());

    //         if (!match(punc(","))) break;
    //     }

    //     consume(punc("}"), "expected '}' following enum declaration body");

    //     return new AstEnum{{AstNodeType::Enum}, type, values.to_list()};
    // }

    // LANG_forceinline
    // AstNode* finish_match()
    // {
    //     auto* expr = expression();
    //     consume(punc("{"), "expected '{' followed match expression");

    //     AstNodeListBuilder arms;

    //     for (;;) {
    //         if (iter.is_at_end() || is(punc("}"))) break;

    //         if (is("\\{")) {
    //             arms.append(splice());
    //         } else {
    //             auto pattern = expression();
    //             consume(punc("->"), "expected '->' following match expression");
    //             auto body = block();

    //             arms.append(new AstMatchArm{{AstNodeType::MatchArm}, pattern, body});
    //         }
    //     }

    //     consume(punc("}"), "expected '}' following match body");

    //     return new AstMatch{{AstNodeType::Match}, expr, arms.to_list()};
    // }

    // AstTokenSequence* token_sequence()
    // {
    //     auto open = advance();

    //     AstNodeListBuilder builder;

    //     while (iter.index() != open.punctuation.matching) {
    //         if (is("\\{")) {
    //             builder.append(splice());
    //         } else if (is("\\id{")) {
    //             builder.append(splice_identifier());
    //         } else {
    //             builder.append(new AstTokenBasic{{AstNodeType::TokenBasic}, advance()});
    //         }
    //     }

    //     advance();

    //     return new AstTokenSequence{{AstNodeType::TokenSequence}, builder.to_list()};
    // }

    // AstSpliceIdentifier* splice_identifier()
    // {
    //     consume("\\id{", "expected '\\id{' to introduce splice identifier");

    //     auto block = finish_block();

    //     return new AstSpliceIdentifier{{AstNodeType::SpliceIdentifier}, block};
    // }

    // AstSplice* splice()
    // {
    //     consume("\\{", "expected '\\{' to introduce splice");

    //     auto block = finish_block();

    //     return new AstSplice{{AstNodeType::Splice}, block};
    // }

    LANG_forceinline
    AstOffset named_identifier_unchecked()
    {
        auto token = advance();
        auto offset = ast.write(AstHeader::Identifier);
        if (token.type == TokenTypes::Identifier) {
            // pure identifier
            ast.write(constants->identifiers.get_id(token.identifier.value));
        } else {
            // identifier-like keyword
            ast.write(constants->identifiers.get_id(get_keyword_str(token.type)));
        }
        return offset;
    }

    LANG_forceinline
    AstOffset maybe_non_recursive_expression()
    {
        LANG_PARSER_TRACE_FUNC("");

        auto next = peek();
        switch (next.type) {
            PARSE_MaybeBlockExpr
            case keyword("true"):
                advance();
                return ast.write(AstHeader::Literal_True);
            case keyword("false"):
                advance();
                return ast.write(AstHeader::Literal_False);
            case keyword("enum"):
                advance();
                raise_error("TODO");
                // return finish_enum();
            case keyword("struct"):
                advance();
                raise_error("TODO");
                // return finish_struct();
            case keyword("match"):
                advance();
                raise_error("TODO");
                // return finish_match();
            case TokenTypes::Number: {
                advance();
                auto offset = ast.write(AstHeader::Literal_Number);
                ast.write(constants->numbers.get_id(next.number.value));
                return offset;
            }
            case TokenTypes::String: {
                advance();
                auto offset = ast.write(AstHeader::Literal_String);
                if (next.string.has_escapes) {
                    raise_error("TODO: Handle interning of escaped strings");
                }
                ast.write(constants->strings.get_id(next.string.value));
                return offset;
            }
            TOKEN_IsIdentifierLikeCases:
                return named_identifier_unchecked();
            case punc("${"):
                raise_error("TODO");
                // return token_sequence();
            case keyword("fn"):
                advance();
                return finish_function();
            case punc("("):
                // Grouping
                advance();
                auto expr = expression();
                consume(punc(")"), "expected ')' to terminate group");
                return expr;
        }

        return {};
    }

    AstOffset finish_var_decl(AstOffset target)
    {
        LANG_PARSER_TRACE_FUNC("");

        AstOffset type_expr{};
        bool constant = true;

        if (!is(punc("=")) && !is(punc(":"))) {
            type_expr = expression(Operator::None, ExpressionMode::TypeAnnotation);
        }

        bool has_init = false;
        if (match(punc("="))) {
            has_init = true;
            constant = false;
        } else if (match(punc(":"))) {
            has_init = true;
        } else {
            constant = false;
        }

        AstOffset init;
        if (has_init) {
            init = expression(Operator::VarDecl);
        }

        auto offset = ast.write(get_vardecl_header(constant, bool(type_expr), has_init));
        ast.write(target);
        if (type_expr) ast.write(type_expr);
        if (has_init) ast.write(init);

        return offset;
    }

    AstOffset expression(Operator left_op = Operator::None, ExpressionMode mode = ExpressionMode::Normal)
    {
        LANG_PARSER_TRACE_FUNC("left_op = '{}', mode = '{}'", get_op_name(left_op), get_expr_mode_name(mode));

        auto lhs = maybe_non_recursive_expression();
        if (!lhs) {
            auto op_token = advance();
            auto op = get_prefix_op(op_token.type, mode);
            if (op == Operator::None) raise_error("expected valid lhs or prefix operator, got {}", get_token_type_name(op_token.type));

            switch (op) {
// -----------------------------------------------------------------------------
            #define X(name, ...) \
                case Operator::name: { \
                    if (is_end_of_expression_token(peek())) { \
                        lhs = ast.write(AstHeader::name##_Empty); \
                    } else { \
                        auto rhs = expression(op, mode); \
                        lhs = ast.write(AstHeader::name); \
                        ast.write(rhs); \
                    } \
                    break; \
                }
                OPS_ControlFlow
            #undef X
// -----------------------------------------------------------------------------
            #define X(name, ...) \
                case Operator::name: { \
                    auto rhs = expression(op, mode); \
                    lhs = ast.write(AstHeader::name); \
                    ast.write(rhs); \
                    break; \
                }
                OPS_Prefix
            #undef X
// -----------------------------------------------------------------------------
                default:
                    raise_error("unhandled prefix operator: {}", get_op_name(op));
            }
        }

        for (;;) {
            auto op_token = peek();

            auto op = get_other_op(op_token.type, mode);
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

            advance();
            if (op == Operator::VarDecl) {
                // Variable Declaration
                lhs = finish_var_decl(lhs);

            } else  if (is_op_postfix(op)) {
                switch (op) {
                    case Operator::FieldAccess: {
                        auto field = advance();
                        if (field.type != TokenTypes::Identifier)
                            raise_error("expected field following field access operator '.', got {}", get_token_type_name(field.type));
                        auto new_lhs = ast.write(AstHeader::FieldAccess);
                        ast.write(constants->identifiers.get_id(field.identifier.value));
                        ast.write(lhs);
                        lhs = new_lhs;
                        break;
                    }
                    case Operator::Call: {
                        auto args = arguments(punc(")"));
                        consume(punc(")"), "expected ')' to terminate call");
                        auto new_lhs = ast.write(AstHeader::Call);
                        ast.write(lhs);
                        ast.write(args);
                        lhs = new_lhs;
                        break;
                    }
                    case Operator::Subscript: {
                        auto args = arguments(punc("]"));
                        consume(punc("]"), "expected ']' to terminate subscript");
                        auto new_lhs = ast.write(AstHeader::Subscript);
                        ast.write(lhs);
                        ast.write(args);
                        lhs = new_lhs;
                        break;
                    }
                #define X(name, ...) case Operator::name: \
                    { \
                        auto new_lhs = ast.write(AstHeader::name); \
                        ast.write(lhs); \
                        lhs = new_lhs; \
                        break; \
                    }
                    X(Dereference)
                    X(Postincrement)
                    X(Postdecrement)
                #undef X
                    default:
                        raise_error("TODO");
                }
            } else {
                auto rhs = expression(op, mode);
                switch (op) {
                #define X(name, ...) \
                    case Operator::name: { \
                        auto new_lhs = ast.write(AstHeader::name); \
                        ast.write(lhs); \
                        ast.write(rhs); \
                        lhs = new_lhs; \
                        break; \
                    }
                    OPS_Arithmetic
                    OPS_Bitwise
                    OPS_Logical
                    OPS_Comparison
                    OPS_Range
                    OPS_Assign
                #undef X
                    default:
                        raise_error("unhandled infix operator: {}", get_op_name(op));
                }
            }
        }

        return lhs;
    }
};

AstOffset parse(Ast& out, Scanner* scanner, Constants& constants)
{
    Parser parser {
        .constants = &constants,
        .scanner = scanner,
        .ast = out
    };
    auto offset = parser.file();
    return offset;
}

NAMESPACE_END
