#include "read_input_functions.h"

#include <iostream>

using namespace std::string_literals;

std::string ReadLine() {
    std::string s;
    std::getline(std::cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    std::cin >> result;
    ReadLine();
    return result;
}

std::vector<int> ReadLineWithNumbers() {
    int count;
    int value;
    std::cin >> count;
    std::vector<int> numbers(count);
    for (int i = 0; i < count; i++) {
        std::cin >> value;
        numbers[i] = value;
    }
    ReadLine();
    return numbers;
}

std::vector<std::string> SplitIntoWords(const std::string& text) {
    std::vector<std::string> words;
    std::string word;
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
