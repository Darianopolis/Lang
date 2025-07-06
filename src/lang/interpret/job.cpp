#if 0
#include "interpret.hpp"

#define USE_BYTECODE_INTERPRETER 1

#if USE_BYTECODE_INTERPRETER
#include "bytecode.hpp"
#else
#include "treewalk.hpp"
#endif

NAMESPACE_BEGIN(lang)

void CompileJobSystem::add_fragment(AstNode* n, FileContext* file_context)
{
#if USE_BYTECODE_INTERPRETER
    auto bytecode = bytecode_generate(n);

    log_debug("");
    log_debug("==== Fragment ====");
    ast_debug_print(n);
    log_debug("==== Bytecode ====");
    bytecode_debug_print(bytecode);

    auto context = bytecode_execution_context_create(file_context, bytecode);
    context->job_system = this;
    unblocked.emplace_back(context);
#else
    log_debug("");
    log_debug("==== Fragment ====");
    ast_debug_print(n);

    auto context = treewalk_execution_context_create(file_context, n);
    context->job_system = this;
    unblocked.emplace_back(context);
#endif
}

void CompileJobSystem::try_unblock(NamespaceAccess* access)
{
    std::erase_if(blocked, [&](const std::pair<ExecutionContext*, NamespaceAccess*>& pair) -> bool {
        auto[ctx, blocked_access] = pair;
        if (blocked_access->_namespace == access->_namespace
                && blocked_access->field == access->field) {

            unblocked.emplace_back(ctx);

            return true;
        }
        return false;
    });
}

void CompileJobSystem::compile()
{
    while (!unblocked.empty()) {
        auto context = unblocked.back();
        unblocked.pop_back();

        log_debug("running context [{}]", (void*)context);

        auto res = context->run();
        if (res.code == ExecutionResultCode::Blocked) {
            if (res.value.type != ValueType::NamespaceAccess) {
                raise_error("expected NamespaceAccess as value when execution blocked, got: {}", get_value_type_name(res.value.type));
            }
            blocked.emplace_back(context, static_cast<NamespaceAccess*>(res.value.ptr));
            log_debug("  blocked!");
        } else {
            completed.emplace_back(context, res);
            log_debug("  completed!");
        }
    }

    for (auto[ctx, res] : completed) {
        if (res.code == ExecutionResultCode::Success) {
            if (res.value.type != ValueType::_invalid) {
                log_info("context [{}] completed with result: {}", (void*)ctx, value_to_string(res.value.type, res.value.ptr));
            } else {
                log_info("context [{}] completed", (void*)ctx);
            }
        } else {
            log_error("context [{}] failed", (void*)ctx);
        }
    }
}

NAMESPACE_END
#endif
