#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <utility>
#include <map>
#include <set>
#include <deque>
#include <numeric>
#include <algorithm>
#include <iterator>
#include <execution>
#include <future>
#include <type_traits>

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

using namespace std::string_literals;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double RESEDUAL_OF_DOCUMENT_RELEVANCE = 1e-6;

class SearchServer {
public:
    template <typename Container>
    explicit SearchServer(const Container& stop_words_container);
    explicit SearchServer(const std::string& stop_words_string);
    explicit SearchServer(const std::string_view& stop_words_string);
    explicit SearchServer();

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename ExecutionPolicy, typename DocumentFilter>
    std::vector<Document> FindTopDocuments(ExecutionPolicy& policy, std::string_view raw_query, DocumentFilter document_filter) const;
    template <typename DocumentFilter>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentFilter document_filter) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy& policy, std::string_view raw_query, const DocumentStatus& document_status=DocumentStatus::ACTUAL) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query, const DocumentStatus& document_status=DocumentStatus::ACTUAL) const;

    int GetDocumentCount() const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy policy, std::string_view raw_query, int document_id);
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy policy, std::string_view raw_query, int document_id);
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id);

    void RemoveDocument(std::execution::sequenced_policy policy, int document_id);
    void RemoveDocument(std::execution::parallel_policy policy, int document_id);
    void RemoveDocument(int document_id);

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    auto begin() noexcept {
        return ids_.begin();
    }

    auto cbegin() const noexcept {
        return ids_.cbegin();
    }

    auto end() noexcept {
        return ids_.end();
    }

    auto cend() const noexcept {
        return ids_.cend();
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> ids_;

    std::deque<std::string> string_storage_;

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };
    
    QueryWord ParseQueryWord(std::string_view text) const;

    bool IsStopWord(std::string_view word) const;

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(std::execution::sequenced_policy policy, std::string_view text) const;
    Query ParseQuery(std::execution::parallel_policy policy, std::string_view text) const;
    Query ParseQuery(std::string_view text) const;

    template <typename ExecutionPolicy, typename DocumentFilter>
    std::vector<Document> FindAllDocuments(ExecutionPolicy& policy, const Query& query, DocumentFilter document_filter) const;
    template <typename DocumentFilter>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentFilter document_filter) const;

    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    static bool IsValidWord(std::string_view word);
};

template <typename Container>
SearchServer::SearchServer(const Container& stop_words_container) : stop_words_(MakeUniqueNonEmptyStrings(stop_words_container)) {
    if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Stop-word contains invalid characters"s);
    }
}

template <typename DocumentFilter>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentFilter document_filter) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_filter);
}

template <typename ExecutionPolicy, typename DocumentFilter>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy& policy, std::string_view raw_query, DocumentFilter document_filter) const {
    const Query query = ParseQuery(raw_query);

    std::vector<Document> matched_documents = FindAllDocuments(policy, query, document_filter);

    sort(policy, matched_documents.begin(), matched_documents.end(),
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

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy& policy, std::string_view raw_query, const DocumentStatus& document_status) const {
    return FindTopDocuments(policy, raw_query, [&document_status](int document_id, DocumentStatus status, int rating) { return status == document_status; });
}

template <typename ExecutionPolicy, typename DocumentFilter>
std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy& policy, const Query& query, DocumentFilter document_filter) const {
    if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::sequenced_policy>) {
        std::map<int, double> document_to_relevance;
        for (const std::string_view& word : query.plus_words) {
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

        for (const std::string_view& word : query.minus_words) {
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
    else {
        ConcurrentMap<int, double> cm(150);
        
        for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(),
            [&cm, document_filter, this](const auto word) {
                if (word_to_document_freqs_.count(word) != 0) {
                    const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                    for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                        const auto& document_data = documents_.at(document_id);
                        if (document_filter(document_id,document_data.status, document_data.rating)) {
                            cm[document_id].ref_to_value += term_freq * inverse_document_freq;
                        }
                    }
                }
            });

        for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
            [&cm, document_filter, this](const auto word) {
                if (word_to_document_freqs_.count(word) != 0) {
                    for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                        cm.erase(document_id);
                    }
                } 
            });

        const std::map<int, double> document_to_relevance = cm.BuildOrdinaryMap();
        std::vector<Document> matched_documents;
        matched_documents.reserve(document_to_relevance.size());

        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
}

template <typename DocumentFilter>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentFilter document_filter) const {
    return FindAllDocuments(std::execution::seq, query, document_filter);
}