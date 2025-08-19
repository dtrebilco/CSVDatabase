#include <iostream>
#include <fstream>
#include <filesystem>
#include <format>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <charconv>

#define OUTPUT_MESSAGE(...) { char buf[256]; const auto out = std::format_to_n(buf, std::size(buf) - 1, __VA_ARGS__); *out.out = '\0'; printf("%s\n", buf); }

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

union NumberType
{
  bool Bool;
  
  int8_t Int8;
  uint8_t UInt8;
  
  int16_t Int16;
  uint16_t UInt16;
  
  int32_t Int32;
  uint32_t UInt32;
  
  int64_t Int64;
  uint64_t UInt64;

  float Float32;
  double Float64;
};

bool ParseNumber(ColumnType type, std::string_view string, NumberType& retNumber)
{
  std::from_chars_result strRes;

  const char* start = string.data();
  const char* end = start + string.size();
  switch (type)
  {
  case(ColumnType::Bool):    strRes = std::from_chars(start, end, retNumber.Int8); break; // Using Int8 for bool as temp measure

  case(ColumnType::Int8):    strRes = std::from_chars(start, end, retNumber.Int8); break;
  case(ColumnType::UInt8):   strRes = std::from_chars(start, end, retNumber.UInt8); break;

  case(ColumnType::Int16):   strRes = std::from_chars(start, end, retNumber.Int16); break;
  case(ColumnType::UInt16):  strRes = std::from_chars(start, end, retNumber.UInt16); break;

  case(ColumnType::Int32):   strRes = std::from_chars(start, end, retNumber.Int32); break;
  case(ColumnType::UInt32):  strRes = std::from_chars(start, end, retNumber.UInt32); break;

  case(ColumnType::Int64):   strRes = std::from_chars(start, end, retNumber.Int64); break;
  case(ColumnType::UInt64):  strRes = std::from_chars(start, end, retNumber.UInt64); break;

  case(ColumnType::Float32): strRes = std::from_chars(start, end, retNumber.Float32); break;
  case(ColumnType::Float64): strRes = std::from_chars(start, end, retNumber.Float64); break;
  default:
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

  if (type == ColumnType::Bool)
  {
    if (retNumber.Int8 == 0)
    {
      retNumber.Bool = false;
    }
    else if (retNumber.Int8 == 1)
    {
      retNumber.Bool = true;
    }
    else
    {
      OUTPUT_MESSAGE("Error: String type bool has error converting data \"{}\"", string);
      return false;
    }
  }

  return true;
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
  std::vector<CSVHeader> m_headerData;
  std::vector<std::vector<std::string>> m_rowData;
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

  // Abort if a name is not assigned
  if (out.m_name.size() == 0)
  {
    OUTPUT_MESSAGE("Error: No column header name in ""{}""", field);
    return false;
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

  // Iterate through directory
  std::vector<char> csvFileData;
  for (const auto& entry : std::filesystem::directory_iterator(dirPath))
  {
    if (entry.is_regular_file())
    {
      // Check if file has .csv extension
      std::string extension = entry.path().extension().string();
      if (extension.size() == 4 &&
        std::tolower(extension[0]) == '.' &&
        std::tolower(extension[1]) == 'c' &&
        std::tolower(extension[2]) == 's' &&
        std::tolower(extension[3]) == 'v')
      {
        // Open the file in binary mode to avoid newline conversions
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

        std::string tableName = entry.path().stem().string();

        OUTPUT_MESSAGE("{}", entry.path().string().c_str());
        OUTPUT_MESSAGE("{}", csvFileData.data());

        std::vector<std::vector<std::string>> csvData = readCSV(csvFileData.data());
        for (const auto& row : csvData)
        {
          for (const auto& item : row)
          {
            printf("%s,", item.c_str());
          }
          printf("\n");
        }

        // Check that there is at least one row in addition to the header
        if (csvData.size() < 2)
        {
          OUTPUT_MESSAGE("Error: Table {} does not have at least 2 rows", tableName);
          return 1;
        }

        // Check all columns have the same count
        size_t columnCount = csvData[0].size();
        for (size_t i = 1; i < csvData.size(); i++)
        {
          if (csvData[i].size() != columnCount)
          {
            OUTPUT_MESSAGE("Error: Table {} has column count {} not equal to header count {} != {}", tableName, i, csvData[i].size(), columnCount);
            return 1;
          }
        }

        CSVTable newTable;

        // Check the header data of the table and parse it to a table entry
        for (const std::string& header : csvData[0])
        {
          CSVHeader newHeader;
          if (!ReadHeader(header, newHeader))
          {
            OUTPUT_MESSAGE("Error: Table {} has bad column header {}", tableName, header);
            return 1;
          }

          newTable.m_headerData.push_back(std::move(newHeader));
        }

        // Copy all row data over
        newTable.m_rowData.reserve(csvData.size() - 1);
        for (size_t i = 1; i < csvData.size(); i++)
        {
          newTable.m_rowData.push_back(std::move(csvData[i]));
        }

        // Check that all column names are unique
        {
          std::unordered_set<std::string> uniqueCheck;
          for (const CSVHeader& header : newTable.m_headerData)
          {
            if (uniqueCheck.contains(header.m_name))
            {
              OUTPUT_MESSAGE("Error: Table {} has duplicate column name {}", tableName, header.m_name);
              return 1;
            }
            uniqueCheck.insert(header.m_name);
          }
        }

        // Validate keys and all numbers
        std::unordered_set<std::string> uniqueCheck;
        std::unordered_set<double> uniqueFloatCheck;
        for (size_t i = 0; i < newTable.m_headerData.size(); i++)
        {
          const CSVHeader& header = newTable.m_headerData[i];

          uniqueCheck.clear();
          uniqueFloatCheck.clear();

          // If there is a min/max range get it

          for (const std::vector<std::string>& row : newTable.m_rowData)
          {
            const std::string& entry = row[i];

            // Check for duplicates
            if (header.m_isKey)
            {
              if (uniqueCheck.contains(entry))
              {
                OUTPUT_MESSAGE("Error: Table {} has duplicate key in column name {} \"{}\"", tableName, header.m_name, entry);
                return 1;
              }
              uniqueCheck.insert(entry);
            }

            if (header.m_type != ColumnType::String)
            {
              // Attempt conversion
              NumberType columnNumber;
              if (!ParseNumber(header.m_type, entry, columnNumber))
              {
                OUTPUT_MESSAGE("Error: Table {} has bad data in column {} entry \"{}\"", tableName, newTable.m_headerData[i].m_name, entry);
                return 1;
              }

              // Check min / max ranges


              // Check duplicate key float values
              if (header.m_isKey && (header.m_type == ColumnType::Float64 || header.m_type == ColumnType::Float32))
              {
                double testFloat = header.m_type == ColumnType::Float64 ? columnNumber.Float64 : columnNumber.Float32;

                if (uniqueFloatCheck.contains(testFloat))
                {
                  OUTPUT_MESSAGE("Error: Table {} has duplicate key in column name {} \"{}\"", tableName, header.m_name, entry);
                  return 1;
                }
                uniqueFloatCheck.insert(testFloat);
              }
            }
          }
        }

        // Add to a hashmap of all the csv files
        g_tables[tableName] = std::move(newTable);

      }
    }
  }

  // Check that the table references match up

     // Check foreign keys that are numbers, do the number check again

  // If checking table layout - sort by table keys, save out removal of extra spaces


  // Add code gen of runtime types

     // Handle enum tables

     // Handle global tables
       // Check that there is only one row

  return 0;
}