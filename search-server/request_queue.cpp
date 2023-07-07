#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server) : search_server_{search_server} 
{ 

}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    const std::vector<Document> resp =  search_server_.FindTopDocuments(raw_query, status);
    HandleResponse(resp);
    return resp;
}
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    const std::vector<Document> resp = search_server_.FindTopDocuments(raw_query);
    HandleResponse(resp);
    return resp;
}

int RequestQueue::GetNoResultRequests() const {
    return no_results_request_;
}

void RequestQueue::HandleResponse(const std::vector<Document>& resp) {
    if (!requests_.empty() && min_in_day_ <= (++current_time_ - requests_.front().time) ) {
        if (requests_.front().is_empty && no_results_request_ > 0) {
            --no_results_request_;
        }
        requests_.pop_front();
    }

    bool is_empty = resp.empty();
    requests_.push_back({current_time_,is_empty});

    if (is_empty) {
        ++no_results_request_;
    }
}