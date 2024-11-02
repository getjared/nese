<div align="center">

## ｎｅｓｅ
**[ a NES CHR ROM tile extractor ]**

[![License: Unlicense](https://img.shields.io/badge/License-Unlicense-pink.svg)](http://unlicense.org/)
[![Made with C](https://img.shields.io/badge/Made%20with-C-purple.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
</div>

## ✧ philosophy
a quick and easy thing i needed for getting stuff out of my legally owned nes roms

## ✧ preview
<p align="center">
    <img width="500" src="preview.png" alt="nese tilesheet output example">
</p>

## ✧ features
- 🎮 extracts CHR ROM data from NES files
- 🖼️ exports to individual PNG tiles or complete tilesheets
- 🎨 customizable palette support
- 📏 adjustable tile dimensions
- 📄 generates JSON/XML metadata
- ⚡ fast and efficient processing
- 🔧 minimal dependencies

## ✧ installation
```bash
git clone https://github.com/your-username/nese.git
cd nese
make
sudo make install
```

## ✧ dependencies
- 📝 c compiler (gcc or clang)
- 🔧 make
- 📚 stb_image_write.h (included)

## ✧ usage
### basic usage
```bash
nese <input_rom> <output_path> [options]
```

### options
| option | description |
|--------|-------------|
| `-d, --dir` | export tiles to individual files |
| `-p <file>` | specify custom palette file |
| `-w <width>` | set tile width (default: 8) |
| `-h <height>` | set tile height (default: 8) |
| `-m <path>` | generate metadata file |
| `-f <format>` | metadata format (json/xml) |

### examples
Export as tilesheet:
```bash
nese game.nes tiles.png
```

Export individual tiles:
```bash
nese game.nes tiles/ -d
```

Custom palette with metadata:
```bash
nese game.nes tiles.png -p palette.txt -m metadata.json
```

## ✧ configuration
### palette format
Create a text file with 4 RGB color values (0-255):
```
0 0 0
85 85 85
170 170 170
255 255 255
```

### metadata output
JSON format:
```json
{
    "tile_width": 8,
    "tile_height": 8,
    "tiles_per_row": 16,
    "total_tiles": 256,
    "tiles": [
        {
            "index": 1,
            "filename": "tile_0001.png",
            "position": { "x": 0, "y": 0 }
        }
    ]
}
```

## ✧ inspiration
crafted with inspiration from:
- NES development tools
- Tile extraction utilities
- Image processing libraries

<div align="center">

```ascii
╭─────────────────────────╮
│  made with ♥ by you     │
╰─────────────────────────╯
```
</div>