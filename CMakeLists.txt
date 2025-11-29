# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Unused components have been moved to components/unused/
# They are not included in the build to avoid unnecessary dependencies
# See components/unused/README.md for details

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

# Define FD_SETSIZE for LWIP_MAX_SOCKETS=64
# LWIP_SOCKET_OFFSET = FD_SETSIZE - MAX_SOCKETS, and we need LWIP_SOCKET_OFFSET >= 6
# So FD_SETSIZE >= MAX_SOCKETS + 6 = 64 + 6 = 70
# Also need room for stdout, stderr, stdin (+3), so use 73 for safety
add_compile_definitions(FD_SETSIZE=73)

project(ENIP-Scale)

# Add .cache folders to the clean process
# These folders are not automatically cleaned by idf.py clean, but can cause build issues
set_directory_properties(PROPERTIES
    ADDITIONAL_CLEAN_FILES
    "${CMAKE_SOURCE_DIR}/components/.cache;${CMAKE_SOURCE_DIR}/.cache"
)

# Copy firmware binary to FirmwareImages folder with unique name after build
# The copy script will generate a filename with timestamp and optional git commit hash
idf_build_get_property(binary_dir BINARY_DIR)
idf_build_get_property(project_name PROJECT_NAME)
set(source_binary "${binary_dir}/${project_name}.bin")
set(firmware_images_dir "${CMAKE_SOURCE_DIR}/FirmwareImages")
set(copy_script "${CMAKE_SOURCE_DIR}/scripts/copy_firmware.py")

# Find Python (ESP-IDF requires Python, so it should be available)
find_package(Python3 QUIET COMPONENTS Interpreter)
if(NOT Python3_Interpreter_FOUND)
    # Fallback to python command
    set(PYTHON_CMD "python")
else()
    set(PYTHON_CMD "${Python3_EXECUTABLE}")
endif()

# Create a custom command that runs after the binary is generated
# The binary is created by ESP-IDF's build system after linking
# The Python script includes retry logic to wait for the binary to be created
# We use a marker file to track when the copy has been done (for incremental builds)
set(copy_marker "${binary_dir}/.firmware_copied")
# Note: We can't directly depend on the binary file because CMake doesn't track it as a build artifact
# Instead, we depend on the .elf file (which ESP-IDF creates) and the Python script will wait for the .bin
# Get the .elf file path - ESP-IDF creates this during linking
set(source_elf "${binary_dir}/${project_name}.elf")
add_custom_command(
    OUTPUT "${copy_marker}"
    COMMAND ${CMAKE_COMMAND} -E env "PYTHONIOENCODING=utf-8"
        ${PYTHON_CMD} "${copy_script}" "${source_binary}" "${firmware_images_dir}" "${project_name}"
    COMMAND ${CMAKE_COMMAND} -E touch "${copy_marker}"
    # Depend on the .elf file - this is created during linking, and the .bin is created shortly after
    # The Python script includes retry logic to wait for the .bin file to be created
    DEPENDS "${source_elf}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Copying firmware to FirmwareImages with unique name"
    VERBATIM
)

# Create a custom target that depends on the marker file
# This ensures the copy command runs during the build
# The marker file depends on the binary, so this will run after the binary is created
add_custom_target(copy_firmware_image ALL
    DEPENDS "${copy_marker}"
)

# Hook into ESP-IDF's build process to ensure this runs after the main executable is built
# Get the main executable target name from ESP-IDF
idf_build_get_property(idf_component_main COMPONENT_MAIN)
if(idf_component_main)
    # Add dependency on the main component's library target
    # This ensures the copy runs after linking
    add_dependencies(copy_firmware_image ${idf_component_main})
endif()

# Generate embedded EDS file for File Object
# This must be done in the main CMakeLists.txt to avoid ESP-IDF component requirements phase restrictions
set(EDS_FILE_PATH "${CMAKE_SOURCE_DIR}/eds/ESP32P4_OPENER.eds")
# Use the build directory for the opener component (matching path in component CMakeLists.txt)
set(OPENER_BUILD_DIR "${CMAKE_BINARY_DIR}/opener")
set(EMBEDDED_EDS_HEADER "${OPENER_BUILD_DIR}/embedded_eds_file.h")
set(EMBEDDED_EDS_SOURCE "${OPENER_BUILD_DIR}/embedded_eds_file.c")
set(EMBED_EDS_SCRIPT "${CMAKE_SOURCE_DIR}/components/opener/src/cip_objects/OpENerFileObject/embed_eds.py")

# Ensure the directory exists (component CMakeLists.txt also creates it, but this ensures it's early)
file(MAKE_DIRECTORY "${OPENER_BUILD_DIR}")

# Generate embedded EDS file during build
add_custom_command(
    OUTPUT "${EMBEDDED_EDS_HEADER}" "${EMBEDDED_EDS_SOURCE}"
    COMMAND ${PYTHON_CMD} "${EMBED_EDS_SCRIPT}"
            "${EDS_FILE_PATH}"
            "${EMBEDDED_EDS_HEADER}"
            "${EMBEDDED_EDS_SOURCE}"
    DEPENDS "${EDS_FILE_PATH}"
            "${EMBED_EDS_SCRIPT}"
    COMMENT "Embedding EDS file into build"
    VERBATIM
)

# Generate embedded icon GZ file for File Object instance 201
# Create GZ file from favicon.ico with ESP32P4-EIP.ico inside, then embed it
set(FAVICON_ICO_PATH "${CMAKE_SOURCE_DIR}/eds/favicon.ico")
set(ICON_GZ_FILE_PATH "${OPENER_BUILD_DIR}/EDSCollection.gz")
set(EMBEDDED_ICON_HEADER "${OPENER_BUILD_DIR}/embedded_icon_file.h")
set(EMBEDDED_ICON_SOURCE "${OPENER_BUILD_DIR}/embedded_icon_file.c")
set(CREATE_ICON_GZ_SCRIPT "${CMAKE_SOURCE_DIR}/components/opener/src/cip_objects/OpENerFileObject/create_icon_gz.py")
set(EMBED_ICON_GZ_SCRIPT "${CMAKE_SOURCE_DIR}/components/opener/src/cip_objects/OpENerFileObject/embed_icon_gz.py")

# Step 1: Create GZ file from favicon.ico (with ESP32P4-EIP.ico inside)
add_custom_command(
    OUTPUT "${ICON_GZ_FILE_PATH}"
    COMMAND ${PYTHON_CMD} "${CREATE_ICON_GZ_SCRIPT}"
            "${FAVICON_ICO_PATH}"
            "${ICON_GZ_FILE_PATH}"
    DEPENDS "${CREATE_ICON_GZ_SCRIPT}" "${FAVICON_ICO_PATH}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Creating EDSCollection.gz from favicon.ico"
    VERBATIM
)

# Mark the intermediate GZ file as GENERATED so CMake knows it's build output
set_source_files_properties("${ICON_GZ_FILE_PATH}" PROPERTIES GENERATED TRUE)

# Step 2: Embed the GZ file into C source
add_custom_command(
    OUTPUT "${EMBEDDED_ICON_HEADER}" "${EMBEDDED_ICON_SOURCE}"
    COMMAND ${PYTHON_CMD} "${EMBED_ICON_GZ_SCRIPT}"
            "${ICON_GZ_FILE_PATH}"
            "${EMBEDDED_ICON_HEADER}"
            "${EMBEDDED_ICON_SOURCE}"
    DEPENDS "${EMBED_ICON_GZ_SCRIPT}" "${ICON_GZ_FILE_PATH}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating embedded icon GZ file from ${ICON_GZ_FILE_PATH}"
    VERBATIM
)

# Mark the generated C files as GENERATED so CMake knows they're build output
set_source_files_properties("${EMBEDDED_ICON_HEADER}" "${EMBEDDED_ICON_SOURCE}" PROPERTIES GENERATED TRUE)

# Create a target for the embedded EDS and icon file generation that runs early
add_custom_target(generate_embedded_eds ALL
    DEPENDS "${EMBEDDED_EDS_HEADER}" "${EMBEDDED_EDS_SOURCE}"
            "${EMBEDDED_ICON_HEADER}" "${EMBEDDED_ICON_SOURCE}"
)

# Ensure the opener component builds after the EDS and icon files are generated
add_dependencies(__idf_opener generate_embedded_eds)