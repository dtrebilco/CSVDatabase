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

#define OUTPUT_MESSAGE(...) { char buf[512]; const auto out = std::format_to_n(buf, std::size(buf) - 1, __VA_ARGS__); *out.out = '\0'; printf("%s\n", buf); }

enum class ColumnType
{
  Unknown = 0,

  String,
  Bool,
  Int8,
  UInt8,
  Int16,
  UInt16,
  Int32,
  UInt32,
  Int64,
  UInt64,
  Float32,
  Float64,

  ColumnType_Count
};

ColumnType GetColumnType(std::string_view name)
{
  if (name == "string") { return ColumnType::String; }
  if (name == "bool") { return ColumnType::Bool; }

  if (name == "int8") { return ColumnType::Int8; }
  if (name == "int16") { return ColumnType::Int16; }
  if (name == "int23") { return ColumnType::Int32; }
  if (name == "int64") { return ColumnType::Int64; }

  if (name == "uint8") { return ColumnType::UInt8; }
  if (name == "uint16") { return ColumnType::UInt16; }
  if (name == "uint23") { return ColumnType::UInt32; }
  if (name == "uint64") { return ColumnType::UInt64; }

  if (name == "float32") { return ColumnType::Float32; }
  if (name == "float64") { return ColumnType::Float64; }

  static_assert((int)ColumnType::ColumnType_Count == 13, "Update lookup");

  return ColumnType::Unknown;
}

using FieldType = std::variant<std::string, bool, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double>;

std::string to_string(const FieldType& var)
{
  return std::visit([]<typename T>(const T & e)
  {
    if constexpr (std::is_same_v<T, std::string>)
    {
      return e;
    }
    else // float/int
    { 
      return std::to_string(e);
    }
  }, var);
}

bool ParseField(ColumnType type, std::string_view string, FieldType& retField)
{
  if (type == ColumnType::String)
  {
    retField = std::string(string);
    return true;
  }

  std::from_chars_result strRes;

  const char* start = string.data();
  const char* end = start + string.size();
  switch (type)
  {
  case(ColumnType::Bool):
  {
    int8_t val = 0;
    strRes = std::from_chars(start, end, val);
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
      OUTPUT_MESSAGE("Error: String type bool has error converting data \"{}\"", string);
      return false;
    }
    break;
  }

  case(ColumnType::Int8):
  {
    int8_t val = 0;
    strRes = std::from_chars(start, end, val);
    retField = val;
    break;
  }
  case(ColumnType::UInt8):
  {
    uint8_t val = 0;
    strRes = std::from_chars(start, end, val);
    retField = val;
    break;
  }
  case(ColumnType::Int16):
  {
    int16_t val = 0;
    strRes = std::from_chars(start, end, val);
    retField = val;
    break;
  }
  case(ColumnType::UInt16):
  {
    uint16_t val = 0;
    strRes = std::from_chars(start, end, val);
    retField = val;
    break;
  }

  case(ColumnType::Int32): 
  { 
    int32_t val = 0;
    strRes = std::from_chars(start, end, val);
    retField = val;
    break; 
  }
  case(ColumnType::UInt32):
  {  
    uint32_t val = 0;
    strRes = std::from_chars(start, end, val);
    retField = val;
    break;
  }

  case(ColumnType::Int64): 
  { 
    int64_t val = 0;
    strRes = std::from_chars(start, end, val);
    retField = val;
    break; 
  }
  case(ColumnType::UInt64):
  { 
    uint64_t val = 0;
    strRes = std::from_chars(start, end, val);
    retField = val;
    break;
  }

  case(ColumnType::Float32):
  { 
    float val = 0;
    strRes = std::from_chars(start, end, val);
    retField = val;
    break;
  }
  case(ColumnType::Float64): 
  { 
    double val = 0;
    strRes = std::from_chars(start, end, val);
    retField = val;
    break;
  }
  default:
    OUTPUT_MESSAGE("Error: String type {} is unknown for \"{}\"", (int)type, string);
    return false;
  }

  if (strRes.ptr != end)
  {
    OUTPUT_MESSAGE("Error: String type {} has excess data \"{}\"", (int)type, string);
    return false;
  }

  if (strRes.ec != std::errc())
  {
    OUTPUT_MESSAGE("Error: String type {} has error converting data \"{}\"", (int)type, string);
    return false;
  }

  return true;
}

bool ParseFieldMove(ColumnType type, std::string&& string, FieldType& retField)
{
  if (type == ColumnType::String)
  {
    retField = std::move(string);
    return true;
  }

  return ParseField(type, string, retField);
}

struct CSVHeader
{
  std::string m_name;        // Column name
  ColumnType m_type = ColumnType::Unknown;// Column type
  bool m_isKey = false;      // Is table key
  bool m_isIgnored = false;  // Is ignored

  std::string m_minValue;    // The min range string value (if any)
  std::string m_maxValue;    // The max range string value (if any)

  std::string m_comment;     // Comment field (if any)
  std::string m_foreignTable;// Foreign table link (if any)
};

struct CSVTable
{
  std::vector<CSVHeader> m_headerData; // Header data that is info for each column
  std::vector<uint32_t> m_keyColumns;  // Index of the columns that are keys in the table

  std::vector<std::vector<FieldType>> m_rowData;
};

std::unordered_map<std::string, CSVTable> g_tables;


std::vector<std::vector<std::string>> readCSV(const char* srcData) 
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

bool ReadHeader(const std::string& field, CSVHeader& out)
{
  out = CSVHeader{};

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
        ColumnType newType = GetColumnType(tag);
        if (newType != ColumnType::Unknown)
        {
          if (out.m_type != ColumnType::Unknown)
          {
            OUTPUT_MESSAGE("Duplicate types in field {}", field);
            return false;
          }
          out.m_type = newType;
        }
        else
        {
          OUTPUT_MESSAGE("Unknown tag? {}", tag);
        }
      }

      tags = tags.substr(tag.size());
    }

  } while (tags.size() > 0);

  // If there is no type assigned, assign string
  if (out.m_type == ColumnType::Unknown)
  {
    out.m_type = ColumnType::String;
  }

  // If ignored, reset values
  if (out.m_isIgnored)
  {
    out.m_isKey = false;
    out.m_type = ColumnType::String;
    out.m_foreignTable.clear();
    out.m_comment.clear();
    out.m_minValue.clear();
    out.m_maxValue.clear();
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
  std::vector<std::vector<std::string>> csvData = readCSV(fileString);
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
      uniqueCheck.insert(header.m_name);
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

      if (header.m_type != ColumnType::String)
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

  // Sort by the keys, check if duplicate rows
  if (newTable.m_keyColumns.size() > 0 && newTable.m_rowData.size() > 1)
  {
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
          errorKeys += to_string(curr[index]) + " ";
        }
        OUTPUT_MESSAGE("Error: Table has duplicate keys {}", errorKeys);
        return false;
      }
    }
  }

  return true;
}

bool ValidateTables()
{
  // Check that the table references match up
  std::vector<bool> processed;
  std::vector<uint32_t> matchIndices;
  std::string searchName;
  for (const auto& [tableName, table] : g_tables)
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
      if (!g_tables.contains(header.m_foreignTable))
      {
        OUTPUT_MESSAGE("Error: Table {} has link to unknown table {}", tableName, header.m_foreignTable);
        return false;
      }

      const CSVTable& foreignTable = g_tables[header.m_foreignTable];
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
            OUTPUT_MESSAGE("Error: Table {} has link to table {} with out key {}", tableName, header.m_foreignTable, searchName);
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
              const FieldType& aVal = a[foreignTable.m_keyColumns[i]];
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
            errorKeys += to_string(row[index]) + " ";
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

int main(int argc, char* argv[])
{
  // Check if directory path is provided
  if (argc != 2)
  {
    OUTPUT_MESSAGE("Usage: CSVProcessor <directory_path>");
    return 1;
  }

  // Get directory path from command line
  const char* dirPath = argv[1];

  // Check if directory exists
  std::error_code error;
  std::filesystem::file_status dirPathStatus = std::filesystem::status(dirPath, error);
  if (!std::filesystem::exists(dirPath) ||
      !std::filesystem::is_directory(dirPathStatus))
  {
    OUTPUT_MESSAGE("Error: {} is not a valid directory", dirPath);
    return 1;
  }

  // Iterate through files in directory
  std::vector<char> csvFileData;
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
      // Open the file in binary mode to avoid newline conversions
      {
        std::ifstream file(entry.path(), std::ios::binary);
        if (!file.is_open())
        {
          OUTPUT_MESSAGE("Error: Unable to open file {}", entry.path().string().c_str());
          return 1;
        }

        // Seek to the end to determine file size
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        // Read into a buffer
        csvFileData.resize(fileSize + 1);
        csvFileData[fileSize] = 0; // Add null terminator

        file.read(csvFileData.data(), fileSize);
        if (!file)
        {
          OUTPUT_MESSAGE("Error: Unable to read file contents {}", entry.path().string().c_str());
          return 1;
        }
        file.close();
      }
      std::string tableName = entry.path().stem().string();

      // Read in the table data from the file
      CSVTable newTable;
      if (!ReadTable(csvFileData.data(), newTable))
      {
        OUTPUT_MESSAGE("Error: Reading table {}", tableName);
        return 1;
      }

      // Add to a hashmap of all the csv files
      g_tables[tableName] = std::move(newTable);
    }
  }

  // Validate tables
  if (!ValidateTables())
  {
    return 1;
  }

  // If checking table layout - format and compare against source

  // Add code gen of runtime types

     // Handle enum tables

     // Handle global tables
       // Check that there is only one row

  return 0;
}


// TODO: Add test where the key is a foreign key - part of a multi key also
