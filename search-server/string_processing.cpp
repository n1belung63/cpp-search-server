#include "string_processing.h"

#include <string>
#include <iostream>

using namespace std::string_literals;

std::ostream& operator<<(std::ostream& out, DocumentStatus status)
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

void PrintDocument(const Document& document) {
    std::cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s;
}