#include <heimdall/exceptions.hpp>
#include <heimdall/sample_info_holder.hpp>
#include <heimdall_common/chained_column_view.hpp>
#include <icm/bit_vector.hpp>
#include <query_core/index_holder.hpp>
// Test-only access to invoke the actual inline implementation explicitly.
// Otherwise the SDK-owned out-of-line key function installs its old vtable.
#define private public
#include <heimdall_common/impl/filtered_column.hpp>
#undef private
#include <heimdall_common/array_column_view.hpp>
#include <iostream>
#include <stdexcept>

// Only the index search is controlled. Filtering and reverse mapping execute
// the actual filtered_column class, with a real array_column as its source.
class controlled_index : public query_core::index_holder
{
public:
    void reset_index_data() override
    {
    }
    std::vector<deeplake_core::index_type> get_indexes() const override
    {
        return {};
    }
    std::string to_string() const override
    {
        return "fixture";
    }
    bool can_run_query(const query_core::top_k_search_info&) const override
    {
        return true;
    }
    bool can_run_query(const query_core::text_search_info&) const override
    {
        return false;
    }
    bool can_run_query(const query_core::inverted_index_search_info&) const override
    {
        return false;
    }
    async::promise<query_core::query_results> run_query(const query_core::top_k_search_info&,
                                                        const query_core::static_data_t&,
                                                        std::shared_ptr<const icm::roaring> filter) override
    {
        std::vector<int64_t> selected;
        std::cerr << "source filter:";
        for (int64_t i : {9, 2, 7})
            std::cerr << ' ' << i << '=' << (!filter || filter->contains(i));
        std::cerr << '\n';
        for (int64_t i : {9, 2, 7})
            if (!filter || filter->contains(i))
                selected.push_back(i);
        query_core::query_results results;
        results.emplace_back(icm::index_mapping_t<int64_t>::list(std::move(selected)));
        return async::fulfilled(std::move(results));
    }
    async::promise<std::vector<icm::roaring>> run_query(const query_core::text_search_info&) override
    {
        throw std::runtime_error("unused");
    }
    async::promise<std::vector<icm::roaring>> run_query(const query_core::inverted_index_search_info&) override
    {
        throw std::runtime_error("unused");
    }
};
class indexed_array : public heimdall_common::array_column
{
    std::shared_ptr<controlled_index> index_ = std::make_shared<controlled_index>();

public:
    indexed_array()
        : array_column("fixture", deeplake_core::type{}, nd::array{})
    {
    }
    int64_t samples_count() const noexcept override
    {
        return 10;
    }
    std::shared_ptr<query_core::index_holder> index_holder() override
    {
        return index_;
    }
};

void check(std::vector<int64_t> mapping,
           std::vector<uint32_t> positions,
           std::vector<int64_t> expected,
           bool filtered = true)
{
    auto source = std::make_shared<indexed_array>();
    auto column = std::make_shared<heimdall_common::impl::filtered_column>(
        source, icm::index_mapping_t<int64_t>::list(std::move(mapping)));
    auto filter = std::make_shared<icm::roaring>();
    for (auto position : positions)
        filter->add(position);
    if (!filtered)
        filter.reset();
    auto results =
        column->heimdall_common::impl::filtered_column::run_query(query_core::top_k_search_info{}, {}, filter).get();
    std::vector<int64_t> actual;
    for (auto index : results.at(0).indices)
        actual.push_back(index);
    if (actual != expected) {
        std::cerr << "actual:";
        for (auto i : actual)
            std::cerr << ' ' << i;
        std::cerr << " expected:";
        for (auto i : expected)
            std::cerr << ' ' << i;
        std::cerr << '\n';
        throw std::runtime_error("filtered-column result mismatch");
    }
}
int main()
{
    int failures = 0;
    auto run = [&](const char* name, auto&& test) {
        try {
            test();
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cout << "FAIL " << name << ": " << error.what() << '\n';
        }
    };
    run("sparse highest bit", [] {
        check({9, 2, 7}, {2}, {2});
    });
    run("multiple sparse bits", [] {
        check({9, 2, 7}, {1, 2}, {1, 2});
    });
    run("eligible duplicate", [] {
        check({9, 2, 9}, {2}, {2});
    });
    run("duplicate first eligible match", [] {
        check({9, 2, 9}, {0, 2}, {0});
    });
    run("unfiltered first match", [] {
        check({9, 2, 9}, {}, {0, 1}, false);
    });
    return failures ? 1 : 0;
}
