# ========================================================================
#  CMakeLists.txt for c-ws-replay
#  Saffire Tradewings library Collection
# ========================================================================

# ------------------------------------------------------------------------
# 1. Minimum CMake Version and Project Metadata
# ------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)

project(
		ws-replay
		VERSION 1.0.0  # Format: MAJOR.MINOR.PATCH
		DESCRIPTION "Saffire Tradewings library Collection"
		LANGUAGES C
)

# ------------------------------------------------------------------------
# 2. Install Prefix and Build Options
# ------------------------------------------------------------------------

# Set default install prefix to /usr/local if not specified
if (NOT DEFINED CMAKE_INSTALL_PREFIX OR CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Install path prefix" FORCE)
endif ()

# Build options (can be toggled via -D on the command line)
option(STW_BUILD_SHARED "Build shared library" ON)
option(STW_BUILD_STATIC "Build static library" ON)
option(STW_STRIP_BINS "Strip binaries on install (Unix-like)" OFF)

# ------------------------------------------------------------------------
# 3. Project Layout and Source Discovery
# ------------------------------------------------------------------------

set(PKGNAME "stwwsr")
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(PUBLIC_INC_DIR "${INC_DIR}/stw")

# Gather all C source files
file(GLOB SRCS
		"${SRC_DIR}/*.*"
		"${SRC_DIR}/*/*.*"
		"${SRC_DIR}/*/*/*.*"
)
list(FILTER SRCS EXCLUDE REGEX ".*/archive/.*")
if (NOT SRCS)
	message(FATAL_ERROR "No C sources found in ${SRC_DIR}. Expected at least ${SRC_DIR}/exchange.c")
endif ()

# ------------------------------------------------------------------------
# 4. Output Directories (Standalone vs. Subproject)
# ------------------------------------------------------------------------

if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
	# Standalone build: use build/ subdirectory
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build/lib")
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build/lib")
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build/bin")
else ()
	# Subproject build: no extra build/ subdirectory
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif ()

# ------------------------------------------------------------------------
# 5. Common Compile Options (Interface Target)
# ------------------------------------------------------------------------

add_library(${PKGNAME}_compileopts INTERFACE)
target_include_directories(${PKGNAME}_compileopts INTERFACE "${INC_DIR}")
target_compile_features(${PKGNAME}_compileopts INTERFACE c_std_99)
add_compile_definitions($<$<CONFIG:Debug>:STW_DEBUG=1>)
if (MSVC)
	target_compile_options(${PKGNAME}_compileopts INTERFACE /W4)
else ()
	target_compile_options(${PKGNAME}_compileopts INTERFACE -O3 -Wall -Wextra -Wpedantic -fPIC)
endif ()

include(GNUInstallDirs)

# ------------------------------------------------------------------------
# 6. Library Targets and Installation
# ------------------------------------------------------------------------

# --- 6.1 Shared Library ---
if (STW_BUILD_SHARED)
	add_library(${PKGNAME}_shared SHARED ${SRCS})
	add_library(${PKGNAME}::shared ALIAS ${PKGNAME}_shared)
	target_link_libraries(${PKGNAME}_shared PRIVATE ${PKGNAME}_compileopts)
	set_target_properties(${PKGNAME}_shared PROPERTIES
			OUTPUT_NAME "${PKGNAME}"
			VERSION "${PROJECT_VERSION}"            # e.g., libstwwsr.so.1.0.0
			SOVERSION "${PROJECT_VERSION_MAJOR}"    # e.g., libstwwsr.so.1
	)
	install(TARGETS ${PKGNAME}_shared
			LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/stw"
			RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"  # For DLLs on Windows
			ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/stw"
	)
endif ()

# --- 6.2 Static Library ---
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

# --- 6.3 Public Headers ---
file(GLOB PUBLIC_HEADERS "${PUBLIC_INC_DIR}/*.h")
if (PUBLIC_HEADERS)
	install(FILES ${PUBLIC_HEADERS}
			DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/stw"
	)
endif ()

# ------------------------------------------------------------------------
# 7. Optional: Strip Binaries on Install (Unix-like)
# ------------------------------------------------------------------------

if (STW_STRIP_BINS)
	find_program(CMAKE_STRIP NAMES strip)
	if (CMAKE_STRIP)
		# Strip shared library after install
		if (TARGET ${PKGNAME}_shared)
			install(CODE "
                message(STATUS \"Stripping shared: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/stw/lib${PKGNAME}.so.${PROJECT_VERSION}\")
                execute_process(COMMAND \"${CMAKE_STRIP}\" --strip-unneeded
                    \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/stw/lib${PKGNAME}.so.${PROJECT_VERSION}\"
                    RESULT_VARIABLE _rv)
            ")
		endif ()
		# Strip static library after install
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

# ========================================================================
# 8. Custom Aggregate Targets (Build+Install, Clean, Uninstall)
# ========================================================================

# ------------------------------------------------------------------------
# 8.1 Custom Static Build+Install Target (Manual, Only This Library)
# ------------------------------------------------------------------------

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

# Manual install - ONLY this library and its headers
set(INSTALL_LIB_DIR \"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/stw\")
set(INSTALL_INC_DIR \"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/stw\")

# Determine library location based on build context
if(\"${CMAKE_SOURCE_DIR}\" STREQUAL \"${CMAKE_CURRENT_SOURCE_DIR}\")
  # Standalone build
  set(LIB_PATH \"${CMAKE_BINARY_DIR}/build/lib/lib${PKGNAME}.a\")
else()
  # Subproject build
  set(LIB_PATH \"${CMAKE_BINARY_DIR}/lib/lib${PKGNAME}.a\")
endif()

# Install the static library
if(EXISTS \"\${LIB_PATH}\")
  file(MAKE_DIRECTORY \"\${INSTALL_LIB_DIR}\")
  file(INSTALL \"\${LIB_PATH}\" DESTINATION \"\${INSTALL_LIB_DIR}\")
  message(STATUS \"Installed: \${LIB_PATH} -> \${INSTALL_LIB_DIR}\")
else()
  message(FATAL_ERROR \"Library not found: \${LIB_PATH}\")
endif()

# Install headers
file(GLOB HEADER_FILES \"${PUBLIC_INC_DIR}/*.h\")
if(HEADER_FILES)
  file(MAKE_DIRECTORY \"\${INSTALL_INC_DIR}\")
  file(INSTALL \${HEADER_FILES} DESTINATION \"\${INSTALL_INC_DIR}\")
  message(STATUS \"Installed headers to: \${INSTALL_INC_DIR}\")
endif()

message(STATUS \"Successfully installed ${PKGNAME} static library\")
")

add_custom_target(install_static_${PROJECT_NAME}
		COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/do_install_static.cmake"
		USES_TERMINAL
)

# ------------------------------------------------------------------------
# 8.2 Custom Shared Build+Install Target (Manual, Only This Library)
# ------------------------------------------------------------------------

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

# Manual install - ONLY this library and its headers
set(INSTALL_LIB_DIR \"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/stw\")
set(INSTALL_INC_DIR \"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/stw\")

# Determine library location based on build context
if(\"${CMAKE_SOURCE_DIR}\" STREQUAL \"${CMAKE_CURRENT_SOURCE_DIR}\")
  # Standalone build
  set(LIB_PATH \"${CMAKE_BINARY_DIR}/build/lib/lib${PKGNAME}.so.${PROJECT_VERSION}\")
  set(SONAME_PATH \"${CMAKE_BINARY_DIR}/build/lib/lib${PKGNAME}.so.${PROJECT_VERSION_MAJOR}\")
else()
  # Subproject build
  set(LIB_PATH \"${CMAKE_BINARY_DIR}/lib/lib${PKGNAME}.so.${PROJECT_VERSION}\")
  set(SONAME_PATH \"${CMAKE_BINARY_DIR}/lib/lib${PKGNAME}.so.${PROJECT_VERSION_MAJOR}\")
endif()

# Install the shared library (full versioned .so)
if(EXISTS \"\${LIB_PATH}\")
  file(MAKE_DIRECTORY \"\${INSTALL_LIB_DIR}\")
  file(INSTALL \"\${LIB_PATH}\" DESTINATION \"\${INSTALL_LIB_DIR}\")
  message(STATUS \"Installed: \${LIB_PATH} -> \${INSTALL_LIB_DIR}\")
else()
  message(FATAL_ERROR \"Library not found: \${LIB_PATH}\")
endif()

# Install the SONAME symlink if it exists (optional, for Linux)
if(EXISTS \"\${SONAME_PATH}\")
  file(INSTALL \"\${SONAME_PATH}\" DESTINATION \"\${INSTALL_LIB_DIR}\")
  message(STATUS \"Installed SONAME: \${SONAME_PATH} -> \${INSTALL_LIB_DIR}\")
endif()

# Install headers
file(GLOB HEADER_FILES \"${PUBLIC_INC_DIR}/*.h\")
if(HEADER_FILES)
  file(MAKE_DIRECTORY \"\${INSTALL_INC_DIR}\")
  file(INSTALL \${HEADER_FILES} DESTINATION \"\${INSTALL_INC_DIR}\")
  message(STATUS \"Installed headers to: \${INSTALL_INC_DIR}\")
endif()

message(STATUS \"Successfully installed ${PKGNAME} shared library\")
")

add_custom_target(install_shared_${PROJECT_NAME}
		COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/do_install_shared.cmake"
		USES_TERMINAL
)

# ------------------------------------------------------------------------
# 8.3 Deep Clean Target (Removes Entire Build Directory)
# ------------------------------------------------------------------------

add_custom_target(realclean_${PROJECT_NAME}
		COMMAND "${CMAKE_COMMAND}" -E echo "Removing build directory: ${CMAKE_BINARY_DIR}"
		COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CMAKE_BINARY_DIR}"
		USES_TERMINAL
)

# ------------------------------------------------------------------------
# 8.4 Custom Uninstall Target (Removes All stwwsr Artifacts)
# ------------------------------------------------------------------------

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/do_uninstall_\" ${PROJECT_NAME}\".cmake" "
set(PKGNAME \"${PKGNAME}\")
set(PROJECT_VERSION \"${PROJECT_VERSION}\")
set(PROJECT_VERSION_MAJOR \"${PROJECT_VERSION_MAJOR}\")
set(INSTALL_LIB_DIR \"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/stw\")
set(INSTALL_INC_DIR \"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/stw\")

# Static library
set(STATIC_LIB \"\${INSTALL_LIB_DIR}/lib\${PKGNAME}.a\")

# Shared library (full versioned .so, SONAME, and unversioned symlink)
set(SHARED_LIB \"\${INSTALL_LIB_DIR}/lib\${PKGNAME}.so.\${PROJECT_VERSION}\")
set(SHARED_SONAME \"\${INSTALL_LIB_DIR}/lib\${PKGNAME}.so.\${PROJECT_VERSION_MAJOR}\")
set(SHARED_UNVERSIONED \"\${INSTALL_LIB_DIR}/lib\${PKGNAME}.so\")

# Remove static lib
if(EXISTS \"\${STATIC_LIB}\")
  file(REMOVE \"\${STATIC_LIB}\")
  message(STATUS \"Removed static library: \${STATIC_LIB}\")
endif()

# Remove shared lib
if(EXISTS \"\${SHARED_LIB}\")
  file(REMOVE \"\${SHARED_LIB}\")
  message(STATUS \"Removed shared library: \${SHARED_LIB}\")
endif()

# Remove SONAME symlink/file if present
if(EXISTS \"\${SHARED_SONAME}\")
  file(REMOVE \"\${SHARED_SONAME}\")
  message(STATUS \"Removed shared SONAME: \${SHARED_SONAME}\")
endif()

# Remove unversioned symlink/file if present
if(EXISTS \"\${SHARED_UNVERSIONED}\")
  file(REMOVE \"\${SHARED_UNVERSIONED}\")
  message(STATUS \"Removed unversioned symlink: \${SHARED_UNVERSIONED}\")
endif()

# Remove headers
file(GLOB HEADER_FILES \"${PUBLIC_INC_DIR}/*.h\")
foreach(hdr \${HEADER_FILES})
  get_filename_component(hdr_name \${hdr} NAME)
  set(installed_hdr \"\${INSTALL_INC_DIR}/\${hdr_name}\")
  if(EXISTS \"\${installed_hdr}\")
    file(REMOVE \"\${installed_hdr}\")
    message(STATUS \"Removed header: \${installed_hdr}\")
  endif()
endforeach()

message(STATUS \"Uninstall of \" ${PROJECT_NAME}\" static/shared and headers complete\")
")

add_custom_target(uninstall_${PROJECT_NAME}
		COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/do_uninstall_\" ${PROJECT_NAME}\".cmake"
		USES_TERMINAL
)
# ------------------------------------------------------------------------
# 9. Main Executable Target (main.c or main.cpp from project root)
# ------------------------------------------------------------------------

# Detect main.c or main.cpp in the project root
set(MAIN_SRC "")
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.c")
	set(MAIN_SRC "${CMAKE_CURRENT_SOURCE_DIR}/main.c")
	set(MAIN_LANG C)
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
	set(MAIN_SRC "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
	set(MAIN_LANG CXX)
endif ()

if (MAIN_SRC)
	add_executable(main_${PROJECT_NAME} ${MAIN_SRC} ${SRCS})

	# Use correct language for target if C++
	if (MAIN_LANG STREQUAL "CXX")
		set_target_properties(main_${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)
	endif ()

	# --- Ensure exe is placed in the source folder (c-logger) regardless of where CMake was invoked ---
	# Detect whether this CMake invocation is for this directory (standalone) or a top-level add_subdirectory()
	if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
		set(${PROJECT_NAME}_BUILD_MODE "standalone")
	else ()
		set(${PROJECT_NAME}_BUILD_MODE "subdir")
	endif ()

	# Place runtime binary into the source directory of this component.
	# This makes the executable end up in stw/c-logger/ so it can be run with relative paths expected by main.c
	set(${PROJECT_NAME}_EXE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

	# Include directories for headers
	target_include_directories(main_${PROJECT_NAME} PRIVATE "${INC_DIR}")

	# Link STW libraries
	target_link_directories(main_${PROJECT_NAME} PRIVATE /usr/local/lib/stw)
	target_link_libraries(main_${PROJECT_NAME} PRIVATE
			stwalign
			stwcommon
			stwtime
			stwreq
			stwlog
			stwgc
			stwjson
			stwcsv
	)

	# Find and link external dependencies
	find_package(OpenSSL REQUIRED)
	find_package(ZLIB REQUIRED)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)
	pkg_check_modules(CJSON REQUIRED libcjson)

	target_link_libraries(main_${PROJECT_NAME}
			PRIVATE
			OpenSSL::SSL
			OpenSSL::Crypto
			ZLIB::ZLIB
			${LIBWEBSOCKETS_LIBRARIES}
			${CJSON_LIBRARIES}
	)
	target_include_directories(main_${PROJECT_NAME}
			PRIVATE
			${LIBWEBSOCKETS_INCLUDE_DIRS}
			${CJSON_INCLUDE_DIRS}
	)

	set_target_properties(main_${PROJECT_NAME} PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
			#RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	)


	add_custom_target(run_main_${PROJECT_NAME}
			COMMAND main_${PROJECT_NAME}
			DEPENDS main_${PROJECT_NAME}
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			COMMENT "Running main_${PROJECT_NAME}..."
			USES_TERMINAL
	)
endif ()

# ========================================================================
#  End of CMakeLists.txt
# ========================================================================