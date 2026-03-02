#ifndef TMPDIR_H_
#define TMPDIR_H_

#include <boost/filesystem.hpp>
#include <iostream>
#include <string>

class TmpDir {       
  public:             
    void static setTempRoot(const std::string& tempRoot);
    void static createTempFile(std::string& tcf, std::string& tcd);
    void static cleanupAll();

  private:
    void static registerForCleanup(const std::string& tempDir);
};

#endif  // TMPDIR_H_
