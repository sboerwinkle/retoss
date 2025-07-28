#include <stdlib.h>
#include <string.h>
#include <png.h>

// Based heavily on libpng's `example.c`
void png_read(char **data, int *width, int *height, char const *filename) {
	// Other unspecified fields will be empty-initialized as per the C++ standard.
	// This is good, as libpng explicitly wants the struct zeroed before you start.
	png_image image = {.version = PNG_IMAGE_VERSION};

	if (!png_image_begin_read_from_file(&image, filename)) {
		printf("ERROR: png.cpp reading %s: during initial read libpng says: %s\n", filename, image.message);
		// Zero this out before `cleanup` so we don't try to free some random bytes
		*data = NULL;
		goto cleanup;
	}

	*width = image.width;
	*height = image.height;
	/* Set the format in which to read the PNG file; this code chooses a
	 * simple sRGB format with a non-associated alpha channel, adequate to
	 * store most images.
	 */
	image.format = PNG_FORMAT_RGBA;

	/* Now allocate enough memory to hold the image in this format; the
	 * PNG_IMAGE_SIZE macro uses the information about the image (width,
	 * height and format) stored in 'image'.
	 */
	*data = (char*)malloc(PNG_IMAGE_SIZE(image));

	if (!*data) {
		printf("ERROR: png.cpp reading %s: Couldn't allocate %d bytes\n", filename, PNG_IMAGE_SIZE(image));
		goto cleanup;
	}

	// `background` not needed since our requested format includes alpha.
	// `row_stride` can be 0, since we're using the default minimum stride.
	//   (offset in memory between rows, measured in "components"(?))
	// `colormap` not needed since we didn't request an indexed format.
	if (!png_image_finish_read(&image, NULL/*background*/, *data, 0/*row_stride*/, NULL/*colormap*/)) {
		printf("ERROR: png.cpp reading %s: libpng says: %s\n", filename, image.message);
		goto cleanup;
	}

	// Happy-path exit. We've already assigned `width`, `height`, and `data`,
	// and libpng has now successfully read into `data`.
	return;

	// Common error handling / cleanup logic. I use `goto`s to get here rather than
	// invert all my `if` checks and nest the happy path several indents deep.
	cleanup:
	*width = *height = 0;
	free(*data);
	*data = NULL;
	// This may not be necessary (depending on when we had our error),
	// but it's always safe to do.
	png_image_free(&image);
}
