#ifndef INPUTFILERESOLVER_H_
#define INPUTFILERESOLVER_H_

#include <string>
#include <vector>

class InputFileResolver {
 public:
  static std::vector<std::string> readInputFileList(const std::string& path);
  static std::vector<std::string> expandPatterns(
      const std::vector<std::string>& entries);
  static std::vector<std::string> deduplicateKeepFirst(
      const std::vector<std::string>& paths);
  static std::vector<std::string> materializeGzipInputs(
      const std::vector<std::string>& paths);
};

#endif  // INPUTFILERESOLVER_H_
