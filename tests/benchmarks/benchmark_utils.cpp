#include <filesystem>
#include <fstream>
#include <functional>

using namespace std;
namespace fs = std::filesystem;

void createModFilesOnDisk(const std::filesystem::path& path, const int width,
                          const int depth, const int count)
{
  // create files
  function<void(const fs::path&, int)> addLevel = [&](const fs::path& dir,
                                                      const int currentDepth) {
    if (currentDepth >= depth) {
      return;
    }

    for (int i = 0; i < width; ++i) {
      if (currentDepth == depth - 1) {
        ofstream ofs(dir / to_string(i));
        ofs << "TEST";
      } else {
        fs::path childPath = dir / to_string(i);
        create_directories(childPath);
        addLevel(childPath, currentDepth + 1);
      }
    }
  };
  for (int i = 0; i < count; ++i) {
    addLevel(path / to_string(i), 0);
  }
}
