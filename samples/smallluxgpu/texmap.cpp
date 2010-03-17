/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include <ImfRgbaFile.h>
#include <ImfArray.h>
#include <ImathBox.h>

#include <png.h>

#include "texmap.h"

using namespace std;

TextureMap::TextureMap(const string &fileName) {
	// Check if it is a EXR or PNG texture
	if ((fileName.length() >= 4) && (fileName.substr(fileName.length() - 4) == ".exr")) {
		// Read the EXR file
		cerr << "Reading HDR texture map: " << fileName << endl;
		Imf::RgbaInputFile file(fileName.c_str());

		Imath::Box2i dw = file.dataWindow();
		width = dw.max.x - dw.min.x + 1;
		height = dw.max.y - dw.min.y + 1;

		Imf::Array2D<Imf::Rgba> img;
		img.resizeErase(height, width);
		file.setFrameBuffer(&img[0][0] - dw.min.x - dw.min.y * width, 1, width);
		file.readPixels(dw.min.y, dw.max.y);

		cerr << "HDR texture map size: " << width << "x" << height << " (" << width * height *sizeof(Spectrum) / 1024 << "Kbytes)" << endl;
		pixels = new Spectrum[width * height];
		for (unsigned int y = 0; y < height; ++y) {
			for (unsigned int x = 0; x < width; ++x) {
				const unsigned int idx = y * width + x;

				pixels[idx].r = img[y][x].r;
				pixels[idx].g = img[y][x].g;
				pixels[idx].b = img[y][x].b;
			}
		}
	} else {
		// Load the png file
		cerr << "Reading PNG texture map: " << fileName << endl;

		png_byte header[8];
		FILE *file = fopen(fileName.c_str(), "rb");
		if (!file)
			throw runtime_error("Unable to open texture map file: " + fileName);

		if (!fread(header, 1, 8, file))
			throw runtime_error("Error reading the texture map file: " + fileName);
		if (png_sig_cmp(header, 0, 8))
			throw runtime_error("Texture map file isn't in PNG format: " + fileName);

		png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		png_infop info = png_create_info_struct(png);

		png_init_io(png, file);
		png_set_sig_bytes(png, 8);

		png_read_info(png, info);

		width = info->width;
		height = info->height;
		cerr << "PNG texture map size: " << width << "x" << height << " (" <<
				width * height * sizeof(Spectrum) / 1024 << "Kbytes)" << endl;

		if (info->color_type != PNG_COLOR_TYPE_RGB)
			throw runtime_error("Texture map PNG file must be in RGB format");

		png_read_update_info(png, info);

		png_bytep *byteImage = new png_bytep[sizeof(png_bytep) * height];
		for (unsigned int y = 0; y < height; y++)
			byteImage[y] = new png_byte[info->rowbytes];

		png_read_image(png, byteImage);
		fclose(file);

		pixels = new Spectrum[width * height];
		for (unsigned int y = 0; y < height; y++) {
			const png_byte *row = byteImage[y];

			for (unsigned int x = 0; x < width; x++) {
				size_t idx = width * y + x;
				const png_byte *ptr = &(row[x * 3]);

				pixels[idx].r = ptr[0] / 255.0f;
				pixels[idx].g = ptr[1] / 255.0f;
				pixels[idx].b = ptr[2] / 255.0f;
			}
		}

		for (unsigned int y = 0; y < height; y++)
			delete[] byteImage[y];
		delete[] byteImage;
	}
}

TextureMap::TextureMap(Spectrum *cols, const unsigned int w, const unsigned int h) {
	pixels = cols;
	width = w;
	height = h;
}

TextureMap::~TextureMap() {
	delete[] pixels;
}

TextureMapCache::TextureMapCache() {
}

TextureMapCache::~TextureMapCache() {
	for (std::map<std::string, TextureMap *>::const_iterator it = maps.begin(); it != maps.end(); ++it)
		delete it->second;
}

TextureMap *TextureMapCache::GetTextureMap(const string &fileName) {
	// Check if the texture map has been already loaded
	std::map<std::string, TextureMap *>::const_iterator it = maps.find(fileName);

	if (it == maps.end()) {
		// I have yet to load the file

		TextureMap *tm = new TextureMap(fileName);
		maps.insert(std::make_pair(fileName, tm));

		return tm;
	} else {
		cerr << "Cached texture map: " << fileName << endl;
		return it->second;
	}
}