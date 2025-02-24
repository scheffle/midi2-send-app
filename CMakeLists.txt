cmake_minimum_required(VERSION 3.25)
set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0 CACHE STRING "")

project(MIDI2-Send VERSION 0.0.1)

set(VSTGUI_SRC_DIR "../vstgui" CACHE PATH "Path to the VSTGUI library")

set(VSTGUI_DISABLE_UNITTESTS 1)
set(VSTGUI_STANDALONE_EXAMPLES 0)
set(VSTGUI_TOOLS 0)

add_subdirectory(${VSTGUI_SRC_DIR}/vstgui)

##########################################################################################
set(target MIDI2-Send)

set(${target}_sources
  "source/app.cpp"
)

set(simple_standalone_resources
"resource/Window.uidesc"
)

vstgui_add_executable(${target} 
  "source/app.cpp"
)
vstgui_add_resources(${target}
  "resource/Window.uidesc"
)
vstgui_set_target_bundle_id(${target} "com.steinberg.midi2-send-app")
vstgui_set_target_infoplist(${target} "resource/Info.plist")
vstgui_set_target_rcfile(${target} "resource/winresource.rc")
vstgui_set_cxx_version(${target} 17)
target_include_directories(${target} PRIVATE ../../../../)
set_target_properties(${target} PROPERTIES ${APP_PROPERTIES}})

