# File Object Integration and Message Router Modifications

## Overview

This document describes the integration of the CIP File Object (Class 0x37) into the OpENer EtherNet/IP stack and the modifications made to the Message Router Object (Class 0x02) to properly advertise supported CIP objects.

## File Object Integration

### Purpose

The CIP File Object enables EtherNet/IP devices to serve Electronic Data Sheet (EDS) files and icon files directly to configuration tools like RSLinx and EtherNet/IP Explorer. This eliminates the need for users to manually download and install EDS files.

### Implementation Details

#### Source Code Location

The File Object implementation is located in:
```
components/opener/src/cip_objects/OpENerFileObject/
```

This directory was cloned from the official OpENer File Object repository:
- Repository: https://github.com/EIPStackGroup/OpENerFileObject
- Files:
  - `cipfile.c` - Main File Object implementation (modified)
  - `cipfile.h` - File Object header definitions
  - `memfile.c` - Memory-based FILE wrapper for embedded files (added)
  - `memfile.h` - Memory file wrapper header (added)

#### Modifications to OpENerFileObject Code

The original OpENerFileObject implementation was designed for filesystem-based file access. This project required significant modifications to support embedded files and compatibility with RSLinx and EtherNet/IP Explorer:

**1. Function Signature Fixes:**
- Updated function signatures to match OpENer API requirements:
  - Changed `const int encapsulation_session` to `const CipSessionHandle encapsulation_session`
  - Added `const` qualifiers to callback function parameters
  - Fixed `CipServiceFunction` and `CipCallback` type mismatches

**2. Embedded File Handling:**
- Added support for embedded EDS and icon files using `fmemopen()` or custom `memfile_open()` wrapper
- Modified `CipFileCreateEDSAndIconFileInstance()` to prioritize embedded data over filesystem files
- Created `memfile.c` and `memfile.h` to provide a `FILE*` interface for memory buffers
- Added fallback logic when embedded files are not available

**3. Instance Creation:**
- Modified to create two instances (200 for EDS, 201 for icon) instead of single instance
- Set instance names: "EDS and Icon Files" (200) and "Related EDS and Icon Files" (201)
- Configured instance 201 to handle empty icon files gracefully

**4. File Naming and Encoding:**
- Changed EDS filename from default to "EDS.txt" (matches Rockwell Micro850 convention)
- Changed icon filename to "EDSCollection.gz" (matches Rockwell Micro850 convention)
- Set EDS encoding to Binary (0) instead of ASCIIText
- Set icon encoding to CompressedFile (1) for GZ archive format

**5. File Revision Attribute:**
- Added logic to set File Revision (attribute 5) to match EDS file revision (1.0)
- Critical for RSLinx compatibility - RSLinx validates revision before initiating upload
- Set both major and minor revision values explicitly

**6. Attribute Type Fixes:**
- Changed File Revision attribute (5) from `kCipAny` to `kCipUsintUsint`
- Changed File Name attribute (4) from `kCipAny` to `kCipStringI`
- Ensures proper encoding and decoding of attribute values

**7. Service Registration:**
- Added `GetAttributeAll` service (0x01) to File Object class services
- Updated class service count to include all instance services

**8. Error Handling:**
- Replaced `kCipErrorGeneralError` (undeclared) with `kCipErrorResourceUnavailable`
- Improved error handling for NULL file handles and file read operations
- Added graceful fallback when embedded files fail to load

**9. Build System Integration:**
- Added Python scripts for embedding EDS and icon files:
  - `embed_eds.py` - Converts EDS file to C array
  - `create_icon_gz.py` - Creates GZ-compressed icon archive
  - `embed_icon_gz.py` - Converts GZ file to C array
- Integrated embedding scripts into CMake build process

**Why These Modifications Were Necessary:**
- **No Filesystem**: ESP32-P4 firmware embeds files directly in memory, not on a filesystem
- **RSLinx Compatibility**: Required specific file names, encoding formats, and revision matching
- **API Compatibility**: OpENer API changes required function signature updates
- **EtherNet/IP Explorer**: Required proper attribute types and service registration

**Note**: These modifications are specific to this project's requirements. The original OpENerFileObject code is designed for filesystem-based file access and may work differently in other environments.

#### Build System Integration

**CMakeLists.txt (Root):**
- Generates embedded EDS file from `eds/ESP32P4_OPENER.eds` using `embed_eds.py`
- Generates embedded icon file from `eds/favicon.ico` using `create_icon_gz.py` and `embed_icon_gz.py`
- Creates custom targets `generate_embedded_eds` and `generate_embedded_icon`
- Ensures the `opener` component depends on these generated files

**components/opener/CMakeLists.txt:**
- Includes File Object source files: `cipfile.c`, `memfile.c`, and generated embedded files
- Adds File Object include directory to build paths
- Defines `CIP_FILE_OBJECT=1` compile definition

#### File Object Instances

The implementation creates two File Object instances as per EtherNet/IP specification:

**Instance 200 (0xC8) - EDS File:**
- **Instance Name**: "EDS and Icon Files"
- **File Name**: "EDS.txt"
- **File Encoding Format**: Binary (0)
- **File Revision**: 1.0 (major=1, minor=0) - matches EDS file revision
- **State**: FileLoaded (2)
- **Access**: Read-only
- **Content**: Embedded EDS file data from `eds/ESP32P4_OPENER.eds`

**Instance 201 (0xC9) - Icon File:**
- **Instance Name**: "Related EDS and Icon Files"
- **File Name**: "EDSCollection.gz"
- **File Encoding Format**: CompressedFile (1)
- **File Revision**: 1.0 (major=1, minor=0)
- **State**: FileLoaded (2) if icon available, FileEmpty (1) if not
- **Access**: Read-only
- **Content**: Compressed GZ file containing "ESP32P4-EIP.ico" (from `eds/favicon.ico`)

#### Embedded File Handling

The EDS and icon files are embedded directly into the firmware at build time:

1. **EDS File Embedding:**
   - `embed_eds.py` reads `eds/ESP32P4_OPENER.eds`
   - Generates `build/opener/embedded_eds_file.h` and `embedded_eds_file.c`
   - Creates a C array with the EDS file data

2. **Icon File Embedding:**
   - `create_icon_gz.py` reads `eds/favicon.ico`, creates a GZ archive with "ESP32P4-EIP.ico" inside
   - `embed_icon_gz.py` embeds the GZ file into C source files
   - Generates `build/opener/embedded_icon_file.h` and `embedded_icon_file.c`

3. **Memory File Wrapper:**
   - Since embedded files are in memory (not on a filesystem), a custom `memfile.c` implementation provides a `FILE*` interface
   - Uses `fopencookie` (GNU extension) or a custom implementation to create a FILE stream from memory buffers
   - Allows the File Object code to use standard `fread()`, `fseek()`, `ftell()` functions

#### File Object Services

The File Object implements the following CIP services:

- **GetAttributeSingle** (0x0E) - Read individual attributes
- **GetAttributeAll** (0x01) - Read all attributes
- **InitiateUpload** (0x4B) - Start file upload/download
- **UploadTransfer** (0x4F) - Transfer file data chunks
- **InitiateDownload** (0x4C) - Start file download (not supported for instances 200/201)
- **DownloadTransfer** (0x4E) - Transfer download data (not supported for instances 200/201)
- **ClearFile** (0x4D) - Clear file (not supported for instances 200/201)
- **Delete** (0x09) - Delete instance (not supported for instances 200/201)

#### File Object Attributes

All 12 standard File Object attributes are implemented:

1. **State** (USINT) - Current file state (NonExistent, FileEmpty, FileLoaded, etc.)
2. **InstanceName** (STRING_I) - Human-readable instance name
3. **FileFormatVersion** (UINT) - File format version
4. **FileName** (STRING_I) - Name of the file
5. **FileRevision** (USINT_USINT) - File revision (major.minor)
6. **FileSize** (UDINT) - Size of file in bytes
7. **FileChecksum** (UDINT) - CRC32 checksum of file
8. **InvocationMethod** (USINT) - How file is invoked
9. **FileSaveParameters** (UINT) - Save parameters
10. **FileAccessRule** (USINT) - Read/write access permissions
11. **FileEncodingFormat** (USINT) - Encoding format (Binary, CompressedFile, ASCIIText)
12. **TransferSize** (USINT) - Maximum transfer size per chunk

### Critical Implementation Details

#### File Revision Attribute

**Important**: The File Revision attribute (attribute 5) must match the revision specified in the EDS file's `[File]` section. RSLinx checks this before initiating the file download:

- EDS file contains: `Revision = 1.0;`
- File Object instance 200 must have: `file_revision.major_revision = 1`, `file_revision.minor_revision = 0`

If the revision doesn't match, RSLinx will not initiate the `InitiateUpload` service.

#### File Encoding Format

The File Encoding Format attribute (attribute 11) must be set correctly:
- **Instance 200 (EDS)**: Binary (0) - matches Rockwell Micro850 behavior
- **Instance 201 (Icon)**: CompressedFile (1) - matches Rockwell Micro850 behavior

#### File Name Format

- **Instance 200**: "EDS.txt" - matches Rockwell Micro850 convention
- **Instance 201**: "EDSCollection.gz" - matches Rockwell Micro850 convention

## Message Router Modifications

### Purpose

The Message Router Object (Class 0x02) was modified to include vendor-specific instance attributes that advertise which CIP objects are supported by the device. This allows EtherNet/IP Explorer to automatically discover available objects without manual configuration.

**Note**: RSLinx does not use Message Router `SupportedObjects` for discovery - it directly accesses File Object instances. The Message Router modifications are primarily for EtherNet/IP Explorer compatibility.

### Implementation Details

#### Source Code Location

Modifications are in:
```
components/opener/src/cip/cipmessagerouter.c
```

#### Instance #1 Attributes

Message Router instance #1 now includes two vendor-specific attributes:

**Attribute 1 - SupportedObjects:**
- **Type**: STRUCT (kCipAny)
- **Structure**:
  - `Number` (UINT) - Number of supported CIP classes
  - `ClassesId` (UINT array) - Array of CIP class codes
  - `MaxConnectionsSupported` (UINT) - Maximum number of connections
  - `NumberOfCurrentConnections` (UINT) - Current number of active connections

**Attribute 2 - ActiveConnections:**
- **Type**: STRUCT (kCipAny)
- **Structure**:
  - `active_connections_count` (UINT) - Number of entries in array
  - `active_connections` (UINT array) - Array of active connection IDs

#### Dynamic Class Discovery

The `SupportedObjects` attribute dynamically discovers all registered CIP classes at runtime:

1. Iterates through the global CIP class linked list (`g_first_object`)
2. Counts all registered classes
3. Allocates an array to hold class codes
4. Populates the array with class codes of all registered classes
5. Encodes the STRUCT with:
   - Number of classes
   - Array length (same as number)
   - Array of class codes
   - MaxConnectionsSupported (24)
   - NumberOfCurrentConnections (0)

**Important**: The STRUCT format does NOT include a separate "Number" field. The structure is:
```
ArrayLen (UINT)
Array elements (UINT array)
MaxConnectionsSupported (UINT)
NumberOfCurrentConnections (UINT)
```

The "Number" field shown in EtherNet/IP Explorer is derived from the array length, not encoded separately.

#### Instance Data Structure

A new structure `CipMessageRouterInstanceData` was added to hold instance-specific data:

```c
typedef struct {
    CipUint supported_objects_number;
    CipUint *supported_objects_class_ids;
    CipUint max_connections_supported;
    CipUint number_of_current_connections;
    CipUint active_connections_count;
    CipUint *active_connections;
} CipMessageRouterInstanceData;
```

This structure is allocated for instance #1 and assigned to `instance->data`.

#### Encoding Functions

**EncodeMessageRouterSupportedObjects:**
- Dynamically counts all registered CIP classes
- Allocates/reallocates the class ID array as needed
- Populates the array with class codes
- Encodes the STRUCT into the outgoing message

**EncodeMessageRouterActiveConnections:**
- Encodes the active connections count and array
- Currently all connections are initialized to 0 (unused)

#### Service Support

Message Router instance #1 now supports:
- **GetAttributeSingle** (0x0E) - Read individual attributes
- **GetAttributeAll** (0x01) - Read all attributes

### Critical Implementation Details

#### STRUCT Encoding Format

The `SupportedObjects` STRUCT must be encoded without a separate "Number" field. The correct format is:

```
UINT: ArrayLen (number of classes)
UINT[]: ClassesId array (class codes)
UINT: MaxConnectionsSupported
UINT: NumberOfCurrentConnections
```

**NOT**:
```
UINT: Number (separate field)  ← This causes misalignment!
UINT: ArrayLen
UINT[]: ClassesId array
UINT: MaxConnectionsSupported
UINT: NumberOfCurrentConnections
```

If a separate "Number" field is encoded, EtherNet/IP Explorer will misinterpret the data:
- The "Number" value will be read as the first array element
- The array will be shifted by one element
- The last array element will be read as `MaxConnectionsSupported`

#### Class Registration

The File Object class must be registered before the Message Router initializes, so it appears in the `SupportedObjects` list. The initialization order is:

1. File Object class is created and registered
2. File Object instances (200, 201) are created
3. Message Router initializes and can discover the File Object class

## Integration with EtherNet/IP Tools

### RSLinx

RSLinx uses the File Object to automatically download EDS files. **RSLinx does NOT query Message Router instance #1 `SupportedObjects`** - it goes directly to File Object instances:

1. **Direct Access**: RSLinx directly queries File Object instances 200 (0xC8) and 201 (0xC9) without using Message Router discovery
2. **Attribute Queries**: RSLinx reads File Object attributes in this order:
   - Attribute 4 (FileName) - Must be "EDS.txt"
   - Attribute 6 (FileSize) - File size in bytes
   - Attribute 11 (FileEncodingFormat) - Must be Binary (0)
   - Attribute 5 (FileRevision) - **CRITICAL**: Must match EDS file revision (1.0)
3. **Upload Initiation**: If all attributes are correct (especially File Revision), RSLinx calls `InitiateUpload` (0x4B)
4. **File Transfer**: RSLinx calls `UploadTransfer` (0x4F) repeatedly to download file chunks
5. **Installation**: RSLinx saves the downloaded EDS file and installs it

**Important**: If File Revision (attribute 5) is 0x00 or doesn't match the EDS file revision, RSLinx will not proceed to `InitiateUpload`, even if all other attributes are correct.

### EtherNet/IP Explorer

EtherNet/IP Explorer uses Message Router instance #1 to discover available objects:

1. **Query SupportedObjects**: Reads Message Router instance #1 attribute 1 (`SupportedObjects`)
2. **Display Objects**: Shows all classes in the `ClassesId` array in the object tree
3. **File Object Discovery**: If class 55 (File Object) is in the array, it appears in the tree

**Note**: The Message Router `SupportedObjects` attribute is used by EtherNet/IP Explorer for object discovery, but RSLinx uses a different approach and does not query this attribute.

## Build Process

### EDS File Embedding

1. Build system runs `embed_eds.py`:
   ```python
   python embed_eds.py eds/ESP32P4_OPENER.eds build/opener/embedded_eds_file.h build/opener/embedded_eds_file.c
   ```

2. Generated files contain:
   - `embedded_eds_file_data[]` - C array with EDS file bytes
   - `embedded_eds_file_size` - Size of EDS file

3. Files are compiled into the firmware

### Icon File Embedding

1. Build system runs `create_icon_gz.py`:
   ```python
   python create_icon_gz.py eds/favicon.ico build/opener/EDSCollection.gz
   ```
   - Creates a GZ-compressed archive
   - Contains "ESP32P4-EIP.ico" (renamed from favicon.ico)

2. Build system runs `embed_icon_gz.py`:
   ```python
   python embed_icon_gz.py build/opener/EDSCollection.gz build/opener/embedded_icon_file.h build/opener/embedded_icon_file.c
   ```

3. Generated files are compiled into the firmware

## Configuration

### Enabling File Object

The File Object is enabled via compile definition:
```cmake
CIP_FILE_OBJECT=1
```

This is set in `components/opener/CMakeLists.txt`.

### OpENer Configuration

In `components/opener/src/ports/ESP32/scale_application/opener_user_conf.h`:
```c
#define CIP_FILE_OBJECT 1
```

## Testing and Verification

### Verification Steps

1. **EtherNet/IP Explorer**:
   - Open EtherNet/IP Explorer
   - Browse to device → Message Router → Instance #1
   - Verify `SupportedObjects` attribute shows class 55 (File Object)
   - Verify File Object appears in object tree
   - Browse File Object instances 200 and 201
   - Verify attributes are correct

2. **RSLinx**:
   - Use RSLinx Device Description File Installation Tool
   - Select "Upload from device"
   - Device should appear in list
   - EDS file should download automatically
   - Verify EDS file is installed correctly

3. **Serial Logs**:
   - Check for File Object initialization messages
   - Verify both instances (200, 201) are created
   - Verify file sizes and states are correct

## Troubleshooting

### File Object Not Appearing in EtherNet/IP Explorer

**Symptoms**: File Object (class 55) not in object tree

**Possible Causes**:
1. Message Router instance #1 `SupportedObjects` not encoding correctly
2. File Object class not registered before Message Router initialization
3. STRUCT encoding format incorrect (separate "Number" field causing misalignment)

**Solution**: Verify Message Router instance #1 `SupportedObjects` attribute encoding. Ensure no separate "Number" field is encoded.

### RSLinx Not Downloading EDS File

**Symptoms**: RSLinx queries attributes but never calls `InitiateUpload`

**Root Cause**: File Revision (attribute 5) was 0x00 instead of matching the EDS file revision (1.0). RSLinx validates the File Revision before initiating upload.

**Possible Causes**:
1. **File Revision (attribute 5) doesn't match EDS file revision** - This is the most critical issue
2. File Name (attribute 4) incorrect format
3. File Encoding Format (attribute 11) incorrect
4. File State (attribute 1) not FileLoaded

**Solution**: Verify all File Object attributes match expected values. **The File Revision must match the EDS file revision exactly** (e.g., if EDS has `Revision = 1.0;`, then `file_revision.major_revision = 1` and `file_revision.minor_revision = 0`). RSLinx will not call `InitiateUpload` if the revision doesn't match.


