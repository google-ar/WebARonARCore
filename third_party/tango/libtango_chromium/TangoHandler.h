/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef _TANGO_HANDLER_H_
#define _TANGO_HANDLER_H_

#include "tango_client_api.h"   // NOLINT
#include "tango_client_api2.h"   // NOLINT
#include "tango_support.h"  // NOLINT
#include "tango_markers.h"  // NOLINT

#include <ctime>

#include <jni.h>

#include <string>
#include <vector>
#include <queue>
#include <unordered_map>

#include <mutex>
#include <shared_mutex>
#include <thread>
#include <condition_variable>

#include <chrono>

#include "AnchorManager.h"

namespace tango_chromium {

typedef std::chrono::system_clock clock;
typedef std::chrono::time_point < clock, std::chrono::milliseconds > time_type;

class Hit
{
public:
  float modelMatrix[16];
};

class Plane
{
public:
  long identifier;
  long timestamp;
  float modelMatrix[16];
  float extent[2];
  std::vector<float> vertices;
  uint count;
};

class PlaneDeltas
{
public:
  std::vector<Plane> added;
  std::vector<Plane> updated;
  std::vector<long> removed;
};

class TangoHandlerEventListener
{
public:
  virtual void anchorsUpdated(
    const std::vector<std::shared_ptr<Anchor>>& anchors) = 0;
};

class Marker
{
public:
  Marker(TangoMarkers_MarkerType type, int id, const std::string& content, const double* position, const double* orientation, const float* modelMatrix): type(type), id(id), content(content)
  {
    memcpy(this->position, position, sizeof(this->position));
    memcpy(this->orientation, orientation, sizeof(this->orientation));
    memcpy(this->modelMatrix, modelMatrix, sizeof(this->modelMatrix));
  }

  TangoMarkers_MarkerType getType() const
  {
    return type;
  }

  int getId() const
  {
    return id;
  }

  std::string getContent() const
  {
    return content;
  }

  const double* getPosition() const
  {
    return position;
  }

  const double* getOrientation() const
  {
    return orientation;
  }

  const float* getModelMatrix() const
  {
    return modelMatrix;
  }

private:
  TangoMarkers_MarkerType type;
  int id;
  std::string content;
  double position[3];
  double orientation[4];
  float modelMatrix[16];
};

// TangoHandler provides functionality to communicate with the Tango Service.
class TangoHandler {
public:
  static TangoHandler* getInstance();
  static void releaseInstance();

  TangoHandler();

  TangoHandler(const TangoHandler& other) = delete;
  TangoHandler& operator=(const TangoHandler& other) = delete;

  ~TangoHandler();

  void onCreate(JNIEnv* env, jobject activity, int activityOrientation, int sensorOrientation);
  void onTangoServiceConnected(JNIEnv* env, jobject tango);
  void onPause();
  void onDeviceRotationChanged(int activityOrientation, int sensorOrientation);
  void onTangoEventAvailable(const TangoEvent* event);
  void onFrameAvailable(const TangoImageBuffer* imageBuffer);

  bool isConnected() const;

  bool getPose(TangoPoseData* tangoPoseData);
  bool getProjectionMatrix(float near, float far, float* projectionMatrix);
  bool hitTest(float x, float y, std::vector<Hit>& hits);
  bool getPlaneDeltas(PlaneDeltas& planeDeltas);
  std::shared_ptr<Anchor> createAnchor(
    const float* anchorModelMatrix);
  void removeAnchor(uint32_t identifier);

  void resetPose();

  void reset();

  bool updateCameraIntrinsics();
  bool getCameraImageSize(uint32_t* width, uint32_t* height);
  bool getCameraImageTextureSize(uint32_t* width, uint32_t* height);
  bool getCameraFocalLength(double* focalLengthX, double* focalLengthY);
  bool getCameraPoint(double* x, double* y);
  bool updateCameraImageIntoTexture(uint32_t textureId);

  int getSensorOrientation() const;
  int getActivityOrientation() const;

  void addTangoHandlerEventListener(TangoHandlerEventListener* listener);
  void removeTangoHandlerEventListener(TangoHandlerEventListener* listener);
  void removeAllTangoHandlerEventListeners();

  bool getMarkers(TangoMarkers_MarkerType markerType, float markerSize, 
                  std::vector<Marker>& markers);

private:
  void connect();
  void disconnect();
  bool hasLastTangoImageBufferTimestampChangedLately();
  bool getPlanes(std::vector<Plane>& planes);
  void removeThisMarkerDetectionThread();
  void waitForAllMarkerDetectionThreads();

  static TangoHandler* instance;

  bool connected;
  TangoConfig tangoConfig;
  TangoCameraIntrinsics tangoCameraIntrinsics;
  double lastTangoImageBufferTimestamp;
  std::time_t lastTangoImagebufferTimestampTime;

  std::unordered_map<long, Plane> planeMap;

  uint32_t cameraImageWidth;
  uint32_t cameraImageHeight;
  uint32_t cameraImageTextureWidth;
  uint32_t cameraImageTextureHeight;

  bool textureReaderInitialized;
  TangoImageBuffer cameraImageBuffer;
  std::mutex textureReaderMutex;
  std::condition_variable textureReaderCV;
  size_t textureReadRequestCounter;
  uint32_t textureReaderTextureId; 

  int activityOrientation;
  int sensorOrientation;

  std::string tangoCoreVersionString;

  AnchorManager anchorManager;

  std::vector<TangoHandlerEventListener*> listeners;

  time_type lastGetMarkersCallTime;
  std::vector<Marker> detectedMarkers;
  std::mutex detectedMarkersMutex;
  TangoSupport_ImageBufferManager* imageBufferManager;
  int imageBufferManagerWidth;
  int imageBufferManagerHeight;
  TangoPoseData poseForMarkerDetection;
  std::mutex poseForMarkerDetectionMutex;
  bool poseForMarkerDetectionIsCorrect;
  std::vector<std::thread> markerDetectionThreads;
  std::mutex markerDetectionThreadsMutex;
  bool joiningMarkerDetectionThreads; 
};

}  // namespace tango_chromium

#endif  // _TANGO_HANDLER_H_
