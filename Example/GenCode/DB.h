// Generated Database file - do not edit manually
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

enum class WeaponTypes : uint8_t
{
  None = 0, // A none type of weapon
  Gun = 1, // A gun type of weapon
};
constexpr uint32_t WeaponTypes_MAX = 2; // For using the enum in lookup arrays
const char* to_string(WeaponTypes value);
bool find_enum(std::string_view name, WeaponTypes& out);

class SpecialWeapons
{
public:
  using ID = IDType<SpecialWeapons>;
  using Iter = const IterType<SpecialWeapons>::Data;

  std::string Name;
  WeaponTypes Type = WeaponTypes::None;
  SpecialWeapons::ID Default;
};

class Weapons
{
public:
  using ID = IDType<Weapons>;
  using Iter = const IterType<Weapons>::Data;

  std::string Name;
  WeaponTypes Type = WeaponTypes::None;
};

class Characters
{
public:
  using ID = IDType<Characters>;
  using Iter = const IterType<Characters>::Data;

  std::string Name;
  Weapons::ID LeftWeapon;
  Weapons::ID RightWeapon;
};

class GlobalNones
{
public:
  using ID = IDType<GlobalNones>;
  using Iter = const IterType<GlobalNones>::Data;

  Weapons::ID Weapon;
};

class DB
{
public:

  template<typename T> const std::vector<T>& GetTable() const;
  template<typename T> IterType<T> Iter() const { return IterType<T>(GetTable<T>()); }
  template<typename T> const T& Get(IDType<T> id) const { return GetTable<T>()[id.m_dbIndex]; }
  template<typename T> bool ToID(uint32_t index, IDType<T>& id) const { if (index < GetTable<T>().size()) { id = IDType<T>(index); return true; } return false; }

  std::vector<SpecialWeapons> SpecialWeaponsValues;
  std::vector<Weapons> WeaponsValues;
  std::vector<Characters> CharactersValues;
  GlobalNones GlobalNonesValues;
};
template<> inline const std::vector<SpecialWeapons>& DB::GetTable() const { return SpecialWeaponsValues; }
template<> inline const std::vector<Weapons>& DB::GetTable() const { return WeaponsValues; }
template<> inline const std::vector<Characters>& DB::GetTable() const { return CharactersValues; }

} // namespace DB
