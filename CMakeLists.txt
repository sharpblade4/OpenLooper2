cmake_minimum_required(VERSION 3.22)

project(OpenLooper2Cm)

set(CMAKE_CXX_STANDARD 17)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)
include(cmake/cpm.cmake)
# cmake -S . -B build

CPMAddPackage(
    NAME JUCE
    GITHUB_REPOSITORY juce-framework/JUCE
    GIT_TAG 8.0.4
    VERSION 8.0.4
    SOURCE_DIR ${LIB_DIR}/juce
)

add_compile_options(-Wall -Wextra -Wpedantic)

add_subdirectory(plugin)
# cmake --build build

# in VSCode you need to save.
# then build, and link to a custom directory in ableton live:
# /Volumes/workplace/personal/juce_projects/harmonics_explorer/build/plugin/HarmonicExplorerPlugin_artefacts/VST3
# to reload plugin ableton needs to be restarted. 

# 10023  git config --local user.email "sharpblade4@gmail.com"
# 10024  git config --local user.name "Ron U"
# 10025  git commit -a -m "knobs for per-harmonic level (yet focusing out)"
# 10026  git push
# 10027  ssh-add ~/.ssh/id_rsa_github
# 10028  git push
