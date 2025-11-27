# NAU7802 Component Checklist

This document verifies that all required files are present for using this component in other projects.

## Required Files

### Core Driver Files
- [x] `nau7802.c` - Main driver implementation
- [x] `include/nau7802.h` - Main driver header
- [x] `nau7802_calibration_storage.c` - Calibration storage implementation
- [x] `include/nau7802_calibration_storage.h` - Calibration storage header

### Build System Files
- [x] `CMakeLists.txt` - ESP-IDF component registration
- [x] `idf_component.yml` - Component Manager manifest (optional but recommended)

### Documentation
- [x] `README.md` - Main documentation
- [x] `LICENSE` - License file
- [x] `docs/mainpage.md` - Doxygen main page
- [x] `Doxyfile.in` - Doxygen configuration template

### Examples
- [x] `examples/example_basic.c` - Basic usage example
- [x] `examples/example_calibration.c` - Calibration example
- [x] `examples/example_interrupt.c` - Interrupt example

## Dependencies

The component requires:
- `driver` - ESP-IDF driver component (for I2C)
- `nvs_flash` - ESP-IDF NVS flash component (for calibration storage)

## Usage in Other Projects

### Method 1: Copy Component Folder
1. Copy the entire `nau7802` folder to your project's `components` directory
2. Add `nau7802` to your component's `REQUIRES` in `CMakeLists.txt`:
   ```cmake
   idf_component_register(
       ...
       REQUIRES nau7802
   )
   ```

### Method 2: Component Manager (if published)
Add to your project's `idf_component.yml`:
```yaml
dependencies:
  nau7802:
    git: "https://github.com/yourusername/nau7802-esp-idf.git"
    # or
    path: "../path/to/nau7802"
```

## Include Files

In your code, include:
```c
#include "nau7802.h"                          // Main driver
#include "nau7802_calibration_storage.h"      // Optional: calibration storage
```

## Verification

All files are self-contained within the `components/nau7802` directory. No dependencies on files outside this directory.

