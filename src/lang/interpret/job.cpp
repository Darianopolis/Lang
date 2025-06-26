#include "interpret.hpp"

NAMESPACE_BEGIN(lang)

void CompileJobSystem::try_unblock(CollectionAccess* access)
{
    std::erase_if(blocked, [&](const std::pair<ExecutionContext*, CollectionAccess*>& pair) -> bool {
        auto[ctx, blocked_access] = pair;
        if (blocked_access->collection == access->collection
                && blocked_access->field.raw == access->field.raw) {

            unblocked.emplace_back(ctx);

            return true;
        }
        return false;
    });
}

void CompileJobSystem::add_file_node_list(AstNodeList nodes, FileContext* file_context)
{
    log_debug("loading file fragment, generating bytecode for top level statements");
    for (auto n = nodes.head; n; n = n->next) {
        auto bytecode = bytecode_generate(n);
        log_debug("--- chunk ---");
        bytecode_debug_print(bytecode);
        auto context = execution_context_create(file_context, bytecode);
        context->job_system = this;
        unblocked.emplace_back(context);
    }
}

void CompileJobSystem::compile()
{
    while (!unblocked.empty()) {
        auto context = unblocked.back();
        unblocked.pop_back();

        log_debug("running context [{}]", (void*)context);

        auto res = execution_context_run(context);
        if (res.code == ExecutionResultCode::Blocked) {
            if (!res.value || res.value->type != ValueType::CollectionAccess) {
                raise_error("expected CollectionAcces as value when execution blocked");
            }
            blocked.emplace_back(context, static_cast<CollectionAccess*>(res.value));
            log_debug("  blocked!");
        } else {
            completed.emplace_back(context, res);
            log_debug("  completed!");
        }
    }

    for (auto[ctx, res] : completed) {
        if (res.code == ExecutionResultCode::Success) {
            if (res.value) {
                log_info("context [{}] completed with result: {}", (void*)ctx, value_to_string(res.value));
            } else {
                log_info("context [{}] completed", (void*)ctx);
            }
        } else {
            log_error("context [{}] failed", (void*)ctx);
        }
    }
}

NAMESPACE_END
