# Filtered-column regression

`filtered_column_query_test.cpp` executes the real `filtered_column` and
`array_column` classes. Only the index search is controlled, so source hits have
a deterministic order. It does not reimplement filter expansion or reverse
mapping. It covers sparse filters whose highest set bit exceeds cardinality,
duplicate source indices, and the original unfiltered first-match behavior.

The test explicitly qualifies the inline `filtered_column::run_query` call using
test-only private access. This is necessary when linking the public SDK: the
out-of-line key function makes the SDK own the class vtable, which otherwise
dispatches to the old implementation even when the patched header is included.
This test does not validate the Python extension, query planner, index search,
or PostgreSQL integration.

For the standalone reproduction, use the Linux x86-64 DeepLake API 4.4.4 SDK.
Its `impl/filtered_column.hpp` interface matches this PR; only the filter bound,
callback capture, and eligible reverse-match condition differ. The 4.5.2 SDK
changes vector types and other virtual interfaces, so mixing it with this class
definition is not a valid ABI test. Use CRoaring 4.2.1 and Abseil 20240116.2
headers; the latter matches the SDK's `lts_20240116` exported queue ABI.

With those dependencies installed, create a temporary overlay of the SDK's
`heimdall_common` headers, then replace **only** its
`impl/filtered_column.hpp` with the PR file. Put that overlay before the SDK
include directory. For example, from the repository root:

```sh
mkdir -p "$OVERLAY"
cp -R "$SDK/include/heimdall_common" "$OVERLAY/"
cp cpp/heimdall_common/impl/filtered_column.hpp \
  "$OVERLAY/heimdall_common/impl/filtered_column.hpp"
g++ -std=c++20 -O2 -fvisibility=hidden -fno-semantic-interposition \
  -I"$ABSEIL" -I"$OVERLAY" -I"$SDK/include" -I"$SDK/include/base" \
  -I"$ROARING/include" -I"$EXTRA_HEADERS" -I/usr/include/eigen3 \
  cpp/tests/filtered_column_query_test.cpp \
  -L"$SDK/lib" -Wl,-rpath,"$SDK/lib" -ldeeplake_api \
  -L"$ROARING/lib" -lroaring -o "$OVERLAY/filtered-column-test"
"$OVERLAY/filtered-column-test"
```

`EXTRA_HEADERS` must include `nlohmann/fifo_map.hpp`; the remaining headers are
the SDK's normal third-party dependencies. Run the same command with the
unmodified SDK header in the overlay for the before comparison: three sparse
or duplicate-filter cases fail, while the two first-match controls pass.
With the PR header all five cases pass. The nested real-dataset Python tests
remain the separate integration checks for an extension built from the PR.
