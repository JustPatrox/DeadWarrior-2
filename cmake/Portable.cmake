include(FetchContent)
set(DW2_RELEASE_VERSION "development" CACHE STRING "Release identification")
set(DW2_BUNDLE_BUILD "0" CACHE STRING "Numeric app bundle build")
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    URL https://codeload.github.com/libsdl-org/SDL/tar.gz/f00dc919ca35b9756316e86c4777392de442f9b2
    URL_HASH SHA256=e6a9c4e75de8fe719f922dddc208627f77754a502909f457a1355981f20c21e0)
FetchContent_MakeAvailable(SDL3)
FetchContent_Declare(stb
    URL https://codeload.github.com/nothings/stb/tar.gz/9600522de53973336fa033a289858a4354cbb04c
    URL_HASH SHA256=8546b1281cf61e4b5229e9eb05055d765f1b076f6ba03aab377df09a20fb613b)
FetchContent_MakeAvailable(stb)
add_executable(DeadWarrior2 MACOSX_BUNDLE src/sdl_main.cpp src/sdl_render.cpp
    src/sdl_audio.cpp src/sdl_display.cpp src/pause_menu.cpp)
target_compile_features(DeadWarrior2 PRIVATE cxx_std_20)
target_include_directories(DeadWarrior2 SYSTEM PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(DeadWarrior2 PRIVATE campaign_core SDL3::SDL3-static)
set_target_properties(DeadWarrior2 PROPERTIES OUTPUT_NAME "DeadWarrior 2"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.justpatrox.deadwarrior2"
    MACOSX_BUNDLE_BUNDLE_NAME "DeadWarrior 2"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.0.0"
    MACOSX_BUNDLE_BUNDLE_VERSION "0.0.${DW2_BUNDLE_BUILD}"
    MACOSX_BUNDLE_INFO_STRING "DeadWarrior 2 ${DW2_RELEASE_VERSION}")
if(APPLE)
    set(resource_dir "DeadWarrior 2.app/Contents/Resources")
    install(TARGETS DeadWarrior2 BUNDLE DESTINATION .)
else()
    set(resource_dir ".")
    install(TARGETS DeadWarrior2 RUNTIME DESTINATION .)
endif()
install(DIRECTORY assets DESTINATION "${resource_dir}")
install(FILES icon.png DESTINATION "${resource_dir}")
install(DIRECTORY art/fonts DESTINATION "${resource_dir}/assets")
install(FILES NOTICE.md DESTINATION "${resource_dir}")
install(FILES "${SDL3_SOURCE_DIR}/LICENSE.txt" DESTINATION "${resource_dir}/licenses" RENAME SDL.txt)
install(FILES "${stb_SOURCE_DIR}/LICENSE" DESTINATION "${resource_dir}/licenses" RENAME stb.txt)
