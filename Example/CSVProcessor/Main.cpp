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

  DBTables db;
  if (!ReadDB(dirPath, db))
  {
    return 1;
  }

  // Resolve column types based on foreign table links (especially enums)
  if (!ResolveForeignLinkTypes(db))
  {
    return 1;
  }

  // Sort the table data by column and check for duplicates
  for (auto& [tableName, table] : db.m_tables)
  {
    if (!SortTable(table))
    {
      OutputMessage("Error: Table {} failed to sort", tableName);
      return 1;
    }
  }

  // Validate tables
  if (!ValidateTables(db.m_tables))
  {
    return 1;
  }

  // DT_TODO: Add command line for resave of all tables
  std::string csvFileData;
  for (const auto& path : db.m_csvFilePaths)
  {
    std::string tableName = path.stem().string();
    if (!ReadToString(path, csvFileData))
    {
      return 1;
    }

    std::string_view existingFile(csvFileData.data(), csvFileData.size() - 1);

    std::string outFile;
    SaveToString(db.m_tables[tableName], db.m_tables, existingFile, outFile);

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
  if (outputPathStr && !CodeGenCpp(outputPathStr, db.m_tables, db.m_tablesEnumRaw))
  {
    return 1;
  }

  return 0;
}
