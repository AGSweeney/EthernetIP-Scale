# OpENer File Object Integration Plan

## Overview

This document describes the process for integrating the OpENerFileObject into the ENIP Scale project to enable on-device EDS (Electronic Data Sheet) file serving via the CIP File Object.

The OpENerFileObject is an extension object to OpENer that provides an implementation of the CIP File Object (Class Code 0x37), allowing EtherNet/IP devices to serve files over the network using standard CIP services.

## Prerequisites

- OpENerFileObject repository: https://github.com/EIPStackGroup/OpENerFileObject
- Existing OpENer integration with hooks already in place (`cipcommon.c`)
- EDS file: `eds/ESP32P4_OPENER.eds`

## Integration Steps

### Step 1: Clone OpENerFileObject Repository

Clone the OpENerFileObject repository into the `cip_objects` directory:

```bash
cd components/opener/src/cip_objects
git clone https://github.com/EIPStackGroup/OpENerFileObject.git
```

This will create the directory structure:
```
components/opener/src/cip_objects/OpENerFileObject/
├── cipfile.c
├── cipfile.h
├── CMakeLists.txt
├── data/
└── README.md
```

### Step 2: Enable CIP_FILE_OBJECT in Configuration

Edit `components/opener/src/ports/ESP32/scale_application/opener_user_conf.h`:

**Change:**
```c
#ifndef CIP_FILE_OBJECT
  #define CIP_FILE_OBJECT 0
#endif
```

**To:**
```c
#ifndef CIP_FILE_OBJECT
  #define CIP_FILE_OBJECT 1
#endif
```

This enables the File Object compilation and initialization hooks that are already present in `cipcommon.c`.

### Step 3: Update CMakeLists.txt

Modify `components/opener/CMakeLists.txt` to include the File Object sources and headers.

#### 3.1 Add File Object Sources

After the `CIP_SRCS` definition (around line 40), add:

```cmake
set(CIP_FILE_OBJECT_SRCS
    "${OPENER_SRC_DIR}/cip_objects/OpENerFileObject/cipfile.c"
)
```

#### 3.2 Conditionally Include File Object Sources

Modify the `idf_component_register` section to conditionally include the File Object sources. Update the `SRCS` parameter:

```cmake
idf_component_register(
    SRCS 
        ${ESP32_PORT_SRCS}
        ${PORTS_GENERIC_SRCS}
        ${CIP_SRCS}
        ${ENET_ENCAP_SRCS}
        ${UTILS_SRCS}
        ${NVDATA_SRCS}
        $<$<BOOL:${CIP_FILE_OBJECT}>:${CIP_FILE_OBJECT_SRCS}>  # Add this line
    ...
)
```

**Note:** If conditional compilation doesn't work with ESP-IDF's CMake, you may need to use a compile definition check instead. Alternative approach:

```cmake
# At the top of CMakeLists.txt, check for CIP_FILE_OBJECT definition
if(DEFINED CIP_FILE_OBJECT AND CIP_FILE_OBJECT)
    list(APPEND CIP_SRCS "${OPENER_SRC_DIR}/cip_objects/OpENerFileObject/cipfile.c")
endif()
```

#### 3.3 Add Include Directory

Add the OpENerFileObject include directory to `INCLUDE_DIRS`:

```cmake
INCLUDE_DIRS 
    "${OPENER_SRC_DIR}"
    "${OPENER_PORTS_DIR}"
    "${OPENER_ESP32_DIR}"
    "${OPENER_ESP32_DIR}/scale_application"
    "${OPENER_SRC_DIR}/cip"
    "${OPENER_SRC_DIR}/enet_encap"
    "${OPENER_SRC_DIR}/utils"
    "${OPENER_PORTS_DIR}/nvdata"
    "${OPENER_SRC_DIR}/cip_objects/OpENerFileObject"  # Add this line
```

### Step 4: Embed EDS File

There are two approaches for making the EDS file available via the File Object:

#### Option A: Store in File Object Data Directory (Recommended for Development)

1. Copy the EDS file to the File Object's data directory:
   ```bash
   cp eds/ESP32P4_OPENER.eds components/opener/src/cip_objects/OpENerFileObject/data/
   ```

2. The File Object will serve files from its configured data directory path.

#### Option B: Embed as Flash Resource (Recommended for Production)

1. Convert the EDS file to a C array using a tool or script:
   ```bash
   # Example using xxd (Linux/WSL) or PowerShell (Windows)
   xxd -i ESP32P4_OPENER.eds > eds_file.c
   ```

2. Create a header file `eds_file.h`:
   ```c
   #ifndef EDS_FILE_H_
   #define EDS_FILE_H_
   
   extern const unsigned char eds_file_data[];
   extern const unsigned int eds_file_size;
   
   #endif
   ```

3. Modify the File Object implementation to serve the embedded EDS from flash memory instead of the filesystem.

4. Add the embedded file to the build:
   ```cmake
   set(EDS_FILE_SRCS
       "${OPENER_ESP32_DIR}/eds_file.c"
   )
   ```

### Step 5: Configure File Object Storage Path (ESP32-Specific)

The File Object needs to know where to find files. You may need to implement platform-specific file access functions or configure a storage path.

Check `components/opener/src/ports/ESP32/` for any existing file system integration, or implement:
- SPIFFS/LittleFS filesystem mount
- File path configuration
- File read/write functions for the File Object

### Step 6: Verify Integration

1. **Build the project:**
   ```bash
   idf.py build
   ```

2. **Check for compilation errors:**
   - Ensure `cipfile.h` is found
   - Verify `CipFileInit()` is called during stack initialization
   - Check that File Object class (0x37) is registered

3. **Test File Object services:**
   - Use a CIP client tool (e.g., RSLinx, Studio 5000, or custom tool)
   - Connect to the device
   - Access File Object instance 1
   - Read file attributes
   - Open and read the EDS file

## Code Verification Points

### Existing Hooks Already in Place

The following code hooks are already present in the codebase:

1. **Include guard** (`cipcommon.c` line 35-37):
   ```c
   #if defined(CIP_FILE_OBJECT) && 0 != CIP_FILE_OBJECT
     #include "OpENerFileObject/cipfile.h"
   #endif
   ```

2. **Initialization call** (`cipcommon.c` line 67-70):
   ```c
   #if defined(CIP_FILE_OBJECT) && 0 != CIP_FILE_OBJECT
     eip_status = CipFileInit();
     OPENER_ASSERT(kEipStatusOk == eip_status);
   #endif
   ```

### CMake Integration Point

The `cip_objects` directory has a CMakeLists.txt that includes from the build directory:
```
components/opener/src/cip_objects/CMakeLists.txt
```

This file includes: `INCLUDE(${CMAKE_BINARY_DIR}/cip_objects/CMakeLists.txt)`

This suggests OpENer expects CIP objects to be integrated via CMake configuration. However, since we're using ESP-IDF's component system, we'll integrate directly in the opener component's CMakeLists.txt.

## Testing Checklist

- [ ] Project builds without errors
- [ ] File Object class (0x37) appears in device identity
- [ ] File Object instance 1 is accessible
- [ ] EDS file can be read via CIP File Object services
- [ ] File attributes are correct
- [ ] File size matches expected EDS file size
- [ ] Multiple clients can access the file simultaneously (if supported)

## Troubleshooting

### Common Issues

1. **Compilation Error: "cipfile.h: No such file or directory"**
   - Verify the include directory is added to CMakeLists.txt
   - Check that OpENerFileObject was cloned correctly

2. **Link Error: "undefined reference to CipFileInit"**
   - Ensure `cipfile.c` is included in the SRCS list
   - Verify `CIP_FILE_OBJECT` is defined as 1

3. **File Object Not Appearing**
   - Check that `CipFileInit()` returns `kEipStatusOk`
   - Verify File Object class registration in message router
   - Enable debug traces to see initialization messages

4. **EDS File Not Found**
   - Verify file path configuration
   - Check filesystem mount status (if using filesystem)
   - Verify file exists in expected location

## References

- OpENerFileObject Repository: https://github.com/EIPStackGroup/OpENerFileObject
- CIP File Object Specification: Volume 1, Chapter 5-37 (CIP Specification)
- OpENer Documentation: See OpENer main repository

## Notes

- The File Object implementation may require platform-specific file I/O functions
- Consider memory constraints when embedding large EDS files
- File Object supports read/write operations - ensure proper access control if implementing write support
- EDS files are typically read-only in production devices

## Future Enhancements

- Implement write protection for EDS files
- Add support for multiple file instances
- Implement file versioning
- Add file integrity checking (CRC/checksum)

