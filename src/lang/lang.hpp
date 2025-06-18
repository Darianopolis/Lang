#pragma once

#include <lang/core.hpp>

#include <vector>
#include <variant>

LANG_NAMESPACE_BEGIN

// -----------------------------------------------------------------------------

constexpr std::array Keywords
{
    "if", "else",
    "for", "while",
    "break", "continue",
    "return", "defer",
    "true", "false",
};

constexpr std::array PunctuationSequences
{
    "+", "+=", "++",
    "-", "-=", "--", "->",
    "*", "*=", "**", "**=",
    "/", "/=",
    "^", "^=",
    "%", "%=",
    "<", "<=", "<<", "<<=", "<=>",
    ">", ">=", ">>", ">>=",
    "&", "&=", "&&",
    "|", "|=", "||",
    "=", "==",
    "!", "!=",
    "~",

    "(", ")",
    "{", "}",
    "[", "]",

    ".", "..", "..=",
    "?",
    ":",
};

// -----------------------------------------------------------------------------

struct BindingPower
{
    u8 left{};
    u8 right{};

    operator bool() const noexcept { return left || right; };
};

enum class Associativity
{
    LeftToRight,
    RightToLeft,
};

enum class FixType
{
    Prefix,
    Infix,
    Postfix,
};

struct OperatorGroup {
    Associativity dir;
};

using OperatorDeclStep = std::variant<std::string_view, FixType, OperatorGroup>;

constexpr auto TypeExprOperatorKey = "::<typexpr>";

constexpr auto Operators = std::to_array<OperatorDeclStep>({
    FixType::Postfix,
    OperatorGroup{Associativity::LeftToRight}, "++", "--", "(", "[", ".",
    FixType::Prefix,
    OperatorGroup{Associativity::RightToLeft}, "++", "--", "+", "-", "!", "~", "&", "*",
    FixType::Infix,
    OperatorGroup{Associativity::LeftToRight}, "*", "/", "%", "**",
    OperatorGroup{Associativity::LeftToRight}, "+", "-",
    OperatorGroup{Associativity::LeftToRight}, "<<", ">>",
    OperatorGroup{Associativity::LeftToRight}, "<=>",
    OperatorGroup{Associativity::LeftToRight}, "<", "<=", ">", ">=",
    OperatorGroup{Associativity::LeftToRight}, "==", "!=",
    OperatorGroup{Associativity::LeftToRight}, "&",
    OperatorGroup{Associativity::LeftToRight}, "^",
    OperatorGroup{Associativity::LeftToRight}, "|",
    OperatorGroup{Associativity::LeftToRight}, "..", "..=",
    OperatorGroup{Associativity::LeftToRight}, "&&",
    OperatorGroup{Associativity::LeftToRight}, "||",
    FixType::Prefix,
    OperatorGroup{Associativity::RightToLeft}, TypeExprOperatorKey,
    FixType::Infix,
    OperatorGroup{Associativity::RightToLeft},
        "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=", "**=",
    FixType::Prefix,
        "return",
});

// -----------------------------------------------------------------------------

enum class TokenType {
    Invalid = 0,

    Keyword,     // if else
    Identifier,  // foo
    String,      // "foo"
    Number,      // 123.4
    Punctuation, // +=/*...
};

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

struct TokenList
{
    std::vector<Token> tokens;

    struct TokenIterator begin() const;

    TokenIterator matching_pair_for(Token token) const;
};

struct TokenIterator
{
    const TokenList* list;
    u32 current;

    bool is_at_end() const { return current >= list->tokens.size();            }
    u32      index() const { return is_at_end() ? InvalidTokenIndex : current; }

    Token advance()    { return is_at_end() ? InvalidToken : list->tokens[current++]; }
    Token peek() const { return is_at_end() ? InvalidToken : list->tokens[current];   }

    Token peek_next() const
    {
        auto next = current + 1;
        return next >= list->tokens.size() ? InvalidToken : list->tokens[next];
    }

    Token peek_prev() const
    {
        return current == 0 ? InvalidToken : TokenIterator(list, current - 1).peek();
    }
};

inline
TokenIterator lang::TokenList::matching_pair_for(Token token) const
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

TokenList scan(std::string_view);

// -----------------------------------------------------------------------------

#define NodeTypes \
    X_NodeTypes(VarDecl) \
    X_NodeTypes(Function) \
    X_NodeTypes(If) \
    X_NodeTypes(Return) \
    X_NodeTypes(While) \
    X_NodeTypes(Block) \
    X_NodeTypes(NamedReference) \
    X_NodeTypes(OpExpr) \
    X_NodeTypes(Call) \
    X_NodeTypes(CallNamedArgument) \
    X_NodeTypes(Literal) \
    X_NodeTypes(FieldAccess) \

enum class AstNodeType
{
#define X_NodeTypes(v) v,
NodeTypes
#undef X_NodeTypes
};

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
    Token name;
    AstNode* type_expr{};
    bool constant = true;
    AstNode* init{};
};

struct AstFunction : AstNode
{
    AstNodeList parameters; // AstVarDecl[]
    AstNode* return_type{};
    AstNodeList body{};
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
};

struct AstBlock : AstNode
{
    AstNodeList statements;
    AstNode* terminal_expression{};
};

struct AstNamedReference : AstNode
{
    Token token;
};

struct AstOpExpr : AstNode
{
    AstNode* lhs{};
    Token token;
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
    AstNodeList arguments; // AstCallArgument[]
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
#define X_NodeTypes(t) case AstNodeType::t: return fn(*static_cast<Ast##t*>(expr));
NodeTypes
#undef X_NodeTypes
    }
}

AstNodeList parse(TokenList& tokens);

inline
void debug_print(AstNodeList tree)
{
    auto print_root = [&](this auto&& print_node, AstNode* _node, u32 depth) -> void {
        auto indent = [&] {
            for (u32 i = 0; i < depth; ++i) {
                std::print("  ");
            }
        };

        auto println = [&]<class ...Args>(const std::format_string<Args...> fmt, Args&&...args) {
            indent();
            std::println(fmt, std::forward<Args>(args)...);
        };

        visit(overload_set {
            [&](const AstVarDecl& node) {
                println("Var_Decl({}){}", node.name.raw, node.constant ? " const" : "");
                depth++;
                if (node.type_expr) {
                    println("Type_Expr");
                    print_node(node.type_expr, depth + 1);
                }
                if (node.init) {
                    println("Init");
                    print_node(node.init, depth + 1);
                }
            },
            [&](const AstFunction& node) {
                println("Function");
                depth++;
                if (node.parameters.head) {
                    println("Parameters");
                    for (auto p = node.parameters.head; p; p = p->next) {
                        print_node(p, depth + 1);
                    }
                }
                if (node.return_type) {
                    println("Return_Type");
                    print_node(node.return_type, depth + 1);
                }
                if (node.body.head) {
                    println("Body");
                    for (auto s = node.body.head; s; s = s->next) {
                        print_node(s, depth + 1);
                    }
                }
            },
            [&](const AstCall& node) {
                if (node.token.raw == "(") {
                    println("Call");
                } else {
                    println("Subscript");
                }
                depth++;
                print_node(node.callee, depth);
                if (node.arguments.head) {
                    println("Args");
                    for (auto arg = node.arguments.head; arg; arg = arg->next) {
                        print_node(arg, depth + 1);
                    }
                }
            },
            [&](const AstCallNamedArgument& node) {
                println("NamedArgument({})", node.field.raw);
                print_node(node.expr, depth + 1);
            },
            [&](const AstLiteral& node) {
                println("Literal({})", node.token.raw);
            },
            [&](const AstNamedReference& node) {
                println("Named_Reference({})", node.token.raw);
            },
            [&](const AstOpExpr& node) {
                if (node.lhs && node.rhs) {
                    println("Infix_Operator({})", node.token.punctuation.value);
                    print_node(node.lhs, depth + 1);
                    print_node(node.rhs, depth + 1);
                } else if (node.lhs) {
                    println("Postfix_Operator({})", node.token.punctuation.value);
                    print_node(node.lhs, depth + 1);
                } else {
                    println("Prefix_Operator({})", node.token.punctuation.value);
                    print_node(node.rhs, depth + 1);
                }
            },
            [&](const AstFieldAccess& node) {
                println("Field_Access({})", node.field.raw);
                print_node(node.lhs, depth + 1);
            },
            [&](const auto& node) {
                log_error("unknown node type: {}", std::to_underlying(node.type));
            },
        }, _node);
    };

    for (auto root = tree.head; root; root = root->next) {
        print_root(root, 0);
    }
}

LANG_NAMESPACE_END
