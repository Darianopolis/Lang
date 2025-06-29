#pragma once

#include <lang/core.hpp>

#include <vector>
#include <variant>

NAMESPACE_BEGIN(lang)

// -----------------------------------------------------------------------------

constexpr std::array Keywords
{
    "if", "else",
    "for", "while", "match",
    "break", "continue",
    "return", "defer",
    "true", "false",
    "struct", "enum",
};

constexpr std::array PunctuationSequences
{
    "^",    // dereference

    "+",    // arithmetic identity / add
    "+=",   // add assign
    "++",   // pre/post increment
    "-",    // arithmetic negation / subtract
    "-=",   // subtract assign
    "--",   // pre/post decrement
    "->",   // return type introducer
    "*",    // multiply
    "*=",   // multiply assign
    "**",   // power
    "**=",  // power assign
    "/",    // divide
    "/=",   // divide assign
    "%",    // modulo
    "%=",   // moduloe assign
    "<",    // less than
    "<=",   // less greater
    "<<",   // shift left
    "<<=",  // shift left assign
    ">",    // greater
    ">=",   // greater equal
    ">>",   // shift right
    ">>=",  // shift right assign
    "&",    // bitwise and
    "&=",   // bitwise and assign
    "&&",   // logical and
    "|",    // bitwise or
    "|=",   // bitwise or assign
    "||",   // logical or
    "=",    // assign
    "==",   // equal
    "!",    // logical negate
    "!=",   // not equal
    "~",    // bitwise negate
    "~|",   // xor
    "~|=",  // xor assign

    "(", ")",   // parentheses
    "{", "}",   // braces
    "[", "]",   // brackets

    ".",    // field access
    "..",   // range exclusive
    "..=",  // range inclusive
    ":",    // declaration / type annotation introducer / const init

    ";",    // statement delimiter
    ",",    // expression delimiter

    "?",    // *reserved
};


consteval std::string_view lang_str(std::string_view str)
{
    for (auto key : Keywords) if (str == key) return str;
    for (auto punc : PunctuationSequences) if (str == punc) return str;

    raise_error("Expected valid language string, got '{}'", str);
}

struct StaticLangString
{
    std::string_view str;

    template<size_t Size>
    consteval StaticLangString(const char (&str)[Size])
        : str(lang_str(std::string_view(str)))
    {}

    consteval StaticLangString(std::string_view _str)
        : str(lang_str(_str))
    {}
};

struct RuntimeLangString
{
    std::string_view str;

    explicit RuntimeLangString(std::string_view _str): str(_str) {}
};

// -----------------------------------------------------------------------------

#define OPS_Postfix \
    X(Call) X(Subscript) X(FieldAccess) X(Postincrement) X(Postdecrement) X(Dereference)

#define OPS_Prefix \
    X(Preincrement) X(Predecrement) X(Arithmetic_Identity) \
    X(Arithmetic_Negate) X(Bitwise_Negate) X(Logical_Negate) X(AddressOf)

#define OPS_Arithmetic \
    X(Add) X(Subtract) X(Multiply) X(Divide) X(Modulo) X(Power)

#define OPS_Bitwise \
    X(ShiftLeft) X(ShiftRight) X(Xor) X(Bitwise_And) X(Bitwise_Or)

#define OPS_Logical \
    X(Logical_And) X(Logical_Or)

#define OPS_Comparison \
    X(Equal) X(NotEqual) X(Greater) X(GreaterEqual) X(Less) X(LessEqual)

#define OPS_Range \
    X(Range_Exclusive) X(Range_Inclusive)

#define OPS_VarDecl \
    X(VarDecl)

#define OPS_Assign \
    X(Assign) \
    X(Assign_Add) X(Assign_Subtract) X(Assign_Multiply) X(Assign_Divide) X(Assign_Modulo) X(Assign_Exponentiate) \
    X(Assign_ShiftLeft) X(Assign_ShiftRight) X(Assign_Xor) X(Assign_Bitwise_And) X(Assign_Bitwise_Or)

#define OPS_ControlFlow \
    X(Break) X(Return)

#define OPS_All \
    OPS_Postfix \
    OPS_Prefix \
    OPS_Arithmetic \
    OPS_Bitwise \
    OPS_Logical \
    OPS_Comparison \
    OPS_Range \
    OPS_VarDecl \
    OPS_Assign \
    OPS_ControlFlow

enum class Operator : u16
{
    None,

#define X(i) i,
    OPS_All
#undef X
};

constexpr
std::string_view get_op_name(Operator op)
{
    switch (op) {
        case Operator::None: return "None";

#define X(o) case Operator::o: return #o;
        OPS_All
#undef X
    }
}

enum class OperatorString : u32 { };

constexpr OperatorString make_opstr(const char* op, u32 len)
{
    u32 hash {};
    for (u32 i = 0; i < len; ++i) {
        hash |= u32(op[i]) << (i * 8);
    }

    return OperatorString(hash);
}

constexpr OperatorString make_opstr(RuntimeLangString op)
{
    return make_opstr(op.str.data(), u32(op.str.size()));
}

consteval OperatorString make_opstr(StaticLangString op)
{
    return make_opstr(op.str.data(), u32(op.str.size()));
}

enum class ExpressionMode
{
    Normal,
    TypeAnnotation,
};

constexpr std::string_view get_expr_mode_name(ExpressionMode mode)
{
    switch (mode) {
        case ExpressionMode::Normal: return "Normal";
        case ExpressionMode::TypeAnnotation: return "TypeAnnotation";
    }
}

constexpr Operator get_prefix_op(std::string_view op, ExpressionMode mode)
{
    if (op == lang_str("break")) return Operator::Break;
    if (op == lang_str("return")) return Operator::Return;

    switch (make_opstr(RuntimeLangString(op))) {
        case make_opstr("&"): return Operator::AddressOf;

        case make_opstr("++"): return Operator::Preincrement;
        case make_opstr("--"): return Operator::Predecrement;

        case make_opstr("+"): return Operator::Arithmetic_Identity;
        case make_opstr("-"): return Operator::Arithmetic_Negate;
        case make_opstr("~"): return Operator::Bitwise_Negate;
        case make_opstr("!"): return Operator::Logical_Negate;
    }
    return Operator::None;
}

constexpr Operator get_other_op(std::string_view op, ExpressionMode mode)
{
    switch (make_opstr(RuntimeLangString(op))) {
        case make_opstr("^"):   return Operator::Dereference;
        case make_opstr("++"):  return Operator::Postincrement;
        case make_opstr("--"):  return Operator::Postdecrement;
        case make_opstr("("):   return Operator::Call;
        case make_opstr("["):   return Operator::Subscript;
        case make_opstr("."):   return Operator::FieldAccess;

        case make_opstr("+"):   return Operator::Add;
        case make_opstr("-"):   return Operator::Subtract;
        case make_opstr("*"):   return Operator::Multiply;
        case make_opstr("/"):   return Operator::Divide;
        case make_opstr("%"):   return Operator::Modulo;
        case make_opstr("**"):  return Operator::Power;

        case make_opstr("<<"):  return Operator::ShiftLeft;
        case make_opstr(">>"):  return Operator::ShiftRight;
        case make_opstr("~|"):  return Operator::Xor;
        case make_opstr("&"):   return Operator::Bitwise_And;
        case make_opstr("|"):   return Operator::Bitwise_Or;

        case make_opstr("&&"):  return Operator::Logical_And;
        case make_opstr("||"):  return Operator::Logical_Or;

        case make_opstr("=="):  return Operator::Equal;
        case make_opstr("!="):  return Operator::NotEqual;
        case make_opstr(">"):   return Operator::Greater;
        case make_opstr(">="):  return Operator::GreaterEqual;
        case make_opstr("<"):   return Operator::Less;
        case make_opstr("<="):  return Operator::LessEqual;

        case make_opstr(".."):  return Operator::Range_Exclusive;
        case make_opstr("..="): return Operator::Range_Inclusive;

        case make_opstr("="):   return (mode == ExpressionMode::TypeAnnotation) ? Operator::None : Operator::Assign;
        case make_opstr(":"):   return (mode == ExpressionMode::TypeAnnotation) ? Operator::None : Operator::VarDecl;

        case make_opstr("+="):  return Operator::Assign_Add;
        case make_opstr("-="):  return Operator::Assign_Subtract;
        case make_opstr("*="):  return Operator::Assign_Multiply;
        case make_opstr("/="):  return Operator::Assign_Divide;
        case make_opstr("%="):  return Operator::Assign_Modulo;
        case make_opstr("**="): return Operator::Assign_Exponentiate;
        case make_opstr("<<="): return Operator::Assign_ShiftLeft;
        case make_opstr(">>="): return Operator::Assign_ShiftRight;
        case make_opstr("~|="): return Operator::Assign_Xor;
        case make_opstr("&="):  return Operator::Assign_Bitwise_And;
        case make_opstr("|="):  return Operator::Assign_Bitwise_Or;
    }
    return Operator::None;
}

constexpr bool is_op_postfix(Operator op)
{
    switch (op) {
        case Operator::Dereference:
        case Operator::Postincrement:
        case Operator::Postdecrement:
        case Operator::Call:
        case Operator::Subscript:
        case Operator::FieldAccess:
            return true;
        default:
            return false;
    }
}

// -----------------------------------------------------------------------------

#define TOKEN_TYPE_LIST(DO) \
    DO(Eof, = 1) \
    \
    DO(Keyword) \
    \
    DO(Identifier) \
    DO(String) \
    DO(Number) \
    DO(Punctuation)

#define TOKEN_TYPE_DECLARE(type, ...) type __VA_ARGS__,

enum class TokenType {
    Invalid = 0,

    TOKEN_TYPE_LIST(TOKEN_TYPE_DECLARE)
};

constexpr std::string_view get_token_type_name(TokenType type)
{
    switch (type)
    {
#define TOKEN_TYPE_TO_STRING_SWITCH(type, ...) case TokenType::type: return #type;
        TOKEN_TYPE_LIST(TOKEN_TYPE_TO_STRING_SWITCH)
        default:
            raise_error("invalid token type");
    }
}

constexpr auto InvalidTokenIndex = ~0u;

struct Token
{
    TokenType type = TokenType::Invalid;
    std::string_view raw;
    union {
        struct {
            std::string_view value;
        } keyword;
        struct {
            bool is_at;
            std::string_view value;
        } identifier;
        struct {
            bool is_raw;
            std::string_view value;
        } string;
        struct {
            u32 base;
            std::string_view raw;
        } number;
        struct {
            std::string_view value;
            u32 matching = InvalidTokenIndex;
        } punctuation;
    };
};

constexpr auto InvalidToken = Token { .type = TokenType::Invalid };
constexpr auto EofToken = Token { .type = TokenType::Eof };

struct TokenList
{
    std::vector<Token> tokens;

    struct TokenIterator begin() const;

    TokenIterator matching_pair_for(Token token) const;
};

#define LANG_NOISY_TOKEN_ITERATOR 1

struct TokenIterator
{
    const TokenList* list;
    u32 current;

#if LANG_NOISY_TOKEN_ITERATOR
    mutable bool _peeked = false;
#endif

    bool is_at_end() const { return current >= list->tokens.size();            }
    u32      index() const { return is_at_end() ? InvalidTokenIndex : current; }

    void decrement()
    {
        if (current > 0) {
            current--;

#if LANG_NOISY_TOKEN_ITERATOR
            if (_peeked) {
                log_trace("token.decrement");
                _peeked = false;
            }
#endif
        }
    }

    Token advance()
    {
#if LANG_NOISY_TOKEN_ITERATOR
        if (!_peeked) {
            peek();
        }
        _peeked = is_at_end();
#endif
        return is_at_end() ? EofToken : list->tokens[current++];
    }

    Token peek() const
    {
        auto token = is_at_end() ? EofToken : list->tokens[current];
#if LANG_NOISY_TOKEN_ITERATOR
        if (!_peeked) {
            log_trace("token.peek '{}'", token.raw);
            _peeked = true;
        }
#endif
        return token;
    }

    Token peek_next() const
    {
        auto next = current + 1;
        auto token = next >= list->tokens.size() ? EofToken : list->tokens[next];
#if LANG_NOISY_TOKEN_ITERATOR
        log_trace("token.peek_next '{}'", token.raw);
#endif
        return token;
    }

    Token peek_prev() const
    {
        auto token = current == 0 ? EofToken : TokenIterator(list, current - 1).peek();
#if LANG_NOISY_TOKEN_ITERATOR
        log_trace("token.peek_prev '{}'", token.raw);
#endif
        return token;
    }

    union Internal {
        std::monostate _{};
        Token token;
        Internal(): _{} { }
    } _internal;
          Token    operator *()      { return is_at_end() ? EofToken : list->tokens[current]; }
    const Token*   operator->()      { _internal.token = **this; return &_internal.token; }
    TokenIterator& operator++()      { current++; return *this; }
    TokenIterator& operator--()      { current--; return *this; };
    TokenIterator  operator++(int)   { auto copy = *this; current++; return copy; }
    TokenIterator  operator--(int)   { auto copy = *this; current--; return copy; }
    TokenIterator  operator +(i32 o) { return {list, current + u32(o)}; };
    TokenIterator  operator -(i32 o) { return {list, current - u32(o)}; };

    bool operator==(const TokenIterator& other) const { return index() == other.index(); };
    operator bool() const { return !is_at_end(); }
};

inline
TokenIterator TokenList::matching_pair_for(Token token) const
    {
        if (token.type != TokenType::Punctuation) return TokenIterator{this, InvalidTokenIndex};
        auto i = token.punctuation.matching;
        return TokenIterator{this, i};
    }

inline
TokenIterator TokenList::begin() const
{
    return TokenIterator{this, 0};
}

constexpr bool is_end_of_expression_token(Token token)
{
    switch (make_opstr(RuntimeLangString(token.raw))) {
        case make_opstr(")"):
        case make_opstr("}"):
        case make_opstr("]"):
        case make_opstr(";"):
        case make_opstr(","):
            return true;
        default:
            return false;
    }
}

TokenList scan(std::string_view);

// -----------------------------------------------------------------------------

#define NodeTypes \
    X(VarDecl) \
    X(Function) \
    X(If) \
    X(Return) \
    X(For) \
    X(While) \
    X(Defer) \
    X(Block) \
    X(NamedReference) \
    X(OpExpr) \
    X(Call) \
    X(CallNamedArgument) \
    X(Literal) \
    X(FieldAccess) \
    X(Enum) \
    X(EnumValue) \
    X(Struct) \
    X(Match) \
    X(MatchArm)

enum class AstNodeType
{
#define X(v) v,
NodeTypes
#undef X
};

constexpr
std::string_view get_ast_node_type_name(AstNodeType t)
{
    switch (t) {
#define X(v) case AstNodeType::v: return #v;
NodeTypes
#undef X
    }
}

struct AstNode
{
    AstNodeType type;
    AstNode* next{};
};

struct AstNodeList
{
    AstNode* head{};

    usz count()
    {
        usz count = 0;
        for (auto* node = head; node; node = node->next) {
            count++;
        }
        return count;
    }
};

struct AstNodeListBuilder
{
    AstNode* head = nullptr;
    AstNode* tail = nullptr;

    AstNodeListBuilder() = default;
    AstNodeListBuilder(AstNodeList list)
        : head(list.head)
        , tail(list.head)
    {
        if (!tail) return;

        while (tail->next) {
            tail = tail->next;
        }
    }

    void append(AstNode* to_append)
    {
        if (!to_append) return;

        if (!head) {
            head = tail = to_append;
        } else {
            tail->next = to_append;
            tail = tail->next;
        }
    }

    AstNodeList to_list() const
    {
        return { head };
    }
};

struct AstVarDecl : AstNode
{
    AstNode* target;
    AstNode* type_expr{};
    bool constant = true;
    AstNode* init{};
};

struct AstFunction : AstNode
{
    AstNodeList parameters; // AstVarDecl[]
    AstNode* return_type{};
    AstNode* body{};
};

struct AstIf : AstNode
{
    AstNode* cond{};
    AstNode* then_branch{};
    AstNode* else_branch{};
};

struct AstReturn : AstNode
{
    AstNode* value{};
};

struct AstWhile : AstNode
{
    AstNode* cond{};
    AstNode* body{};
    AstNode* else_expr{};
};

struct AstFor : AstNode
{
    AstNode* range_expr;
    AstNodeList parameters;
    AstNode* body;
    AstNode* else_expr{};
};

struct AstBlock : AstNode
{
    AstNodeList statements;
    AstNode* terminal_expression{};
};

struct AstDefer : AstNode
{
    AstNode* body;
};

struct AstNamedReference : AstNode
{
    Token token;
};

struct AstEnumValue : AstNode
{
    Token identifier;
    AstNode* init;
};

struct AstEnum : AstNode
{
    AstNode* type_expr;
    AstNodeList values; // AstEnumValue[]
};

struct AstMatchArm : AstNode
{
    AstNode* pattern;
    AstNode* body;
};

struct AstMatch : AstNode
{
    AstNode* expr;
    AstNodeList arms; // AstMatchArm[]
};

struct AstStruct : AstNode
{
    AstNodeList body;
};

struct AstOpExpr : AstNode
{
    Token token;
    Operator op;

    AstNode* lhs{};
    AstNode* rhs{};
};

struct AstCallNamedArgument : AstNode
{
    Token field;
    AstNode* expr;
};

struct AstCall : AstNode
{
    AstNode* callee;
    Token token;
    AstNodeList arguments;
};

struct AstLiteral : AstNode
{
    Token token;
};

struct AstFieldAccess : AstNode
{
    AstNode* lhs;
    Token field;
};

template<class Ret = void, class Fn>
Ret visit(Fn&& fn, AstNode* expr)
{
    if (!expr) return Ret{};
    switch (expr->type) {
#define X(t) case AstNodeType::t: return fn(*static_cast<Ast##t*>(expr));
NodeTypes
#undef X
    }
}

constexpr
bool is_ast_node_block_expr(AstNode* t)
{
    if (!t) return false;
    switch (t->type) {
        case AstNodeType::If:
        case AstNodeType::For:
        case AstNodeType::While:
        case AstNodeType::Defer:
        case AstNodeType::Block:
            return true;
        case AstNodeType::Function:
            return static_cast<AstFunction*>(t)->body;
        default:
            return false;
    }
}

AstNodeList parse(TokenList& tokens);

inline
void ast_debug_print(AstNode* root)
{
    auto print_root = [&](this auto&& print_node, AstNode* _node, u32 depth) -> void {
        auto print = [&]<class ...Args>(const std::format_string<Args...> fmt, Args&&...args) {
            auto msg = std::format(fmt, std::forward<Args>(args)...);
            log_debug("{}{}", indent_str(depth, ". "), msg);
        };

// #define HEADING_INDENT depth++
// #define HEADING_PREFIX ""

#define HEADING_INDENT
#define HEADING_PREFIX "."

        auto print_block_body = [&](const AstNode* body) {
            auto& node = *static_cast<const AstBlock*>(body);
            for (auto s = node.statements.head; s; s = s->next) {
                print_node(s, depth + 1);
            }
            if (node.terminal_expression) {
                print_node(node.terminal_expression, depth + 1);
            }
        };

        visit(overload_set {
            [&](const AstVarDecl& node) {
                print("Var_Decl(const = {})", node.constant);
                print_node(node.target, depth + 1);
                HEADING_INDENT;
                if (node.type_expr) {
                    print(HEADING_PREFIX "Type_Expr");
                    print_node(node.type_expr, depth + 1);
                }
                if (node.init) {
                    print(HEADING_PREFIX "Init");
                    print_node(node.init, depth + 1);
                }
            },
            [&](const AstBlock& node) {
                print("Block");
                print_block_body(&node);
            },
            [&](const AstFunction& node) {
                print("Function");
                HEADING_INDENT;
                if (node.parameters.head) {
                    print(HEADING_PREFIX "Parameters");
                    for (auto p = node.parameters.head; p; p = p->next) {
                        print_node(p, depth + 1);
                    }
                }
                if (node.return_type) {
                    print(HEADING_PREFIX "Return_Type");
                    print_node(node.return_type, depth + 1);
                }
                if (node.body) {
                    print(HEADING_PREFIX "Body");
                    print_block_body(node.body);
                }
            },
            [&](const AstCall& node) {
                if (node.token.raw == "(") {
                    print("Call");
                } else {
                    print("Subscript");
                }
                HEADING_INDENT;
                print_node(node.callee, depth + 1);
                if (node.arguments.head) {
                    print(HEADING_PREFIX "Args");
                    for (auto arg = node.arguments.head; arg; arg = arg->next) {
                        print_node(arg, depth + 1);
                    }
                }
            },
            [&](const AstCallNamedArgument& node) {
                print("NamedArgument({})", node.field.raw);
                print_node(node.expr, depth + 1);
            },
            [&](const AstLiteral& node) {
                print("Literal({})", node.token.raw);
            },
            [&](const AstNamedReference& node) {
                print("Named_Reference({})", node.token.raw);
            },
            [&](const AstEnumValue& node) {
                print("EnumValue({})", node.identifier.raw);
                if (node.init) {
                    print_node(node.init, depth + 1);
                }
            },
            [&](const AstEnum& node) {
                print("Enum");
                HEADING_INDENT;
                if (node.type_expr) {
                    print(HEADING_PREFIX "Type");
                    print_node(node.type_expr, depth + 1);
                }
                if (node.values.head) {
                    print(HEADING_PREFIX "Values");
                    for (auto v = node.values.head; v; v = v->next) {
                        print_node(v, depth + 1);
                    }
                }
            },
            [&](const AstStruct& node) {
                print("Struct");
                for (auto f = node.body.head; f; f = f->next) {
                    print_node(f, depth + 1);
                }
            },
            [&](const AstMatchArm& node) {
                print("MatchArm");
                print_node(node.pattern, depth + 1);
                print_node(node.body, depth + 1);
            },
            [&](const AstMatch& node) {
                print("Match");
                print_node(node.expr, depth + 1);
                for (auto a = node.arms.head; a; a = a->next) {
                    print_node(a, depth + 1);
                }
            },
            [&](const AstOpExpr& node) {
                print("Operator({})", get_op_name(node.op));
                if (node.lhs) print_node(node.lhs, depth + 1);
                if (node.rhs) print_node(node.rhs, depth + 1);
            },
            [&](const AstFieldAccess& node) {
                print("Field_Access({})", node.field.raw);
                print_node(node.lhs, depth + 1);
            },
            [&](const AstIf& node) {
                print("If");
                HEADING_INDENT;
                print(HEADING_PREFIX "Condition");
                print_node(node.cond, depth + 1);
                print(HEADING_PREFIX "Then");
                print_block_body(node.then_branch);
                if (node.else_branch) {
                    print(HEADING_PREFIX "Else");
                    print_block_body(node.else_branch);
                }
            },
            [&](const AstFor& node) {
                print("For");
                HEADING_INDENT;
                print(HEADING_PREFIX "Range_Expression");
                print_node(node.range_expr, depth + 1);
                print(HEADING_PREFIX "Parameters");
                for (auto p = node.parameters.head; p; p = p->next) {
                    print_node(p, depth + 1);
                }
                print(HEADING_PREFIX "Body");
                print_block_body(node.body);
                if (node.else_expr) {
                    print(HEADING_PREFIX "Else");
                    print_block_body(node.else_expr);
                }
            },
            [&](const AstWhile& node) {
                print("While");
                HEADING_INDENT;
                print(HEADING_PREFIX "Condition");
                print_node(node.cond, depth + 1);
                print(HEADING_PREFIX "Body");
                print_block_body(node.body);
                if (node.else_expr) {
                    print(HEADING_PREFIX "Else");
                    print_block_body(node.else_expr);
                }
            },
            [&](const AstDefer& node) {
                print("Defer");
                print_block_body(node.body);
            },
            [&](const auto& node) {
                log_error("unknown node type: {} ({})", get_ast_node_type_name(node.type), std::to_underlying(node.type));
            },
        }, _node);
    };

    print_root(root, 0);
}

NAMESPACE_END
