# Lossless native cinematic decoding. No encoder or command-line executable is
# included in the game; maintained asset preparation performs offline encoding.
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(WEBP_LINK_STATIC ON CACHE BOOL "" FORCE)
set(WEBP_USE_THREAD OFF CACHE BOOL "" FORCE)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
foreach(option ANIM_UTILS CWEBP DWEBP GIF2WEBP IMG2WEBP VWEBP WEBPINFO LIBWEBPMUX WEBPMUX EXTRAS WEBP_JS FUZZTEST)
    set(WEBP_BUILD_${option} OFF CACHE BOOL "" FORCE)
endforeach()
FetchContent_Declare(libwebp
    URL https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.6.0.tar.gz
    URL_HASH SHA256=e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(libwebp)
# Distribute the game and explicit license copies, rather than the upstream
# development SDK (whose optional libraries are not built by our targets).
# Directory exclusion retains linked dependencies and supports CMake 3.24.
set_property(DIRECTORY "${libwebp_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)
target_sources(campaign_core PRIVATE src/chunk_movie.cpp)
target_link_libraries(campaign_core PRIVATE webpdemux webp)
if(APPLE)
    set(dw2_codec_license "DeadWarrior 2.app/Contents/Resources/licenses")
else()
    set(dw2_codec_license "licenses")
endif()
install(FILES "${libwebp_SOURCE_DIR}/COPYING" DESTINATION "${dw2_codec_license}" RENAME libwebp.txt)
install(FILES "${libwebp_SOURCE_DIR}/PATENTS" DESTINATION "${dw2_codec_license}" RENAME libwebp-PATENTS.txt)
