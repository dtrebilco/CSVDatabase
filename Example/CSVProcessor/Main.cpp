#include "CSVProcessor.h"
#include "CodeGenCpp.h"

#include <fstream>

int main(int argc, char* argv[])
{
  // Check if directory path is provided
  if (argc < 2)
  {
    OutputMessage("Usage: CSVProcessor <directory_path> <optional_output_path>");
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
    OutputMessage("Error: {} is not a valid directory", dirPath);
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
      OutputMessage("Error: Duplicate table name {}", tableName);
      return 1;
    }

    // Read in the table data from the file
    CSVTable newTable;
    if (!ReadTable(csvFileData.data(), newTable))
    {
      OutputMessage("Error: Reading table {}", tableName);
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
      OutputMessage("Error: Enum table {} need three columns, single key and no foreign table links", tableName);
      return 1;
    }

    // Store a copy of the raw table before sorting
    tablesEnumRaw[tableName] = newTable;

    // Sort the table data by column and check for duplicates
    if (!SortTable(newTable))
    {
      OutputMessage("Error: Enum table {} failed to sort", tableName);
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
      OutputMessage("Error: Enum table {} failed to sort by value", tableName);
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
      OutputMessage("Error: Duplicate table name {}", tableName);
      return 1;
    }

    // Read in the table data from the file
    CSVTable newTable;
    if (!ReadTable(csvFileData.data(), newTable))
    {
      OutputMessage("Error: Reading table {}", tableName);
      return 1;
    }

    // Check that Global and Enum tables have the correct format
    if (IsGlobalTable(tableName) && newTable.m_rowData.size() != 1)
    {
      OutputMessage("Error: Global table {} can only have one row - has {}", tableName, newTable.m_rowData.size());
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
        OutputMessage("Error: Table {} has link issues with {}", tableName, header.m_name);
        return 1;
      }

      // If a foreign key is an enum table
      if (IsEnumTable(finalTableName))
      {
        // Get the lookup table
        auto enumTableIter = tablesEnumNameSort.find(finalTableName);
        if (enumTableIter == tablesEnumNameSort.end())
        {
          OutputMessage("Error: Unable to find linked enum table {} for table {}", finalTableName, tableName);
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
            OutputMessage("Error: Table {} has link to table {} with a missing lookup column key {}", tableName, header.m_foreignTable, to_string(row[h]));
            return 1;
          }

          // Swap the name for the integer
          row[h] = (*findInfo)[1];
        }
      }
      // Only convert if the new type is not already a string
      else if (!std::holds_alternative<std::string>(newType))
      {
        header.m_type = newType;
        for (std::vector<FieldType>& row : table.m_rowData)
        {
          // Should always be a string here
          if (const std::string* accessField = std::get_if<std::string>(&row[h]))
          {
            if (!ParseField(header.m_type, *accessField, newType))
            {
              OutputMessage("Error: Table has bad data in column {} - {}", header.m_name, *accessField);
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
      OutputMessage("Error: Table {} failed to sort", tableName);
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
        OutputMessage("Error: Unable to open file for writing {}", path.string());
        return 1;
      }

      if (!file.write(outFile.data(), outFile.size()))
      {
        OutputMessage("Error: Unable to write file contents {}", path.string());
        return 1;
      }
      file.close();
    }
  }

  // Save out code gen files
  if (outputPathStr && !CodeGenCpp(outputPathStr, tables, tablesEnumRaw))
  {
    return 1;
  }

  return 0;
}
