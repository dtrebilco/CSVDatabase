# CSVDatabase
A simple database that is stored in CSV files. Suitable for databases that need to be in source control.

No NULL values, but you can specify a NULL or None row and test for it via a GlobalTable (see below)


## Tokens

* Key - Is a key value for this table.  There can be multiple keys columns in a table. The combination of all Key values need to be unique for a table.

* "+TableName" or "\*TableName" - Link to a another table via it's key. If a foreign table has multiple keys, specify them with "+TableName:KeyColum". Use the '*' for a table link that might make table cycles (eg. references another row in the same table table)

* 

* Min=Value and Max=Value - For value types, specify the valid range 

* Ignore - This column is just notes or comments. To be ignored at runtime.


## Special tables

### Enum tables
These tables are named starting with "Enum" and must have the column names Name, Value and Comment: eg:

| Name Key| Value UInt8 | Comment |
| ------- | ----------- | -----|
|Easy|0|Easiest difficulty |
|Medium|1|Standard difficulty |
|Hard|2|Hardest difficulty |

Enum tables are not loaded at runtime, but have their values baked into the runtime by having code generated. These are needed when the runtime has logic that directly uses types.

```C++
enum class GameMode : uint8_t
{
	Easy   = 0, // Easiest difficulty
	Medium = 1, // Standard difficulty
	Hard   = 2, // Hardest difficulty
};

```


### Global tables
Global tables are tables that start with the name "Global" will only ever have one row. 

Global tables are the place to specify all the global variables need in the data base.

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

As show above, the internal code can test for a "None" WeaponType as the enum is baked into the code. To test for a "None" Weapon, code will have compare against GlobalNones::Weapon 





