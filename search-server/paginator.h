#pragma once

#include <cmath>
#include <vector>
#include <iterator>

template <typename Iterator>
class IteratorRange {
public: 
    IteratorRange(Iterator iterator_begin, Iterator iterator_end) :
        iterator_begin_(iterator_begin), 
        iterator_end_(iterator_end)
        { }

    Iterator begin() {
        return iterator_begin_;
    }

    Iterator end() {
        return iterator_end_;
    }

    size_t size() {
        return std::distance(iterator_begin_, iterator_end_);
    }

private:
    Iterator iterator_begin_;
    Iterator iterator_end_;
};

template <typename Iterator>
class Paginator {
public:
    explicit Paginator(Iterator b, Iterator e, int page_size) : 
        pages_(FillPages(b, e, page_size)) 
        { }

    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

    size_t size() const {
        return pages_.size();
    }

private:
    std::vector<IteratorRange<Iterator>> pages_;

    std::vector<IteratorRange<Iterator>> FillPages(Iterator b, Iterator e, int page_size) {
        std::vector<IteratorRange<Iterator>> pages;

        size_t dist = distance(b, e);

        for (Iterator it = b; it < e; std::advance(it, page_size)) {
            pages.push_back(IteratorRange(it, it + std::min(static_cast<size_t>(page_size) ,dist)));
            dist -= page_size;
        }

        return pages;
    }
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(c.begin(), c.end(), page_size);
}