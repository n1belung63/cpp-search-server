#pragma once

#include <vector>
#include <string>
#include <queue>

#include "document.h"
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        const std::vector<Document> resp =   search_server_.FindTopDocuments(raw_query, document_predicate);
        HandleResponse(resp);
        return resp;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        uint64_t time;
        bool is_empty;
    };
    std::deque<QueryResult> requests_;

    const class SearchServer & search_server_;
    const static int min_in_day_ = 1440;
    
    uint64_t current_time_ = 0;
    int no_results_request_ = 0;
    
    void HandleResponse(const std::vector<Document>& resp);
};