# Cancellation result regression

`cancelled_result_test.cpp` uses the real `bg_queue_promise::call`, `handle_base`,
`result`, and spin-lock headers. A worker is held inside the producer after its
initial cancellation check; the main thread cancels the handle and then lets the
producer return a value, return void, or throw. Cancellation must remain terminal,
and neither the original callback nor a later subscriber may run. Normal
completion still delivers its value.

For standalone header validation, compile with C++20, pthreads, Abseil and
`-DASYNC_STANDALONE_QUEUE_TEST`, including `cpp/` and a directory containing the
build's `config.hpp`. This flag supplies only queue-linkage stubs: the queue ID is
unsubmitted, callbacks explicitly use the inline/null queue, and any actual
scheduler execution terminates the test. It does not replace the producer,
result-state or locking implementation. Do not define that flag when linking to
the complete async queue implementation.

The focused standalone check does not exercise a real queue scheduler or the
full Deep Lake extension. A separate check exercises the real `promise::get_future`:
successful cancellation destroys the callback's captured `std::promise`, making
the future ready with `std::future_errc::broken_promise`. The previous warning that
the future remained locked was stale; the regression preserves the actual behavior.
