/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef CAMERA_UTILITY_H_
#define CAMERA_UTILITY_H_

#ifdef __cplusplus
extern "C" {
#endif
/**
 * Image format definitions. These values shall be used as pixelFormat param
 * when calling TextureReader_readPixels() function.
 */
#define IMAGE_FORMAT_RGBA 0     // Image format for RGBA8888.
#define IMAGE_FORMAT_YUV420 1   // Image format for YUV420.

/**
 * Initialize texture reader.
 *
 * @param textureId: the id of the OpenGL texture.
 * @param textureTarget: the texture target type of the textureId. This can be
 *   either GL_TEXTURE_2D or GL_TEXTURE_EXTERNAL_OES.
 * @param textureWidth: the width of the texture, in pixels.
 * @param textureHeight: the height of the texture, in pixels.
 * @param useComputeShader: compute shader method is a lot faster but does not
 *   work on some devices.
 */
void TextureReader_initialize(int textureId, int textureTarget,
                              int textureWidth, int textureHeight,
                              bool useComputeShader = false);
/**
 * Read pixels from the texture reader with a specific format.
 *
 * @param bufferSize[output]: the size of the buffer returned, in bytes.
 * @pixelFormat: the format of the pixel to be read. This can be either
 *   IMAGE_FORMAT_RGBA or IMAGE_FORMAT_YUV.
 * @return: the internal buffer of the pixels. The caller cannot release this
 *   buffer. The same buffer gets reused in multiple reads.
 */
unsigned char* TextureReader_readPixels(int* bufferSize, int pixelFormat);

/**
 * Release texture reader and any internal resource.
 */
void TextureReader_release();

#ifdef __cplusplus
}
#endif

#endif // CAMERA_UTILITY_H_
