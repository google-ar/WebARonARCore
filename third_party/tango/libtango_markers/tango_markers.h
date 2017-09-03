// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TANGO_MARKERS_TANGO_MARKERS_H_
#define TANGO_MARKERS_TANGO_MARKERS_H_

#include <tango_client_api.h>
#include <tango_support.h>

/// @file tango_markers.h
/// @brief File containing marker detection functions of Tango Support C API.
///   The Tango Support C API provides helper functions useful to external
///   developers for building Tango applications. This API is experimental and
///   subject to change.

#ifdef __cplusplus
extern "C" {
#endif

/// @defgroup MarkerDetectionSupport Marker Detection Support Functions
/// @brief Functions for detecting markers.
/// @{
/// @brief A type to define all combinations of markers supported.
typedef enum {
  TANGO_MARKERS_MARKER_ARTAG = 0x01,
  TANGO_MARKERS_MARKER_QRCODE = 0x02,
} TangoMarkers_MarkerType;

/// @brief A structure to define parameters for passing marker detection
/// parameters.
typedef struct TangoMarkers_DetectParam {
  /// Type of marker to be detected.
  TangoMarkers_MarkerType type;

  /// The physical size of the marker in meters.
  double marker_size;
} TangoMarkers_DetectParam;

/// @brief A structure to define contents of a marker, which can be any of the
/// marker types supported.
typedef struct TangoMarkers_Marker {
  /// The type of the marker.
  TangoMarkers_MarkerType type;

  /// The timestamp of the image from which the marker was detected.
  double timestamp;

  /// The content of the marker. For AR tags, this is the string format of the
  /// tag id. For QR codes, this is the string content of the code.
  char* content;

  /// The size of content, in bytes.
  int content_size;

  /// Marker corners in input image pixel coordinates.
  /// For all marker types, the first corner is the lower left corner, the
  /// second corner is the lower right corner, the third corner is the upper
  /// right corner, and the last corner is the upper left corner.
  ///
  /// P3 -- P2
  /// |     |
  /// P0 -- P1
  ///
  float corners_2d[4][2];

  /// Marker corners in the output frame, which is defined by the translation
  /// and orientation pair passed to TangoMarkers_detectMarkers() function. The
  /// location of the corner is the same as in corners_2d field.
  float corners_3d[4][3];

  /// Marker pose - orientation is a Hamilton quaternion specified as
  /// (x, y, z, w). Both translation and orientation are defined in the output
  /// frame, which is defined by the translation and orientation pair passed to
  /// TangoMarkers_detectMarkers() function.
  /// The marker pose defines a marker local frame, in which:
  ///  X = to the right on the tag
  ///  Y = to the up on the tag
  ///  Z = pointing out of the tag towards the user.
  double translation[3];
  double orientation[4];
} TangoMarkers_Marker;

/// @brief A structure that stores a list of markers. After calling
///   TangoMarkers_detectMarkers() with a TangoMarkers_MarkerList object, the
///   object needs to be released by calling TangoSupport_freeMarkersList()
///   function.
typedef struct TangoMarkers_MarkerList {
  TangoMarkers_Marker* markers;
  int marker_count;
} TangoMarkers_MarkerList;

/// @brief Detect one or more markers in the input image.
/// @param image_buffer The image buffer. Cannot be NULL.
/// @param camera_id The identification of the camera that captured the
////  image_buffer.
/// @param translation The translation component of the transformation from the
///   the input camera space to the output frame. Cannot be NULL.
/// @param orientation The orientation component (as a quaternion)
///   of the transformation from the input camera space to the output frame.
///   Cannot be NULL.
/// @param param The parameters for marker detection. Cannot be NULL.
/// @param list The output marker list. The caller needs to release the
///   memory by calling TangoMarkers_freeMarkerList() function.
/// @return @c TANGO_SUCCESS on success, @c TANGO_INVALID on invalid input, and
///   @c TANGO_ERROR on failure.
TangoErrorType TangoMarkers_detectMarkers(const TangoImageBuffer* image,
                                          const TangoCameraId camera_id,
                                          const double translation[3],
                                          const double orientation[4],
                                          const TangoMarkers_DetectParam* param,
                                          TangoMarkers_MarkerList* list);

/// @brief Free memory allocated in TangoMarkers_detectMarkers().
///
/// @param list Marker list to free.
void TangoMarkers_freeMarkerList(TangoMarkers_MarkerList* list);

/// @}
#ifdef __cplusplus
}
#endif

#endif  // TANGO_MARKERS_TANGO_MARKERS_H_
