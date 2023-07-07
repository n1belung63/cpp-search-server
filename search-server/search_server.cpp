#include "search_server.h"

#include <cmath>

using namespace std::string_literals;

SearchServer::SearchServer() { }

SearchServer::SearchServer(const std::string & stop_words_string) : SearchServer::SearchServer(SplitIntoWords(stop_words_string)) { }

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("Document id less than zero"s);
    }

    if (documents_.count(document_id) != 0) {
        throw std::invalid_argument("Document with this id already exists in the database"s);
    }

    const std::vector<std::string> words = SplitIntoWordsNoStop(document);

    id_to_num_[id_to_num_.size()] = document_id;
    const double inv_word_count = 1.0 / words.size();
    for (const std::string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, const DocumentStatus& document_status) const {
    return SearchServer::FindTopDocuments(raw_query, [&document_status](int document_id, DocumentStatus status, int rating) { return status == document_status; });
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);

    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }

    return std::tuple(matched_words, documents_.at(document_id).status);
}

int SearchServer::GetDocumentId(int index) const {
    if (index <0 && index >= id_to_num_.size()) {
        throw std::out_of_range("Invalid document id"s);
    }
    else {
        return id_to_num_.at(index);
    }
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
    if (text.size() == 1 && text[0] == '-') {
        throw std::invalid_argument("Word contains only \"-\" character"s);
    }

    if (text.size() >= 2 && text[0] == '-' && text[1] == '-') {
        throw std::invalid_argument("Word contains more than one \"-\" character at the beginning"s);
    }

    if (!IsValidWord(text)) {
        throw std::invalid_argument("Word contains invalid characters"s);
    }

    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    return SearchServer::QueryWord{text, is_minus, IsStopWord(text)};
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word contains invalid characters"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
    SearchServer::Query query_words;

    for (const std::string& word : SplitIntoWords(text)) {
        const SearchServer::QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query_words.minus_words.insert(query_word.data);
            } else {
                query_words.plus_words.insert(query_word.data);
            }
        }
    }
    
    return query_words;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return std::log(SearchServer::GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    else {
        return std::accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
    }    
}

bool SearchServer::IsValidWord(const std::string& word) {
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}