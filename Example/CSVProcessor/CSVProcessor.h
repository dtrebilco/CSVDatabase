#pragma once
#include <variant>
#include <string>
#include <vector>
#include <format>
#include <unordered_map>
#include <filesystem>
#include <functional>

using FieldType = std::variant<std::string, bool, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double>;

// Override this method to redirect output messages
extern std::function<void (const char*)> OutputMessageFunc;

template<typename... Args>
inline void OutputMessage(std::format_string<Args...> fmt, Args&&... args)
{
  char buf[512];
  auto out = std::format_to_n(buf, std::size(buf) - 1, fmt, std::forward<Args>(args)...);
  *out.out = '\0';
  OutputMessageFunc(buf);
}

struct CSVHeader
{
  std::string m_rawField;    // The raw full field string

  std::string m_name;        // Column name
  FieldType m_type;          // Column type
  bool m_isKey = false;      // Is table key
  bool m_isIgnored = false;  // Is ignored

  std::string m_minValue;    // The min range string value (if any)
  std::string m_maxValue;    // The max range string value (if any)

  std::string m_comment;     // Comment field (if any)
  std::string m_foreignTable;// Foreign table link (if any)
  bool m_isWeakForeignTable = false; // If a foreign table link is a weak link
};

struct CSVTable
{
  std::vector<CSVHeader> m_headerData; // Header data that is info for each column
  std::vector<uint32_t> m_keyColumns;  // Index of the columns that are keys in the table

  std::vector<std::vector<FieldType>> m_rowData;
};

constexpr bool IsGlobalTable(std::string_view tableName) { return tableName.starts_with("Global"); }
constexpr bool IsEnumTable(std::string_view tableName) { return tableName.starts_with("Enum"); }

bool GetColumnType(std::string_view name, FieldType& retType);

void AppendToString(const FieldType& var, std::string& appendStr);
std::string to_string(const FieldType& var);
bool IsEqual(const FieldType& var, size_t val);

bool ParseField(const FieldType& type, std::string_view string, FieldType& retField);
bool ParseFieldMove(const FieldType& type, std::string&& string, FieldType& retField);

std::vector<std::vector<std::string>> ReadCSV(const char* srcData);
bool ReadHeader(std::string_view field, CSVHeader& out);
bool ReadTable(const char* fileString, CSVTable& newTable);
bool SortTable(CSVTable& newTable);
bool FindSourceHeaderColumn(const std::string& columnName, const std::string& foreignTableName, const std::unordered_map<std::string, CSVTable>& tables, std::string& outTableName, FieldType& outField);
bool ValidateTables(const std::unordered_map<std::string, CSVTable>& tables);

void SaveToString(const CSVTable& table, const std::unordered_map<std::string, CSVTable>& tables, std::string_view existingFile, std::string& outFile);
bool CalculateTableDepth(const std::string& tableName, const std::unordered_map<std::string, CSVTable>& tables, std::unordered_map<std::string, uint32_t>& tableDepths, uint32_t& depth);

bool ReadToString(const std::filesystem::path& path, std::string& outStr);
