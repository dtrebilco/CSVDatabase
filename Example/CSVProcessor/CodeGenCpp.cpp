
#include "CodeGenCpp.h"
#include "CSVProcessor.h"

#include <iostream>
#include <fstream>
#include <filesystem>


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

bool CodeGenCpp(const char* outputPathStr, const std::unordered_map<std::string, CSVTable>& tables, const std::unordered_map<std::string, CSVTable>& tablesEnumRaw)
{
  std::filesystem::path outputPath(outputPathStr);
  std::error_code error;
  std::filesystem::file_status dirPathStatus = std::filesystem::status(outputPath, error);
  if (!std::filesystem::is_directory(dirPathStatus))
  {
    OutputMessage("Error: {} is not a valid directory", outputPathStr);
    return false;
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
      outBodyString += "const char* DB::to_string(" + enumName + " value)\n{\n";
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
      OutputMessage("Error: {} Recursive table link - Use \"*TableName\" instead of +TabeName on one link", tableName);
      return false;
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
    auto findTable = tables.find(tableName);
    if (findTable == tables.end())
    {
      OutputMessage("Error: Unknown table {}", tableName);
      return false;
    }
    const CSVTable& writeTable = findTable->second;

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
          auto enumFindTable = tablesEnumRaw.find(header.m_foreignTable);
          if (enumFindTable == tablesEnumRaw.end())
          {
            OutputMessage("Error: Unknown table {}", tableName);
            return false;
          }
          const CSVTable& enumTable = enumFindTable->second;

          std::string enumName = header.m_foreignTable.substr(4);
          outHeaderString += "  ";
          outHeaderString += enumName;
          outHeaderString += " ";
          outHeaderString += header.m_name;
          outHeaderString += " = ";
          outHeaderString += enumName;
          outHeaderString += "::";
          AppendToString(enumTable.m_rowData[0][0], outHeaderString);
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
    auto findTable = tables.find(tableName);
    if (findTable == tables.end())
    {
      OutputMessage("Error: Unknown table {}", tableName);
      return false;
    }
    const CSVTable& table = findTable->second;

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
      OutputMessage("Error: Unable to open file for writing {}", outputPathHeader.string());
      return false;
    }

    if (!headerFile.write(outHeaderString.data(), outHeaderString.size()))
    {
      OutputMessage("Error: Unable to write to file {}", outputPathHeader.string());
      return false;
    }
  }
  {
    std::filesystem::path outputPathBody = outputPath / "DB.cpp";
    std::ofstream bodyFile(outputPathBody, std::ios::binary);
    if (!bodyFile.is_open())
    {
      OutputMessage("Error: Unable to open file for writing {}", outputPathBody.string());
      return false;
    }

    if (!bodyFile.write(outBodyString.data(), outBodyString.size()))
    {
      OutputMessage("Error: Unable to write to file {}", outputPathBody.string());
      return false;
    }
  }

// Add code gen of runtime types

// Write out common DB util header

  // OverrideIfDifferent(std::string_view newContents, std::string& workingBuffer, const std::filesystem::path& writePath);


   // Handle Enum tables

   // Handle Global tables
  return true;
}
