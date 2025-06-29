#include "treewalk.hpp"

NAMESPACE_BEGIN(lang)

TreeWalkExecutionContext* treewalk_execution_context_create(FileContext* file, AstNode* root)
{
    auto ctx = new TreeWalkExecutionContext{};
    ctx->file = file;
    ctx->root = root;
    return ctx;
}

Value* evaluate_literal(AstLiteral* literal);
Value* evaluate_namedref(TreeWalkExecutionContext* context, AstNamedReference* named_ref);
InterpretTask evaluate_vardecl(TreeWalkExecutionContext* context, AstVarDecl* var_decl);
InterpretTask evaluate_operator(TreeWalkExecutionContext* context, AstOpExpr* op_expr);

#define EVALUATE_NODE(node, value_out) \
    switch (node->type) { \
        case AstNodeType::Literal: value_out = evaluate_literal(static_cast<AstLiteral*>(node)); break; \
        case AstNodeType::NamedReference: value_out = evaluate_namedref(context, static_cast<AstNamedReference*>(node)); break; \
        case AstNodeType::VarDecl:  value_out = co_await evaluate_vardecl(context, static_cast<AstVarDecl*>(node)); break; \
        case AstNodeType::OpExpr:  value_out = co_await evaluate_operator(context, static_cast<AstOpExpr*>(node)); break; \
        default: raise_error("unsupported ast node type: {}", get_ast_node_type_name(node->type)); \
    }

#define LOAD_VALUE(value) \
    if (value) switch (value->type) { \
        case ValueType::NamespaceAccess: { \
            auto namespace_access = static_cast<NamespaceAccess*>(value); \
            auto loaded = namespace_access->_namespace->get(namespace_access->field.raw); \
            if (loaded) { \
                value = loaded; \
            } else { \
                context->blocked_access = namespace_access; \
                co_yield context; \
                log_error("LOAD_VALUE resumed..."); \
                loaded = namespace_access->_namespace->get(namespace_access->field.raw); \
            } \
            break; \
        } \
        default: {} \
    }

Value* evaluate_literal(AstLiteral* literal)
{
    switch (literal->token.type) {
        case TokenType::Number: {
            auto raw = literal->token.number.raw;
            f64 value;
            auto res = std::from_chars(raw.begin(), raw.end(), value);
            if (!res) {
                raise_error("error parsing number from token: {}", std::make_error_code(res.ec).message());
            }
            return tag_value(new Number{ .value = value });
        }
        case TokenType::String:
            return tag_value(new String{ .value = std::string(literal->token.string.value) });
        default:
            raise_error("unknown token type for literal");
    }
};

Value* evaluate_namedref(TreeWalkExecutionContext* context, AstNamedReference* named_ref)
{
    return tag_value(new NamespaceAccess{
        ._namespace = context->file->global,
        .field = named_ref->token,
    });
}

InterpretTask evaluate_vardecl(TreeWalkExecutionContext* context, AstVarDecl* var_decl)
{
    Value* init{};

    if (var_decl->init) {
        EVALUATE_NODE(var_decl->init, init);
        LOAD_VALUE(init);
    }

    Value* target{};
    EVALUATE_NODE(var_decl->target, target);

    if (!target || target->type != ValueType::NamespaceAccess) raise_error("target of variable declaration must be a namespace access");
    auto access = static_cast<NamespaceAccess*>(target);

    access->_namespace->set(access->field.raw, init);
    context->job_system->try_unblock(access);

    co_return nullptr;
}

InterpretTask evaluate_operator(TreeWalkExecutionContext* context, AstOpExpr* op_expr)
{
    Value *lhs{}, *rhs{};

    if (op_expr->lhs) {
        EVALUATE_NODE(op_expr->lhs, lhs);
        LOAD_VALUE(lhs);
    }
    if (op_expr->rhs) {
        EVALUATE_NODE(op_expr->rhs, rhs);
        LOAD_VALUE(rhs);
    }

#define EXPECT_NUMBER(val) static_cast<Number*>(val); if (val->type != ValueType::Number) { \
    raise_error("expected number, got: {}", get_value_type_name(val->type)); \
}

#define BINARY_NUMBER_OP(op_sym, op_name) \
    case Operator::op_name: { \
        auto lhs_num = EXPECT_NUMBER(lhs); \
        auto rhs_num = EXPECT_NUMBER(rhs); \
        log_trace("{} {} {} = {}", lhs_num->value, #op_sym, rhs_num->value, lhs_num->value op_sym rhs_num->value); \
        co_return tag_value(new Number{ .value = double(lhs_num->value op_sym rhs_num->value) }); \
    }

    switch (op_expr->op) {
        BINARY_NUMBER_OP(+, Add)
        BINARY_NUMBER_OP(-, Subtract)
        BINARY_NUMBER_OP(*, Multiply)
        BINARY_NUMBER_OP(/, Divide)

        default: raise_error("unsupported operator: {}", get_op_name(op_expr->op));
    }
}

InterpretTask evaluate_node(TreeWalkExecutionContext* context, AstNode* node)
{
    Value* value;
    log_trace("evaluating node...");
    EVALUATE_NODE(node, value);
    log_trace("evaluated node...");
    co_return value;
}

ExecutionResult TreeWalkExecutionContext::run()
{
    if (!current.handle) {
        current = evaluate_node(this, root);
    } else {
        current.handle.resume();
    }

    log_trace("[{}]", (void*)&current.handle.promise());

    if (current.handle.done()) return { ExecutionResultCode::Success, current.handle.promise().return_slot };

    log_warn("not done, blocked_access = {}", value_to_string(blocked_access));

    return { ExecutionResultCode::Blocked, blocked_access };
}

NAMESPACE_END
