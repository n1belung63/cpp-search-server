#include "search_server.h"

#include <cmath>
#include <deque>
#include <string>
#include <string_view>

using namespace std::string_literals;

SearchServer::SearchServer() { }

SearchServer::SearchServer(const std::string_view& stop_words_string) : SearchServer::SearchServer(SplitIntoWords(stop_words_string)) { }

SearchServer::SearchServer(const std::string& stop_words_string) : SearchServer::SearchServer(std::string_view(stop_words_string)) { }

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("Document id less than zero"s);
    }

    if (documents_.count(document_id) != 0) {
        throw std::invalid_argument("Document with this id already exists in the database"s);
    }

    string_storage_.emplace_back(document);

    const std::vector<std::string_view> words = SplitIntoWordsNoStop(string_storage_.back());

    const double inv_word_count = 1.0 / words.size();
    for (const std::string_view& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, const DocumentStatus& document_status) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_status);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy policy, std::string_view raw_query, int document_id) {
    if (!documents_.count(document_id)) {
        throw std::out_of_range("No document with this id"s);
    }

    string_storage_.emplace_back(raw_query);
    const Query query = ParseQuery(string_storage_.back());
    std::vector<std::string_view> matched_words;
    bool is_minus_word_in_document = false;

    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            is_minus_word_in_document = true;
            break;
        }
    }

    if (!is_minus_word_in_document) {
        for (std::string_view word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
    }
    
    return std::tuple(matched_words, documents_.at(document_id).status);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy policy, std::string_view raw_query, int document_id) {
    if (!documents_.count(document_id)) {
        throw std::out_of_range("No document with this id"s);
    }

    string_storage_.emplace_back(raw_query);
    const Query query = ParseQuery(policy, string_storage_.back());
    std::vector<std::string_view> matched_words;

    bool is_minus_word_in_document = std::any_of(policy, query.minus_words.begin(), query.minus_words.end(),
        [this, &document_id](std::string_view word) {
            if (word_to_document_freqs_.count(word) == 0) {
                return false;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                return true;
            }    
            return false;
        });

    if (!is_minus_word_in_document) {
        matched_words.resize(word_to_document_freqs_.size());
        auto it = std::copy_if(policy, query.plus_words.begin(), query.plus_words.end(),
            matched_words.begin(),
            [this, &document_id](std::string_view word) {
                if (word_to_document_freqs_.count(word) == 0) {
                    return false;
                }
                if (word_to_document_freqs_.at(word).count(document_id)) {
                    return true;
                }
                return false;
            });
        matched_words.resize(std::distance(matched_words.begin(), it));

        std::sort(matched_words.begin(), matched_words.end());
        auto it2 = matched_words.erase(std::unique(matched_words.begin(), matched_words.end()), matched_words.end());
        matched_words.resize(std::distance(matched_words.begin(), it2));
    }

    return std::tuple(matched_words, documents_.at(document_id).status);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query, int document_id) {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

static std::map<std::string_view, double> word_freqs_empty_;

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (document_to_word_freqs_.count(document_id) == 0) {
        return word_freqs_empty_;
    }
    else {
        return document_to_word_freqs_.at(document_id);
    }
}

void SearchServer::RemoveDocument(int document_id) {
    SearchServer::RemoveDocument(std::execution::seq, document_id);
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy, int document_id) {
    if (!documents_.count(document_id)) {
        throw std::out_of_range("No document with this id"s);
    }

    documents_.erase(document_id);

    for (auto & [word, _] : document_to_word_freqs_[document_id]) {
        word_to_document_freqs_.at(word).erase(document_id);
    }

    document_to_word_freqs_.erase(document_id);
    ids_.erase(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy, int document_id) {
    if (!documents_.count(document_id)) {
        throw std::out_of_range("No document with this id"s);
    }

    documents_.erase(document_id);

    std::map<std::string_view, double>& word_to_freq_in_document_with_id = document_to_word_freqs_.at(document_id);
    std::vector<std::string_view> words(word_to_freq_in_document_with_id.size());

    std::transform( std::execution::par, 
                    word_to_freq_in_document_with_id.begin(), word_to_freq_in_document_with_id.end(),
                    words.begin(),
                    [](const std::pair<const std::string_view, double>& ptr_to_word) {
                        return ptr_to_word.first;
                    });

    std::for_each(  std::execution::par,
                    words.begin(), words.end(),
                    [this, document_id](const std::string_view word) {
                        word_to_document_freqs_.at(word).erase(document_id);
                    });

    document_to_word_freqs_.erase(document_id);
    ids_.erase(document_id);
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
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

bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(word) > 0;
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
    std::vector<std::string_view> words_no_stop;
    for (const std::string_view& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word contains invalid characters"s);
        }
        if (!IsStopWord(word)) {
            words_no_stop.push_back(word);
        }
    }
    return words_no_stop;
}

SearchServer::Query SearchServer::ParseQuery(std::execution::sequenced_policy policy, std::string_view text) const {
    SearchServer::Query query_words = ParseQuery(std::execution::par, text);

    std::sort( query_words.plus_words.begin(), query_words.plus_words.end() );
    query_words.plus_words.erase( std::unique( query_words.plus_words.begin(), query_words.plus_words.end() ), query_words.plus_words.end() );

    std::sort( query_words.minus_words.begin(), query_words.minus_words.end() );
    query_words.minus_words.erase( std::unique( query_words.minus_words.begin(), query_words.minus_words.end() ), query_words.minus_words.end() );

    return query_words;
}

SearchServer::Query SearchServer::ParseQuery(std::execution::parallel_policy policy, std::string_view text) const {
    const std::vector<std::string_view> words = SplitIntoWords(text);
    SearchServer::Query query_words;

    query_words.plus_words.reserve(words.size());
    query_words.minus_words.reserve(words.size());

    std::for_each(words.begin(), words.end(), [&query_words, this](std::string_view word) {
        const SearchServer::QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query_words.minus_words.push_back(query_word.data);
            } else {
                query_words.plus_words.push_back(query_word.data);
            }
        }
    });

    return query_words;
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text) const {
    return ParseQuery(std::execution::seq, text);
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
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

bool SearchServer::IsValidWord(std::string_view word) {
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}