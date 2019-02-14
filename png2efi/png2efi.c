// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2017 lambdadroid

#include <stdlib.h>
#include <stdio.h>
#include <png.h>

static int write_image(const char *identifier, const char *filename, png_image *image, png_bytep buffer) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        return EXIT_FAILURE;
    }

    fprintf(file,
            "#include \"graphics.h\"\n"
            "static EFI_GRAPHICS_OUTPUT_BLT_PIXEL %s_blt[] = {", identifier);

    for (png_bytep end = buffer + PNG_IMAGE_SIZE(*image); buffer < end; buffer += 3) {
        fprintf(file, "{%d,%d,%d,0}, ", buffer[0], buffer[1], buffer[2]);
    }

    fprintf(file, "};\n");
    fprintf(file, "struct graphics_image %s_image = {%u, %u, %s_blt};\n",
            identifier, image->width, image->height, identifier);

    return fclose(file);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: png2efi <identifier> <input.png> <output.c>\n");
        return EXIT_FAILURE;
    }

    png_image image = {
        .version = PNG_IMAGE_VERSION
    };

    if (!png_image_begin_read_from_file(&image, argv[2])) {
        fprintf(stderr, "Failed to open PNG file: %s\n", image.message);
        return EXIT_FAILURE;
    }

    image.format = PNG_FORMAT_BGR;

    png_byte buffer[PNG_IMAGE_SIZE(image)];
    if (!png_image_finish_read(&image, NULL, buffer, 0, NULL)) {
        fprintf(stderr, "Failed to read PNG file: %s\n", image.message);
        return EXIT_FAILURE;
    }

    if (write_image(argv[1], argv[3], &image, buffer)) {
        perror("Failed to write C file");
        return EXIT_FAILURE;
    }

    png_image_free(&image);
    return 0;
}
