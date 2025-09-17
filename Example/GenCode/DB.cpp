// Generated Database file - do not edit manually
#include "DB.h"

const char* DB::to_string(WeaponTypes value)
{
  switch (value)
  {
  case(WeaponTypes::Gun): return "Gun";
  case(WeaponTypes::None): return "None";
  }
  return "";
}

bool DB::find_enum(std::string_view name, WeaponTypes& out)
{
  std::string_view names[] =
  {
    "Gun",
    "None",
  };
  WeaponTypes values[] =
  {
    WeaponTypes::Gun,
    WeaponTypes::None,
  };
  auto lowerBound = std::lower_bound(std::begin(names), std::end(names), name);
  if (lowerBound == std::end(names) ||
      *lowerBound != name)
  {
    out = WeaponTypes::None;
    return false;
  }
  out = values[std::distance(std::begin(names), lowerBound)];
  return true;
}

bool DB::DB::Find(std::string_view name, WeaponTypes type, SpecialWeapons::ID& _ret) const
{
  auto _searchLowerBound = std::lower_bound(SpecialWeaponsValues.begin(), SpecialWeaponsValues.end(), 0, [&](const SpecialWeapons& left, int)
  {
    return (left.Name < name) ||
           (left.Name == name && left.Type < type);
  });
  if (_searchLowerBound == SpecialWeaponsValues.end() ||
      _searchLowerBound->Name != name ||
      _searchLowerBound->Type != type)
  {
    _ret = SpecialWeapons::ID(0);
    return false;
  }
  _ret = SpecialWeapons::ID((uint32_t)std::distance(SpecialWeaponsValues.begin(), _searchLowerBound));
  return true;
}

bool DB::DB::Find(std::string_view name, Weapons::ID& _ret) const
{
  auto _searchLowerBound = std::lower_bound(WeaponsValues.begin(), WeaponsValues.end(), 0, [&](const Weapons& left, int)
  {
    return (left.Name < name);
  });
  if (_searchLowerBound == WeaponsValues.end() ||
      _searchLowerBound->Name != name)
  {
    _ret = Weapons::ID(0);
    return false;
  }
  _ret = Weapons::ID((uint32_t)std::distance(WeaponsValues.begin(), _searchLowerBound));
  return true;
}

bool DB::DB::Find(std::string_view name, Characters::ID& _ret) const
{
  auto _searchLowerBound = std::lower_bound(CharactersValues.begin(), CharactersValues.end(), 0, [&](const Characters& left, int)
  {
    return (left.Name < name);
  });
  if (_searchLowerBound == CharactersValues.end() ||
      _searchLowerBound->Name != name)
  {
    _ret = Characters::ID(0);
    return false;
  }
  _ret = Characters::ID((uint32_t)std::distance(CharactersValues.begin(), _searchLowerBound));
  return true;
}
