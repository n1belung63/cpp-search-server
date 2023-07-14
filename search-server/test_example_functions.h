#pragma once

#include "search_server.h"

void AddDocument(SearchServer & search_server, int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

void FindTopDocuments(SearchServer & search_server, const std::string& raw_query, const DocumentStatus& document_status=DocumentStatus::ACTUAL);

void MatchDocument(SearchServer & search_server, const std::string& raw_query);