# SQLite 3.54.1 snapshot helper dependency

`sqlite3.c` and `sqlite3.h` are the official SQLite 3.54.1 amalgamation,
obtained from:

- Archive: <https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip>
- Official archive SHA3-256:
  `92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30`
- SQLite download and version information: <https://www.sqlite.org/download.html>
- SQLite amalgamation build information: <https://sqlite.org/amalgamation.html>

The vendored source files were extracted unchanged from that archive. Their
SHA-256 digests are:

| File | SHA-256 |
| --- | --- |
| `sqlite3.c` | `0c657666d819f261cc4007526ad57e1669de9949df7a8ea847aa284a2bf0ee52` |
| `sqlite3.h` | `d6c84557261893439b341d2e6c35187ae3ea24fbfd843ad94616f72d89242b8c` |

SQLite's source code is in the public domain. This private static build is
used only by `DatabaseFileOperations::Internal::createOnlineBackupSnapshot`;
it opens both Online Backup API handles itself and never consumes a native
handle from Qt's QSQLITE plugin. CMake checks the exact header version and
compiles the amalgamation with hidden symbol visibility. The dependency is
vendored so normal and release builds remain offline and add no runtime
SQLite DLL, shared library, or framework requirement.

To update it, download the official amalgamation archive for the intended
version, verify its published SHA3-256, extract only `sqlite3.c` and
`sqlite3.h` into a matching version directory, update the pinned version in
`cmake/sqlite.cmake`, and record the new archive and source-file hashes here.
Review the SQLite release notes before upgrading the separate snapshot engine
used alongside Qt's QSQLITE plugin.
