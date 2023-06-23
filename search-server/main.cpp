#include <iostream>
#include <string>
#include <utility>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <vector>
#include <map>
#include <set>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double RESEDUAL_OF_DOCUMENT_RELEVANCE = 1e-6;

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

vector<int> ReadLineWithNumbers() {
    int count;
    int value;
    cin >> count;
    vector<int> numbers(count);
    for (int i = 0; i < count; i++) {
        cin >> value;
        numbers[i] = value;
    }
    ReadLine();
    return numbers;
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
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template <typename DocumentFilter>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentFilter document_filter) const {
        const Query query = ParseQuery(raw_query);
        vector<Document> matched_documents = FindAllDocuments(query,document_filter);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < RESEDUAL_OF_DOCUMENT_RELEVANCE) {
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

    vector<Document> FindTopDocuments(const string& raw_query, const DocumentStatus& document_status=DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [&document_status](int document_id, DocumentStatus status, int rating) { return status == document_status; });
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };
    
    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

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

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query_words;

        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
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

    template <typename DocumentFilter>
    vector<Document> FindAllDocuments(const Query& query, DocumentFilter document_filter) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
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

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        else {
            return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
        }    
    } 
};

template <typename Key, typename Value>
ostream& operator<<(ostream& out, const pair<Key, Value>& pair) {
    out << pair.first << ": "s << pair.second;
    return out;
}

template <typename Cont>
ostream& Print(ostream& out, const Cont container) {
    bool is_first = true;
    for (const auto & element : container) {
        if (is_first) {
            is_first = false;
            out << element;
        }
        else {
            out << ", "s << element;
        }
    }
    return out;
}

template <typename Term>
ostream& operator<<(ostream& out, const vector<Term>& container) {
    out << "["s;
    Print<vector<Term>>(out, container);
    out << "]"s;
    return out;
}  

template <typename Term>
ostream& operator<<(ostream& out, const set<Term>& container) {
    out << "{"s;
    Print<set<Term>>(out, container);
    out << "}"s;
    return out;
}

template <typename Key, typename Value>
ostream& operator<<(ostream& out, const map<Key, Value>& container) {
    out << "{"s;
    Print<map<Key,Value>>(out, container);
    out << "}"s;
    return out;
}

ostream& operator<<(ostream& out, DocumentStatus status)
{
    switch(status)
    {
        case DocumentStatus::ACTUAL : return out << "DocumentStatus::ACTUAL"s ;
        case DocumentStatus::BANNED : return out << "DocumentStatus::BANNED"s ;
        case DocumentStatus::IRRELEVANT : return out << "DocumentStatus::IRRELEVANT"s ;
        case DocumentStatus::REMOVED : return out << "DocumentStatus::REMOVED"s ;
    }

    return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename Func>
void RunTestImpl(const Func& func, const string& func_name) {
    func();
    cerr << func_name << " OK"s << endl;
}

#define RUN_TEST(func)  RunTestImpl((func), #func)

// -------- Начало модульных тестов поисковой системы ----------

void TestAddingDocuments() {
    const int doc_id = 2;
    const string content = "fluffy cat fluffy tail"s;
    const vector<int> ratings = {7, 2, 7};

    {
        SearchServer server;
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 0u, "Server should be empty"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 1u, "Server should contain 1 document"s);
    }
}

void TestFindDocumentByQuery() {
    const int doc_id = 2;
    const string content = "fluffy cat fluffy tail"s;
    const vector<int> ratings = {7, 2, 7};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, doc_id, "Document IDs should be equal"s);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("black starling evgeny"s).empty(), "Document should not be found here"s);
    }
}

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, doc_id, "Document IDs should be equal"s);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

void TestExcludeDocumentWithMinusWords() {
    {
        SearchServer server;
        server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(2, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s);
        ASSERT_EQUAL(found_docs.size(), 2u);
    }

    {
        SearchServer server;
        server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(2, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat -collar"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
    }
}

void TestMatchingDocuments() {
    const int doc_id = 2;
    const string content = "fluffy cat fluffy tail"s;
    const vector<int> ratings = {7, 2, 7};
    const DocumentStatus status = DocumentStatus::ACTUAL;

    {
        SearchServer server;
        server.AddDocument(doc_id, content, status, ratings);
        const auto [words, doc_status] = server.MatchDocument("fluffy groomed cat"s, doc_id);
        ASSERT_EQUAL(words.size(), 2u);
        ASSERT_EQUAL(words.at(0), "cat"s);
        ASSERT_EQUAL(words.at(1), "fluffy"s);   
        ASSERT_EQUAL(status, doc_status);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, status, ratings);
        const auto [words, doc_status] = server.MatchDocument("-fluffy groomed cat"s, doc_id);
        ASSERT_HINT(words.empty(), "Document mast be excluded by minus word"s);     
    }
}

void TestSortingByRelevance() {
    {
        SearchServer server;
        server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(2, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s);
        ASSERT_EQUAL(found_docs.size(), 2u);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        ASSERT_HINT(doc0.relevance > doc1.relevance, "Wrong order"s);
    }
}

void TestAveragingRating() {
    const int doc_id = 2;
    const string content = "fluffy cat fluffy tail"s;
    const vector<int> ratings = {7, 2, 7};
    const int averaged_rating = accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_HINT(doc0.rating == averaged_rating, "Wrong average rating"s);
    }
}

void TestFilteringByPredicat() {
    {
        SearchServer server;
        server.SetStopWords("and in on"s);
        server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(2, "groomed dog expressive eyes"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        server.AddDocument(3, "groomed starling evgeny"s, DocumentStatus::BANNED, {9});
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        ASSERT_EQUAL(found_docs.size(), 2u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 0);
        const Document& doc1 = found_docs[1];
        ASSERT_EQUAL(doc1.id, 2);
    }
}

void TestFindDocumentsWithStatus() {
    {
        SearchServer server;
        server.SetStopWords("and in on"s);
        server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::IRRELEVANT, {7, 2, 7});
        server.AddDocument(2, "groomed dog expressive eyes"s, DocumentStatus::REMOVED, {5, -12, 2, 1});
        server.AddDocument(3, "groomed starling evgeny"s, DocumentStatus::BANNED, {9});
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 3);
    }

    {
        SearchServer server;
        server.SetStopWords("and in on"s);
        server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::IRRELEVANT, {7, 2, 7});
        server.AddDocument(2, "groomed dog expressive eyes"s, DocumentStatus::REMOVED, {5, -12, 2, 1});
        server.AddDocument(3, "groomed starling evgeny"s, DocumentStatus::BANNED, {9});
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 1);
    }

    {
        SearchServer server;
        server.SetStopWords("and in on"s);
        server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::IRRELEVANT, {7, 2, 7});
        server.AddDocument(2, "groomed dog expressive eyes"s, DocumentStatus::REMOVED, {5, -12, 2, 1});
        server.AddDocument(3, "groomed starling evgeny"s, DocumentStatus::BANNED, {9});
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 2);
    }
}

void TestRelevanceCalculation() {
    {
        SearchServer server;
        server.SetStopWords("and in on"s);
        server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(2, "groomed dog expressive eyes"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        server.AddDocument(3, "groomed starling evgeny"s, DocumentStatus::BANNED, {9});
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s);
        ASSERT_EQUAL(found_docs.size(), 3u);
        const Document& doc0 = found_docs[0];
        const double relevance_for_document_1 = log(4.0/1.0)*(2.0/4.0) + log(4.0/2.0)*(0.0/4.0) + log(4.0/2.0)*(1.0/4.0);
        ASSERT_HINT(abs(doc0.relevance-relevance_for_document_1) <= 1e-5, "Wrong relevance"s);
    }
}

void TestSearchServer() {
    RUN_TEST(TestAddingDocuments);
    RUN_TEST(TestFindDocumentByQuery);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeDocumentWithMinusWords);
    RUN_TEST(TestMatchingDocuments);
    RUN_TEST(TestSortingByRelevance);
    RUN_TEST(TestAveragingRating);
    RUN_TEST(TestFilteringByPredicat);
    RUN_TEST(TestFindDocumentsWithStatus);
    RUN_TEST(TestRelevanceCalculation);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    cout << "Search server testing finished"s << endl;
}