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
