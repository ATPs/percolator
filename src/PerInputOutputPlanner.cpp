#include "PerInputOutputPlanner.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include <boost/filesystem.hpp>

#include "MyException.h"

namespace {

const char* kSourceDelimiter = "::PERC_SRC::";

bool endsWith(const std::string& value, const std::string& suffix) {
  if (value.size() < suffix.size()) {
    return false;
  }
  return value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
         0;
}

std::string removePinLikeSuffixes(const std::string& input) {
  std::string stem = input;
  if (endsWith(stem, ".pin.gz")) {
    stem.resize(stem.size() - 7);
    return stem;
  }
  if (endsWith(stem, ".pin")) {
    stem.resize(stem.size() - 4);
    return stem;
  }
  if (endsWith(stem, ".gz")) {
    stem.resize(stem.size() - 3);
    if (endsWith(stem, ".pin")) {
      stem.resize(stem.size() - 4);
    }
  }
  return stem;
}

std::string sanitizePathLikeString(const std::string& input) {
  std::string out;
  out.reserve(input.size() + 8);
  for (std::string::const_iterator it = input.begin(); it != input.end();
       ++it) {
    const char c = *it;
    if (c == '/' || c == '\\') {
      out += "__";
    } else if (std::isalnum(static_cast<unsigned char>(c)) || c == '.' ||
               c == '_' || c == '-') {
      out.push_back(c);
    } else {
      out.push_back('_');
    }
  }
  if (out.empty()) {
    out = "input";
  }
  return out;
}

std::string basenamePrefix(const std::string& inputPath) {
  boost::filesystem::path p(inputPath);
  std::string filename = p.filename().string();
  std::string stem = removePinLikeSuffixes(filename);
  if (stem.empty()) {
    stem = removePinLikeSuffixes(inputPath);
  }
  return sanitizePathLikeString(stem);
}

std::string flattenedRelativePrefix(const std::string& inputPath) {
  boost::filesystem::path p(inputPath);
  boost::filesystem::path rel = p;
  boost::system::error_code ec;
  if (p.is_absolute()) {
    boost::filesystem::path cwd = boost::filesystem::current_path(ec);
    if (!ec) {
      boost::filesystem::path maybeRel = boost::filesystem::relative(p, cwd, ec);
      if (!ec && !maybeRel.empty()) {
        rel = maybeRel;
      } else {
        rel = p;
      }
    }
  }
  std::string stem = removePinLikeSuffixes(rel.generic_string());
  if (stem.empty()) {
    stem = removePinLikeSuffixes(p.generic_string());
  }
  return sanitizePathLikeString(stem);
}

std::vector<std::string> findDuplicateValues(
    const std::vector<std::string>& values) {
  std::map<std::string, size_t> counts;
  for (std::vector<std::string>::const_iterator it = values.begin();
       it != values.end(); ++it) {
    ++counts[*it];
  }
  std::vector<std::string> duplicates;
  for (std::map<std::string, size_t>::const_iterator it = counts.begin();
       it != counts.end(); ++it) {
    if (it->second > 1) {
      duplicates.push_back(it->first);
    }
  }
  return duplicates;
}

std::string joinPath(const std::string& folder, const std::string& name) {
  return (boost::filesystem::path(folder) / name).string();
}

}  // namespace

const char* PerInputOutputPlanner::sourceDelimiter() { return kSourceDelimiter; }

std::vector<PerInputOutputPlanner::Entry> PerInputOutputPlanner::buildEntries(
    const std::vector<std::string>& originalPaths,
    const std::vector<std::string>& materializedPaths) {
  if (originalPaths.size() != materializedPaths.size()) {
    throw MyException(
        "Error: input planning failed due to mismatched path vectors.");
  }

  std::vector<Entry> entries;
  entries.reserve(originalPaths.size());

  for (size_t i = 0; i < originalPaths.size(); ++i) {
    Entry entry;
    entry.originalPath = originalPaths[i];
    entry.materializedPath = materializedPaths[i];
    entry.prefix = basenamePrefix(entry.originalPath);
    entries.push_back(entry);
  }

  std::vector<std::string> prefixes;
  prefixes.reserve(entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    prefixes.push_back(entries[i].prefix);
  }

  std::vector<std::string> duplicates = findDuplicateValues(prefixes);
  if (!duplicates.empty()) {
    std::set<std::string> duplicateSet(duplicates.begin(), duplicates.end());
    for (size_t i = 0; i < entries.size(); ++i) {
      if (duplicateSet.find(entries[i].prefix) != duplicateSet.end()) {
        entries[i].prefix = flattenedRelativePrefix(entries[i].originalPath);
      }
    }
  }

  prefixes.clear();
  for (size_t i = 0; i < entries.size(); ++i) {
    prefixes.push_back(entries[i].prefix);
  }
  duplicates = findDuplicateValues(prefixes);
  if (!duplicates.empty()) {
    std::ostringstream oss;
    oss << "Error: duplicate output prefixes detected after path flattening: ";
    for (size_t i = 0; i < duplicates.size(); ++i) {
      if (i > 0) {
        oss << ", ";
      }
      oss << duplicates[i];
    }
    throw MyException(oss.str());
  }

  for (size_t i = 0; i < entries.size(); ++i) {
    entries[i].sourceKey = entries[i].prefix;
  }

  return entries;
}

bool PerInputOutputPlanner::ensureOutputFolder(const std::string& folderPath,
                                               bool& createdDirectory,
                                               std::string& error) {
  createdDirectory = false;
  error.clear();

  boost::system::error_code ec;
  boost::filesystem::path outputDir(folderPath);
  if (boost::filesystem::exists(outputDir, ec)) {
    if (ec) {
      error = "Error: failed to inspect output folder " + folderPath + ".";
      return false;
    }
    if (!boost::filesystem::is_directory(outputDir, ec) || ec) {
      error = "Error: output path exists but is not a directory: " + folderPath;
      return false;
    }
    return true;
  }

  if (!boost::filesystem::create_directories(outputDir, ec) || ec) {
    error = "Error: failed to create output folder: " + folderPath;
    return false;
  }
  createdDirectory = true;
  return true;
}

bool PerInputOutputPlanner::reserveOutputFile(const std::string& folderPath,
                                              const std::string& fileName,
                                              std::string& outputPath,
                                              bool& createdDirectory,
                                              std::string& error) {
  outputPath.clear();
  error.clear();

  if (fileName.empty()) {
    error = "Error: output filename must not be empty.";
    return false;
  }

  if (!ensureOutputFolder(folderPath, createdDirectory, error)) {
    return false;
  }

  boost::filesystem::path outputFile =
      boost::filesystem::path(folderPath) / fileName;
  outputPath = outputFile.string();

  boost::system::error_code ec;
  if (boost::filesystem::exists(outputFile, ec)) {
    if (ec) {
      error = "Error: failed to inspect output file: " + outputPath;
    } else {
      error = "Error: output file already exists: " + outputPath;
    }
    return false;
  }

  std::ofstream outputStream(outputPath.c_str(), std::ios::out);
  if (!outputStream.is_open()) {
    error = "Error: failed to create output file: " + outputPath;
    return false;
  }

  return true;
}

PerInputOutputPlanner::FixedOutputs PerInputOutputPlanner::buildFixedOutputs(
    const std::string& folderPath,
    const Entry& entry,
    bool writeTargetPeptides,
    bool writeDecoyPeptides,
    bool writeTargetPsms,
    bool writeDecoyPsms) {
  FixedOutputs out;
  if (writeTargetPeptides) {
    out.targetPeptides =
        joinPath(folderPath, entry.prefix + ".target.peptides.tsv");
  }
  if (writeDecoyPeptides) {
    out.decoyPeptides =
        joinPath(folderPath, entry.prefix + ".decoy.peptides.tsv");
  }
  if (writeTargetPsms) {
    out.targetPsms = joinPath(folderPath, entry.prefix + ".target.psms.tsv");
  }
  if (writeDecoyPsms) {
    out.decoyPsms = joinPath(folderPath, entry.prefix + ".decoy.psms.tsv");
  }
  return out;
}

std::string PerInputOutputPlanner::buildTemplateOutput(
    const std::string& folderPath,
    const Entry& entry,
    const std::string& templatePath) {
  if (templatePath.empty()) {
    return "";
  }
  std::string basename = boost::filesystem::path(templatePath).filename().string();
  if (basename.empty()) {
    return "";
  }
  return joinPath(folderPath, entry.prefix + "." + basename);
}

std::string PerInputOutputPlanner::tagPsmId(const std::string& sourceKey,
                                            const std::string& psmId) {
  return sourceKey + sourceDelimiter() + psmId;
}

bool PerInputOutputPlanner::parseTaggedPsmId(const std::string& taggedId,
                                             std::string& sourceKey,
                                             std::string& originalId) {
  const std::string delim = sourceDelimiter();
  const size_t pos = taggedId.find(delim);
  if (pos == std::string::npos) {
    return false;
  }
  sourceKey = taggedId.substr(0, pos);
  originalId = taggedId.substr(pos + delim.size());
  return true;
}
