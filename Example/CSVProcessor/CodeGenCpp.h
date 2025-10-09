#pragma once
#include "CSVProcessor.h"

bool CodeGenCpp(const char* outputPathStr, const std::unordered_map<std::string, CSVTable>& tables, const std::unordered_map<std::string, CSVTable>& tablesEnumRaw);
