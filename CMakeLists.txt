cmake_minimum_required(VERSION 3.25)
set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0 CACHE STRING "")

##########################################################################################
function(add_mac_frameworks)
    if(NOT APPLE)
        return()
    endif()
    set(oneValueArgs TARGET TYPE)
    set(multiValueArgs FRAMEWORKS)
    cmake_parse_arguments(SMTG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT TARGET ${SMTG_TARGET})
        message(FATAL_ERROR "[SMTG] Target '${SMTG_TARGET}' does not exist.")
    endif()

    foreach(framework ${SMTG_FRAMEWORKS})
        message(DEBUG "[SMTG] Looking for framework: ${framework}")
        find_library(SMTG_FRAMEWORK_${framework} ${framework})

        if(NOT SMTG_FRAMEWORK_${framework})
            message(FATAL_ERROR "[SMTG] Framework ${framework} not found.")
        endif()

        target_link_libraries(${SMTG_TARGET} PRIVATE ${SMTG_FRAMEWORK_${framework}})
    endforeach()
endfunction()

##########################################################################################
project(MIDI2-Send VERSION 0.0.1)

set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(VSTGUI_SRC_DIR "../vstgui" CACHE PATH "Path to the VSTGUI library")
set(NIMIDI2_SRC_DIR "3rdparty/ni-midi2" CACHE PATH "Path to the NI-MIDI2 library")

set(VSTGUI_DISABLE_UNITTESTS 1)
set(VSTGUI_STANDALONE_EXAMPLES 0)
set(VSTGUI_TOOLS 0)

add_subdirectory(${VSTGUI_SRC_DIR}/vstgui ${PROJECT_BINARY_DIR}/vstgui)
add_subdirectory(${NIMIDI2_SRC_DIR} ${PROJECT_BINARY_DIR}/ni-midi2)

##########################################################################################
set(target MIDI2-Send)

set(${target}_sources
  "source/app.cpp"
)

vstgui_add_executable(${target} 
  "source/app.cpp"
)
vstgui_add_resources(${target}
  "resource/Window.uidesc"
)
vstgui_set_target_bundle_id(${target} "com.steinberg.midi2-send-app")
vstgui_set_target_rcfile(${target} "resource/winresource.rc")
vstgui_set_cxx_version(${target} 17)
target_include_directories(${target} PRIVATE "${VSTGUI_SRC_DIR}")

set_target_properties(${target}
	PROPERTIES
		MACOSX_BUNDLE_GUI_IDENTIFIER "com.steinberg.midi2-send-app"
		MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
		MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
		MACOSX_BUNDLE_LONG_VERSION_STRING ${PROJECT_VERSION}
		MACOSX_BUNDLE_INFO_STRING ${PROJECT_VERSION}
		MACOSX_BUNDLE_BUNDLE_NAME ${target}
)

target_link_libraries(${target} 
	PRIVATE 
		ni-midi2
)

add_mac_frameworks(TARGET ${target} FRAMEWORKS CoreMIDI)