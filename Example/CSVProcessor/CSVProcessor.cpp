
#include <filesystem>
#include <string>

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
        printf("%s\n", entry.path().filename().string().c_str());
      }
    }
  }

  return 0;
}