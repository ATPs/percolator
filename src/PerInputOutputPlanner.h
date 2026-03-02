#ifndef PER_INPUT_OUTPUT_PLANNER_H_
#define PER_INPUT_OUTPUT_PLANNER_H_

#include <string>
#include <vector>

class PerInputOutputPlanner {
 public:
  struct Entry {
    std::string originalPath;
    std::string materializedPath;
    std::string sourceKey;
    std::string prefix;
  };

  struct FixedOutputs {
    std::string targetPsms;
    std::string decoyPsms;
    std::string targetPeptides;
    std::string decoyPeptides;
  };

  static const char* sourceDelimiter();

  static std::vector<Entry> buildEntries(
      const std::vector<std::string>& originalPaths,
      const std::vector<std::string>& materializedPaths);

  static bool ensureOutputFolder(const std::string& folderPath,
                                 bool& createdDirectory,
                                 std::string& error);
  static bool reserveOutputFile(const std::string& folderPath,
                                const std::string& fileName,
                                std::string& outputPath,
                                bool& createdDirectory,
                                std::string& error);

  static FixedOutputs buildFixedOutputs(const std::string& folderPath,
                                        const Entry& entry,
                                        bool writeTargetPeptides,
                                        bool writeDecoyPeptides,
                                        bool writeTargetPsms,
                                        bool writeDecoyPsms);

  static std::string buildTemplateOutput(const std::string& folderPath,
                                         const Entry& entry,
                                         const std::string& templatePath);

  static std::string tagPsmId(const std::string& sourceKey,
                              const std::string& psmId);

  static bool parseTaggedPsmId(const std::string& taggedId,
                               std::string& sourceKey,
                               std::string& originalId);
};

#endif  // PER_INPUT_OUTPUT_PLANNER_H_
