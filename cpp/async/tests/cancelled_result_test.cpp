// Exercises the real producer/handle/result headers with cancellation during work.
// Standalone builds isolate only queue linkage; no scheduler is exercised.
#include <async/impl/bg_queue_promise.hpp>
#include <async/promise.hpp>

#include <cassert>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

#ifdef ASYNC_STANDALONE_QUEUE_TEST
namespace async {
queue::~queue() = default;
main_queue::main_queue() : queue(nullptr) {}
main_queue& main() noexcept { static main_queue value; return value; }
queue::id_type::~id_type() noexcept = default;
void queue::id_type::remove() const noexcept { assert(!static_cast<bool>(*this)); }
void queue::id_type::set_priority(int) const { assert(!static_cast<bool>(*this)); }
bool queue::is_this_thread_worker() const noexcept { std::terminate(); }
void queue::submit(base::function<void()>&&, int, id_type*) { std::terminate(); }
} // namespace async
#endif

template <typename T, typename Producer>
void cancelled_running_producer(Producer produce)
{
    async::impl::bg_queue_promise<T> handle;
    int callbacks = 0;
    handle.set_callback(async::callback_type<T>([&](async::result<T>&&) { ++callbacks; }, nullptr));
    std::promise<void> entered;
    std::promise<void> release;
    auto proceed = release.get_future();
    std::thread worker([&] {
        handle.call([&]() -> T {
            entered.set_value();
            proceed.wait();
            return produce();
        });
    });
    entered.get_future().wait(); // call() has passed its initial cancellation check.
    assert(handle.cancel());
    release.set_value();
    worker.join();
    if (!handle.is_cancelled() || handle.is_ready() || callbacks != 0) {
        throw std::runtime_error("late producer result resurrected a cancelled handle");
    }
    handle.set_callback(async::callback_type<T>([&](async::result<T>&&) { ++callbacks; }, nullptr));
    assert(callbacks == 0); // Cancellation remains terminal for later subscribers too.
}

int main()
{
    cancelled_running_producer<int>([] { return 42; });
    cancelled_running_producer<void>([] {});
    cancelled_running_producer<int>([]() -> int { throw std::runtime_error("producer failed"); });
    cancelled_running_producer<void>([] { throw std::runtime_error("producer failed"); });
    async::handle_base<int> normal;
    int delivered = 0;
    normal.set_callback(async::callback_type<int>([&](async::result<int>&& value) { delivered = std::move(value).get(); }, nullptr));
    normal.set_value(7);
    assert(delivered == 7);
    async::impl::bg_queue_promise<int> waiting;
    async::promise<int> promise(waiting);
    auto future = promise.get_future();
    assert(promise.cancel());
    assert(future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
    try {
        (void)future.get();
        assert(false);
    } catch (const std::future_error& error) {
        assert(error.code() == std::make_error_code(std::future_errc::broken_promise));
    }
    std::cout << "four cancelled producers, future cancellation and normal completion passed\n";
}
