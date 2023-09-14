#include "remove_duplicates.h"

#include <vector>
#include <set>
#include <string>

using namespace std::string_literals;

void RemoveDuplicates(SearchServer& search_server) {
    std::vector<int> duplicates;
    std::set<std::set<std::string>> unique_documents;
    std::set<std::string> words;

    for (const int document_id : search_server) {
        words.clear();

        for (const auto& pair : search_server.GetWordFrequencies(document_id)) {
            words.insert(std::string(pair.first));
        }

        if (unique_documents.count(words) > 0) {
            duplicates.push_back(document_id);
            std::cout << "Found duplicate document id "s << document_id << std::endl;
        }
        else {
            unique_documents.insert(words);
        }    
    }

    for (const int document_id : duplicates) {
        search_server.RemoveDocument(document_id);
    }
}