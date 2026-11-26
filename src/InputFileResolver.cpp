#include "InputFileResolver.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <zlib.h>

#include "MyException.h"
#include "TmpDir.h"

namespace {

std::string trim(const std::string& input) {
  size_t begin = 0;
  while (begin < input.size() &&
         std::isspace(static_cast<unsigned char>(input[begin]))) {
    ++begin;
  }
  if (begin == input.size()) {
    return "";
  }
  size_t end = input.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(input[end - 1]))) {
    --end;
  }
  return input.substr(begin, end - begin);
}

bool hasWildcardChars(const std::string& path) {
  return path.find('*') != std::string::npos ||
         path.find('?') != std::string::npos ||
         path.find('[') != std::string::npos;
}

bool hasGzipSuffix(const std::string& path) {
  if (path.size() < 3) {
    return false;
  }
  std::string suffix = path.substr(path.size() - 3);
  std::transform(
      suffix.begin(), suffix.end(), suffix.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return suffix == ".gz";
}

bool matchCharClass(const std::string& pattern, size_t& pIndex, char value) {
  size_t i = pIndex + 1;
  bool negate = false;
  if (i < pattern.size() && (pattern[i] == '!' || pattern[i] == '^')) {
    negate = true;
    ++i;
  }

  bool matched = false;
  bool hasClosingBracket = false;
  char previous = '\0';
  bool hasPrevious = false;
  for (; i < pattern.size(); ++i) {
    if (pattern[i] == ']') {
      hasClosingBracket = true;
      break;
    }
    if (pattern[i] == '-' && hasPrevious && i + 1 < pattern.size() &&
        pattern[i + 1] != ']') {
      char rangeEnd = pattern[i + 1];
      if (previous <= value && value <= rangeEnd) {
        matched = true;
      }
      previous = rangeEnd;
      hasPrevious = true;
      ++i;
    } else {
      if (pattern[i] == value) {
        matched = true;
      }
      previous = pattern[i];
      hasPrevious = true;
    }
  }

  if (!hasClosingBracket) {
    return value == '[';
  }

  pIndex = i + 1;
  return negate ? !matched : matched;
}

bool wildcardMatch(const std::string& pattern,
                   size_t pIndex,
                   const std::string& text,
                   size_t tIndex) {
  while (pIndex < pattern.size()) {
    char p = pattern[pIndex];
    if (p == '*') {
      while (pIndex < pattern.size() && pattern[pIndex] == '*') {
        ++pIndex;
      }
      if (pIndex == pattern.size()) {
        return true;
      }
      for (size_t i = tIndex; i <= text.size(); ++i) {
        if (wildcardMatch(pattern, pIndex, text, i)) {
          return true;
        }
      }
      return false;
    }

    if (tIndex >= text.size()) {
      return false;
    }

    if (p == '?') {
      ++pIndex;
      ++tIndex;
      continue;
    }

    if (p == '[') {
      size_t nextPatternIndex = pIndex;
      if (!matchCharClass(pattern, nextPatternIndex, text[tIndex])) {
        return false;
      }
      if (nextPatternIndex == pIndex) {
        if (text[tIndex] != '[') {
          return false;
        }
        ++pIndex;
      } else {
        pIndex = nextPatternIndex;
      }
      ++tIndex;
      continue;
    }

    if (p != text[tIndex]) {
      return false;
    }
    ++pIndex;
    ++tIndex;
  }

  return tIndex == text.size();
}

bool wildcardMatch(const std::string& pattern, const std::string& text) {
  return wildcardMatch(pattern, 0, text, 0);
}

namespace fs = std::filesystem;

void expandPathPattern(const fs::path& currentPath,
                       const std::vector<std::string>& segments,
                       size_t segmentIndex,
                       std::vector<std::string>& matches) {
  if (segmentIndex == segments.size()) {
    if (fs::exists(currentPath) && !fs::is_directory(currentPath)) {
      matches.push_back(currentPath.string());
    }
    return;
  }

  const std::string& segment = segments[segmentIndex];
  if (segment == ".") {
    expandPathPattern(currentPath, segments, segmentIndex + 1, matches);
    return;
  }
  if (segment == "..") {
    expandPathPattern(currentPath.parent_path(), segments, segmentIndex + 1,
                      matches);
    return;
  }

  if (!hasWildcardChars(segment)) {
    expandPathPattern(currentPath / segment, segments, segmentIndex + 1,
                      matches);
    return;
  }

  if (!fs::exists(currentPath) || !fs::is_directory(currentPath)) {
    return;
  }

  std::vector<fs::path> candidates;
  for (const fs::directory_entry& entry : fs::directory_iterator(currentPath)) {
    candidates.push_back(entry.path());
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const fs::path& lhs, const fs::path& rhs) {
              return lhs.filename().string() < rhs.filename().string();
            });

  for (std::vector<fs::path>::const_iterator it = candidates.begin();
       it != candidates.end(); ++it) {
    if (wildcardMatch(segment, it->filename().string())) {
      expandPathPattern(*it, segments, segmentIndex + 1, matches);
    }
  }
}

std::vector<std::string> expandSinglePattern(const std::string& pattern) {
  fs::path inputPath(pattern);
  fs::path base =
      inputPath.is_absolute() ? inputPath.root_path() : fs::current_path();
  fs::path relative =
      inputPath.is_absolute() ? inputPath.relative_path() : inputPath;

  std::vector<std::string> segments;
  for (fs::path::const_iterator it = relative.begin(); it != relative.end();
       ++it) {
    segments.push_back(it->string());
  }

  std::vector<std::string> matches;
  expandPathPattern(base, segments, 0, matches);
  std::sort(matches.begin(), matches.end());
  return matches;
}

}  // namespace

std::vector<std::string> InputFileResolver::readInputFileList(
    const std::string& path) {
  std::ifstream file(path.c_str());
  if (!file.is_open()) {
    std::ostringstream msg;
    msg << "ERROR: could not open input file list " << path;
    throw MyException(msg.str());
  }

  std::vector<std::string> entries;
  std::string line;
  while (std::getline(file, line)) {
    std::string stripped = trim(line);
    if (stripped.empty()) {
      continue;
    }
    if (!stripped.empty() && stripped[0] == '#') {
      continue;
    }
    entries.push_back(stripped);
  }
  return entries;
}

std::vector<std::string> InputFileResolver::expandPatterns(
    const std::vector<std::string>& entries) {
  std::vector<std::string> resolved;
  for (std::vector<std::string>::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    if (!hasWildcardChars(*it)) {
      resolved.push_back(*it);
      continue;
    }
    std::vector<std::string> matches = expandSinglePattern(*it);
    if (matches.empty()) {
      std::ostringstream msg;
      msg << "ERROR: no files matched pattern \"" << *it << "\"";
      throw MyException(msg.str());
    }
    resolved.insert(resolved.end(), matches.begin(), matches.end());
  }
  return resolved;
}

std::vector<std::string> InputFileResolver::deduplicateKeepFirst(
    const std::vector<std::string>& paths) {
  std::set<std::string> seen;
  std::vector<std::string> uniquePaths;
  for (std::vector<std::string>::const_iterator it = paths.begin();
       it != paths.end(); ++it) {
    fs::path normalizedPath = fs::absolute(fs::path(*it)).lexically_normal();
    std::string key = normalizedPath.string();
    if (seen.insert(key).second) {
      uniquePaths.push_back(*it);
    }
  }
  return uniquePaths;
}

std::vector<std::string> InputFileResolver::materializeGzipInputs(
    const std::vector<std::string>& paths) {
  std::vector<std::string> resolved;
  const int kBufferSize = 1 << 16;
  char buffer[kBufferSize];

  for (std::vector<std::string>::const_iterator it = paths.begin();
       it != paths.end(); ++it) {
    if (!hasGzipSuffix(*it)) {
      resolved.push_back(*it);
      continue;
    }

    gzFile inputFile = gzopen(it->c_str(), "rb");
    if (!inputFile) {
      std::ostringstream msg;
      msg << "ERROR: could not open gzip input file " << *it;
      throw MyException(msg.str());
    }

    std::string tempFilePath;
    std::string tempDirPath;
    try {
      TmpDir::createTempFile(tempFilePath, tempDirPath);
    } catch (...) {
      gzclose(inputFile);
      throw;
    }
    std::ofstream outputFile(tempFilePath.c_str(), std::ios::binary);
    if (!outputFile.is_open()) {
      gzclose(inputFile);
      std::ostringstream msg;
      msg << "ERROR: could not open temporary file " << tempFilePath
          << " for decompression";
      throw MyException(msg.str());
    }

    int bytesRead = 0;
    while ((bytesRead = gzread(inputFile, buffer, kBufferSize)) > 0) {
      outputFile.write(buffer, bytesRead);
      if (!outputFile.good()) {
        gzclose(inputFile);
        outputFile.close();
        std::ostringstream msg;
        msg << "ERROR: failed while writing decompressed data to "
            << tempFilePath;
        throw MyException(msg.str());
      }
    }

    if (bytesRead < 0) {
      int errorNumber = Z_OK;
      const char* message = gzerror(inputFile, &errorNumber);
      const std::string errorMessage = message ? message : "";
      gzclose(inputFile);
      outputFile.close();
      std::ostringstream msg;
      msg << "ERROR: failed to decompress " << *it;
      if (!errorMessage.empty()) {
        msg << " (" << errorMessage << ")";
      }
      throw MyException(msg.str());
    }

    int errorNumber = Z_OK;
    const char* message = gzerror(inputFile, &errorNumber);
    const std::string errorMessage = message ? message : "";
    const int closeResult = gzclose(inputFile);
    outputFile.close();
    if (errorNumber != Z_OK && errorNumber != Z_STREAM_END) {
      throw MyException("ERROR: failed to decompress " + *it + " (" +
                        errorMessage + ")");
    }
    if (closeResult != Z_OK || !outputFile) {
      throw MyException("ERROR: could not finish decompressing " + *it);
    }
    resolved.push_back(tempFilePath);
  }

  return resolved;
}
