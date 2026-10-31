# gdi32 / gdi32full conformance corpus

Black-box tests for Windows 10 x64 `gdi32.dll` and `gdi32full.dll`. Neither is linked: tests resolve entry
points with `GetProcAddress` (`U(Name)` searches gdi32, then gdi32full, then user32; `UG`/`UF` pin a module,
which matters for forwarders), typed from the SDK headers.

    make                        # build/<area>.exe (mingw-w64)
    build\<area>.exe [substr]   # on Windows; optional test-name filter

Output: `RUN/OK/FAIL/SKIP` per test, `  FAIL file:line expr (detail)` per failed check, `SUMMARY` last; exit 1 on failure.
Run from an interactive desktop session. `tools/gdi32_exports.txt` and `tools/gdi32full_exports.txt` list named exports.
