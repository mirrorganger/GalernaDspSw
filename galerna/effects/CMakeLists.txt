add_library(galerna_effects INTERFACE)
add_library(galerna::effects ALIAS galerna_effects)

target_include_directories(galerna_effects INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(galerna_effects INTERFACE cxx_std_23)

# ThxDeepNote.hpp/ThxVoice.hpp use StateVariableFilter/WavetableOscillator.
target_link_libraries(galerna_effects INTERFACE galerna::core)

if(GALERNA_BUILD_TESTS)
    target_sources(galerna_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestThxDeepNote.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestThxVoice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestWindChimes.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestWindChimeVoice.cpp
    )
endif()
