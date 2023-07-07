#pragma once

#include <iostream>
#include <utility>
#include <vector>
#include <set>
#include <map>

#include "document.h"
#include "paginator.h"

using namespace std::string_literals;

template <typename Key, typename Value>
std::ostream& operator<<(std::ostream& out, const std::pair<Key, Value>& pair) {
    out << pair.first << ": "s << pair.second;
    return out;
}

template <typename Cont>
std::ostream& Print(std::ostream& out, const Cont container) {
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
std::ostream& operator<<(std::ostream& out, const std::vector<Term>& container) {
    out << "["s;
    Print<std::vector<Term>>(out, container);
    out << "]"s;
    return out;
}  

template <typename Term>
std::ostream& operator<<(std::ostream& out, const std::set<Term>& container) {
    out << "{"s;
    Print<std::set<Term>>(out, container);
    out << "}"s;
    return out;
}

template <typename Key, typename Value>
std::ostream& operator<<(std::ostream& out, const std::map<Key, Value>& container) {
    out << "{"s;
    Print<std::map<Key,Value>>(out, container);
    out << "}"s;
    return out;
}

std::ostream& operator<<(std::ostream& out, DocumentStatus status);

void PrintDocument(const Document& document);

template <typename Iterator>
std::ostream& operator<<(std::ostream& out, IteratorRange<Iterator> page)
{
    for (auto doc = page.begin(); doc != page.end(); ++doc) {
        PrintDocument(*doc);
    }
    return out;
}
