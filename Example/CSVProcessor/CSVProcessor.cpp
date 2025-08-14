#include <iostream>
#include <fstream>
#include <filesystem>
#include <format>
#include <string>
#include <vector>
#include <unordered_map>

#define ERROR_MESSAGE(...) { char buf[256]; const auto out = std::format_to_n(buf, std::size(buf) - 1, __VA_ARGS__); *out.out = '\0'; printf("%s\n", buf); }

enum class ColumnType
{
  Unknown,

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
};

struct CSVHeader
{
  std::string m_name;        // Name
  ColumnType m_type = ColumnType::Unknown;// Type
  bool m_isKey = false;      // Is Key
  // Min
  // Max
  bool m_isIgnored = false;  // Is ignored
  std::string m_comment;     // Comment field
  std::string m_foreignTable;// Foreign table link
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


int main(int argc, char* argv[])
{
  // Check if directory path is provided
  if (argc != 2)
  {
    ERROR_MESSAGE("Usage: CSVProcessor <directory_path>");
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
    ERROR_MESSAGE("Error: {} is not a valid directory", dirPath);
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
          ERROR_MESSAGE("Error: Unable to open file {}", entry.path().string().c_str());
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
          ERROR_MESSAGE("Error: Unable to read file contents {}", entry.path().string().c_str());
          return 1;
        }

        std::string tableName = entry.path().stem().string();

        ERROR_MESSAGE("{}", entry.path().string().c_str());
        ERROR_MESSAGE("{}", csvFileData.data());

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
          ERROR_MESSAGE("Error: Table {} does not have at least 2 rows", tableName);
          return 1;
        }

        // Check all columns have the same count
        size_t columnCount = csvData[0].size();
        for (size_t i = 1; i < csvData.size(); i++)
        {
          if (csvData[i].size() != columnCount)
          {
            ERROR_MESSAGE("Error: Table {} has column count not equal to header count {} != {}", tableName, csvData[i].size(), columnCount);
            return 1;
          }
        }

        CSVTable newTable;

        // Check the header data of the table and parse it to a table entry

        // Check that all table keys are unique
           // If a float or number - check that multiple representations are not made of the type (ie spaces)

        // Check that the types are valid and in range

        // Copy all row data over
        newTable.m_rowData.reserve(csvData.size() - 1);
        for (size_t i = 1; i < csvData.size(); i++)
        {
          newTable.m_rowData.push_back(std::move(csvData[i]));
        }

        // Add to a hashmap of all the csv files
        g_tables[tableName] = std::move(newTable);

      }
    }
  }

  // Check that the table references match up

  // Add code gen of runtime types

     // Handle enum tables

     // Handle global tables
       // Check that there is only one row

  return 0;
}