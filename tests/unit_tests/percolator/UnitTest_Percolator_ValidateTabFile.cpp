/*
 * Copyright 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <gtest/gtest.h>

#include <fstream>
#include <string>
#include <vector>

#include <filesystem>
#include "TmpDir.h"

#include "PerInputOutputPlanner.h"
#include "ValidateTabFile.h"

namespace {

class ScopedTempDir {
 public:
  ScopedTempDir() {
    path_ = owner_.getPath();
  }

  ~ScopedTempDir() { std::filesystem::remove_all(path_); }

  std::filesystem::path path() const { return path_; }

 private:
  TmpDir owner_;
  std::filesystem::path path_;
};

void writePin(const std::filesystem::path& path, const std::string& idValue) {
  std::ofstream out(path.string().c_str());
  ASSERT_TRUE(out.is_open());
  out << "SpecId\tLabel\tScanNr\tf1\tPeptide\tProteins\n";
  out << idValue << "\t1\t1\t0.1\tA.AAA.A\tP1\n";
  out.close();
}

void writeText(const std::filesystem::path& path, const std::string& text) {
  std::ofstream out(path.string().c_str());
  ASSERT_TRUE(out.is_open());
  out << text;
  out.close();
  ASSERT_TRUE(out.good());
}

std::vector<std::string> readLines(std::istream& input) {
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(input, line)) {
    lines.push_back(line);
  }
  return lines;
}

std::string firstField(const std::string& line) {
  const std::string::size_type tab = line.find('\t');
  return line.substr(0, tab);
}

}  // namespace

TEST(ValidateTabFileTest, ConcatenationCanAnnotateSourceTags) {
  ScopedTempDir tempDir;
  std::filesystem::path pinA = tempDir.path() / "a.pin";
  std::filesystem::path pinB = tempDir.path() / "b.pin";
  writePin(pinA, "idA");
  writePin(pinB, "idB");

  std::vector<std::string> files;
  files.push_back(pinA.string());
  files.push_back(pinB.string());

  std::vector<std::string> sourceKeys;
  sourceKeys.push_back("srcA");
  sourceKeys.push_back("srcB");

  ValidateTabFile validateTab;
  std::string merged = validateTab.concatenateMultiplePINs(files, sourceKeys);

  std::ifstream in(merged.c_str());
  ASSERT_TRUE(in.is_open());
  std::string all;
  std::string line;
  while (std::getline(in, line)) {
    all += line;
    all += "\n";
  }

  EXPECT_NE(std::string::npos,
            all.find(PerInputOutputPlanner::tagPsmId("srcA", "idA")));
  EXPECT_NE(std::string::npos,
            all.find(PerInputOutputPlanner::tagPsmId("srcB", "idB")));
}

TEST(ValidateTabFileTest,
     StreamSourceTagsDisambiguateIdsAndPreserveDefaultDirectionOnRewind) {
  ScopedTempDir tempDir;
  const std::filesystem::path pinA = tempDir.path() / "a.pin";
  const std::filesystem::path pinB = tempDir.path() / "b.pin";
  const std::string header =
      "SpecId\tLabel\tScanNr\tf1\tPeptide\tProteins\n";
  writeText(pinA, header +
                      "DefaultDirection\t0\t0\t1\t0\tP1\n"
                      "shared\t1\t1\t0.1\tA.AAA.A\tP1\n");
  writeText(pinB, header +
                      "DefaultDirection\t0\t0\t1\t0\tP1\n"
                      "shared\t-1\t2\t0.2\tB.BBB.B\tDECOY_P1\n");

  ConcatenatedPinStream input({pinA.string(), pinB.string()}, {"srcA", "srcB"});
  const std::vector<std::string> firstPass = readLines(input);

  ASSERT_EQ(4u, firstPass.size());
  EXPECT_EQ("DefaultDirection", firstField(firstPass[1]));
  EXPECT_EQ(PerInputOutputPlanner::tagPsmId("srcA", "shared"),
            firstField(firstPass[2]));
  EXPECT_EQ(PerInputOutputPlanner::tagPsmId("srcB", "shared"),
            firstField(firstPass[3]));

  input.clear();
  input.seekg(0);
  EXPECT_EQ(firstPass, readLines(input));
}

TEST(ValidateTabFileTest,
     StreamSourceTagsSurviveMissingChargePaddingAndHeaderOnlySource) {
  ScopedTempDir tempDir;
  const std::filesystem::path widePin = tempDir.path() / "wide.pin";
  const std::filesystem::path emptyPin = tempDir.path() / "empty.pin";
  const std::filesystem::path shortPin = tempDir.path() / "short.pin";
  writeText(widePin,
            "SpecId\tLabel\tScanNr\tCharge1\tCharge2\tf1\tPeptide\t"
            "Proteins\n"
            "wide-id\t1\t1\t2\t3\t0.1\tA.AAA.A\tP1\n");
  writeText(emptyPin,
            "SpecId\tLabel\tScanNr\tf1\tPeptide\tProteins\n");
  writeText(shortPin,
            "SpecId\tLabel\tScanNr\tf1\tPeptide\tProteins\n"
            "shared\t-1\t7\t0.3\tC.CCC.C\tDECOY_P1\n");

  ConcatenatedPinStream input({widePin.string(), emptyPin.string(),
                               shortPin.string()},
                              {"srcWide", "srcEmpty", "srcShort"});
  const std::vector<std::string> lines = readLines(input);

  ASSERT_EQ(3u, lines.size());
  EXPECT_EQ("SpecId\tLabel\tScanNr\tCharge1\tCharge2\tf1\tPeptide\t"
            "Proteins\t",
            lines[0]);
  EXPECT_EQ(PerInputOutputPlanner::tagPsmId("srcShort", "shared"),
            firstField(lines[2]));
  EXPECT_NE(std::string::npos,
            lines[2].find("\t-1\t7\t0\t0\t0.3\tC.CCC.C\tDECOY_P1"));
}
