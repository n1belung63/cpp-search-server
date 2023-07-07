#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <utility>
#include <map>
#include <set>
#include <numeric>
#include <algorithm>

#include "document.h"
#include "read_input_functions.h"

using namespace std::string_literals;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double RESEDUAL_OF_DOCUMENT_RELEVANCE = 1e-6;

class SearchServer {
public:
    template <typename Container>
    explicit SearchServer(const Container& stop_words_container) {
        for (const auto & word : MakeUniqueNonEmptyStrings(stop_words_container)) {
            if (!SearchServer::IsValidWord(word)) {
                throw std::invalid_argument("Stop-word contains invalid characters"s);
            }
            stop_words_.insert(word);
        }
    }

    explicit SearchServer(const std::string& stop_words_string);

    explicit SearchServer();

    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentFilter>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentFilter document_filter) const {
        const Query query = ParseQuery(raw_query);

        std::vector<Document> matched_documents = FindAllDocuments(query, document_filter);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (std::abs(lhs.relevance - rhs.relevance) < RESEDUAL_OF_DOCUMENT_RELEVANCE) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

    std::vector<Document> FindTopDocuments(const std::string& raw_query, const DocumentStatus& document_status=DocumentStatus::ACTUAL) const;

    int GetDocumentCount() const;

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;

    int GetDocumentId(int index) const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::map<int, int> id_to_num_; 

    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };
    
    QueryWord ParseQueryWord(std::string text) const;

    bool IsStopWord(const std::string& word) const;

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    Query ParseQuery(const std::string& text) const;

    template <typename DocumentFilter>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentFilter document_filter) const {
        std::map<int, double> document_to_relevance;
        for (const std::string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_filter(document_id,document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const std::string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }

    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    static bool IsValidWord(const std::string& word);
};
