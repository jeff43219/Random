# Random QOL Scripts

Collection of productivity utilities: image compression, disk mapping, file encryption, duplicate removal.

## Tools

| Tool                | Purpose                    | Usage                         |
| ------------------- | -------------------------- | ----------------------------- |
| `imageshrinker.exe` | Reduce image file size     | `.\shrink.bat <image_path>`   |
| `compressor.exe`    | General file compression   | `.\autobuild.bat`             |
| `encryptor.exe`     | Encrypt/decrypt files      | `.\crypt.bat <file>`          |
| `dupedeleter.exe`   | Find and remove duplicates | `dupedeleter.exe <directory>` |

## Setup

### Python Scripts

```bash
python python/dashboard.py      # System dashboard
python python/disk_map.py       # Disk usage map
python python/tree_maker.py     # Directory tree generator
```

### Batch Files

- `autobuild.bat` – Compile C++ sources
- `shrink.bat` – Compress images
- `crypt.bat` – Encrypt files
- `mapit.bat` – Map disk usage
- `run_shrinker.bat` – Execute imageshrinker

## Building from Source

```bash
autobuild.bat
```

Requires: MSVC or MinGW C++ compiler.

## Requirements

- Python 3.8+
- C++ compiler (for building from source)
- Windows OS

## License

[Specify license if applicable]
