# SDL ellipse construction

The SDL renderer keeps the original 40-segment ellipses. [The geometry helper](../src/sdl_ellipse.hpp) stores the same 41 float sine/cosine pairs once and constructs the 42 vertices in a fixed stack array. Its 120 triangle indices are constant. [The renderer](../src/sdl_render.cpp) submits the same vertices, colors, texture coordinates, triangle order and blend mode as before; polygons and lines retain their existing paths.

This changes CPU geometry construction only. It does not change simulation updates, game RNG, animation timing, assets, texture filtering or ellipse detail.

## Verification and measured scope

[The standalone probe](../tools/check_sdl_ellipse.cpp) contains the previous vector-based construction and compares it directly with the helper used by the renderer. It needs SDL and C++20, but no game resources or SWF.

- 16,007 varied cases include fractional coordinates, partial off-screen placement, different colors/opacity and very small/large dimensions. All 672,294 vertices and every triangle index were byte-identical on the tested Windows build.
- SDL software rendering at 1280x720 and 1920x1080 compares all 2,995,200 pixels, including overlap, transparency and clipping. Both before/after PNG pairs were identical.
- With MSVC 14.51, x64 Release `/O2`, constructing 200,000 ellipses took a median 571.052 ms before and 15.331 ms after across seven alternating rounds. This includes the previous temporary vectors and excludes GPU submission, decoding and all other game work. These numbers are not a game FPS measurement.

The trigonometric values use the same math library and expression as the previous code. The probe checks before/after equality on the platform where it runs; it does not establish that different operating systems' math libraries return identical bits. Native platform builds must run their own checks before release.

The probe writes `checks.json` and four PNG files to its requested output directory. The tested PNG SHA-256 values were:

| Resolution | Before and after SHA-256 |
| --- | --- |
| 1280x720 | `f91278e84596321a6ecd891136c959339f5a5eae2738e9407ed20e5d71bfe77d` |
| 1920x1080 | `2fc78e5b2fde19137b059b09eddd6690e57ec69657f0ae6cc49982bad0f0d1c5` |

## Reproduce independently

Use the same SDL revision as the portable game build. The following PowerShell commands build that dependency and create an isolated probe project without changing the repository's main CMake configuration. Run them from the repository root with CMake and a C++ toolchain available:

```powershell
cmake -S . -B build/sdl-check -DDW2_SDL_FRONTEND=ON
cmake --build build/sdl-check --config Release --target SDL3-static

$ellipseRoot = (Get-Location).Path
$ellipseSdl = (Resolve-Path build/sdl-check/_deps/sdl3-build).Path
New-Item -ItemType Directory -Force out/ellipse-probe-source | Out-Null
@'
cmake_minimum_required(VERSION 3.24)
project(EllipseProbe LANGUAGES CXX)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
find_package(SDL3 CONFIG REQUIRED)
add_executable(ellipse_probe "${DW2_SOURCE_DIR}/tools/check_sdl_ellipse.cpp")
target_include_directories(ellipse_probe PRIVATE "${DW2_SOURCE_DIR}/src")
target_compile_features(ellipse_probe PRIVATE cxx_std_20)
target_link_libraries(ellipse_probe PRIVATE SDL3::SDL3-static)
'@ | Set-Content -Encoding utf8 out/ellipse-probe-source/CMakeLists.txt

cmake -S out/ellipse-probe-source -B build/ellipse-probe `
    "-DDW2_SOURCE_DIR=$ellipseRoot" "-DSDL3_DIR=$ellipseSdl" -DCMAKE_BUILD_TYPE=Release
cmake --build build/ellipse-probe --config Release --target ellipse_probe
& build/ellipse-probe/Release/ellipse_probe.exe out/ellipse-checks
```

For a single-configuration generator, the executable is directly under `build/ellipse-probe`. Linux and macOS can use the same standalone CMake project with the appropriate absolute `DW2_SOURCE_DIR` and SDL configuration directory, then run `build/ellipse-probe/ellipse_probe out/ellipse-checks`.

A zero exit code requires topology, vertex bytes and rendered pixels to match. Benchmark speed varies with compiler, machine and load; the probe reports the measured medians without imposing a performance threshold.
