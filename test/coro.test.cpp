#include "test.hpp"
#include "lang/scan.hpp"

#include <boost/coroutine2/coroutine.hpp>

NAMESPACE_BEGIN()

NAMESPACE_USE(lang::test)
NAMESPACE_USE(lang)

using coro_t = boost::coroutines2::coroutine<int>;

void counter_generator(coro_t::push_type& yield) {
    int counter = 1;
    while (counter <= 10) {
        yield(counter);
        ++counter;
    }
}

Registered _1 = register_test("coroutine", [] -> TestResults {

    coro_t::pull_type source(
        boost::coroutines2::fixedsize_stack(1024 * 1024),
        counter_generator);

    for (int value : source) {
        std::println("Counter: {}", value);
    }

    return TestResult::Success;
});

NAMESPACE_END
