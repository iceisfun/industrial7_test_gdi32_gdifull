# gdi32 / gdi32full conformance corpus

Black-box tests for Windows 10 x64 `gdi32.dll` and `gdi32full.dll`. Neither is linked: tests resolve entry
points with `GetProcAddress` (`U(Name)` searches gdi32, then gdi32full, then user32; `UG`/`UF` pin a module,
which matters for forwarders), typed from the SDK headers.

    make                        # build/<area>.exe (mingw-w64)
    build\<area>.exe [substr]   # on Windows; optional test-name filter

Output: `RUN/OK/FAIL/SKIP` per test, `  FAIL file:line expr (detail)` per failed check, `SUMMARY` last; exit 1 on failure.
Run from an interactive desktop session. `tools/gdi32_exports.txt` and `tools/gdi32full_exports.txt` list named exports.

## Areas

Each directory under `tests/` builds to `build/<area>.exe`. Test names are unique project-wide and prefixed by area.

| area | what it covers |
|---|---|
| core_smoke | harness sanity: module resolution and a few trivial calls |
| objects_stock | stock objects, GetObject/GetObjectType, handle lifetime, DeleteObject rules, GDI object limits |
| dc_lifecycle | CreateDC/CreateCompatibleDC/CreateIC, DeleteDC, SaveDC/RestoreDC, GetDeviceCaps |
| dc_attributes | colors, modes, text attributes, brush origin, fill modes, SaveDC state, DC stress |
| mapping_transform | map modes, extents/origins, world transforms, coordinate conversion |
| clipping | clip regions, ExcludeClipRect/IntersectClipRect/SelectClipRgn, metaregion, visibility tests |
| regions | region creation, combine, boxes, hit tests, GetRegionData/ExtCreateRegion |
| paths | path brackets, flatten/widen, path-to-region, stroke/fill, GetPath |
| pens_brushes | pens (all styles, extended), brushes (solid, hatch, pattern, DIB), attributes |
| bitmaps | CreateBitmap/CompatibleBitmap, Get/SetBitmapBits, pixels, selection rules, flood fill |
| dib | DIBSections, Get/SetDIBits, StretchDIBits, color tables, bit-depth variants |
| blt_rops | BitBlt/StretchBlt/PatBlt/MaskBlt/PlgBlt, ROP3/ROP4, alpha and transparent blits, per-depth |
| drawing_prims | lines, curves, shapes, pen ROP modes, exact pixel output on DIB DCs |
| text_output | TextOut/ExtTextOut/DrawText-related, extents, alignment, justification (no ClearType pixel checks) |
| fonts_enum | font creation, selection, metrics, EnumFonts*, resource add/remove, linked fonts |
| glyphs_charwidths | GetCharWidth/ABC, glyph indices, glyph outlines, kerning, font data |
| palette | logical palettes, realization, nearest colour, system palette use (read only), halftone |
| color_icm | colour spaces, ICM mode, colour adjustment, gamma ramps (invalid args only) |
| metafile_emf | enhanced metafiles: record, enumerate, play, headers, bits, pixel format |
| metafile_wmf | Windows metafiles: record, play, placeable headers, bits, conversion |
| escape_pixfmt | Escape/ExtEscape, printer-style APIs via error paths, pixel formats, device caps |
| win32u_forwarders | gdi32/gdi32full forwarder resolution and NtGdi* syscall stubs (error paths) |
| gap_00 .. gap_10 | exports not reached by the topic areas: undocumented/internal entry points, rarely used APIs, implementation-level tests of gdi32full functions |

See `COVERAGE.md` for which exports are covered and which are only probed.

## Running everything on Windows

1. Build on Linux: `make clean && make -j8` (mingw-w64, produces `build/*.exe`, statically linked, no runtime needed).
2. Copy the whole project folder (at least `build\`, `run_all.bat`, `tools\summarize.py`) to a Windows 10 x64 machine and log in to an interactive desktop session.
3. Run `run_all.bat` (optional argument: test-name substring filter). Each exe writes `results\<area>.txt`.
4. Run `python tools\summarize.py results` (add `-v` to list failing tests and checks).

Tests never print, never change display settings and clean up their GDI objects, but a few create temporary windows and files in `%TEMP%`.
A missing export is reported as SKIP, not failure. Exit code of each exe is 1 if any test failed.

`make wine-run` is only a harness sanity check: failures under wine are expected and are not authoritative.
