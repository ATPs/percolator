# Repository Guidelines

## Project Structure & Module Organization
Core C++ sources live in `src/`, with major components under `src/qvality`, `src/converters`, `src/picked_protein`, `src/dblas`, and shared XML schemas in `src/xml`.  
Tests are split by scope: GoogleTest unit tests in `tests/unit_tests/percolator`, Python integration tests in `tests/integration_tests/percolator`, and install-level system tests in `data/system_tests/percolator`.  
Build/release automation scripts are in `admin/builders`, and contributor process docs are in `docs/` (notably `StyleGuide.md` and `CodeReviews.md`).

## Build, Test, and Development Commands
Prefer out-of-source builds.

```bash
cmake -S . -B build/percolator -DCMAKE_BUILD_TYPE=Release -DXML_SUPPORT=ON
cmake --build build/percolator -j"$(nproc)"
```

- `cd build/percolator && make CTEST_OUTPUT_ON_FAILURE=1 test`: run configured unit/integration tests with verbose failures.
- `cd build/percolator && sudo make install && make test-install`: run install-based system tests.
- `./quickbuild.sh -s "$(pwd)"`: dispatch to OS-specific builder scripts in `admin/builders/`.

## Coding Style & Naming Conventions
This project builds with C++14. Follow `docs/StyleGuide.md`:
- Chromium-oriented C++ formatting, 2-space indentation, no tabs, and ~80-char lines.
- `PascalCase` for classes/files, `camelCase` for functions/variables.
- Member fields use trailing underscore (for classes): `memberName_`.
- Constants use `ALL_CAPS_WITH_UNDERSCORES`.
- Keep declarations in `.h` and implementations in `.cpp` with include guards in headers.

## Testing Guidelines
Unit tests follow `UnitTest_Percolator_*.cpp` naming and are linked into `gtest_unit`.  
Integration/system tests are generated from `*.py.cmake` templates at configure time.  
No strict coverage threshold is enforced, but coverage flags exist (`-DCOVERAGE=ON` in unit test CMake).  
Before opening a PR, run both `make ... test` and `make test-install`.

## Commit & Pull Request Guidelines
Recent commits use short, imperative subjects (for example: `Add ...`, `Fix ...`, `Refactor ...`) and often include issue references like `#394` or `Fixes #397`.  
Keep commits focused to one logical change.  
Open PRs from a branch/fork, provide a clear one-line summary plus concise description, link relevant issues, and request at least one reviewer.  
Do not merge until GitHub Actions builds pass across supported platforms.

## notes
when make updates, write important notes in folde `notes` in markdown format.

