#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define HEADER_SIZE 16
#define PRG_ROM_SIZE 16384
#define CHR_ROM_BANK_SIZE 8192

typedef struct {
    int tile_width;
    int tile_height;
    size_t tiles_per_row;
    size_t total_tiles;
} Metadata;

typedef struct {
    int index;
    char filename[256];
    int position_x;
    int position_y;
} TileMetadata;

typedef struct {
    uint8_t *pixels;
    int width;
    int height;
} Tile;

int create_directory_recursively(const char *dir_path) {
    char tmp[PATH_MAX];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", dir_path);
    len = strlen(tmp);
    if(tmp[len - 1] == '/')
        tmp[len - 1] = 0;

    for(p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            if(mkdir(tmp, S_IRWXU) != 0) {
                if(errno != EEXIST) {
                    perror("Error creating directory");
                    return -1;
                }
            }
            *p = '/';
        }
    }

    if(mkdir(tmp, S_IRWXU) != 0) {
        if(errno != EEXIST) {
            perror("Error creating directory");
            return -1;
        }
    }

    return 0;
}

int parse_palette(const char *filename, uint8_t palette[4][3]) {
    FILE *pf = fopen(filename, "r");
    if (!pf) {
        perror("Error opening palette file");
        return -1;
    }

    char line[256];
    for (int i = 0; i < 4; i++) {
        if (!fgets(line, sizeof(line), pf)) {
            fprintf(stderr, "Error: Palette file must contain exactly 4 lines.\n");
            fclose(pf);
            return -1;
        }

        if (sscanf(line, "%hhu %hhu %hhu", &palette[i][0], &palette[i][1], &palette[i][2]) != 3) {
            fprintf(stderr, "Error: Invalid format in palette file on line %d.\n", i + 1);
            fclose(pf);
            return -1;
        }

        for (int j = 0; j < 3; j++) {
            if (palette[i][j] > 255) {
                fprintf(stderr, "Error: RGB values must be between 0 and 255 on line %d.\n", i + 1);
                fclose(pf);
                return -1;
            }
        }
    }

    fclose(pf);
    return 0;
}

int generate_json_metadata(const char *metadata_path, Metadata *meta, TileMetadata *tiles_meta, size_t num_tiles) {
    FILE *mf = fopen(metadata_path, "w");
    if (!mf) {
        perror("Error opening metadata file for writing");
        return -1;
    }

    fprintf(mf, "{\n");
    fprintf(mf, "    \"tile_width\": %d,\n", meta->tile_width);
    fprintf(mf, "    \"tile_height\": %d,\n", meta->tile_height);
    fprintf(mf, "    \"tiles_per_row\": %zu,\n", meta->tiles_per_row);
    fprintf(mf, "    \"total_tiles\": %zu,\n", meta->total_tiles);
    fprintf(mf, "    \"tiles\": [\n");

    for (size_t t = 0; t < num_tiles; t++) {
        fprintf(mf, "        {\n");
        fprintf(mf, "            \"index\": %d,\n", tiles_meta[t].index);
        fprintf(mf, "            \"filename\": \"%s\",\n", tiles_meta[t].filename);
        fprintf(mf, "            \"position\": { \"x\": %d, \"y\": %d }\n", tiles_meta[t].position_x, tiles_meta[t].position_y);
        if (t == num_tiles - 1) {
            fprintf(mf, "        }\n");
        } else {
            fprintf(mf, "        },\n");
        }
    }

    fprintf(mf, "    ]\n");
    fprintf(mf, "}\n");

    fclose(mf);
    return 0;
}

int generate_xml_metadata(const char *metadata_path, Metadata *meta, TileMetadata *tiles_meta, size_t num_tiles) {
    FILE *mf = fopen(metadata_path, "w");
    if (!mf) {
        perror("Error opening metadata file for writing");
        return -1;
    }

    fprintf(mf, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(mf, "<Tilesheet>\n");
    fprintf(mf, "    <TileWidth>%d</TileWidth>\n", meta->tile_width);
    fprintf(mf, "    <TileHeight>%d</TileHeight>\n", meta->tile_height);
    fprintf(mf, "    <TilesPerRow>%zu</TilesPerRow>\n", meta->tiles_per_row);
    fprintf(mf, "    <TotalTiles>%zu</TotalTiles>\n", meta->total_tiles);
    fprintf(mf, "    <Tiles>\n");

    for (size_t t = 0; t < num_tiles; t++) {
        fprintf(mf, "        <Tile index=\"%d\">\n", tiles_meta[t].index);
        fprintf(mf, "            <Filename>%s</Filename>\n", tiles_meta[t].filename);
        fprintf(mf, "            <Position x=\"%d\" y=\"%d\" />\n", tiles_meta[t].position_x, tiles_meta[t].position_y);
        fprintf(mf, "        </Tile>\n");
    }

    fprintf(mf, "    </Tiles>\n");
    fprintf(mf, "</Tilesheet>\n");

    fclose(mf);
    return 0;
}

int export_individual_tiles(Tile *tiles, size_t num_tiles, const uint8_t palette[4][3], const char *dir_name, int tile_width, int tile_height, TileMetadata *tiles_meta) {
    char filename[PATH_MAX];
    size_t img_size = tile_width * tile_height * 3;
    uint8_t *tile_image = malloc(img_size);
    if (!tile_image) {
        fprintf(stderr, "Memory allocation failed for tile image buffer\n");
        return -1;
    }

    size_t tiles_per_row = 16;

    for (size_t t = 0; t < num_tiles; t++) {
        for (int y = 0; y < tile_height; y++) {
            for (int x = 0; x < tile_width; x++) {
                uint8_t color_idx = tiles[t].pixels[y * tile_width + x];
                tile_image[(y * tile_width + x) * 3 + 0] = palette[color_idx][0];
                tile_image[(y * tile_width + x) * 3 + 1] = palette[color_idx][1];
                tile_image[(y * tile_width + x) * 3 + 2] = palette[color_idx][2];
            }
        }
        snprintf(filename, sizeof(filename), "%s/tile_%04zu.png", dir_name, t + 1);
        if (!stbi_write_png(filename, tile_width, tile_height, 3, tile_image, tile_width * 3)) {
            fprintf(stderr, "Failed to write PNG: %s\n", filename);
            free(tile_image);
            return -1;
        }

        if (tiles_meta) {
            tiles_meta[t].index = t + 1;
            const char *base_filename = strrchr(filename, '/');
            if (base_filename)
                base_filename++;
            else
                base_filename = filename;
            strncpy(tiles_meta[t].filename, base_filename, sizeof(tiles_meta[t].filename) - 1);
            tiles_meta[t].filename[sizeof(tiles_meta[t].filename) - 1] = '\0';
            tiles_meta[t].position_x = (t % tiles_per_row) * tile_width;
            tiles_meta[t].position_y = (t / tiles_per_row) * tile_height;
        }

        if ((t + 1) % 100 == 0 || t == num_tiles - 1) {
            printf("Exported %zu/%zu tiles\r", t + 1, num_tiles);
            fflush(stdout);
        }
    }

    free(tile_image);
    printf("\n");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_rom> <output_path> [--dir | -d] [-p <palette_file>] [-w <tile_width>] [-h <tile_height>] [-m <metadata_path>] [-f <format>]\n", argv[0]);
        return 1;
    }

    const char *input_rom = argv[1];
    const char *output_path = argv[2];
    int export_dir = 0;
    const char *palette_file = NULL;
    int tile_width = 8;
    int tile_height = 8;
    const char *metadata_path = NULL;
    const char *metadata_format = "json";

    int opt;
    while ((opt = getopt(argc - 2, argv + 2, "dp:w:h:m:f:")) != -1) {
        switch (opt) {
            case 'd':
                export_dir = 1;
                break;
            case 'p':
                palette_file = optarg;
                break;
            case 'w':
                tile_width = atoi(optarg);
                if (tile_width <= 0) {
                    fprintf(stderr, "Error: Tile width must be a positive integer.\n");
                    return 1;
                }
                break;
            case 'h':
                tile_height = atoi(optarg);
                if (tile_height <= 0) {
                    fprintf(stderr, "Error: Tile height must be a positive integer.\n");
                    return 1;
                }
                break;
            case 'm':
                metadata_path = optarg;
                break;
            case 'f':
                if (strcmp(optarg, "json") == 0 || strcmp(optarg, "xml") == 0) {
                    metadata_format = optarg;
                } else {
                    fprintf(stderr, "Error: Unsupported metadata format '%s'. Supported formats are 'json' and 'xml'.\n", optarg);
                    return 1;
                }
                break;
            default:
                fprintf(stderr, "Usage: %s <input_rom> <output_path> [--dir | -d] [-p <palette_file>] [-w <tile_width>] [-h <tile_height>] [-m <metadata_path>] [-f <format>]\n", argv[0]);
                return 1;
        }
    }

    if (tile_width > 16 || tile_height > 16) {
        fprintf(stderr, "Warning: Unusually large tile size may impact performance and output readability.\n");
    }

    uint8_t palette[4][3] = {
        {0, 0, 0},
        {85, 85, 85},
        {170, 170, 170},
        {255, 255, 255}
    };

    if (palette_file) {
        if (parse_palette(palette_file, palette) != 0) {
            fprintf(stderr, "Failed to parse the palette file.\n");
            return 1;
        }
    }

    FILE *f = fopen(input_rom, "rb");
    if (!f) {
        perror("Error opening ROM file");
        return 1;
    }

    uint8_t header[HEADER_SIZE];
    if (fread(header, 1, HEADER_SIZE, f) != HEADER_SIZE) {
        fprintf(stderr, "Error reading ROM header\n");
        fclose(f);
        return 1;
    }

    if (header[0] != 'N' || header[1] != 'E' || header[2] != 'S' || header[3] != 0x1A) {
        fprintf(stderr, "Invalid iNES header\n");
        fclose(f);
        return 1;
    }

    uint8_t prg_banks = header[4];
    uint8_t chr_banks = header[5];
    if (chr_banks == 0) {
        fprintf(stderr, "No CHR ROM found\n");
        fclose(f);
        return 1;
    }

    if (fseek(f, prg_banks * PRG_ROM_SIZE, SEEK_CUR) != 0) {
        fprintf(stderr, "Error seeking PRG ROM\n");
        fclose(f);
        return 1;
    }

    size_t chr_size = chr_banks * CHR_ROM_BANK_SIZE;
    uint8_t *chr_data = malloc(chr_size);
    if (!chr_data) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(f);
        return 1;
    }

    if (fread(chr_data, 1, chr_size, f) != chr_size) {
        fprintf(stderr, "Error reading CHR ROM\n");
        free(chr_data);
        fclose(f);
        return 1;
    }

    fclose(f);

    size_t bytes_per_tile = tile_height * 2;

    if (chr_size % bytes_per_tile != 0) {
        fprintf(stderr, "Error: CHR ROM size (%zu bytes) is not divisible by the bytes per tile (%zu bytes). Please choose a different tile size.\n", chr_size, bytes_per_tile);
        free(chr_data);
        return 1;
    }

    size_t num_tiles = chr_size / bytes_per_tile;
    Tile *tiles = malloc(num_tiles * sizeof(Tile));
    if (!tiles) {
        fprintf(stderr, "Memory allocation failed for tiles\n");
        free(chr_data);
        return 1;
    }

    for (size_t t = 0; t < num_tiles; t++) {
        tiles[t].width = tile_width;
        tiles[t].height = tile_height;
        tiles[t].pixels = malloc(tile_width * tile_height * sizeof(uint8_t));
        if (!tiles[t].pixels) {
            fprintf(stderr, "Memory allocation failed for tile %zu\n", t + 1);
            for (size_t i = 0; i < t; i++) {
                free(tiles[i].pixels);
            }
            free(tiles);
            free(chr_data);
            return 1;
        }

        for (int row = 0; row < tile_height; row++) {
            uint8_t byte1 = chr_data[t * bytes_per_tile + row];
            uint8_t byte2 = chr_data[t * bytes_per_tile + row + tile_height];
            for (int col = 0; col < tile_width; col++) {
                uint8_t bit1 = (byte1 >> (7 - col)) & 1;
                uint8_t bit2 = (byte2 >> (7 - col)) & 1;
                tiles[t].pixels[row * tile_width + col] = (bit2 << 1) | bit1;
            }
        }
    }

    free(chr_data);

    Metadata meta;
    meta.tile_width = tile_width;
    meta.tile_height = tile_height;
    size_t tiles_per_row = export_dir ? 16 : 16;
    meta.tiles_per_row = tiles_per_row;
    meta.total_tiles = num_tiles;

    TileMetadata *tiles_meta = NULL;
    if (metadata_path) {
        tiles_meta = malloc(num_tiles * sizeof(TileMetadata));
        if (!tiles_meta) {
            fprintf(stderr, "Memory allocation failed for metadata\n");
            for (size_t t = 0; t < num_tiles; t++) {
                free(tiles[t].pixels);
            }
            free(tiles);
            return 1;
        }
    }

    if (export_dir) {
        if (create_directory_recursively(output_path) != 0) {
            for (size_t t = 0; t < num_tiles; t++) {
                free(tiles[t].pixels);
            }
            free(tiles);
            if (tiles_meta)
                free(tiles_meta);
            return 1;
        }

        if (export_individual_tiles(tiles, num_tiles, palette, output_path, tile_width, tile_height, tiles_meta) != 0) {
            for (size_t t = 0; t < num_tiles; t++) {
                free(tiles[t].pixels);
            }
            free(tiles);
            if (tiles_meta)
                free(tiles_meta);
            return 1;
        }

        for (size_t t = 0; t < num_tiles; t++) {
            free(tiles[t].pixels);
        }
        free(tiles);
        printf("Exported %zu tiles to directory: %s\n", num_tiles, output_path);
    } else {
        size_t tiles_per_row_sheet = 16;
        size_t sheet_width = tiles_per_row_sheet * tile_width;
        size_t sheet_height = ((num_tiles + tiles_per_row_sheet - 1) / tiles_per_row_sheet) * tile_height;

        size_t img_size = sheet_width * sheet_height * 3;
        uint8_t *image = calloc(img_size, 1);
        if (!image) {
            fprintf(stderr, "Memory allocation failed for image\n");
            for (size_t t = 0; t < num_tiles; t++) {
                free(tiles[t].pixels);
            }
            free(tiles);
            if (tiles_meta)
                free(tiles_meta);
            return 1;
        }

        if (tiles_meta) {
            for (size_t t = 0; t < num_tiles; t++) {
                tiles_meta[t].index = t + 1;
                strncpy(tiles_meta[t].filename, output_path, sizeof(tiles_meta[t].filename) - 1);
                tiles_meta[t].filename[sizeof(tiles_meta[t].filename) - 1] = '\0';
                tiles_meta[t].position_x = (t % tiles_per_row_sheet) * tile_width;
                tiles_meta[t].position_y = (t / tiles_per_row_sheet) * tile_height;
            }
        }

        for (size_t t = 0; t < num_tiles; t++) {
            size_t row = t / tiles_per_row_sheet;
            size_t col = t % tiles_per_row_sheet;
            for (int y = 0; y < tile_height; y++) {
                for (int x = 0; x < tile_width; x++) {
                    size_t pixel_x = col * tile_width + x;
                    size_t pixel_y = row * tile_height + y;
                    size_t index = (pixel_y * sheet_width + pixel_x) * 3;
                    uint8_t color_idx = tiles[t].pixels[y * tile_width + x];
                    image[index + 0] = palette[color_idx][0];
                    image[index + 1] = palette[color_idx][1];
                    image[index + 2] = palette[color_idx][2];
                }
            }

            if (tiles_meta) {
                tiles_meta[t].index = t + 1;
                strncpy(tiles_meta[t].filename, output_path, sizeof(tiles_meta[t].filename) - 1);
                tiles_meta[t].filename[sizeof(tiles_meta[t].filename) - 1] = '\0';
                tiles_meta[t].position_x = (t % tiles_per_row_sheet) * tile_width;
                tiles_meta[t].position_y = (t / tiles_per_row_sheet) * tile_height;
            }

            if ((t + 1) % 100 == 0 || t == num_tiles - 1) {
                printf("Processed %zu/%zu tiles\r", t + 1, num_tiles);
                fflush(stdout);
            }
        }
        printf("\n");

        for (size_t t = 0; t < num_tiles; t++) {
            free(tiles[t].pixels);
        }
        free(tiles);

        if (!stbi_write_png(output_path, sheet_width, sheet_height, 3, image, sheet_width * 3)) {
            fprintf(stderr, "Failed to write PNG\n");
            free(image);
            if (tiles_meta)
                free(tiles_meta);
            return 1;
        }

        free(image);
        printf("Tilesheet saved to %s\n", output_path);
    }

    if (metadata_path) {
        int meta_result = 0;
        if (strcmp(metadata_format, "json") == 0) {
            meta_result = generate_json_metadata(metadata_path, &meta, tiles_meta, num_tiles);
        } else if (strcmp(metadata_format, "xml") == 0) {
            meta_result = generate_xml_metadata(metadata_path, &meta, tiles_meta, num_tiles);
        }

        if (meta_result != 0) {
            fprintf(stderr, "Failed to generate metadata file.\n");
        } else {
            printf("Metadata file generated at %s\n", metadata_path);
        }

        if (tiles_meta)
            free(tiles_meta);
    }

    return 0;
}
