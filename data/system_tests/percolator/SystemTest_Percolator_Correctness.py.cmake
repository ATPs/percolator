# Mattia Tomasoni - Percolator Project
# Script that tests the correctness of percolator
# Parameters: none

import os
import sys
import tempfile
import gzip
import shutil
import csv
import math

# ctest sets SYSTEM_TEST_BIN_DIR to the binaries of the build tree
pathToBinaries = os.environ.get("SYSTEM_TEST_BIN_DIR", "@pathToBinaries@")
pathToData = "@pathToData@"
pathToOutputData = "@pathToOutputData@"
xmlSupport = @xmlSupport@

class Tester:
  failures = 0
  def doTest(self, result):
    if result:
      print("...succeeded")
    else:
      self.failures += 1

print("PERCOLATOR CORRECTNESS")

# validating output against schema
def validate(what,file):
  success = True
  print("(*) validating "+what+" output...")
  outSchema = doubleQuote(os.path.join(pathToData,"../src/xml/percolator_out.xsd"))
  processFile = os.popen("xmllint --noout --schema " + outSchema + " " + file)
  exitStatus = processFile.close()
  if exitStatus is not None:
    print("...TEST FAILED: percolator (on "+what+" probabilities) produced an invalid output")
    print("check "+file+" for details")
    success = False
  return success

def canPercRunThisXml(testName,flags,testFile):
  return canPercRunThis(testName,flags,testFile,"-k")

def canPercRunThisTab(testName,flags,testFile):
  return canPercRunThis(testName,flags,testFile,"",False)

def canPercRunWithInputArgsNoXml(testName, flags, inputArgs):
  success = True
  outputPath = os.path.join(pathToOutputData,"PERCOLATOR_"+testName)
  txtOutput = doubleQuote(outputPath + ".txt")
  percExe = doubleQuote(os.path.join(pathToBinaries, "percolator"))
  cmd = ' '.join([percExe, inputArgs, flags, '-S 2', '>', txtOutput,'2>&1'])
  processFile = os.popen(cmd)
  exitStatus = processFile.close()
  if exitStatus is not None:
    print(cmd)
    print("...TEST FAILED: percolator ("+testName+") terminated with " + os.strerror(exitStatus) + " exit status")
    print("check "+ txtOutput +" for details")
    success = False
  return success

def canPercFailWithInputArgsNoXml(testName, flags, inputArgs):
  outputPath = os.path.join(pathToOutputData,"PERCOLATOR_"+testName)
  txtOutput = doubleQuote(outputPath + ".txt")
  percExe = doubleQuote(os.path.join(pathToBinaries, "percolator"))
  cmd = ' '.join([percExe, inputArgs, flags, '-S 2', '>', txtOutput,'2>&1'])
  processFile = os.popen(cmd)
  exitStatus = processFile.close()
  if exitStatus is None:
    print(cmd)
    print("...TEST FAILED: percolator ("+testName+") succeeded but failure was expected")
    print("check "+ txtOutput +" for details")
    return False
  return True

def filesExist(paths):
  for path in paths:
    if not os.path.exists(path):
      print("...TEST FAILED: expected output missing: " + path)
      return False
  return True

def removeDirIfExists(path):
  if os.path.isdir(path):
    shutil.rmtree(path)

def readTsvRows(path):
  with open(path, "r", newline="") as resultFile:
    return list(csv.DictReader(resultFile, delimiter="\t"))

def pinIds(path):
  ids = set()
  with open(path, "r") as pinFile:
    next(pinFile, None)
    for line in pinFile:
      fields = line.rstrip("\n").split("\t")
      if fields and fields[0].lower() != "defaultdirection":
        ids.add(fields[0])
  return ids

def perInputPsmOutputsAreUsable(paths, expectedIds):
  valid = True
  sawNonzeroPep = False
  for path in paths:
    if not os.path.isfile(path):
      print("...TEST FAILED: expected output missing: " + path)
      return False
    rows = readTsvRows(path)
    if not rows:
      print("...TEST FAILED: per-input PSM output has no rows: " + path)
      valid = False
      continue
    for row in rows:
      psmId = row.get("PSMId", "")
      if not psmId or psmId not in expectedIds or "::PERC_SRC::" in psmId:
        print("...TEST FAILED: invalid or source-tagged PSM ID: " + psmId)
        valid = False
      try:
        pep = float(row["posterior_error_prob"])
        qValue = float(row["q-value"])
      except (KeyError, TypeError, ValueError):
        print("...TEST FAILED: missing or invalid PEP/q-value in " + path)
        valid = False
        continue
      if not math.isfinite(pep) or not math.isfinite(qValue) or not 0.0 <= pep <= 1.0 or not 0.0 <= qValue <= 1.0:
        print("...TEST FAILED: out-of-range PEP/q-value in " + path)
        valid = False
      if pep > 0.0:
        sawNonzeroPep = True
  if not sawNonzeroPep:
    print("...TEST FAILED: all per-input PSM PEPs are zero")
    valid = False
  return valid

def filesMatch(paths):
  if len(paths) < 2:
    return True
  with open(paths[0], "rb") as firstFile:
    firstContents = firstFile.read()
  for path in paths[1:]:
    with open(path, "rb") as otherFile:
      if otherFile.read() != firstContents:
        print("...TEST FAILED: outputs differ for identical inputs: " + path)
        return False
  return True

def canPercRunThis(testName,flags,testFile,testFileFlag="",checkValidXml=True):
  success = True
  outputPath = os.path.join(pathToOutputData,"PERCOLATOR_"+testName)
  txtOutput = doubleQuote(outputPath + ".txt")
  readPath = doubleQuote(os.path.join(pathToData, testFile))
  percExe = doubleQuote(os.path.join(pathToBinaries, "percolator"))
  cmd = ' '.join([percExe, testFileFlag, readPath, '-S 2', flags, '>', txtOutput,'2>&1'])
  processFile = os.popen(cmd)
  exitStatus = processFile.close()
  if exitStatus is not None:
    print(cmd)
    print("...TEST FAILED: percolator ("+testName+") terminated with " + os.strerror(exitStatus) + " exit status")
    print("check "+ txtOutput +" for details")
    success = False
  return success

def createMiniPin(sourcePath, targetPath, maxLines=250):
  with open(sourcePath, "r") as src:
    with open(targetPath, "w") as dst:
      for i, line in enumerate(src):
        dst.write(line)
        if i >= maxLines:
          break

def gzipFile(sourcePath, targetPath):
  with open(sourcePath, "rb") as src:
    with gzip.open(targetPath, "wb") as dst:
      shutil.copyfileobj(src, dst)

def ensureDir(path):
  if not os.path.isdir(path):
    os.makedirs(path)

# puts double quotes around the input string, needed for windows shell
def doubleQuote(path):
  return ''.join(['"',path,'"'])


T = Tester()

# running percolator with option to process tab-delimited input
print("- PERCOLATOR TAB FORMAT")

print("(*) running percolator to calculate psm probabilities...")
T.doTest(canPercRunThisTab("tab_psms","-y -U","percolator/tab/percolatorTab"))

print("(*) running percolator to calculate peptide probabilities...")
T.doTest(canPercRunThisTab("tab_peptides","-y","percolator/tab/percolatorTab"))

print("(*) running percolator to calculate protein probabilities with picked-protein...")
T.doTest(canPercRunThisTab("tab_proteins","-f auto -P decoy_","percolator/tab/percolatorTab"))

print("(*) running percolator with subset training option...")
T.doTest(canPercRunThisTab("tab_subset_training","-y -N 1000 -U","percolator/tab/percolatorTab"))

print("(*) running percolator with reset-algorithm option...")
T.doTest(canPercRunThisTab("tab_reset","--reset-algorithm","percolator/tab/percolatorTab"))

print("(*) running percolator with static model option...")
weights = os.path.join(tempfile.gettempdir(), "test_weights.txt")
T.doTest(canPercRunThisTab("save_weights", "-w %s" % weights, "percolator/tab/percolatorTab"))
T.doTest(canPercRunThisTab("static_model", "--static -W %s" % weights, "percolator/tab/percolatorTab"))
os.remove(weights)

print("(*) running percolator with quoted wildcard input over .pin.gz files...")
globInputRoot = os.path.join(pathToOutputData, "glob_inputs")
globSubDirA = os.path.join(globInputRoot, "a")
globSubDirB = os.path.join(globInputRoot, "b")
ensureDir(globSubDirA)
ensureDir(globSubDirB)

sourceTab = os.path.join(pathToData, "percolator/tab/percolatorTab")
pinA = os.path.join(globSubDirA, "input_a.pin")
pinB = os.path.join(globSubDirB, "input_b.pin")
createMiniPin(sourceTab, pinA)
createMiniPin(sourceTab, pinB)
gzA = pinA + ".gz"
gzB = pinB + ".gz"
gzipFile(pinA, gzA)
gzipFile(pinB, gzB)

wildcardArg = doubleQuote(os.path.join(globInputRoot, "*", "*.pin.gz"))
T.doTest(canPercRunWithInputArgsNoXml("tab_glob_gz", "-y -U", wildcardArg))

print("(*) running percolator with --input-file-list containing pin.gz paths and patterns...")
inputListPath = os.path.join(pathToOutputData, "pin_inputs.txt")
with open(inputListPath, "w") as listFile:
  listFile.write("# Mixed list of paths and wildcard patterns\n")
  listFile.write(gzA + "\n")
  listFile.write("\n")
  listFile.write(os.path.join(globInputRoot, "b", "*.pin.gz") + "\n")
  listFile.write(gzA + "\n")

fileListArgs = "--input-file-list " + doubleQuote(inputListPath)
T.doTest(canPercRunWithInputArgsNoXml(
  "tab_file_list_gz", "-y -U", fileListArgs))

print("(*) running percolator with --output-each-folder in combined training mode...")
outputEachCombined = os.path.join(pathToOutputData, "output_each_combined")
removeDirIfExists(outputEachCombined)
flagsOutputEachCombined = (
  "-y --output-each-folder " + doubleQuote(outputEachCombined) +
  " --results-peptides ignored --results-psms ignored")
inputArgsOutputEach = doubleQuote(gzA) + " " + doubleQuote(gzB)
expectedCombined = [
  os.path.join(outputEachCombined, "input_a.target.peptides.tsv"),
  os.path.join(outputEachCombined, "input_a.target.psms.tsv"),
  os.path.join(outputEachCombined, "input_b.target.peptides.tsv"),
  os.path.join(outputEachCombined, "input_b.target.psms.tsv"),
]
combinedOk = canPercRunWithInputArgsNoXml("tab_output_each_combined",
                                          flagsOutputEachCombined,
                                          inputArgsOutputEach)
combinedOk = combinedOk and filesExist(expectedCombined)
expectedIds = pinIds(pinA)
combinedPsmPaths = [expectedCombined[1], expectedCombined[3]]
combinedOk = combinedOk and perInputPsmOutputsAreUsable(
  combinedPsmPaths, expectedIds)
T.doTest(combinedOk)

print("(*) running percolator with --output-each-folder in combined mode and shared weights...")
outputEachCombinedWeights = os.path.join(pathToOutputData, "output_each_combined_weights")
if os.path.isdir(outputEachCombinedWeights):
  shutil.rmtree(outputEachCombinedWeights)
flagsOutputEachCombinedWeights = (
  "-y --output-each-folder " + doubleQuote(outputEachCombinedWeights) +
  " --results-peptides ignored --results-psms ignored -w ignored.txt")
expectedCombinedWeights = [
  os.path.join(outputEachCombinedWeights, "input_a.target.peptides.tsv"),
  os.path.join(outputEachCombinedWeights, "input_a.target.psms.tsv"),
  os.path.join(outputEachCombinedWeights, "input_b.target.peptides.tsv"),
  os.path.join(outputEachCombinedWeights, "input_b.target.psms.tsv"),
  os.path.join(outputEachCombinedWeights, "percolator.model.weights"),
]
combinedWeightsOk = canPercRunWithInputArgsNoXml(
  "tab_output_each_combined_weights",
  flagsOutputEachCombinedWeights,
  inputArgsOutputEach)
combinedWeightsOk = combinedWeightsOk and filesExist(expectedCombinedWeights)
T.doTest(combinedWeightsOk)

print("(*) running percolator with a pre-existing shared weights file (expect failure)...")
outputEachCombinedWeightsExists = os.path.join(pathToOutputData, "output_each_combined_weights_exists")
if os.path.isdir(outputEachCombinedWeightsExists):
  shutil.rmtree(outputEachCombinedWeightsExists)
ensureDir(outputEachCombinedWeightsExists)
existingWeightsFile = os.path.join(outputEachCombinedWeightsExists, "percolator.model.weights")
with open(existingWeightsFile, "w") as weightsFile:
  weightsFile.write("existing")
flagsOutputEachCombinedWeightsExists = (
  "-y --output-each-folder " + doubleQuote(outputEachCombinedWeightsExists) +
  " --results-peptides ignored --results-psms ignored -w ignored.txt")
T.doTest(canPercFailWithInputArgsNoXml(
  "tab_output_each_combined_weights_exists",
  flagsOutputEachCombinedWeightsExists,
  inputArgsOutputEach))

print("(*) running percolator with --output-each-folder and --train-each...")
outputEachTrainEach = os.path.join(pathToOutputData, "output_each_train_each")
removeDirIfExists(outputEachTrainEach)
flagsOutputEachTrainEach = (
  "-y --output-each-folder " + doubleQuote(outputEachTrainEach) +
  " --train-each --results-peptides ignored --results-psms ignored -w weights.txt")
expectedTrainEach = [
  os.path.join(outputEachTrainEach, "input_a.target.peptides.tsv"),
  os.path.join(outputEachTrainEach, "input_a.target.psms.tsv"),
  os.path.join(outputEachTrainEach, "input_a.weights.txt"),
  os.path.join(outputEachTrainEach, "input_b.target.peptides.tsv"),
  os.path.join(outputEachTrainEach, "input_b.target.psms.tsv"),
  os.path.join(outputEachTrainEach, "input_b.weights.txt"),
]
trainEachOk = canPercRunWithInputArgsNoXml("tab_output_each_train_each",
                                           flagsOutputEachTrainEach,
                                           inputArgsOutputEach)
trainEachOk = trainEachOk and filesExist(expectedTrainEach)
for suffix in ["target.psms.tsv", "target.peptides.tsv", "weights.txt"]:
  trainEachOk = trainEachOk and filesMatch([
    os.path.join(outputEachTrainEach, "input_a." + suffix),
    os.path.join(outputEachTrainEach, "input_b." + suffix),
  ])
T.doTest(trainEachOk)

print("(*) running combined per-input PSM output with -N subset training...")
outputEachSubset = os.path.join(pathToOutputData, "output_each_subset")
removeDirIfExists(outputEachSubset)
flagsOutputEachSubset = (
  "-y -U -N 100 --output-each-folder " + doubleQuote(outputEachSubset) +
  " --results-psms ignored --decoy-results-psms ignored")
expectedSubset = [
  os.path.join(outputEachSubset, "input_a.target.psms.tsv"),
  os.path.join(outputEachSubset, "input_a.decoy.psms.tsv"),
  os.path.join(outputEachSubset, "input_b.target.psms.tsv"),
  os.path.join(outputEachSubset, "input_b.decoy.psms.tsv"),
]
subsetOk = canPercRunWithInputArgsNoXml(
  "tab_output_each_subset", flagsOutputEachSubset, inputArgsOutputEach)
subsetOk = subsetOk and filesExist(expectedSubset)
subsetOk = subsetOk and perInputPsmOutputsAreUsable(
  [expectedSubset[0], expectedSubset[2]], expectedIds)
subsetOk = subsetOk and perInputPsmOutputsAreUsable(
  [expectedSubset[1], expectedSubset[3]], expectedIds)
T.doTest(subsetOk)

print("(*) checking that per-input output does not overwrite an input file...")
collisionDir = os.path.join(pathToOutputData, "output_each_input_collision")
removeDirIfExists(collisionDir)
ensureDir(collisionDir)
collisionInputA = os.path.join(collisionDir, "a.pin")
collisionInputB = os.path.join(collisionDir, "a.target.psms.tsv")
createMiniPin(sourceTab, collisionInputA)
createMiniPin(sourceTab, collisionInputB)
with open(collisionInputB, "rb") as collisionFile:
  collisionContents = collisionFile.read()
collisionArgs = doubleQuote(collisionInputA) + " " + doubleQuote(collisionInputB)
collisionFlags = (
  "-y --output-each-folder " + doubleQuote(collisionDir) +
  " --results-psms ignored")
collisionOk = canPercFailWithInputArgsNoXml(
  "tab_output_each_input_collision", collisionFlags, collisionArgs)
collisionLog = os.path.join(
  pathToOutputData, "PERCOLATOR_tab_output_each_input_collision.txt")
with open(collisionLog, "r") as logFile:
  collisionLogText = logFile.read()
collisionOk = collisionOk and "which is the input file" in collisionLogText
collisionOk = collisionOk and collisionInputB in collisionLogText
with open(collisionInputB, "rb") as collisionFile:
  collisionOk = collisionOk and collisionFile.read() == collisionContents
T.doTest(collisionOk)

# if no errors were encountered, succeed
if T.failures == 0:
  print("...ALL TESTS SUCCEEDED")
  exit(0)
else:
  print("..." + str(T.failures) + " TESTS FAILED")
  exit(1)
