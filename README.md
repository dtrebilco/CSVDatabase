# CSVDatabase

A simple database that is stored in CSV files. Suitable for databases that need to be storable and mergable via source control.


## Introduction

When developing games it is standard to have values for designers (or modders) to change how the game operates. These values can be stored in simple text files, json or even binary. 

However, using these formats leads to issues when this data needs to be stored in source control and merge conflicts occur. In most cases this will require a custom merge tool or exclusive locking of assets. 

Another issue is that lots of designers like to use programs like Excel to calculate an update values. 

By storing the data configuration in a simple CSV based Relational Database data can be easily updated, merged and validated before use.

It is important to note that "NULLable" is not a concept in the database, but you can specify a NULL or None row and test for it via a GlobalTable (see below)

## Layout

The database consists of a directory with multiple CSV files.

Each CSV file has a header row that specifies the table specification.

This consists of 
```
<Column Name> <Optional Tokens> <Optional Comment starting with //>
```
Example table header.
| Name Key // The key to the table | Value UInt8 // The value | Link +OtherTable // Links to other table |
| ------- | ----------- | -----|

## Tokens

* Key - Is a key value for this table.  There can be multiple keys columns in a table. The combination of all Key values need to be unique for a table.

* "+TableName" or "\*TableName" - Link to a another table via it's key. If a foreign table has multiple keys, specify them with "+TableName:KeyColumnName". Use the '*' for a table link that might make table cycles (eg. references another row in the same table)

* Bool, Int8/UInt8,Int16/UInt16,Int32/UInt32,Int64/UInt64,Float32/Float64 The type of the column. If no value is specified it is assumed to be a UTF8 string value. 

* Min=Value and Max=Value - For value types, specify the valid range 

* Ignore - This column is just notes or comments. To be ignored at runtime.


## Special tables

### Enum tables
These tables are named starting with "Enum" and must have the columns: Name, Value and Comment
eg:

| Name Key| Value UInt8 | Comment |
| ------- | ----------- | -----|
|Easy|0|Easiest difficulty |
|Medium|1|Standard difficulty |
|Hard|2|Hardest difficulty |

Enum tables are not loaded at runtime, but have their values baked into generated code. These are needed when the runtime has logic that directly uses types.

```C++
enum class GameMode : uint8_t
{
	Easy   = 0, // Easiest difficulty
	Medium = 1, // Standard difficulty
	Hard   = 2, // Hardest difficulty
};

```

### Global tables
Global tables are tables that start with the name "Global" and will only ever have one row. 

Global tables are the place to specify all the global variables needed from the database.

As the database does not support values having a NULL value, one typical global table is GlobalNone or GlobalNull. This tables specifies the "none" or "null" values for a given database row.


**EnumWeaponTypes.csv**

| Name Key | Value UInt8 | Comment |
| --------- | ----- | --|
| None | 0 | A none type of weapon
| Gun | 1 | A gun type of weapon

**Weapons.csv**
| Name Key | Type +EnumWeaponTypes |
| ------------- | ------------- |
| None | None |
| Phaser | Gun |

**Characters.csv**
| Name Key | LeftWeapon +Weapons | RightWeapon +Weapons
| ------------- | ------------- | -- |
| Nerd | Phaser | None |


**GlobalNones.csv**
| Weapon +Weapons  |
| ------------- | 
| None |

As shown above, the internal code can test for a "None" WeaponType as the enum is baked into the code. To test for a "None" Weapon, code can compare against GlobalNones::Weapon 

## Editing

The editing of the CSV files can be done in any program, but to avoid conflicts, a processing step or tool should sort rows via the table keys to make merging easier.

Some workflows load the CSV files into a simple DB like SQLite by creating tables from the CSV header data. Then do editing and processing in that, then dump the contents out again into the same tables. This workflow allows complex queries on the data.


## Processing

Once the database has been specified, you can run a custom generator program (eg. in python) to generate the database types, serialization code and do data validation. (Validate links and types)

eg. This DB table
| Name Key +EnumTypes| Value UInt8 | Link +Table2 // Table2 Link|
| ------- | ----------- | -----|

Could generate this code
```c++

class DBTable
{
   Types m_Name;
   uint8_t m_Value;
   DB::Table2::ID m_Link; // Table2 Link
};

class DB
{
public:
   std::vector<DBTable> m_dbTables;

};

bool ReadDBTables(const char* path)
{
    //...
    // Read and validate values
}

```
It is important that DB tables links are resolved after serialization or the serialization is ordered so table links can be resolved during serialization.


## Patching at runtime

A simple way of allowing patches at runtime (eg mods) is to also auto generate a function that takes json in and patches values.

```json
{
    "TableName": {
        "KeyName1": {
            "ColumnName1": "NewValue",
            "ColumnName2": "NewValue"
        },
        "KeyName2": {
            "ColumnName2": "NewValue"
        }
    }
}
```

```c++

bool PatchDB(const char *json)
{
   // Check for updates to DBTable
      // Find Key 
        // Find ColumnName
           // Assign new Value (with bounds checks etc)

      // If key not found, add new row

}

```


