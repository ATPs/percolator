# Mattia Tomasoni - Percolator Project
# Script that tests the correctness of percolator
# Parameters: none

import os
import sys
import tempfile
import gzip
import shutil

pathToBinaries = "@pathToBinaries@"
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

def canPercRunWithInputArgs(testName, flags, inputArgs, checkValidXml=True):
  success = True
  outputPath = os.path.join(pathToOutputData,"PERCOLATOR_"+testName)
  xmlOutput = doubleQuote(outputPath + ".pout.xml")
  txtOutput = doubleQuote(outputPath + ".txt")
  percExe = doubleQuote(os.path.join(pathToBinaries, "percolator"))
  cmd = ' '.join([percExe, inputArgs, '-S 2 -X', xmlOutput, flags, '>', txtOutput,'2>&1'])
  processFile = os.popen(cmd)
  exitStatus = processFile.close()
  if exitStatus is not None:
    print(cmd)
    print("...TEST FAILED: percolator ("+testName+") terminated with " + os.strerror(exitStatus) + " exit status")
    print("check "+ txtOutput +" for details")
    success = False
  if checkValidXml:
    success = success and validate(testName,xmlOutput)
  return success

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

def canPercRunThis(testName,flags,testFile,testFileFlag="",checkValidXml=True):
  success = True
  outputPath = os.path.join(pathToOutputData,"PERCOLATOR_"+testName)
  xmlOutput = doubleQuote(outputPath + ".pout.xml")
  txtOutput = doubleQuote(outputPath + ".txt")
  readPath = doubleQuote(os.path.join(pathToData, testFile))
  percExe = doubleQuote(os.path.join(pathToBinaries, "percolator"))
  cmd = ' '.join([percExe, testFileFlag, readPath, '-S 2 -X', xmlOutput, flags, '>', txtOutput,'2>&1'])
  processFile = os.popen(cmd)
  exitStatus = processFile.close()
  if exitStatus is not None:
    print(cmd)
    print("...TEST FAILED: percolator ("+testName+") terminated with " + os.strerror(exitStatus) + " exit status")
    print("check "+ txtOutput +" for details")
    success = False
  if checkValidXml:
    success = success and validate(testName,xmlOutput)
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

if xmlSupport:
  print("- PERCOLATOR PIN XML FORMAT")

  print("(*) running percolator to calculate psm probabilities...")
  T.doTest(canPercRunThisXml("psms","-y -U","percolator/pin/pin.xml"))

  print("(*) running percolator to calculate peptide probabilities...")
  T.doTest(canPercRunThisXml("peptides","-y","percolator/pin/pin.xml"))

  print("(*) running percolator to calculate protein probabilities with picked-protein...")
  T.doTest(canPercRunThisXml("proteins","-f auto -P decoy_","percolator/pin/pin.xml"))

  print("(*) running percolator with subset training option...")
  T.doTest(canPercRunThisXml("subset_training","-y -N 1000 -U","percolator/pin/pin.xml"))
  
  print("(*) running percolator to generate tab-delimited input...")
  tabData=os.path.join(pathToOutputData, "percolatorTab ")
  T.doTest(canPercRunThis("tab_generate","-y -U -J " + tabData,"percolator/pin/pin.xml","-k",False))

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
T.doTest(canPercRunWithInputArgs("tab_glob_gz", "-y -U", wildcardArg, False))

print("(*) running percolator with --input-file-list containing pin.gz paths and patterns...")
inputListPath = os.path.join(pathToOutputData, "pin_inputs.txt")
with open(inputListPath, "w") as listFile:
  listFile.write("# Mixed list of paths and wildcard patterns\n")
  listFile.write(gzA + "\n")
  listFile.write("\n")
  listFile.write(os.path.join(globInputRoot, "b", "*.pin.gz") + "\n")
  listFile.write(gzA + "\n")

fileListArgs = "--input-file-list " + doubleQuote(inputListPath)
T.doTest(canPercRunWithInputArgs("tab_file_list_gz", "-y -U", fileListArgs, False))

print("(*) running percolator with --output-each-folder in combined training mode...")
outputEachCombined = os.path.join(pathToOutputData, "output_each_combined")
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
T.doTest(trainEachOk)

# if no errors were encountered, succeed
if T.failures == 0:
  print("...ALL TESTS SUCCEEDED")
  exit(0)
else:
  print("..." + str(T.failures) + " TESTS FAILED")
  exit(1)
