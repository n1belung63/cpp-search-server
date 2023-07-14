#include "test_example_functions.h"
#include "document.h"

#include <iostream>

using namespace std::string_literals;

void AddDocument(SearchServer & search_server, int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    search_server.AddDocument(document_id, document, status, ratings);
}

void FindTopDocuments(SearchServer & search_server, const std::string& raw_query, const DocumentStatus& document_status) {
    for (const auto & document : search_server.FindTopDocuments(raw_query, document_status)) {
        PrintDocument(document);
        std::cout << std::endl;
    }
}

void MatchDocument(SearchServer & search_server, const std::string& raw_query) {
    for (const auto & document_id : search_server) {
        const auto [words, doc_status] = search_server.MatchDocument(raw_query, document_id);
        std::cout << "{ document_id = "s << document_id << ", status = "s << doc_status << ", words = "s << words << " }" << std::endl;
    }
}