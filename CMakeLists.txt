cmake_minimum_required(VERSION 3.14)
project(markov
VERSION 0.1.1
LANGUAGES CXX
)

set(USE_STD_REGEX TRUE CACHE BOOL "Use std::regex instead of boost::regex. Prefered in most cases.")

#if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
#        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
#endif()

#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast")

find_package(SQLite3 REQUIRED)
if (NOT USE_STD_REGEX)
	find_package(Boost 1.66.0 COMPONENTS regex REQUIRED)
endif()

add_library(markov SHARED
	src/connecion.cpp
)

add_library(Markov::markov ALIAS markov)

if (USE_STD_REGEX)
	target_compile_definitions(markov PRIVATE USE_STD_REGEX)
endif()

target_include_directories(markov
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sqlite_modern_cpp/hdr
        
)

target_compile_features(markov PUBLIC cxx_std_17)

target_link_libraries(markov
    PRIVATE
        SQLite::SQLite3
)

if (NOT USE_STD_REGEX)
	target_link_libraries(markov
		PRIVATE
			Boost::regex
	)
endif()

# Copied from online manual. May (and 99% WILL) contain mistakes.

##############################################
# Installation instructions

include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/Markov)

install(TARGETS markov
    EXPORT markov-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

#This is required so that the exported target has the name Markov and not markov
set_target_properties(markov PROPERTIES EXPORT_NAME Markov)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

#Export the targets to a script
install(EXPORT markov-targets
  FILE
    MarkovTargets.cmake
  NAMESPACE
    Markov::
  DESTINATION
    ${INSTALL_CONFIGDIR}
)

#Create a ConfigVersion.cmake file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/MarkovConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(${CMAKE_CURRENT_LIST_DIR}/cmake/MarkovConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/MarkovConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

#Install the config, configversion and custom find modules
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/MarkovConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/MarkovConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

##############################################
## Exporting from the build tree
export(EXPORT markov-targets FILE ${CMAKE_CURRENT_BINARY_DIR}/MarkovTargets.cmake NAMESPACE Markov::)

#Register package in user's package registry
export(PACKAGE Markov)

