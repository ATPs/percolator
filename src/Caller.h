/*******************************************************************************
Copyright 2006-2012 Lukas Käll <lukas.kall@scilifelab.se>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*******************************************************************************/

#ifndef CALLER_H_
#define CALLER_H_
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "Timer.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Enzyme.h"
#include "Globals.h"
#include "Normalizer.h"
#include "ProteinProbEstimator.h"
#include "SanityCheck.h"
#include "Scores.h"
#include "SetHandler.h"

#include <algorithm>

/*
 * Main class that starts and controls the calculations.
 *
 * In addition to the calculation Caller also handles command line
 * initialization, parsing input parameters, handling output of the
 * calculation results.
 *
 */
class Caller {
 public:
  Caller();
  virtual ~Caller();

  // Sets the process-wide settings that the options and the steps of a run
  // change (verbosity, --no-terminate, random seed, PEP method, protein
  // inference, protein separator, spectrum file names, feature names,
  // warnings given once) back to their initial values. The constructor
  // calls it; a run leaves its settings behind, so code that goes on after
  // a run and reads them, like the unit tests, calls it as well.
  static void resetProcessSettings();

  static string greeter();
  string extendedGreeter();
  bool parseOptions(int argc, char** argv);
  int run();

 protected:
  std::unique_ptr<Normalizer> pNorm_;
  int normalizerType_ = Normalizer::STDV;
  SanityCheck check_;
  std::unique_ptr<ProteinProbEstimator> protEstimator_;
  std::unique_ptr<Enzyme> enzyme_;
  // holds the concatenation of multiple input files, removed at the end

  // --version or --help was given: parseOptions printed it and run does
  // nothing
  bool nothingToRun_ = false;

  struct PerInputOutputSpec {
    std::string sourceKey;
    std::string prefix;
    std::string targetPsms;
    std::string decoyPsms;
    std::string targetPeptides;
    std::string decoyPeptides;
  };

  // file input parameters
  bool readStdIn_;
  std::string inputFN_;
  std::vector<std::string> inputFNs_;
  std::string protEstimatorDecoyPrefix_;

  // file output parameters
  std::string tabOutputFN_;
  std::string weightOutputFN_;
  std::string psmResultFN_, peptideResultFN_, proteinResultFN_;
  std::string decoyPsmResultFN_, decoyPeptideResultFN_, decoyProteinResultFN_;
  bool outputRT_ = false;

  // per-input output mode
  bool outputEachFolder_;
  bool trainEach_;
  std::string outputEachFolderPath_;
  bool outputEachTargetPsms_;
  bool outputEachDecoyPsms_;
  bool outputEachTargetPeptides_;
  bool outputEachDecoyPeptides_;
  bool outputEachExtraTab_;
  bool outputEachExtraWeight_;
  bool outputEachExtraProtein_;
  bool outputEachExtraDecoyProtein_;
  std::string tabOutputTemplateFN_;
  std::string weightOutputTemplateFN_;
  std::string proteinResultTemplateFN_;
  std::string decoyProteinResultTemplateFN_;
  std::vector<std::string> resolvedInputOriginalFNs_;
  std::vector<std::pair<std::string, std::string>> protectedInputFiles_;
  std::vector<PerInputOutputSpec> perInputOutputSpecs_;
  std::string sourceTagDelimiter_;
  bool useMixMaxBase_;

  // report level parameters
  bool reportUniquePeptides_;
  bool targetDecoyCompetition_;
  bool useMixMax_;
  std::string inputSearchType_;

  // SVM / cross validation parameters
  double selectionFdr_, initialSelectionFdr_, testFdr_;
  unsigned int numIterations_, maxPSMs_, nestedXvalBins_, numThreads_;
  static constexpr unsigned int kDefaultNumFolds = 3u;
  unsigned int numFolds_ = kDefaultNumFolds;  // --num-folds
  bool numFoldsSet_ = false;    // --num-folds given
  unsigned int foldsInMemory_ = 0u;  // --folds-in-memory, 0: by --num-folds
  double selectedCpos_, selectedCneg_;
  bool classicTraining_ = false;
  bool inPlaceTraining_ = false;
  // The merge of the cross-validation bins as the options give it:
  // --merge-normalization (scheme, and secondChanceScheme where given) and
  // --static (skipNormalizeScores). run() completes it.
  MergePlan mergePlan_;
  // a scheme was given, or set by --direction-check pooled; otherwise run()
  // takes CrossValidation::defaultMergeNormalization of the folds
  bool mergeNormalizationSet_ = false;
  bool reportEachIteration_, quickValidation_, trainBestPositive_,
      useResetAlgorithm_,
      useCompositionMatch_, useIrlsPep_, useInterpolatingPep_, usePavaPep_,
      useTdcPep_;

  // reporting parameters
  std::string call_;

  Timer timer;

  std::istream& getDataInStream(std::ifstream& fileStream);
  void warnAboutSeveralCandidates(const SetHandler& setHandler) const;
  bool loadAndNormalizeData(std::istream& dataStream,
                            SetHandler& setHandler,
                            Scores& allScores);
  int runCore();
  void calcAndOutputResult(Scores& allScores);

  void calculatePSMProb(Scores& allScores, bool uniquePeptideRun);
  void writePerInputResults(Scores& allScores, bool unique);
  void writeResults(Scores& allScores, bool unique, bool writeOutput);
  void calculateProteinProbabilities(Scores& allScores);
  void checkIsWritable(const std::string& filePath);
  // Both lists hold (description, file name) pairs. additionalOutputs lets
  // independent input runs validate the whole batch before writing results.
  void checkOutputFiles(
      const std::vector<std::pair<std::string, std::string> >& inputFiles,
      const std::vector<std::pair<std::string, std::string> >& additionalOutputs
          = {})
      const;
};

#endif /*CALLER_H_*/
