#include "process_queries.h"

#include <algorithm>
#include <vector>
#include <deque>
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> results(queries.size());

    std::vector<std::string_view> queries_sv;
    queries_sv.reserve(queries.size());
    for (const std::string& query : queries) {
        queries_sv.push_back(query);
    }

    std::transform(std::execution::par,
        queries_sv.cbegin(), queries_sv.cend(),
        results.begin(),
        [&search_server](std::string_view query) noexcept{
            return search_server.FindTopDocuments(query);
        });

    return results;
}

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries) {
    const std::vector<std::vector<Document>> results = ProcessQueries(search_server, queries);

    size_t documents_count = std::transform_reduce(std::execution::par,
                                results.cbegin(), results.cend(),
                                static_cast<size_t>(0),
                                std::plus{},
                                [](const std::vector<Document>& result) noexcept {
                                    return result.size();
                                });

    std::vector<Document> vec(documents_count);

    auto it = vec.begin();
    std::for_each(std::execution::seq,
        results.begin(), results.end(),
        [&vec, &it](const std::vector<Document>& result) noexcept {
            it = std::copy(std::execution::par, result.cbegin(), result.cend(), it);
        });

    return vec;
}