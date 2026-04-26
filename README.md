# Random QOL Scripts

Utility suite: image compression, disk mapping, file encryption, duplicate removal, clipboard operations.

## Tools

| Binary              | Purpose                          | Launcher         |
| ------------------- | -------------------------------- | ---------------- |
| `imageshrinker.exe` | Reduce image file size (JPG/PNG) | `shrink.bat`     |
| `compressor.exe`    | Archive/compress files           | Direct Execution |
| `encryptor.exe`     | File encryption/decryption       | `crypt.bat`      |
| `dupedeleter.exe`   | Find & remove duplicate files    | Direct execution |

## Python Scripts

| Script          | Purpose                            |
| --------------- | ---------------------------------- |
| `dashboard.py`  | System resource monitoring         |
| `disk_map.py`   | Disk usage visualization           |
| `tree_maker.py` | Directory structure tree generator |

## Quick Start

### Batch Commands

```cmd
autobuild.bat          # Build all C++ binaries
shrink.bat <image>     # Compress image
crypt.bat <file>       # Encrypt file
mapit.bat              # Map disk usage
copypaste.bat          # Robocopy
```

### Python

```bash
python python/dashboard.py
python python/disk_map.py
python python/tree_maker.py
```

## Requirements

- **OS:** Windows
- **Python:** 3.8+
- **Build:** MSVC/MinGW C++ compiler (for compilation)

## Project Structure

├── bin/ # Compiled executables
├── src/ # C++ source files
├── python/ # Python utilities
└── \*.bat # Batch launchers

## Building

```cmd
autobuild.bat
```

Compiles all C++ sources to `/bin/`.

## Notes

- Image compression uses STB image library (headers included)
- All Python scripts are standalone
- Batch files assume binaries are pre-built in `/bin/`
