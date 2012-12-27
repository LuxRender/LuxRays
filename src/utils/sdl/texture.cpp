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

#if defined (WIN32)
#include <windows.h>
#endif

#include <FreeImage.h>

#include "luxrays/utils/sdl/sdl.h"
#include "luxrays/utils/sdl/texture.h"

using namespace luxrays;
using namespace luxrays::sdl;

//------------------------------------------------------------------------------
// TextureDefinitions
//------------------------------------------------------------------------------

TextureDefinitions::TextureDefinitions() { }

TextureDefinitions::~TextureDefinitions() {
	for (std::vector<Texture *>::const_iterator it = texs.begin(); it != texs.end(); ++it)
		delete (*it);
}

void TextureDefinitions::DefineTexture(const std::string &name, Texture *t) {
	texs.push_back(t);
	texsByName.insert(std::make_pair(name, t));
	indexByName.insert(std::make_pair(name, texs.size() - 1));
}

Texture *TextureDefinitions::GetTexture(const std::string &name) {
	// Check if the texture has been already defined
	std::map<std::string, Texture *>::const_iterator it = texsByName.find(name);

	if (it == texsByName.end())
		throw std::runtime_error("Reference to an undefined texture: " + name);
	else
		return it->second;
}

std::vector<std::string> TextureDefinitions::GetTextureNames() const {
	std::vector<std::string> names;
	names.reserve(texs.size());
	for (std::map<std::string, Texture *>::const_iterator it = texsByName.begin(); it != texsByName.end(); ++it)
		names.push_back(it->first);

	return names;
}

void TextureDefinitions::DeleteTexture(const std::string &name) {
	const u_int index = GetTextureIndex(name);
	texs.erase(texs.begin() + index);
	texsByName.erase(name);
	indexByName.erase(name);
}

u_int TextureDefinitions::GetTextureIndex(const std::string &name) {
	// Check if the texture has been already defined
	std::map<std::string, u_int>::const_iterator it = indexByName.find(name);

	if (it == indexByName.end())
		throw std::runtime_error("Reference to an undefined texture: " + name);
	else
		return it->second;
}

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

ImageMap::ImageMap(const std::string &fileName, const float g) {
	gamma = g;

	SDL_LOG("Reading texture map: " << fileName);

	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(fileName.c_str(), 0);
	if(fif == FIF_UNKNOWN)
		fif = FreeImage_GetFIFFromFilename(fileName.c_str());

	if((fif != FIF_UNKNOWN) && FreeImage_FIFSupportsReading(fif)) {
		FIBITMAP *dib = FreeImage_Load(fif, fileName.c_str(), 0);

		if (!dib)
			throw std::runtime_error("Unable to read texture map: " + fileName);

		width = FreeImage_GetWidth(dib);
		height = FreeImage_GetHeight(dib);

		u_int pitch = FreeImage_GetPitch(dib);
		FREE_IMAGE_TYPE imageType = FreeImage_GetImageType(dib);
		u_int bpp = FreeImage_GetBPP(dib);
		BYTE *bits = (BYTE *)FreeImage_GetBits(dib);

		if ((imageType == FIT_RGBAF) && (bpp == 128)) {
			channelCount = 3;
			SDL_LOG("HDR RGB (128bit) texture map size: " << width << "x" << height << " (" <<
					width * height * sizeof(float) * channelCount / 1024 << "Kbytes)");
			pixels = new float[width * height * channelCount];

			for (u_int y = 0; y < height; ++y) {
				FIRGBAF *pixel = (FIRGBAF *)bits;
				for (u_int x = 0; x < width; ++x) {
					const u_int offset = (x + (height - y - 1) * width) * channelCount;

					if (gamma == 1.f) {
						pixels[offset] = pixel[x].red;
						pixels[offset + 1] = pixel[x].green;
						pixels[offset + 2] = pixel[x].blue;
					} else {
						// Apply reverse gamma correction
						pixels[offset] = powf(pixel[x].red, gamma);
						pixels[offset + 1] = powf(pixel[x].green, gamma);
						pixels[offset + 2] = powf(pixel[x].blue, gamma);
					}
				}

				// Next line
				bits += pitch;
			}
		} else if ((imageType == FIT_RGBF) && (bpp == 96)) {
			channelCount = 3;
			SDL_LOG("HDR RGB (96bit) texture map size: " << width << "x" << height << " (" <<
					width * height * sizeof(float) * channelCount / 1024 << "Kbytes)");
			pixels = new float[width * height * channelCount];

			for (u_int y = 0; y < height; ++y) {
				FIRGBF *pixel = (FIRGBF *)bits;
				for (u_int x = 0; x < width; ++x) {
					const u_int offset = (x + (height - y - 1) * width) * channelCount;

					if (gamma == 1.f) {
						pixels[offset] = pixel[x].red;
						pixels[offset + 1] = pixel[x].green;
						pixels[offset + 2] = pixel[x].blue;
					} else {
						// Apply reverse gamma correction
						pixels[offset] = powf(pixel[x].red, gamma);
						pixels[offset + 1] = powf(pixel[x].green, gamma);
						pixels[offset + 2] = powf(pixel[x].blue, gamma);
					}
				}

				// Next line
				bits += pitch;
			}
		} else if ((imageType == FIT_BITMAP) && (bpp == 32)) {
			channelCount = 4;
			SDL_LOG("RGBA texture map size: " << width << "x" << height << " (" <<
					width * height * sizeof(float) * channelCount / 1024 << "Kbytes)");
			pixels = new float[width * height * channelCount];

			for (u_int y = 0; y < height; ++y) {
				BYTE *pixel = (BYTE *)bits;
				for (u_int x = 0; x < width; ++x) {
					const u_int offset = (x + (height - y - 1) * width) * channelCount;

					if (gamma == 1.f) {
						pixels[offset] = pixel[FI_RGBA_RED] / 255.f;
						pixels[offset + 1] = pixel[FI_RGBA_GREEN] / 255.f;
						pixels[offset + 2] = pixel[FI_RGBA_BLUE] / 255.f;
					} else {
						pixels[offset] = powf(pixel[FI_RGBA_RED] / 255.f, gamma);
						pixels[offset + 1] = powf(pixel[FI_RGBA_GREEN] / 255.f, gamma);
						pixels[offset + 2] = powf(pixel[FI_RGBA_BLUE] / 255.f, gamma);
					}
					pixels[offset + 3] = pixel[FI_RGBA_ALPHA] / 255.f;
					pixel += 4;
				}

				// Next line
				bits += pitch;
			}
		} else if (bpp == 24) {
			channelCount = 3;
			SDL_LOG("RGB texture map size: " << width << "x" << height << " (" <<
					width * height * sizeof(float) * channelCount / 1024 << "Kbytes)");
			pixels = new float[width * height * channelCount];

			for (u_int y = 0; y < height; ++y) {
				BYTE *pixel = (BYTE *)bits;
				for (u_int x = 0; x < width; ++x) {
					const u_int offset = (x + (height - y - 1) * width) * channelCount;

					if (gamma == 1.f) {
						pixels[offset] = pixel[FI_RGBA_RED] / 255.f;
						pixels[offset + 1] = pixel[FI_RGBA_GREEN] / 255.f;
						pixels[offset + 2] = pixel[FI_RGBA_BLUE] / 255.f;
					} else {
						pixels[offset] = powf(pixel[FI_RGBA_RED] / 255.f, gamma);
						pixels[offset + 1] = powf(pixel[FI_RGBA_GREEN] / 255.f, gamma);
						pixels[offset + 2] = powf(pixel[FI_RGBA_BLUE] / 255.f, gamma);
					}
					pixel += 3;
				}

				// Next line
				bits += pitch;
			}
		} else if (bpp == 8) {
			channelCount = 1;
			SDL_LOG("Grey texture map size: " << width << "x" << height << " (" <<
					width * height * sizeof(float) * channelCount / 1024 << "Kbytes)");
			pixels = new float[width * height];

			for (u_int y = 0; y < height; ++y) {
				BYTE pixel;
				for (u_int x = 0; x < width; ++x) {
					FreeImage_GetPixelIndex(dib, x, y, &pixel);
					const u_int offset = (x + (height - y - 1) * width) * channelCount;

					if (gamma == 1.f)
						pixels[offset] = pixel / 255.f;
					else
						pixels[offset] = powf(pixel / 255.f, gamma);
				}

				// Next line
				bits += pitch;
			}
		} else {
			std::stringstream msg;
			msg << "Unsupported bitmap depth (" << bpp << ") in a texture map: " << fileName;
			throw std::runtime_error(msg.str());
		}

		FreeImage_Unload(dib);
	} else
		throw std::runtime_error("Unknown image file format: " + fileName);
}

ImageMap::ImageMap(float *p, const float g, const u_int count,
		const u_int w, const u_int h) {
	gamma = g;

	pixels = p;

	channelCount = count;
	width = w;
	height = h;
}

ImageMap::~ImageMap() {
	delete[] pixels;
}

//------------------------------------------------------------------------------
// ImageMapCache
//------------------------------------------------------------------------------

ImageMapCache::ImageMapCache() {
}

ImageMapCache::~ImageMapCache() {
	for (size_t i = 0; i < imgMapInstances.size(); ++i)
		delete imgMapInstances[i];

	for (std::map<std::string, ImageMap *>::const_iterator it = maps.begin(); it != maps.end(); ++it)
		delete it->second;
}

ImageMap *ImageMapCache::GetImageMap(const std::string &fileName, const float gamma) {
	// Check if the texture map has been already loaded
	std::map<std::string, ImageMap *>::const_iterator it = maps.find(fileName);

	if (it == maps.end()) {
		// I have yet to load the file

		ImageMap *im = new ImageMap(fileName, gamma);
		maps.insert(std::make_pair(fileName, im));

		return im;
	} else {
		//SDL_LOG("Cached image map: " << fileName);
		ImageMap *im = (it->second);
		if (im->GetGamma() != gamma)
			throw std::runtime_error("Texture map: " + fileName + " can not be used with 2 different gamma");
		return im;
	}
}

void ImageMapCache::DefineImgMap(const std::string &name, ImageMap *tm) {
	SDL_LOG("Define ImageMap: " << name);
	maps.insert(std::make_pair(name, tm));
}

ImageMapInstance *ImageMapCache::GetImageMapInstance(const std::string &fileName, const float gamma,
	const float gain, const float uScale, const float vScale, const float uDelta, const float vDelta) {
	ImageMap *im = GetImageMap(fileName, gamma);
	ImageMapInstance *imi = new ImageMapInstance(im, gain, uScale, vScale, uDelta, vDelta);
	imgMapInstances.push_back(imi);

	return imi;
}

void ImageMapCache::GetImageMaps(std::vector<ImageMap *> &tms) {
	for (std::map<std::string, ImageMap *>::const_iterator it = maps.begin(); it != maps.end(); ++it)
		tms.push_back(it->second);
}
