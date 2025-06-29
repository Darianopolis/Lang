#pragma once

#include "interpret.hpp"

#include <coroutine>

NAMESPACE_BEGIN(lang)

struct InterpretTask
{
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    handle_type handle;

    struct promise_type
    {
        std::exception_ptr exception;
        Value* return_slot = {};

        InterpretTask get_return_object() noexcept
        {
            return InterpretTask{ handle_type::from_promise(*this) };
        }

        auto initial_suspend() noexcept
        {
            return std::suspend_never{};
        }

        auto final_suspend() noexcept
        {
            log_warn("[{}] final_suspend...", (void*)this);
            return std::suspend_always{};
        }

        auto yield_value(struct TreeWalkExecutionContext* context);

        auto return_value(Value* value)
        {
            log_warn("[{}] returning...", (void*)this);
            return_slot = value;
        }

        void unhandled_exception()
        {
            exception = std::current_exception();
        }
    };

public:
    bool await_ready()
    {
        return handle.done();
    }

    void await_suspend(handle_type coro)
    {
        log_warn("[{}] waiting on [{}]", (void*)&coro.promise(), (void*)&handle.promise());
    }

    Value* await_resume()
    {
        log_warn("[{}] resumed", (void*)&handle.promise());
        promise_type& promise = handle.promise();
        if (promise.exception) {
            std::rethrow_exception(promise.exception);
        }
        return promise.return_slot;
    }
};

struct TreeWalkExecutionContext : ExecutionContext
{
    AstNode* root;
    InterpretTask current;
    Value* blocked_access;
    InterpretTask blocked_coro;

    ExecutionResult run() final;
};

TreeWalkExecutionContext* treewalk_execution_context_create(FileContext* file, AstNode* root);

inline
auto InterpretTask::promise_type::yield_value(struct TreeWalkExecutionContext* context)
{
    log_warn("[{}] yielding...", (void*)this);
    context->blocked_coro = get_return_object();
    return get_return_object();
}

NAMESPACE_END
