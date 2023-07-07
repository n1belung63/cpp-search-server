#include "test_search_server.h"

#include <vector>
#include <string>
#include <cmath>

using namespace std::string_literals;

void TestNoStopWords() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, doc_id, "Document IDs should be equal"s);
    }
}

void TestAddingDocuments() {
    const int doc_id = 2;
    const std::string content = "fluffy cat fluffy tail"s;
    const std::vector<int> ratings = {7, 2, 7};

    {
        SearchServer server("in the"s);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 0u, "Server should be empty"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 1u, "Server should contain 1 document"s);
    }
}

void TestFindDocumentByQuery() {
    const int doc_id = 2;
    const std::string content = "fluffy cat fluffy tail"s;
    const std::vector<int> ratings = {7, 2, 7};

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, doc_id, "Document IDs should be equal"s);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("black starling evgeny"s).empty(), "Document should not be found here"s);
    }
}

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};

    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, doc_id, "Document IDs should be equal"s);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

void TestExcludeDocumentWithMinusWords() {
    {
        SearchServer server("in the"s);
        server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(2, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s);
        ASSERT_EQUAL(found_docs.size(), 2u);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(2, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat -collar"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
    }
}

void TestMatchingDocuments() {
    const int doc_id = 2;
    const std::string content = "fluffy cat fluffy tail"s;
    const std::vector<int> ratings = {7, 2, 7};
    const DocumentStatus status = DocumentStatus::ACTUAL;

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, status, ratings);
        const auto [words, doc_status] = server.MatchDocument("fluffy groomed cat"s, doc_id);
        ASSERT_EQUAL(words.size(), 2u);
        ASSERT_EQUAL(words.at(0), "cat"s);
        ASSERT_EQUAL(words.at(1), "fluffy"s);   
        ASSERT_EQUAL(status, doc_status);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, status, ratings);
        const auto [words, doc_status] = server.MatchDocument("-fluffy groomed cat"s, doc_id);
        ASSERT_HINT(words.empty(), "Document mast be excluded by minus word"s);     
    }
}

void TestSortingByRelevance() {
    {
        SearchServer server("in the"s);
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
    const std::string content = "fluffy cat fluffy tail"s;
    const std::vector<int> ratings = {7, 2, 7};
    const int averaged_rating = std::accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_HINT(doc0.rating == averaged_rating, "Wrong average rating"s);
    }
}

void TestFilteringByPredicat() {
    {
        SearchServer server("and in on"s);
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
        SearchServer server("and in on"s);
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
        SearchServer server("and in on"s);
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
        SearchServer server("and in on"s);;
        server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::IRRELEVANT, {7, 2, 7});
        server.AddDocument(2, "groomed dog expressive eyes"s, DocumentStatus::REMOVED, {5, -12, 2, 1});
        server.AddDocument(3, "groomed starling evgeny"s, DocumentStatus::BANNED, {9});
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 2);
    }

    {
        SearchServer server("and in on"s);;
        server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::IRRELEVANT, {7, 2, 7});
        server.AddDocument(2, "groomed dog expressive eyes"s, DocumentStatus::REMOVED, {5, -12, 2, 1});
        server.AddDocument(3, "groomed starling evgeny"s, DocumentStatus::BANNED, {9});
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 0);
    }
}

void TestRelevanceCalculation() {
    {
        SearchServer server("and in on"s);
        server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(2, "groomed dog expressive eyes"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        server.AddDocument(3, "groomed starling evgeny"s, DocumentStatus::BANNED, {9});
        const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s);
        ASSERT_EQUAL(found_docs.size(), 3u);
        const Document& doc0 = found_docs[0];
        const double relevance_for_document_1 = std::log(4.0/1.0)*(2.0/4.0) + std::log(4.0/2.0)*(0.0/4.0) + std::log(4.0/2.0)*(1.0/4.0);
        ASSERT_HINT(abs(doc0.relevance-relevance_for_document_1) <= 1e-5, "Wrong relevance"s);
    }
}

void TestSearchServer() {
    RUN_TEST(TestNoStopWords);
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