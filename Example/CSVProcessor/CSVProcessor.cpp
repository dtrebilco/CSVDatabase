#include <iostream>
#include <fstream>
#include <filesystem>
#include <format>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <charconv>
#include <variant>
#include <algorithm>

// TODO: Add test where the key is a foreign key - part of a multi key also
//       Test when key is int type and what to do when referenced by a foreign key
// Test min max range of each type parsing - with/without min max
// TODO: Remove ColumnType and just have a default? Also remove min/max?
// TODO: Have columns of the same type - and parse inline to the final format

#define OUTPUT_MESSAGE(...) { char buf[512]; const auto out = std::format_to_n(buf, std::size(buf) - 1, __VA_ARGS__); *out.out = '\0'; printf("%s\n", buf); }

using FieldType = std::variant<std::string, bool, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double>;

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

static const char s_commonHeaderStart[] = R"header(// Generated Database file - do not edit manually
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <string_view>

namespace DB
{

// Protected DB identifier type. The ID can only be created by the database and helper database methods.
template<typename T>
class IDType
{
public:
  constexpr IDType() = default;
  constexpr uint32_t GetIndex() const { return m_dbIndex; }
  constexpr bool operator == (const IDType& val) const { return m_dbIndex == val.m_dbIndex; }
  constexpr bool operator != (const IDType& val) const { return m_dbIndex != val.m_dbIndex; }
  constexpr bool operator < (const IDType& val) const { return m_dbIndex < val.m_dbIndex; }

private:
  friend class DB;
  template<typename T> friend class IterType;

  uint32_t m_dbIndex = 0;

  constexpr explicit IDType(uint32_t index) : m_dbIndex(index) {}
};

// Iterator type for database tables.
// Usage example for a "Character" table:
//  for (auto& i : Database.Iter<DB::Character>())
//  {
//    DB::Character::ID Id = i.GetID();
//    const DB::Character& Item = Iter.GetValue();
//  }
template <typename T>
class IterType
{
public:
  struct Data
  {
    constexpr IDType<T> GetID() const { return IDType<T>(static_cast<uint32_t>(m_pos)); }
    constexpr const T& GetValue() const { return m_dbArray[m_pos]; }

  protected:
    constexpr Data(size_t pos, const std::vector<T>& array) : m_pos(pos), m_dbArray(array) {}

    size_t m_pos;
    const std::vector<T>& m_dbArray;
  };

  struct Iterator : public Data
  {
    constexpr Iterator& operator++() { this->m_pos++; return *this; }
    constexpr bool operator!=(const Iterator& val) const { return this->m_pos != val.m_pos; }
    constexpr Data& operator *() { return *this; }

  protected:
    friend class IterType;
    constexpr Iterator(size_t pos, const std::vector<T>& array) : Data(pos, array) {}
  };

  constexpr Iterator begin() { return Iterator(0, m_dbArray); }
  constexpr Iterator end() { return Iterator(m_dbArray.size(), m_dbArray); }

private:
  friend class DB;

  constexpr explicit IterType(const std::vector<T>& array) : m_dbArray(array) {}
  const std::vector<T>& m_dbArray;
};

)header";

static const char s_commonHeaderEnd[] = R"header(
} // namespace DB
)header";


static const char s_commonBodyStart[] = R"body(// Generated Database file - do not edit manually
#include "DB.h"

)body";

static const char s_commonBodyEnd[] = R"body()body";


bool IsGlobalTable(std::string_view tableName) { return tableName.starts_with("Global"); }
bool IsEnumTable(std::string_view tableName) { return tableName.starts_with("Enum"); }

bool GetColumnType(std::string_view name, FieldType& retType)
{
  if (name == "string") { retType = FieldType(""); return true;   }
  if (name == "bool")  { retType = FieldType(false); return true; }

  if (name == "int8")  { retType = FieldType(int8_t(0));  return true; }
  if (name == "int16") { retType = FieldType(int16_t(0)); return true; }
  if (name == "int23") { retType = FieldType(int32_t(0)); return true; }
  if (name == "int64") { retType = FieldType(int64_t(0)); return true; }

  if (name == "uint8") { retType = FieldType(uint8_t(0));   return true; }
  if (name == "uint16") { retType = FieldType(uint16_t(0)); return true; }
  if (name == "uint23") { retType = FieldType(uint32_t(0)); return true; }
  if (name == "uint64") { retType = FieldType(uint64_t(0)); return true; }

  if (name == "float32") { retType = FieldType(float(0)); return true;  }
  if (name == "float64") { retType = FieldType(double(0)); return true; }

  return false;
}

const char* CPPTypeString(const FieldType& var)
{
  return std::visit([]<typename T>(const T & e)
  {
    if constexpr (std::is_same_v<T, std::string>) { return "std::string"; }
    else if constexpr (std::is_same_v<T, bool>) { return "bool"; }

    else if constexpr (std::is_same_v<T, int8_t>) { return "int8_t"; }
    else if constexpr (std::is_same_v<T, int16_t>) { return "int16_t"; }
    else if constexpr (std::is_same_v<T, int32_t>) { return "int32_t"; }
    else if constexpr (std::is_same_v<T, int64_t>) { return "int64_t"; }

    else if constexpr (std::is_same_v<T, uint8_t>) { return "uint8_t"; }
    else if constexpr (std::is_same_v<T, uint16_t>) { return "uint16_t"; }
    else if constexpr (std::is_same_v<T, uint32_t>) { return "uint32_t"; }
    else if constexpr (std::is_same_v<T, uint64_t>) { return "uint64_t"; }

    else if constexpr (std::is_same_v<T, float>) { return "float"; }
    else if constexpr (std::is_same_v<T, double>) { return "double"; }

  }, var);
}


void AppendToString(const FieldType& var, std::string& appendStr)
{
  std::visit([&appendStr]<typename T>(const T & e)
  {
    if constexpr (std::is_same_v<T, std::string>)
    {
      appendStr += e;
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
      if (e)
      {
        appendStr += "1";
      }
      else
      {
        appendStr += "0";
      }
    }
    else // float/int
    {
      char buf[100];
      std::to_chars_result result = std::to_chars(buf, buf + std::size(buf), e);
      if (result.ec == std::errc())
      {
        appendStr += std::string_view(buf, result.ptr - buf);
      }
      else
      {
        appendStr += std::to_string(e); // This should never get called
      }
    }
  }, var);
}

std::string to_string(const FieldType& var)
{
  std::string ret;
  AppendToString(var, ret);
  return ret;
}

bool IsEqual(const FieldType& var, size_t val)
{
  return std::visit([val]<typename T>(const T & e)
  {
    if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, bool>)
    {
      return false;
    }
    else
    {
      return e == val;
    }
  }, var);
}

template <typename T>
T ReadNumberValue(const char* start, const char* end, std::from_chars_result& strRes)
{
  T val = 0;
  strRes = std::from_chars(start, end, val);
  return val;
}

bool ParseField(const FieldType& type, std::string_view string, FieldType& retField)
{
  if (std::holds_alternative<std::string>(type))
  {
    retField = std::string(string);
    return true;
  }

  std::from_chars_result strRes = {};
  const char* start = string.data();
  const char* end = start + string.size();

  std::visit([start, end, &strRes, &retField]<typename T>(const T & e)
  {
    if constexpr (std::is_same_v<T, std::string>)
    {
      // String is handled earlier
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
      int8_t val = ReadNumberValue<int8_t>(start, end, strRes);
      if (val == 0)
      {
        retField = false;
      }
      else if (val == 1)
      {
        retField = true;
      }
      else
      {
        strRes.ptr = start;
        strRes.ec = std::errc::result_out_of_range;
        OUTPUT_MESSAGE("Error: String type bool has error converting data \"{}\"", std::string_view(start, end));
      }
    }
    else // float/int
    {
      retField = ReadNumberValue<T>(start, end, strRes);
    }
  }, type);

  if (strRes.ec != std::errc())
  {
    OUTPUT_MESSAGE("Error: String type {} has error converting data \"{}\"", type.index(), string);
    return false;
  }

  if (strRes.ptr != end)
  {
    OUTPUT_MESSAGE("Error: String type {} has excess data \"{}\"", type.index(), string);
    return false;
  }

  return true;
}

bool ParseFieldMove(const FieldType& type, std::string&& string, FieldType& retField)
{
  if (std::holds_alternative<std::string>(type))
  {
    retField = std::move(string);
    return true;
  }

  return ParseField(type, string, retField);
}

std::vector<std::vector<std::string>> ReadCSV(const char* srcData) 
{
  std::vector<std::vector<std::string>> csvTable;
  std::vector<std::string> row;
  std::string field;
  bool inQuotes = false;

  if (!srcData)
  {
    return csvTable;
  }

  char c = *srcData;

  while (c)
  {
    // Note: This is allowing out-of-spec quotes to start and stop in a single field and not encompass the entire field
    if (inQuotes)
    {
      if (c == '"')
      {
        // Check for escaped quote
        if (srcData[1] == '"')
        {
          // Consume the next quote
          srcData++;
          field += '"';
        }
        else
        {
          inQuotes = false; // End of quoted field
        }
      }
      else
      {
        field += c; // Add character to field, including newlines
      }
    }
    else
    {
      if (c == '"')
      {
        inQuotes = true; // Start of quoted field
      }
      else if (c == ',')
      {
        row.push_back(field);
        field.clear();
      }
      else if (c == '\n' || c == '\r')
      {
        // Handle newlines (including \r\n for Windows)
        if (c == '\r' && srcData[1] == '\n')
        {
          srcData++; // Consume \n in \r\n
        }

        // Add the last field of the row // DT_TODO: Should this be discarding empty rows? What if a single column? Verify against other parser
        row.push_back(field);
        field.clear();

        // Add row to data
        csvTable.push_back(row);
        row.clear();
      }
      else
      {
        field += c;
      }
    }

    // Go to next character
    srcData++;
    c = *srcData;
  }

  // Handle the last field and row
  if (!field.empty() || !row.empty())
  {
    row.push_back(field);
    csvTable.push_back(row);
  }

  // How to handle if still in quotes at end? inQuotes
  return csvTable;
}

bool ReadHeader(std::string_view field, CSVHeader& out)
{
  out = CSVHeader{};
  out.m_rawField = field;

  bool hasType = false;
  std::string_view tags = field;

  // Strip off any comment
  size_t commentStart = field.find("//");
  if (commentStart != std::string::npos)
  {
    out.m_comment = field.substr(commentStart + 2);
    tags = tags.substr(0, commentStart);
  }
 
  do
  {
    // Strip off starting whitespace
    tags = tags.substr(tags.find_first_not_of(' '));

    // Get the next tag
    std::string_view tag = tags.substr(0, tags.find_first_of(' '));
    if (tag.size() > 0)
    {
      if (out.m_name.empty())
      {
        out.m_name = tag;
      }
      else if (tag[0] == '+' ||
               tag[0] == '*')
      {
        if (out.m_foreignTable.size() > 0)
        {
          OUTPUT_MESSAGE("Duplicate foreign tables in field {}", field);
          return false;
        }

        out.m_foreignTable.assign(tag, 1, tag.size() - 1);
        out.m_isWeakForeignTable = tag[0] == '*';
      }
      else if (tag == "key")
      {
        out.m_isKey = true;
      }
      else if (tag == "ignore")
      {
        out.m_isIgnored = true;
      }
      else if (tag.starts_with("min="))
      {
        out.m_minValue = tag.substr(4);
      }
      else if (tag.starts_with("max="))
      {
        out.m_maxValue = tag.substr(4);
      }
      else
      {
        if (GetColumnType(tag, out.m_type))
        {
          if (hasType)
          {
            OUTPUT_MESSAGE("Duplicate types in field {}", field);
            return false;
          }
          hasType = true;
        }
        else
        {
          OUTPUT_MESSAGE("Unknown tag? {}", tag);
        }
      }

      tags = tags.substr(tag.size());
    }

  } while (tags.size() > 0);

  // If ignored, reset values
  if (out.m_isIgnored)
  {
    out.m_isKey = false;
    out.m_type = FieldType("");
    out.m_foreignTable.clear();
    out.m_comment.clear();
    out.m_minValue.clear();
    out.m_maxValue.clear();
  }

  // If a foreign table link, set the type to string until it can access the foreign table
  if (out.m_foreignTable.size() > 0)
  {
    out.m_type = FieldType("");
  }

  // Abort if a name is not assigned
  if (out.m_name.size() == 0)
  {
    OUTPUT_MESSAGE("Error: No column header name in ""{}""", field);
    return false;
  }

  return true;
}

bool ReadTable(const char* fileString, CSVTable& newTable)
{
  // Check that there is at least one row in addition to the header
  std::vector<std::vector<std::string>> csvData = ReadCSV(fileString);
  if (csvData.size() < 2)
  {
    OUTPUT_MESSAGE("Error: Table does not have at least 2 rows");
    return false;
  }

  // Check all columns have the same count
  const size_t columnCount = csvData[0].size();
  for (size_t i = 1; i < csvData.size(); i++)
  {
    if (csvData[i].size() != columnCount)
    {
      OUTPUT_MESSAGE("Error: Table has column count {} not equal to header count {} != {}", i, csvData[i].size(), columnCount);
      return false;
    }
  }

  // Check the header data of the table and parse it to a table entry
  newTable.m_headerData.reserve(columnCount);
  for (uint32_t i = 0; i < columnCount; i++)
  {
    const std::string& header = csvData[0][i];

    CSVHeader newHeader;
    if (!ReadHeader(header, newHeader))
    {
      OUTPUT_MESSAGE("Error: Table has bad column header {}", header);
      return false;
    }

    if (newHeader.m_isKey)
    {
      newTable.m_keyColumns.push_back(i);
    }

    newTable.m_headerData.push_back(std::move(newHeader));
  }

  // Check that all column names are unique
  {
    std::unordered_set<std::string_view> uniqueCheck;
    for (const CSVHeader& header : newTable.m_headerData)
    {
      if (uniqueCheck.contains(header.m_name))
      {
        OUTPUT_MESSAGE("Error: Table has duplicate column name {}", header.m_name);
        return false;
      }
      uniqueCheck.insert(header.m_name); // DT_TODO: This will not catch multiple columns with the same name if multiple foreign tables with the same base name
    }
  }

  // Copy all row data over
  newTable.m_rowData.resize(csvData.size() - 1);
  for (std::vector<FieldType>& row : newTable.m_rowData)
  {
    row.resize(columnCount);
  }

  for (size_t h = 0; h < columnCount; h++)
  {
    const CSVHeader& header = newTable.m_headerData[h];

    // If there is a min/max range get it
    FieldType minNumber;
    if (header.m_minValue.size() > 0)
    {
      if (!ParseField(header.m_type, header.m_minValue, minNumber))
      {
        OUTPUT_MESSAGE("Error: Table has bad min value in column {} entry \"{}\"", header.m_name, header.m_minValue);
        return false;
      }
    }
    FieldType maxNumber;
    if (header.m_maxValue.size() > 0)
    {
      if (!ParseField(header.m_type, header.m_maxValue, maxNumber))
      {
        OUTPUT_MESSAGE("Error: Table has bad max value in column {} entry \"{}\"", header.m_name, header.m_maxValue);
        return false;
      }
    }

    // Check all table data
    for (size_t i = 1; i < csvData.size(); i++)
    {
      FieldType& columnField = newTable.m_rowData[i - 1][h];

      // Attempt conversion
      if (!ParseFieldMove(header.m_type, std::move(csvData[i][h]), columnField))
      {
        OUTPUT_MESSAGE("Error: Table has bad data in column {}", header.m_name);
        return false;
      }

      if (!std::holds_alternative<std::string>(header.m_type))
      {
        // Check min / max ranges
        if (header.m_minValue.size() > 0)
        {
          if (columnField < minNumber)
          {
            OUTPUT_MESSAGE("Error: Table has bad data in column {} entry \"{}\" is less than min {}", header.m_name, to_string(columnField), header.m_minValue);
            return false;
          }
        }
        if (header.m_maxValue.size() > 0)
        {
          if (columnField > maxNumber)
          {
            OUTPUT_MESSAGE("Error: Table has bad data in column {} entry \"{}\" is greater than max {} ", header.m_name, to_string(columnField), header.m_maxValue);
            return false;
          }
        }
      }
    }
  }
  return true;
}

bool SortTable(CSVTable & newTable)
{
  if (newTable.m_keyColumns.size() == 0 || newTable.m_rowData.size() <= 1)
  {
    return true;
  }

  // Sort by the keys, check if duplicate rows
  std::sort(newTable.m_rowData.begin(), newTable.m_rowData.end(),
    [&newTable](const std::vector<FieldType>& a, const std::vector<FieldType>& b)
    {
      for (uint32_t index : newTable.m_keyColumns)
      {
        if (a[index] < b[index])
        {
          return true;
        }
        if (a[index] != b[index])
        {
          break;
        }
      }
      return false;
    });

  // Loop and check for duplicate rows
  for (size_t r = 1; r < newTable.m_rowData.size(); r++)
  {
    const std::vector<FieldType>& prev = newTable.m_rowData[r - 1];
    const std::vector<FieldType>& curr = newTable.m_rowData[r];

    bool duplicate = true;
    for (uint32_t index : newTable.m_keyColumns)
    {
      if (curr[index] != prev[index])
      {
        duplicate = false;
        break;
      }
    }
    if (duplicate)
    {
      std::string errorKeys;
      for (uint32_t index : newTable.m_keyColumns)
      {
        AppendToString(curr[index], errorKeys);
        errorKeys += " ";
      }
      OUTPUT_MESSAGE("Error: Table has duplicate keys {}", errorKeys);
      return false;
    }
  }

  return true;
}

bool FindSourceHeaderColumn(const std::string& columnName, const std::string& foreignTableName, const std::unordered_map<std::string, CSVTable>& tables, std::string& outTableName, FieldType& outField)
{
  if (foreignTableName.size() == 0)
  {
    return false;
  }

  // Check foreign table exists
  auto findTable = tables.find(foreignTableName);
  if (findTable == tables.end())
  {
    OUTPUT_MESSAGE("Error: Column {} has link to unknown table {}", columnName, foreignTableName);
    return false;
  }

  const CSVTable& foreignTable = findTable->second;
  if (foreignTable.m_keyColumns.size() == 0)
  {
    OUTPUT_MESSAGE("Error: Column {} has link to table {} with no keys", columnName, foreignTableName);
    return false;
  }

  // Get the base name end position
  size_t headerSplitIndex = columnName.find_first_of(':');

  // If only one foreign key, check for optional foreign table column name
  int32_t foundIndex = -1;
  if (foreignTable.m_keyColumns.size() == 1 && headerSplitIndex == std::string::npos)
  {
    // If only the base name, 
    foundIndex = foreignTable.m_keyColumns[0];
  }
  else
  {
    // Find each base name+ foreign key name
    std::string_view findName(columnName);
    findName = findName.substr(headerSplitIndex + 1);
    for (uint32_t foreignKeyColumn : foreignTable.m_keyColumns)
    {
      if (foreignTable.m_headerData[foreignKeyColumn].m_name == findName)
      {
        foundIndex = foreignKeyColumn;
        break;
      }
    }
  }
  if (foundIndex < 0)
  {
    OUTPUT_MESSAGE("Error: Column {} has link to table {} without key", columnName, foreignTableName);
    return false;
  }
  if (foreignTable.m_headerData[foundIndex].m_foreignTable.size() > 0)
  {
    return FindSourceHeaderColumn(foreignTable.m_headerData[foundIndex].m_name, foreignTable.m_headerData[foundIndex].m_foreignTable, tables, outTableName, outField);
  }

  outTableName = foreignTableName;
  outField = foreignTable.m_headerData[foundIndex].m_type;

  return true;
}

bool ValidateTables(const std::unordered_map<std::string, CSVTable>& tables)
{
  // Check that the table references match up
  std::vector<bool> processed;
  std::vector<uint32_t> matchIndices;
  std::string searchName;
  for (const auto& [tableName, table] : tables)
  {
    // Reset the processed array
    processed.resize(0);
    processed.resize(table.m_headerData.size());

    // Check the table for foreign links
    for (uint32_t h = 0; h < table.m_headerData.size(); h++)
    {
      // Check if this column is already processed or does not have a foreign table link
      const CSVHeader& header = table.m_headerData[h];
      if (processed[h] || header.m_foreignTable.size() == 0)
      {
        continue;
      }

      // Check foreign table exists
      auto findTable = tables.find(header.m_foreignTable);
      if (findTable == tables.end())
      {
        OUTPUT_MESSAGE("Error: Table {} has link to unknown table {}", tableName, header.m_foreignTable);
        return false;
      }

      const CSVTable& foreignTable = findTable->second;
      if (foreignTable.m_keyColumns.size() == 0)
      {
        OUTPUT_MESSAGE("Error: Table {} has link to table {} with no keys", tableName, header.m_foreignTable);
        return false;
      }

      // Get the base name end position
      size_t headerSplitIndex = header.m_name.find_first_of(':');

      // If only one foreign key, check for optional foreign table column name
      matchIndices.resize(0);
      if (foreignTable.m_keyColumns.size() == 1 && headerSplitIndex == std::string::npos)
      {
        // If only the base name, 
        matchIndices.push_back(h);
      }
      else
      {
        // Find each base name+ foreign key name
        for (uint32_t foreignKeyColumn : foreignTable.m_keyColumns)
        {
          searchName.assign(header.m_name, 0, headerSplitIndex);
          searchName += ":";
          searchName += foreignTable.m_headerData[foreignKeyColumn].m_name;

          int32_t foundIndex = -1;
          for (uint32_t i = 0; i < table.m_headerData.size(); i++)
          {
            if (table.m_headerData[i].m_name == searchName &&
                table.m_headerData[i].m_foreignTable == header.m_foreignTable)
            {
              foundIndex = i;
              break;
            }
          }
          if (foundIndex < 0)
          {
            OUTPUT_MESSAGE("Error: Table {} has link to table {} without key {}", tableName, header.m_foreignTable, searchName);
            return false;
          }
          matchIndices.push_back(foundIndex);
        }
      }

      // Search in the foreign table for each of the keys in the main table
      for (const std::vector<FieldType>& row : table.m_rowData)
      {
        auto findInfo = std::lower_bound(foreignTable.m_rowData.begin(), foreignTable.m_rowData.end(), row,
          [&foreignTable, &matchIndices](const std::vector<FieldType>& a, const std::vector<FieldType>& b)
          {
            for (uint32_t i = 0; i < foreignTable.m_keyColumns.size(); i++)
            {
              const FieldType& aVal = a[foreignTable.m_keyColumns[i]]; // Enum sort issue?
              const FieldType& bVal = b[matchIndices[i]];
              if (aVal < bVal)
              {
                return true;
              }
              if (aVal != bVal)
              {
                break;
              }
            }
            return false;
          });

        auto IsEqual = [&foreignTable, &matchIndices](const std::vector<FieldType>& a, const std::vector<FieldType>& b)
          {
            for (uint32_t i = 0; i < foreignTable.m_keyColumns.size(); i++)
            {
              const FieldType& aVal = a[foreignTable.m_keyColumns[i]];
              const FieldType& bVal = b[matchIndices[i]];
              if (aVal != bVal)
              {
                return false;
              }
            }
            return true;
          };

        if (findInfo == foreignTable.m_rowData.end() ||
            !IsEqual(*findInfo, row))
        {
          std::string errorKeys;
          for (uint32_t index : matchIndices)
          {
            AppendToString(row[index], errorKeys);
            errorKeys += " ";
          }
          OUTPUT_MESSAGE("Error: Table {} has link to table {} with a missing lookup column key {}", tableName, header.m_foreignTable, errorKeys);
          return false;
        }
      }

      // Flag all columns as processed
      for (uint32_t index : matchIndices)
      {
        processed[index] = true;
      }
    }
  }

  return true;
}

void SaveToString(const CSVTable& table, const std::unordered_map<std::string, CSVTable>& tables, std::string_view existingFile, std::string& outFile)
{
  std::string fieldStr;
  outFile.reserve(existingFile.size());

  // Find the first type of newline in the file
  std::string newLine = "\n";
  size_t newLineoffset = existingFile.find_first_of("\n\r", 0, 2);
  if (newLineoffset != std::string::npos)
  {
    newLine = existingFile[newLineoffset];

    // Check for windows style \r\n
    if (existingFile[newLineoffset] == '\r' &&
      (newLineoffset + 1) < existingFile.size() &&
      existingFile[newLineoffset + 1] == '\n')
    {
      newLine += '\n';
    }
  }

  // Write header data
  std::vector<std::string> enumTableHeaders;
  {
    bool firstWrite = true;
    for (const CSVHeader& header : table.m_headerData)
    {
      if (!firstWrite)
      {
        outFile += ",";
      }
      firstWrite = false;
      outFile += header.m_rawField;

      // Get what tables need an enum replacement with the text version
      std::string lookupTable;
      FieldType dummyField;
      if (header.m_foreignTable.size() > 0 &&
          FindSourceHeaderColumn(header.m_name, header.m_foreignTable, tables, lookupTable, dummyField) &&
          IsEnumTable(lookupTable))
      {
        enumTableHeaders.push_back(lookupTable);
      }
      else
      {
        enumTableHeaders.push_back(std::string());
      }
    }
  }
  outFile += newLine;

  // Write each row
  for (const std::vector<FieldType>& row : table.m_rowData)
  {
    // Loop and write the fields
    bool firstWrite = true;
    for(uint32_t i = 0; i < row.size(); i++)
    {
      const FieldType* field = &row[i];

      if (!firstWrite)
      {
        outFile += ",";
      }
      firstWrite = false;

      // If an enum type, lookup the string version
      if (enumTableHeaders[i].size() > 0)
      {
        auto findTable = tables.find(enumTableHeaders[i]);
        if (findTable == tables.end())
        {
          OUTPUT_MESSAGE("Error: Unknown table {}", enumTableHeaders[i]);
          return;
        }
        const CSVTable& enumTable = findTable->second;

        // Find the enum - access name column
        auto findInfo = std::lower_bound(enumTable.m_rowData.begin(), enumTable.m_rowData.end(), row,
          [i](const std::vector<FieldType>& a, const std::vector<FieldType>& b)
          {
            return a[1] < b[i];
          });

        auto IsEqual = [i](const std::vector<FieldType>& a, const std::vector<FieldType>& b)
          {
            return a[1] == b[i];
          };

        if (findInfo == enumTable.m_rowData.end() ||
            !IsEqual(*findInfo, row))
        {
          OUTPUT_MESSAGE("Error: Table has link to table {} with a missing lookup column key {}", enumTableHeaders[i], to_string(row[i]));
          return;
        }
        else
        {
          field = &(*findInfo)[0];
        }
      }

      // Get the field in string form
      if (const std::string* accessField = std::get_if<std::string>(field))
      {
        // If the fields contain a comma or quotes, put in quotes
        size_t quoteOffset = accessField->find_first_of('"');
        if (quoteOffset != std::string::npos ||
            accessField->find_first_of(',') != std::string::npos)
        {
          fieldStr = *accessField;
          outFile += "\"";

          // Replace all single quotes with double quotes // DT_TODO: Test this!
          while (quoteOffset != std::string::npos)
          {
            fieldStr.replace(quoteOffset, 1, 2, '"');
            quoteOffset = fieldStr.find_first_of('"', quoteOffset + 2);
          }

          outFile += fieldStr;
          outFile += "\"";
        }
        else
        {
          // Add raw unmodified string
          outFile += *accessField;
        }
      }
      else
      {
        // Add number type
        AppendToString(*field, outFile);
      }
    }
    outFile += newLine;
  }

  // Remove the newline if the existing file does not have it
  if (!existingFile.ends_with(newLine))
  {
    outFile.erase(outFile.size() - newLine.size());
  }
}

bool CalculateTableDepth(const std::string& tableName, const std::unordered_map<std::string, CSVTable>& tables, std::unordered_map<std::string, uint32_t>& tableDepths, uint32_t& depth)
{
  // Enum tables have no depth
  if (IsEnumTable(tableName))
  {
    return true;
  }

  // See if already processed
  auto existingDepth = tableDepths.find(tableName);
  if (existingDepth != tableDepths.end())
  {
    depth = existingDepth->second;

    // Check for a loop in the tree of table links
    if (depth == std::numeric_limits<uint32_t>::max())
    {
      OUTPUT_MESSAGE("Error: Table {} has loop to table {}", tableName, tableName);
      return false;
    }

    return true;
  }

  // Reserve the depth slot
  tableDepths[tableName] = std::numeric_limits<uint32_t>::max();

  const CSVTable& table = tables.find(tableName)->second;

  for (const CSVHeader& header : table.m_headerData)
  {
    if (header.m_foreignTable.size() > 0 && !header.m_isWeakForeignTable)
    {
      // Recurse
      uint32_t newDepth = 0;
      if (!CalculateTableDepth(header.m_foreignTable, tables, tableDepths, newDepth))
      {
        return false;
      }

      // Check if this is a new max depth
      if ((newDepth + 1) > depth)
      {
        depth = newDepth + 1;
      }
    }
  }

  tableDepths[tableName] = depth;
  return true;
}

bool ReadToString(const std::filesystem::path& path, std::string& outStr)
{
  // Open the file in binary mode to avoid newline conversions
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open())
  {
    OUTPUT_MESSAGE("Error: Unable to open file {}", path.string());
    return false;
  }

  // Seek to the end to determine file size
  file.seekg(0, std::ios::end);
  size_t fileSize = file.tellg();
  file.seekg(0, std::ios::beg);

  // Read into a buffer
  outStr.resize(fileSize);

  if (!file.read(outStr.data(), fileSize))
  {
    OUTPUT_MESSAGE("Error: Unable to read file contents {}", path.string());
    return false;
  }
  file.close();
  return true;
}

int main(int argc, char* argv[])
{
  // Check if directory path is provided
  if (argc < 2)
  {
    OUTPUT_MESSAGE("Usage: CSVProcessor <directory_path> <optional_output_path>");
    return 1;
  }

  // Get directory path from command line
  const char* dirPath = argv[1];

  const char* outputPathStr = nullptr;
  if (argc >= 3)
  {
    outputPathStr = argv[2];
  }

  // Check if directory exists
  std::error_code error;
  std::filesystem::file_status dirPathStatus = std::filesystem::status(dirPath, error);
  if (!std::filesystem::is_directory(dirPathStatus))
  {
    OUTPUT_MESSAGE("Error: {} is not a valid directory", dirPath);
    return 1;
  }

  // Iterate through files in directory
  std::unordered_map<std::string, CSVTable> tables;
  std::unordered_map<std::string, CSVTable> tablesEnumRaw; // Unsorted raw enum tables
  std::unordered_map<std::string, CSVTable> tablesEnumNameSort; // Sorted by name enum tables

  std::vector<std::filesystem::path> csvEnumFilePaths;
  std::vector<std::filesystem::path> csvFilePaths;

  // Get the paths of the files sorted into enum tables and regular tables
  for (const auto& entry : std::filesystem::directory_iterator(dirPath))
  {
    // Check if file has .csv extension
    std::string extension = entry.path().extension().string();
    if (entry.is_regular_file() &&
      extension.size() == 4 &&
      std::tolower(extension[0]) == '.' &&
      std::tolower(extension[1]) == 'c' &&
      std::tolower(extension[2]) == 's' &&
      std::tolower(extension[3]) == 'v')
    {
      std::string tableName = entry.path().stem().string();

      if (IsEnumTable(tableName))
      {
        csvEnumFilePaths.push_back(entry.path());
      }
      else
      {
        csvFilePaths.push_back(entry.path());
      }
    }
  }

  // Process enums first as they swap their key column to be based on values
  std::string csvFileData;
  for (const auto& path : csvEnumFilePaths)
  {
    std::string tableName = path.stem().string();
    if (!ReadToString(path, csvFileData))
    {
      return 1;
    }

    if (tables.contains(tableName))
    {
      OUTPUT_MESSAGE("Error: Duplicate table name {}", tableName);
      return 1;
    }

    // Read in the table data from the file
    CSVTable newTable;
    if (!ReadTable(csvFileData.data(), newTable))
    {
      OUTPUT_MESSAGE("Error: Reading table {}", tableName);
      return 1;
    }

    // Enums have a strict layout
    if (newTable.m_headerData.size() != 3 ||
      !newTable.m_headerData[0].m_isKey ||
      newTable.m_headerData[1].m_isKey ||
      newTable.m_headerData[2].m_isKey ||
      newTable.m_headerData[0].m_foreignTable.size() != 0 ||
      newTable.m_headerData[1].m_foreignTable.size() != 0 ||
      newTable.m_headerData[2].m_foreignTable.size() != 0 ||
      newTable.m_headerData[0].m_name != "Name" ||
      newTable.m_headerData[1].m_name != "Value")
    {
      OUTPUT_MESSAGE("Error: Enum table {} need three columns, single key and no foreign table links", tableName);
      return 1;
    }

    // Store a copy of the raw table before sorting
    tablesEnumRaw[tableName] = newTable;

    // Sort the table data by column and check for duplicates
    if (!SortTable(newTable))
    {
      OUTPUT_MESSAGE("Error: Enum table {} failed to sort", tableName);
      return 1;
    }

    tablesEnumNameSort[tableName] = newTable;

    // Swap the key row and re-sort (needs to be sorted by number value)
    newTable.m_headerData[0].m_isKey = false;
    newTable.m_headerData[1].m_isKey = true;
    newTable.m_keyColumns.resize(0);
    newTable.m_keyColumns.push_back(1);
    if (!SortTable(newTable))
    {
      OUTPUT_MESSAGE("Error: Enum table {} failed to sort by value", tableName);
      return 1;
    }

    // Add to a map of all the csv files
    tables[tableName] = std::move(newTable);
  }

  for (const auto& path : csvFilePaths)
  {
    std::string tableName = path.stem().string();
    if (!ReadToString(path, csvFileData))
    {
      return 1;
    }
    if (tables.contains(tableName))
    {
      OUTPUT_MESSAGE("Error: Duplicate table name {}", tableName);
      return 1;
    }

    // Read in the table data from the file
    CSVTable newTable;
    if (!ReadTable(csvFileData.data(), newTable))
    {
      OUTPUT_MESSAGE("Error: Reading table {}", tableName);
      return 1;
    }

    // Check that Global and Enum tables have the correct format
    if (IsGlobalTable(tableName) && newTable.m_rowData.size() != 1)
    {
      OUTPUT_MESSAGE("Error: Global table {} can only have one row - has {}", tableName, newTable.m_rowData.size());
      return 1;
    }

    // Add to a map of all the csv files
    tables[tableName] = std::move(newTable);
  }

  // Follow all foreign table links and get the correct types for columns (enums go to the value type)
  for (auto& [tableName, table] : tables)
  {
    // Check the table for foreign links
    for (uint32_t h = 0; h < table.m_headerData.size(); h++)
    {
      CSVHeader& header = table.m_headerData[h];
      if (header.m_foreignTable.size() == 0)
      {
        continue;
      }

      // Find the ultimate source of the column (DT_TODO check for header loops)
      std::string finalTableName;
      FieldType newType;
      if (!FindSourceHeaderColumn(header.m_name, header.m_foreignTable, tables, finalTableName, newType))
      {
        OUTPUT_MESSAGE("Error: Table {} has link issues with {}", tableName, header.m_name);
        return 1;
      }

      // If a foreign key is an enum table
      if (IsEnumTable(finalTableName))
      {
        // Get the lookup table
        auto enumTableIter = tablesEnumNameSort.find(finalTableName);
        if (enumTableIter == tablesEnumNameSort.end())
        {
          OUTPUT_MESSAGE("Error: Unable to find linked enum table {} for table {}", finalTableName, tableName);
          return 1;
        }
        const CSVTable& enumTable = enumTableIter->second;

        // Loop for all rows
        header.m_type = enumTable.m_headerData[1].m_type;
        for (std::vector<FieldType>& row : table.m_rowData)
        {
          auto findInfo = std::lower_bound(enumTable.m_rowData.begin(), enumTable.m_rowData.end(), row,
            [h](const std::vector<FieldType>& a, const std::vector<FieldType>& b)
            {
              return a[0] < b[h];
            });

          auto IsEqual = [h](const std::vector<FieldType>& a, const std::vector<FieldType>& b)
            {
              return a[0] == b[h];
            };

          if (findInfo == enumTable.m_rowData.end() ||
            !IsEqual(*findInfo, row))
          {
            OUTPUT_MESSAGE("Error: Table {} has link to table {} with a missing lookup column key {}", tableName, header.m_foreignTable, to_string(row[h]));
            return 1;
          }

          // Swap the name for the integer
          row[h] = (*findInfo)[1];
        }
      }
      // Only convert if the new type is not already a string
      else if(!std::holds_alternative<std::string>(newType))
      {
        header.m_type = newType;
        for (std::vector<FieldType>& row : table.m_rowData)
        {
          // Should always be a string here
          if (const std::string* accessField = std::get_if<std::string>(&row[h]))
          {
            if (!ParseField(header.m_type, *accessField, newType))
            {
              OUTPUT_MESSAGE("Error: Table has bad data in column {} - {}", header.m_name, *accessField);
              return 1;
            }
            row[h] = newType;
          }
        }
      }
    }
  }

  // Sort the table data by column and check for duplicates
  for (auto& [tableName, table] : tables)
  {
    if (!SortTable(table))
    {
      OUTPUT_MESSAGE("Error: Table {} failed to sort", tableName);
      return 1;
    }
  }

  // Validate tables
  if (!ValidateTables(tables))
  {
    return 1;
  }

  // DT_TODO: Add command line for resave of all tables
  for (const auto& path : csvFilePaths)
  {
    std::string tableName = path.stem().string();
    if (!ReadToString(path, csvFileData))
    {
      return 1;
    }

    std::string_view existingFile(csvFileData.data(), csvFileData.size() - 1);

    std::string outFile;
    SaveToString(tables[tableName], tables, existingFile, outFile);

    // Check if the file data has changed and re-save it if it has
    if (existingFile != outFile)
    {
      std::ofstream file(path, std::ios::binary);
      if (!file.is_open())
      {
        OUTPUT_MESSAGE("Error: Unable to open file for writing {}", path.string());
        return 1;
      }

      if (!file.write(outFile.data(), outFile.size()))
      {
        OUTPUT_MESSAGE("Error: Unable to write file contents {}", path.string());
        return 1;
      }
      file.close();
    }
  }

  if (outputPathStr)
  {
    std::filesystem::path outputPath(outputPathStr);
    std::error_code error;
    std::filesystem::file_status dirPathStatus = std::filesystem::status(outputPath, error);
    if (!std::filesystem::is_directory(dirPathStatus))
    {
      OUTPUT_MESSAGE("Error: {} is not a valid directory", outputPathStr);
      return 1;
    }

    // Add the common header
    std::string outHeaderString = s_commonHeaderStart;
    std::string outBodyString = s_commonBodyStart;

    // Write out all enum types // DT_TODO: Sort enums by table name for consistency in output?
    for (const auto& [tableName, rawTable] : tablesEnumRaw)
    {
      if (rawTable.m_rowData.size() > 0 && rawTable.m_headerData.size() == 3)
      {
        std::string enumName = tableName.substr(4);
        outHeaderString += "enum class " + enumName + " : " + CPPTypeString(rawTable.m_headerData[1].m_type) + "\n{\n";
        size_t enumCounter = 0;
        bool isSequential = true;
        for (const std::vector<FieldType>& row : rawTable.m_rowData)
        {
          outHeaderString += "  ";
          AppendToString(row[0], outHeaderString);
          outHeaderString += " = ";
          AppendToString(row[1], outHeaderString);
          outHeaderString += ",";

          // Check if a sequential enum
          if (isSequential && !IsEqual(row[1], enumCounter))
          {
            isSequential = false;
          }
          enumCounter++;

          if (const std::string* accessField = std::get_if<std::string>(&row[2]))
          {
            if (accessField->size() > 0)
            {
              outHeaderString += " // ";
              outHeaderString += *accessField;
            }
          }
          outHeaderString += "\n";
        }
        outHeaderString += "};\n";

        // Find if the enum starts at 0 and ascends by one each time
        if (isSequential)
        {
          outHeaderString += "constexpr uint32_t " + enumName + "_MAX = " + to_string(enumCounter) + "; // For using the enum in lookup arrays\n";
        }

        outHeaderString += "const char* to_string(" + enumName + " value);\n";
        outHeaderString += "bool find_enum(std::string_view name, " + enumName + "& out);\n";
        
        // Write out functions in cpp file
        outBodyString += "const char* DB::to_string(" + enumName +" value)\n{\n";
        outBodyString += "  switch (value)\n  {\n";

        // Do to string lookups
        for (const std::vector<FieldType>& row : rawTable.m_rowData)
        {
          outBodyString += "  case(" + enumName + "::";
          AppendToString(row[0], outBodyString);
          outBodyString += "): return \"";
          AppendToString(row[0], outBodyString);
          outBodyString += "\";\n";
        }
        outBodyString += "  }\n  return \"\";\n}\n\n";

        // Create an array sorted by name to do a lookup
        std::vector<std::string> sortedNames;
        for (const std::vector<FieldType>& row : rawTable.m_rowData)
        {
          sortedNames.emplace_back(to_string(row[0]));
        }
        std::sort(sortedNames.begin(), sortedNames.end());

        outBodyString += "bool DB::find_enum(std::string_view name, " + enumName + "& out)\n{\n";
        outBodyString += "  std::string_view names[] =\n  {\n";
        for (const auto& name : sortedNames)
        {
          outBodyString += "    \"";
          outBodyString += name;
          outBodyString += "\",\n";
        }
        outBodyString += "  };\n";

        outBodyString += "  " + enumName + " values[] =\n  {\n";
        for (const auto& name : sortedNames)
        {
          outBodyString += "    ";
          outBodyString += enumName;
          outBodyString += "::";
          outBodyString += name;
          outBodyString += ",\n";
        }
        outBodyString += "  };\n";

        outBodyString += "  auto lowerBound = std::lower_bound(std::begin(names), std::end(names), name);\n";
        outBodyString += "  if (lowerBound == std::end(names) ||\n";
        outBodyString += "      *lowerBound != name)\n";
        outBodyString += "  {\n";
        outBodyString += "    out = " + enumName + "::";
        AppendToString(rawTable.m_rowData[0][0], outBodyString);
        outBodyString += ";\n";
        outBodyString += "    return false;\n";
        outBodyString += "  }\n";
        outBodyString += "  out = values[std::distance(std::begin(names), lowerBound)];\n";
        outBodyString += "  return true;\n";
        outBodyString += "}\n";
      }
    }

    // Sort table names based on the reference order
    std::unordered_map<std::string, uint32_t> tableDepths;
    for (const auto& [tableName, table] : tables)
    {
      uint32_t depth = 0;
      if (!CalculateTableDepth(tableName, tables, tableDepths, depth))
      {
        OUTPUT_MESSAGE("Error: {} Recursive table link - Use \"*TableName\" instead of +TabeName on one link", tableName);
        return 1;
      }
    }

    // Sort by count then by name
    std::vector<std::tuple< uint32_t, std::string>> tableOrdering;
    for (const auto& [tableName, order] : tableDepths)
    {
      tableOrdering.emplace_back(order, tableName);
    }
    std::sort(tableOrdering.begin(), tableOrdering.end());

    // Write out each table
    std::vector<std::string> writtenLinks;
    for (const auto& [_, tableName] : tableOrdering)
    {
      const CSVTable& writeTable = tables[tableName];

      //DT_TODO: Write pre-declare loop reference count types (if not "this" type)

      outHeaderString += "\nclass " + tableName + "\n{\npublic:\n";
      outHeaderString += "  using ID = IDType<" + tableName + ">;\n";
      outHeaderString += "  using Iter = const IterType<" + tableName + ">::Data;\n\n";

      writtenLinks.resize(0);
      for (const CSVHeader& header : writeTable.m_headerData)
      {
        // Test if a table link
        if (header.m_foreignTable.size() > 0)
        {
          if (IsEnumTable(header.m_foreignTable))
          {
            std::string enumName = header.m_foreignTable.substr(4);
            outHeaderString += "  ";
            outHeaderString += enumName;
            outHeaderString += " ";
            outHeaderString += header.m_name;
            outHeaderString += " = ";
            outHeaderString += enumName;
            outHeaderString += "::";
            AppendToString(tablesEnumRaw[header.m_foreignTable].m_rowData[0][0], outHeaderString);
            outHeaderString += ";\n";
          }
          else
          {
            // Check if the link has already been processed
            std::string newLinkName = header.m_name.substr(0, header.m_name.find_first_of(':'));
            if (std::find(writtenLinks.begin(), writtenLinks.end(), newLinkName) == writtenLinks.end())
            {
              writtenLinks.push_back(newLinkName);

              outHeaderString += "  ";
              outHeaderString += header.m_foreignTable;
              outHeaderString += "::ID ";
              outHeaderString += newLinkName;
              outHeaderString += ";\n";
            }
          }
        }
        else
        {
          outHeaderString += "  ";
          outHeaderString += CPPTypeString(header.m_type);
          outHeaderString += " ";
          outHeaderString += header.m_name;

          if (const std::string* accessField = std::get_if<std::string>(&(header.m_type)))
          {
          }
          else if (const bool* accessField = std::get_if<bool>(&(header.m_type)))
          {
            outHeaderString += " = false"; // Perhaps set a value based on min / max ?
          }
          else
          {
            outHeaderString += " = 0";
          }
          outHeaderString += ";\n";
        }
      }
      outHeaderString += "};\n";
    }

    // Write the main database table
    outHeaderString += "\nclass DB\n{\npublic:\n\n";

    outHeaderString += "  template<typename T> const std::vector<T>& GetTable() const;\n";
    outHeaderString += "  template<typename T> IterType<T> Iter() const { return IterType<T>(GetTable<T>()); }\n";
    outHeaderString += "  template<typename T> const T& Get(IDType<T> id) const { return GetTable<T>()[id.m_dbIndex]; }\n";
    outHeaderString += "  template<typename T> bool ToID(uint32_t index, IDType<T>& id) const { if (index < GetTable<T>().size()) { id = IDType<T>(index); return true; } return false; }\n\n";

    // Add Find() methods - type string, param name string, member name string
    std::vector<std::tuple<std::string, std::string, std::string>> params;
    for (const auto& [_, tableName] : tableOrdering)
    {
      if (IsGlobalTable(tableName))
      {
        continue;
      }
      const CSVTable& table = tables[tableName];

      writtenLinks.resize(0);
      params.resize(0);
      for (uint32_t columnID : table.m_keyColumns)
      {
        const CSVHeader& header = table.m_headerData[columnID];
        std::string writeHeaderName = header.m_name;
        writeHeaderName[0] = std::tolower(writeHeaderName[0]);

        // Test if a table link
        if (header.m_foreignTable.size() > 0)
        {
          if (IsEnumTable(header.m_foreignTable))
          {
            params.emplace_back(header.m_foreignTable.substr(4), writeHeaderName, header.m_name);
          }
          else
          {
            // Check if the link has already been processed
            std::string newLinkName = header.m_name.substr(0, header.m_name.find_first_of(':'));
            if (std::find(writtenLinks.begin(), writtenLinks.end(), newLinkName) == writtenLinks.end() && newLinkName.size() > 0)
            {
              writtenLinks.push_back(newLinkName);
              writeHeaderName = newLinkName;
              writeHeaderName[0] = std::tolower(writeHeaderName[0]);
              params.emplace_back(header.m_foreignTable + "::ID", writeHeaderName, newLinkName);
            }
          }
        }
        else
        {
          if (const std::string* accessField = std::get_if<std::string>(&(header.m_type)))
          {
            params.emplace_back("std::string_view", writeHeaderName, header.m_name);
          }
          else
          {
            params.emplace_back(CPPTypeString(header.m_type), writeHeaderName, header.m_name);
          }
        }
      }

      outHeaderString += "  bool Find(";
      for (auto& [type, name, _] : params)
      {
        outHeaderString += type;
        outHeaderString += " ";
        outHeaderString += name;
        outHeaderString += ", ";
      }
      outHeaderString += tableName;
      outHeaderString += "::ID& _ret) const;\n";

      outBodyString += "\nbool DB::DB::Find(";
      for (auto& [type, name, _] : params)
      {
        outBodyString += type;
        outBodyString += " ";
        outBodyString += name;
        outBodyString += ", ";
      }
      outBodyString += tableName;
      outBodyString += "::ID& _ret) const\n{\n";

      outBodyString += "  auto _searchLowerBound = std::lower_bound(" + tableName + "Values.begin(), " + tableName + "Values.end(), 0, [&](const " + tableName + "& left, int)\n  {\n";

      outBodyString += "    return ";
      std::string equalStr;
      for (auto& [type, name, member] : params)
      {
        if (equalStr.size() != 0)
        {
          outBodyString += " ||\n           ";
        }
        outBodyString += "(" + equalStr + "left." + member + " < " + name + ")";
        equalStr += "left." + member + " == " + name + " && ";
      }
      outBodyString += ";\n  });\n";

      outBodyString += "  if (_searchLowerBound == " + tableName + "Values.end()";
      for (auto& [type, name, member] : params)
      {
        outBodyString += " ||\n      _searchLowerBound->" + member + " != " + name;
      }
      outBodyString += ")\n  {\n";
      outBodyString += "    _ret = " + tableName + "::ID(0);\n";
      outBodyString += "    return false;\n";
      outBodyString += "  }\n";


      outBodyString += "  _ret = " + tableName + "::ID((uint32_t)std::distance(" + tableName + "Values.begin(), _searchLowerBound));\n";
      outBodyString += "  return true;\n}\n";
    }
    outHeaderString += "\n"; 

    for (const auto& [_, tableName] : tableOrdering)
    {
      if (IsGlobalTable(tableName))
      {
        outHeaderString += "  " + tableName + " " + tableName + "Values;\n";
      }
      else
      {
        outHeaderString += "  std::vector<" + tableName + "> " + tableName + "Values;\n";
      }
    }

    outHeaderString += "};\n";

    for (const auto& [_, tableName] : tableOrdering)
    {
      if (!IsGlobalTable(tableName))
      {
        outHeaderString += "template<> inline const std::vector<" + tableName + ">& DB::GetTable() const { return " + tableName + "Values; }\n";
      }
    }

    outHeaderString += s_commonHeaderEnd;
    outBodyString += s_commonBodyEnd;

    // DT_TODO: Read existing header, compare and only overwrite if different
    {
      std::filesystem::path outputPathHeader = outputPath / "DB.h";
      std::ofstream headerFile(outputPathHeader, std::ios::binary);
      if (!headerFile.is_open())
      {
        OUTPUT_MESSAGE("Error: Unable to open file for writing {}", outputPathHeader.string());
        return 1;
      }

      if (!headerFile.write(outHeaderString.data(), outHeaderString.size()))
      {
        OUTPUT_MESSAGE("Error: Unable to write to file {}", outputPathHeader.string());
        return 1;
      }
    }
    {
      std::filesystem::path outputPathBody = outputPath / "DB.cpp";
      std::ofstream bodyFile(outputPathBody, std::ios::binary);
      if (!bodyFile.is_open())
      {
        OUTPUT_MESSAGE("Error: Unable to open file for writing {}", outputPathBody.string());
        return 1;
      }

      if (!bodyFile.write(outBodyString.data(), outBodyString.size()))
      {
        OUTPUT_MESSAGE("Error: Unable to write to file {}", outputPathBody.string());
        return 1;
      }
    }

    // Add code gen of runtime types

    // Write out common DB util header

      // OverrideIfDifferent(std::string_view newContents, std::string& workingBuffer, const std::filesystem::path& writePath);


       // Handle Enum tables

       // Handle Global tables
  }

  return 0;
}


/*

*/
