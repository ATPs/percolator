#ifndef TMPDIR_H_
#define TMPDIR_H_

#include <string>

// Owns a unique directory and removes its contents on destruction.
class TmpDir {
 public:
  TmpDir();
  ~TmpDir();
  TmpDir(const TmpDir&) = delete;
  TmpDir& operator=(const TmpDir&) = delete;

  const std::string& getPath() const { return path_; }
  static void setTempRoot(const std::string& tempRoot);
  static void createTempFile(std::string& file, std::string& directory);
  static void cleanupAll();

 private:
  std::string path_;
};

#endif  // TMPDIR_H_
