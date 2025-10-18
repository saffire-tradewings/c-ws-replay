cmake_minimum_required(VERSION 3.16)

project(
        stwwsr
        VERSION 1.0.0  # SOMAJOR.SOMINOR.SOPATCH
        DESCRIPTION "Saffire Tradewings Exchange library"
        LANGUAGES C
)

# Default install prefix -> /usr/local (overridable via -D CMAKE_INSTALL_PREFIX=...)
if (NOT DEFINED CMAKE_INSTALL_PREFIX OR CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Install path prefix" FORCE)
endif ()

# Options matching your Makefile features
option(STW_BUILD_SHARED "Build shared library" ON)
option(STW_BUILD_STATIC "Build static library" ON)
option(STW_STRIP_BINS "Strip binaries on install (Unix-like)" OFF)

# Library name
set(PKGNAME "stwwsr")

# Layout
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(PUBLIC_INC_DIR "${INC_DIR}/stw")

# Sources (fail if none, like Makefile)
file(GLOB SRCS "${SRC_DIR}/*.c")
if (NOT SRCS)
    message(FATAL_ERROR "No C sources found in ${SRC_DIR}. Expected at least ${SRC_DIR}/exchange.c")
endif ()

# Output dirs - conditional based on whether this is a subproject
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    # Standalone build: use build/ subdirectory
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build/lib")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build/lib")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build/bin")
else ()
    # Building as part of parent project: no extra build/ subdirectory
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif ()

# Common compile setup (equivalent to CFLAGS/CPPFLAGS)
add_library(${PKGNAME}_compileopts INTERFACE)
target_include_directories(${PKGNAME}_compileopts INTERFACE "${INC_DIR}")
target_compile_features(${PKGNAME}_compileopts INTERFACE c_std_99)
if (MSVC)
    target_compile_options(${PKGNAME}_compileopts INTERFACE /W4)
else ()
    target_compile_options(${PKGNAME}_compileopts INTERFACE -O3 -Wall -Wextra -Wpedantic -fPIC)
endif ()

include(GNUInstallDirs)

# Shared library with SONAME/version like Makefile:
# SOFULL = lib${PKGNAME}.so.SOMAJOR.SOMINOR.SOPATCH
# SONAME = lib${PKGNAME}.so.SOMAJOR
# CMake will also create the unversioned symlink automatically when installing.
if (STW_BUILD_SHARED)
    add_library(${PKGNAME}_shared SHARED ${SRCS})
    add_library(${PKGNAME}::shared ALIAS ${PKGNAME}_shared)
    target_link_libraries(${PKGNAME}_shared PRIVATE ${PKGNAME}_compileopts)
    set_target_properties(${PKGNAME}_shared PROPERTIES
            OUTPUT_NAME "${PKGNAME}"
            VERSION "${PROJECT_VERSION}"            # X.Y.Z -> lib${PKGNAME}.so.X.Y.Z
            SOVERSION "${PROJECT_VERSION_MAJOR}"    # SONAME -> lib${PKGNAME}.so.X
    )
    install(TARGETS ${PKGNAME}_shared
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/stw"
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"  # for DLLs on Windows
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/stw"
    )
endif ()

# Static library (lib${PKGNAME}.a)
if (STW_BUILD_STATIC)
    add_library(${PKGNAME}_static STATIC ${SRCS})
    add_library(${PKGNAME}::static ALIAS ${PKGNAME}_static)
    target_link_libraries(${PKGNAME}_static PRIVATE ${PKGNAME}_compileopts)
    set_target_properties(${PKGNAME}_static PROPERTIES
            OUTPUT_NAME "${PKGNAME}"
    )
    install(TARGETS ${PKGNAME}_static
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/stw"
    )
endif ()

# Install public headers (include/stw/*.h)
file(GLOB PUBLIC_HEADERS "${PUBLIC_INC_DIR}/*.h")
if (PUBLIC_HEADERS)
    install(FILES ${PUBLIC_HEADERS}
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/stw"
    )
endif ()

# Optional strip on install (Unix-like)
if (STW_STRIP_BINS)
    find_program(CMAKE_STRIP NAMES strip)
    if (CMAKE_STRIP)
        # Strip shared and static if they exist after install
        if (TARGET ${PKGNAME}_shared)
            install(CODE "
                message(STATUS \"Stripping shared: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/stw/lib${PKGNAME}.so.${PROJECT_VERSION}\")
                execute_process(COMMAND \"${CMAKE_STRIP}\" --strip-unneeded
                    \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/stw/lib${PKGNAME}.so.${PROJECT_VERSION}\"
                    RESULT_VARIABLE _rv)
            ")
        endif ()
        if (TARGET ${PKGNAME}_static)
            install(CODE "
                message(STATUS \"Stripping static: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/stw/lib${PKGNAME}.a\")
                execute_process(COMMAND \"${CMAKE_STRIP}\" --strip-debug
                    \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/stw/lib${PKGNAME}.a\"
                    RESULT_VARIABLE _rv)
            ")
        endif ()
    endif ()
endif ()

# ---------------------------------------------
# One-shot, shell-agnostic aggregate targets
# ---------------------------------------------
# We reuse the same robust pattern you used in c-term.

# Static build+install
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/do_install_static.cmake" "
file(MAKE_DIRECTORY \"${CMAKE_BINARY_DIR}\")
if(NOT EXISTS \"${CMAKE_BINARY_DIR}/CMakeCache.txt\")
  execute_process(COMMAND \"${CMAKE_COMMAND}\" -S \"${CMAKE_SOURCE_DIR}\" -B \"${CMAKE_BINARY_DIR}\"
                  -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
                  RESULT_VARIABLE rv)
  if(NOT rv EQUAL 0)
    message(FATAL_ERROR \"initial configure failed: \${rv}\")
  endif()
endif()
execute_process(COMMAND \"${CMAKE_COMMAND}\" -S \"${CMAKE_SOURCE_DIR}\" -B \"${CMAKE_BINARY_DIR}\"
                -DSTW_BUILD_SHARED=OFF -DSTW_BUILD_STATIC=ON -DCMAKE_BUILD_TYPE=Release
                -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
                RESULT_VARIABLE rv)
if(NOT rv EQUAL 0)
  message(FATAL_ERROR \"configure failed: \${rv}\")
endif()
execute_process(COMMAND \"${CMAKE_COMMAND}\" --build \"${CMAKE_BINARY_DIR}\" --config Release --target ${PKGNAME}_static
                RESULT_VARIABLE rv)
if(NOT rv EQUAL 0)
  message(FATAL_ERROR \"build failed: \${rv}\")
endif()
execute_process(COMMAND \"${CMAKE_COMMAND}\" --install \"${CMAKE_BINARY_DIR}\" --config Release
                --prefix $ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}
                RESULT_VARIABLE rv)
if(NOT rv EQUAL 0)
  message(FATAL_ERROR \"install failed: \${rv}\")
endif()
")
add_custom_target(install_static_${PROJECT_NAME}
        COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/do_install_static.cmake"
        USES_TERMINAL
)

# Shared build+install
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/do_install_shared.cmake" "
file(MAKE_DIRECTORY \"${CMAKE_BINARY_DIR}\")
if(NOT EXISTS \"${CMAKE_BINARY_DIR}/CMakeCache.txt\")
  execute_process(COMMAND \"${CMAKE_COMMAND}\" -S \"${CMAKE_SOURCE_DIR}\" -B \"${CMAKE_BINARY_DIR}\"
                  -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
                  RESULT_VARIABLE rv)
  if(NOT rv EQUAL 0)
    message(FATAL_ERROR \"initial configure failed: \${rv}\")
  endif()
endif()
execute_process(COMMAND \"${CMAKE_COMMAND}\" -S \"${CMAKE_SOURCE_DIR}\" -B \"${CMAKE_BINARY_DIR}\"
                -DSTW_BUILD_SHARED=ON -DSTW_BUILD_STATIC=OFF -DCMAKE_BUILD_TYPE=Release
                -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
                RESULT_VARIABLE rv)
if(NOT rv EQUAL 0)
  message(FATAL_ERROR \"configure failed: \${rv}\")
endif()
execute_process(COMMAND \"${CMAKE_COMMAND}\" --build \"${CMAKE_BINARY_DIR}\" --config Release --target ${PKGNAME}_shared
                RESULT_VARIABLE rv)
if(NOT rv EQUAL 0)
  message(FATAL_ERROR \"build failed: \${rv}\")
endif()
execute_process(COMMAND \"${CMAKE_COMMAND}\" --install \"${CMAKE_BINARY_DIR}\" --config Release
                --prefix $ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}
                RESULT_VARIABLE rv)
if(NOT rv EQUAL 0)
  message(FATAL_ERROR \"install failed: \${rv}\")
endif()
")
add_custom_target(install_shared_${PROJECT_NAME}
        COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/do_install_shared.cmake"
        USES_TERMINAL
)

# Deep clean of the entire build directory
add_custom_target(realclean_${PROJECT_NAME}
        COMMAND "${CMAKE_COMMAND}" -E echo "Removing build directory: ${CMAKE_BINARY_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CMAKE_BINARY_DIR}"
        USES_TERMINAL
)
