cmake_minimum_required(VERSION 3.22)

project(PletinaMotor VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Runtime estático en MSVC para que pletina-motor.exe no dependa de las DLL de Visual C++.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)

# JUCE va fijado al commit exacto que señala el submódulo de Tracktion Engine v3.2.0:
# T.E. sigue la rama develop de JUCE, y una release "cercana" no garantiza compilar.
# Sin GIT_SHALLOW: un clon superficial no puede hacer checkout de un SHA suelto.
FetchContent_Declare(juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE
  GIT_TAG 19edd538429c93d277bf95b55aaa7e3eb545f951)

# Tracktion Engine sin submódulos (el de JUCE apunta a una URL SSH que aquí no resuelve)
# y sin su CMake raíz (arrastra los ejemplos): entramos por modules/, que solo define
# tracktion_core, tracktion_graph y tracktion_engine con sus terceros embebidos.
FetchContent_Declare(tracktion
  GIT_REPOSITORY https://github.com/Tracktion/tracktion_engine
  GIT_TAG v3.2.0
  GIT_SHALLOW TRUE
  GIT_SUBMODULES ""
  SOURCE_SUBDIR modules)

FetchContent_MakeAvailable(juce tracktion)

juce_add_console_app(pletina-motor PRODUCT_NAME "pletina-motor")

target_sources(pletina-motor PRIVATE
  src/main.cpp
  src/motor.cpp
  src/efectos.cpp
  src/efectos2.cpp
  src/efectos3.cpp
  src/protocolo.cpp)

target_compile_definitions(pletina-motor PRIVATE
  JUCE_PLUGINHOST_VST3=1
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
  JUCE_MODAL_LOOPS_PERMITTED=1
  JUCE_STRICT_REFCOUNTEDPOINTER=1
  JUCE_USE_MP3AUDIOFORMAT=1
  TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1)

target_link_libraries(pletina-motor PRIVATE
  tracktion::tracktion_core
  tracktion::tracktion_engine
  tracktion::tracktion_graph
  juce::juce_audio_devices
  juce::juce_audio_processors
  juce::juce_audio_utils
  juce::juce_recommended_config_flags)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(pletina-motor PRIVATE -latomic)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  target_compile_options(pletina-motor PRIVATE -Wno-unknown-pragmas)
endif()
