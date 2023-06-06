#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document) {   
        const vector<string> words = SplitIntoWordsNoStop(document);
        float tf_partion = 1.0/words.size();
        if (!words.empty()) {
            document_count_++;
        }
        for (const string & word : words) {
            word_to_document_freqs_[word][document_id] += tf_partion;
        }
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    enum WordAttr {
        None,
        Plus, 
        Minus, 
        Stop 
    };

    /* 
    struct Word {
        string word;
        WordAttr attr;
    };
    */

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
        map<string, WordAttr> words_with_attr;
    };
    
    int document_count_ = 0; 
    
    map<string, map<int, double>> word_to_document_freqs_;

    set<string> stop_words_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    Query ParseQuery(const string& text) const {
        Query query_words;
        string cut_word;
        for (const string& word : SplitIntoWords(text)) {
            if (word.at(0) == '-') {
                cut_word = word.substr(1);
                if (!IsStopWord(cut_word)) {
                    query_words.minus_words.insert(cut_word);
                    query_words.words_with_attr[cut_word] = Minus;
                }
                else {
                    query_words.words_with_attr[cut_word] = Stop;
                }
            }
            else {
                if (!IsStopWord(cut_word)) {
                    query_words.plus_words.insert(word);
                    query_words.words_with_attr[cut_word] = Plus;
                }
                else {
                    query_words.words_with_attr[cut_word] = Stop;
                }
            }
        }
        
        return query_words;
    }

    vector<Document> FindAllDocuments(const Query& query_words) const {
        vector<Document> matched_documents;   
        map<int, double> document_to_relevance;      
        double idf;
        
        for (const string & word : query_words.plus_words) {
            if (word_to_document_freqs_.count(word) != 0) {
                idf = CalculateIdf(word);
                for (const auto & [id, tf] : word_to_document_freqs_.at(word)) {
                    document_to_relevance[id] += idf * tf;
                }
            }  
        }
        
        for (const string & word : query_words.minus_words) {
            if (word_to_document_freqs_.count(word) != 0) {
                for (const auto & [id, tf] : word_to_document_freqs_.at(word)) {
                    document_to_relevance.erase(id);
                }    
            }
        }
        
        for (const auto & [id, relevance] : document_to_relevance) {
            matched_documents.push_back({id, relevance});
        }
 
        return matched_documents;
    }

    double CalculateIdf(const string& word) const {
        return log( static_cast<double>(document_count_) / static_cast<double>(word_to_document_freqs_.at(word).size()) );
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto & [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}