#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>


struct CSVHeader
{
  // Name
  // Type
  // Is Key
  // Min
  // Max
  // Is ignored
  // Comment field
  // Foreign table link
};

struct CSVTable
{
  std::vector<CSVHeader> m_headerData;
  std::vector<std::vector<std::string>> m_rowData;
};

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

        // Add the last field of the row
        if (!field.empty() || !row.empty())
        {
          row.push_back(field);
          field.clear();

          // Add row to data if it's not empty
          csvTable.push_back(row);
          row.clear();
        }
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
    row.push_back(std::move(field));
    csvTable.push_back(std::move(row));
  }

  return csvTable;
}


int main(int argc, char* argv[])
{
  // Check if directory path is provided
  if (argc != 2)
  {
    printf("Usage: CSVProcessor <directory_path>\n");
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
    printf("Error: %s' is not a valid directory\n", dirPath);
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
          printf("Error: Unable to open file %s\n", entry.path().string().c_str());
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
          printf("Error: Unable to read file contents %s\n", entry.path().string().c_str());
          return 1;
        }

        printf("%s\n", entry.path().string().c_str());
        printf("%s\n", csvFileData.data());

        std::vector<std::vector<std::string>> csvData = readCSV(csvFileData.data());
        for (const auto& row : csvData)
        {
          for (const auto& item : row)
          {
            printf("%s,", item.c_str());
          }
          printf("\n");
        }


        // Check all columns have the same count

        // Check that there is at least one row

        // Check that all table keys are unique
           // If a float or number - check that multiple representations are not made of the type (ie spaces)

        // Check that the types are valid and in range

        // Check the header data of the table and parse it to a table entry

        // Add to a hashmap of all the csv files
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