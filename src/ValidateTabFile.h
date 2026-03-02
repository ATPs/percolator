#include <string>
#include <iostream>
#include <map>
#include <fstream>
#include <vector>

#include "DataSet.h"
#include "TmpDir.h"

class ValidateTabFile {       
  public:     
    std::string concatenateMultiplePINs(std::vector<std::basic_string<char>> fileNames);
    std::string concatenateMultiplePINs(const std::vector<std::string>& fileNames,
                                        const std::vector<std::string>& sourceKeys);
};
