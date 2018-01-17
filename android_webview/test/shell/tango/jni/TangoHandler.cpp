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

#include <cstdlib>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

#include <cassert>

#include <cmath>

#include "TangoHandler.h"

#include <sstream>

#include <thread>

#include "glm.hpp"
#include "gtc/type_ptr.hpp"
#include "gtx/transform.hpp"
#include "gtx/quaternion.hpp"

#include "MathUtils.h"
#include "LogUtils.h"
#include "camera_utility.h"

using namespace glm;

namespace tango_chromium {

constexpr int kVersionStringLength = 128;
// The minimum Tango Core version required from this application.
constexpr int kTangoCoreMinimumVersion = 9377;

constexpr long kMarkerDetectionFPS = 30;

const float ANDROID_WEBVIEW_ADDRESS_BAR_HEIGHT = 125;

void onTextureAvailable(void* context, TangoCameraId tangoCameraId) {
  // Do nothing for now.
}

void onFrameAvailable(void* context, TangoCameraId tangoCameraId, 
    const TangoImageBuffer* imageBuffer) {
  tango_chromium::TangoHandler::getInstance()->onFrameAvailable(imageBuffer);
}

void onTangoEventAvailable(void* context, const TangoEvent* event) {
  tango_chromium::TangoHandler* tangoHandler =
      static_cast<tango_chromium::TangoHandler*>(context);
  tangoHandler->onTangoEventAvailable(event);
}

glm::mat4 SS_T_GL;
glm::mat4 SS_T_GL_INV;

glm::mat4 getPlaneMatrixFromPlanePose(const TangoPoseData &pose) {
  // Get the plane transform matrix.
  glm::mat4 planeMatrix = mat4FromTranslationOrientation(
      pose.translation, pose.orientation);

  // Plane is in the tango coordinates, so transform into GL coordinate space.
  planeMatrix = SS_T_GL_INV * planeMatrix * SS_T_GL;

  return planeMatrix;
}

TangoHandler* TangoHandler::instance = 0;

TangoHandler* TangoHandler::getInstance() {
  if (instance == 0)
  {
    instance = new TangoHandler();
  }
  return instance;
}

void TangoHandler::releaseInstance() {
  delete instance;
  instance = 0;
}

TangoHandler::TangoHandler(): connected(false)
  , tangoConfig(nullptr)
  , lastTangoImageBufferTimestamp(0)
  , cameraImageWidth(0)
  , cameraImageHeight(0)
  , cameraImageTextureWidth(0)
  , cameraImageTextureHeight(0)
  , textureReaderInitialized(false)
  , textureReaderTextureId(0)
  , imageBufferManager(nullptr)
  , poseForMarkerDetectionIsCorrect(false)
  , textureReadRequestCounter(0)
  , joiningMarkerDetectionThreads(false) {
  // SS means Start of Service: this matrix does the conversion from Start 
  // of Service transform to OpenGL (tango-space has z as the up-vector, not y).
  float ss_t_gl[16] = {1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1};
  SS_T_GL = make_mat4(ss_t_gl);
  SS_T_GL_INV = inverse(SS_T_GL);
  lastGetMarkersCallTime = 
    std::chrono::time_point_cast<std::chrono::milliseconds>(clock::now());
}

TangoHandler::~TangoHandler() {
  TangoConfig_free(tangoConfig);
  tangoConfig = nullptr;
}

void TangoHandler::onCreate(JNIEnv* env, jobject activity, 
    int activityOrientation, int sensorOrientation) {
  this->activityOrientation = activityOrientation;
  this->sensorOrientation = sensorOrientation;
}

void TangoHandler::onTangoServiceConnected(JNIEnv* env, jobject tango) {
  TangoService_CacheTangoObject(env, tango);

  connect();
}


void TangoHandler::connect() {
  TangoErrorType result;

  // TANGO_CONFIG_DEFAULT is enabling Motion Tracking and disabling Depth
  // Perception.
  tangoConfig = TangoService_getConfig(TANGO_CONFIG_DEFAULT);
  if (tangoConfig == nullptr)
  {
    LOGE("TangoHandler::connect, TangoService_getConfig error.");
    std::exit (EXIT_SUCCESS);
  }

  // Set auto-recovery for motion tracking as requested by the user.
  result = TangoConfig_setBool(tangoConfig, "config_enable_auto_recovery", true);
  if (result != TANGO_SUCCESS) {
    LOGE("TangoHandler::connect, config_enable_auto_recovery activation failed with error code: %d", result);
    std::exit(EXIT_SUCCESS);
  }

  // Enable color camera from config.
  result = TangoConfig_setBool(tangoConfig, "config_enable_color_camera", true);
  if (result != TANGO_SUCCESS) {
    LOGE("TangoHandler::connect, config_enable_color_camera() failed with error code: %d", result);
    std::exit(EXIT_SUCCESS);
  }

  // Note that it is super important for AR applications that we enable low
  // latency IMU integration so that we have pose information available as
  // quickly as possible. Without setting this flag, you will often receive
  // invalid poses when calling getPoseAtTime() for an image.
  result = TangoConfig_setBool(tangoConfig, "config_enable_low_latency_imu_integration", true);
  if (result != TANGO_SUCCESS) {
    LOGE("TangoHandler::connect, failed to enable low latency imu integration.");
    std::exit(EXIT_SUCCESS);
  }

  // Drift correction allows motion tracking to recover after it loses tracking.
  // The drift corrected pose is is available through the frame pair with
  // base frame AREA_DESCRIPTION and target frame DEVICE.
  result = TangoConfig_setBool(tangoConfig, "config_enable_drift_correction", true);
  if (result != TANGO_SUCCESS) {
    LOGE("TangoHandler::connect, enabling config_enable_drift_correction "
      "failed with error code: %d", result);
    std::exit(EXIT_SUCCESS);
  }

  // Enable Depth Perception.
  result = TangoConfig_setBool(tangoConfig, "config_enable_depth", true);
  if (result != TANGO_SUCCESS)
  {
    LOGE("TangoHandler::connect, config_enable_depth activation failed with error code: %d.", result);
    std::exit(EXIT_SUCCESS);
  }

  // Enabling experimental plane finding.
  result = TangoConfig_setBool(tangoConfig,
      "config_experimental_enable_plane_detection", true);
  if (result != TANGO_SUCCESS) {
    LOGE("TangoHandler::connect, failed to enable experimental plane finding.");
    std::exit(EXIT_SUCCESS);
  }

  result = TangoConfig_setInt32(tangoConfig, "config_depth_mode",
      TANGO_POINTCLOUD_XYZC);
  if (result != TANGO_SUCCESS) {
    LOGE(
        "TangoHandler::connect, failed to configure point cloud to "
        "XYZC.");
    std::exit(EXIT_SUCCESS);
  }

  // Get TangoCore version string from service.
  char tangoCoreVersionCharArray[kVersionStringLength];
  result = TangoConfig_getString(tangoConfig, "tango_service_library_version",
      tangoCoreVersionCharArray, kVersionStringLength);
  if (result != TANGO_SUCCESS) {
    LOGE(
        "TangoHandler::connect, get tango core version failed with error"
        "code: %d",
        result);
    std::exit(EXIT_SUCCESS);
  }
  tangoCoreVersionString = tangoCoreVersionCharArray;

  // Connect some callbacks
  result = TangoService_connectOnTextureAvailable(TANGO_CAMERA_COLOR, this, 
      tango_chromium::onTextureAvailable);
  if (result != TANGO_SUCCESS)
  {
    LOGE("TangoHandler::connect, failed to connect texture callback with error code: %d", result);
    std::exit(EXIT_SUCCESS);
  }

  // Attach onEventAvailable callback.
  // The callback will be called after the service is connected.
  result = TangoService_connectOnTangoEvent(
      tango_chromium::onTangoEventAvailable);
  if (result != TANGO_SUCCESS) {
    LOGE(
        "TangoHandler::connect, failed to connect to event callback with error"
        "code: %d",
        result);
    std::exit(EXIT_SUCCESS);
  }

  // Connect the tango service.
  if (TangoService_connect(this, tangoConfig) != TANGO_SUCCESS)
  {
    LOGE("TangoHandler::connect, TangoService_connect error.");
    std::exit (EXIT_SUCCESS);
  }

  // Get the intrinsics for the color camera and pass them on to the depth
  // image.
  result = TangoService_getCameraIntrinsics(TANGO_CAMERA_COLOR, &tangoCameraIntrinsics);
  if (result != TANGO_SUCCESS) {
    LOGE("TangoHandler::connect: Failed to get the intrinsics for the color camera.");
    std::exit(EXIT_SUCCESS);
  }

  imageBufferManagerWidth = tangoCameraIntrinsics.width;
  imageBufferManagerHeight = tangoCameraIntrinsics.height;
  result = TangoSupport_createImageBufferManager(
      TANGO_HAL_PIXEL_FORMAT_YCrCb_420_SP, imageBufferManagerWidth,
      imageBufferManagerHeight, &imageBufferManager);
  if (result != TANGO_SUCCESS) {
    LOGE("TangoHandler::connect, failed to create image buffer manager with error code: %d", result);
    std::exit(EXIT_SUCCESS);
  }

  // Do not use this for now. ARCore is providing an incorrect image buffer. Using
  // Harry Wang's camera utility library to try to get the camera image. 
  // result = TangoService_connectOnFrameAvailable(TANGO_CAMERA_COLOR, this, ::onFrameAvailable);
  // if (result != TANGO_SUCCESS)
  // {
  //   LOGE("TangoHandler::connect, failed to connect frame callback with error code: %d", result);
  //   std::exit(EXIT_SUCCESS);
  // }

  // Initialize TangoSupport context.
  TangoSupport_initialize(TangoService_getPoseAtTime,
      TangoService_getCameraIntrinsics);

  connected = true;

  // Setup the camera image buffer to be ready to start storing the video frames.
  cameraImageBuffer.data = 0;
  cameraImageBuffer.stride = tangoCameraIntrinsics.width;
  cameraImageBuffer.format = TANGO_HAL_PIXEL_FORMAT_YCrCb_420_SP;
  cameraImageBuffer.width = imageBufferManagerWidth;
  cameraImageBuffer.height = imageBufferManagerHeight;

  // Update camera intrinsics after connection which will take into account
  // device rotation
  this->updateCameraIntrinsics();
}

void TangoHandler::disconnect() {  
  waitForAllMarkerDetectionThreads();

  TangoSupport_freeImageBufferManager(imageBufferManager);
  imageBufferManager = nullptr;

  TangoService_disconnect();

  cameraImageWidth = cameraImageHeight =
    cameraImageTextureWidth = cameraImageTextureHeight = 0;

  connected = false;
}

void TangoHandler::onPause() {
  disconnect();
}

void TangoHandler::onDeviceRotationChanged(int activityOrientation, 
    int sensorOrientation) {
  LOGE("TangoHandler::onDeviceRotationChanged; activityOrientation=%d, sensorOrientation=%d",
    activityOrientation, sensorOrientation);
  this->activityOrientation = activityOrientation;
  this->sensorOrientation = sensorOrientation;
  this->updateCameraIntrinsics();
}

void TangoHandler::onTangoEventAvailable(const TangoEvent* event) {
  switch(event->type) 
  {
    case TANGO_EVENT_GENERAL:
    {
      std::string eventKey = event->event_key;
      if (eventKey == "EXPERIMENTAL_PoseHistoryChanged")
      {
        double historyChangeTimestamp = *((double*)event->event_value);

        // LOGI("JUDAX: TangoHandler::onTangoEventAvailable -> EXPERIMENTAL_PoseHistoryChanged: historyChangeTimestamp = %lf", historyChangeTimestamp);

        std::vector<std::shared_ptr<Anchor>> updatedAnchors = 
            anchorManager.update(historyChangeTimestamp, activityOrientation);
        if (!updatedAnchors.empty())
        {

          // LOGI("JUDAX: TangoHandler::onTangoEventAvailable -> updatedAnchors.size() = %ld", updatedAnchors.size());

          for (auto listener: listeners)
          {
            listener->anchorsUpdated(updatedAnchors);
          }
        }
      }
      break;
    }
  }
}

void TangoHandler::onFrameAvailable(const TangoImageBuffer* imageBuffer) {
  // In LeTango, the frame that is passed is not full resolution. If the created image
  // buffer manager does not match, a crash will occur. This check handles that case
  // to recreate the image buffer manager to the correct image buffer size.
  if (imageBuffer->width != imageBufferManagerWidth || 
    imageBuffer-> height != imageBufferManagerHeight)
  {
    imageBufferManagerWidth = imageBuffer->width;
    imageBufferManagerHeight = imageBuffer->height;
    TangoSupport_freeImageBufferManager(imageBufferManager);
    TangoErrorType result = TangoSupport_createImageBufferManager(
        TANGO_HAL_PIXEL_FORMAT_YCrCb_420_SP, imageBufferManagerWidth,
        imageBufferManagerHeight, &imageBufferManager);
    if (result != TANGO_SUCCESS) {
      LOGE("TangoHandler::onFrameAvailable, failed to create image buffer manager with error code: %d", result);
      std::exit(EXIT_SUCCESS);
    }
  }

  TangoSupport_updateImageBuffer(imageBufferManager, imageBuffer);
}

bool TangoHandler::isConnected() const{
  return connected;
}

bool TangoHandler::getPose(TangoPoseData* tangoPoseData) {
  bool result = connected;
  if (connected)
  {
    lastTangoImageBufferTimestamp = hasLastTangoImageBufferTimestampChangedLately() ? lastTangoImageBufferTimestamp : 0.0;

    // LOGI("JUDAX: TangoHandler::getPose lastTangoImageBufferTimestamp = %lf", lastTangoImageBufferTimestamp);

// For reference purpose: ADF to start of service request parameters.
// {TANGO_COORDINATE_FRAME_AREA_DESCRIPTION,
//         TANGO_COORDINATE_FRAME_DEVICE}
// then fallback to:
// {TANGO_COORDINATE_FRAME_START_OF_SERVICE,
//         TANGO_COORDINATE_FRAME_DEVICE}

    result = TangoSupport_getPoseAtTime(
      lastTangoImageBufferTimestamp, TANGO_COORDINATE_FRAME_START_OF_SERVICE,
      TANGO_COORDINATE_FRAME_CAMERA_COLOR, TANGO_SUPPORT_ENGINE_OPENGL,
      TANGO_SUPPORT_ENGINE_OPENGL,
      static_cast<TangoSupport_Rotation>(activityOrientation), tangoPoseData) == TANGO_SUCCESS;
    if (!result)
    {
      LOGE("TangoHandler::getPose: Failed to get the pose.");
    }
    else
    {
      poseForMarkerDetectionMutex.lock();
      poseForMarkerDetectionIsCorrect = TangoSupport_getPoseAtTime(
          lastTangoImageBufferTimestamp, TANGO_COORDINATE_FRAME_START_OF_SERVICE,
          TANGO_COORDINATE_FRAME_CAMERA_COLOR, TANGO_SUPPORT_ENGINE_OPENGL,
          TANGO_SUPPORT_ENGINE_TANGO, 
          static_cast<TangoSupport_Rotation>(activityOrientation),
          &poseForMarkerDetection) == TANGO_SUCCESS;
      poseForMarkerDetectionMutex.unlock();
    }
  }
  return result;
}

bool TangoHandler::getProjectionMatrix(float near, float far, 
    float* projectionMatrix) {
  if (!connected) return false;

  bool result = this->updateCameraIntrinsics();

  if (!result) {
    LOGE(
      "TangoHandler::getProjectionMatrix, failed to get camera intrinsics");
    return false;
  }

  float image_width = static_cast<float>(tangoCameraIntrinsics.width);
  float image_height = static_cast<float>(tangoCameraIntrinsics.height);
  float fx = static_cast<float>(tangoCameraIntrinsics.fx);
  float fy = static_cast<float>(tangoCameraIntrinsics.fy);
  float cx = static_cast<float>(tangoCameraIntrinsics.cx);
  float cy = static_cast<float>(tangoCameraIntrinsics.cy);

  matrixProjection(
    image_width, image_height, fx, fy, cx, cy, near,
    far, projectionMatrix);

  return true;
}

bool TangoHandler::hitTest(float x, float y, std::vector<Hit>& hits) {
  bool result = false;

  if (connected)
  {
    TangoPlaneData* planes = 0;
    size_t numberOfPlanes = 0;
    if (TangoService_Experimental_getPlanes(&planes, &numberOfPlanes) == 
      TANGO_SUCCESS)
    {
      if (numberOfPlanes == 0) {
        return result;
      }

      // Set the projection matrix.
      float* fM;
      glm::mat4 projectionMatrix;
      fM = glm::value_ptr(projectionMatrix);
      // TODO(lincolnfrog): near and far values should not be constants. Use the values
      // supplied by the client.
      getProjectionMatrix(0.01, 10000, fM);

      // Set the model view matrix.
      TangoPoseData poseData;
      getPose(&poseData);
      glm::mat4 viewMatrix = mat4FromTranslationOrientation(
          poseData.translation, poseData.orientation);
      viewMatrix = glm::inverse(viewMatrix);

      // Combine the projection and model view matrices.
      glm::mat4 projViewMatrix = projectionMatrix * viewMatrix;

      // Invert the combined matrix because we need to go from screen -> world.
      projViewMatrix = glm::inverse(projViewMatrix);

      // Create a ray in screen-space for the hit test ([-1, 1] with y flip).
      glm::vec3 rayStart = glm::vec3((2 * x) - 1, (2 * (1 - y)) - 1, 0);
      glm::vec3 rayEnd   = glm::vec3((2 * x) - 1, (2 * (1 - y)) - 1, 1);

      // Transform the ray into world-space.
      glm::vec3 worldRayOrigin = transformVec3ByMat4(rayStart, projViewMatrix);
      glm::vec3 worldRayDirection = transformVec3ByMat4(rayEnd, projViewMatrix);
      worldRayDirection -= worldRayOrigin;
      worldRayDirection = glm::normalize(worldRayDirection);

      // Check each plane for intersections.
      for (int i = 0; i < numberOfPlanes; i++) {
        TangoPlaneData planeData = planes[i];
        if (!planeData.is_valid) {
          // This plane is no longer valid, so skip it.
          continue;
        }
        if (planeData.subsumed_by != -1) {
          // This plane has been subsumed by another plane, so skip it.
          continue;
        }

        // Get the plane transform matrix.
        glm::mat4 planeMatrix = getPlaneMatrixFromPlanePose(planeData.pose);

        // Get the plane center in world-space.
        glm::vec3 planeCenter = glm::vec3(planeData.center_x, 0, planeData.center_y);
        glm::vec3 planePosition = transformVec3ByMat4(planeCenter, planeMatrix);

        // Assume all planes are oriented horizontally.
        glm::vec3 planeNormal = glm::vec3(0, 1, 0);

        // Check if the ray intersects the plane.
        float t = rayIntersectsPlane(planeNormal, planePosition,
            worldRayOrigin, worldRayDirection);

        // If t < 0, there is no intersection.
        if (t < 0) {
          continue;
        }

        // Calculate the intersection point.
        glm::vec3 planeIntersection = worldRayOrigin + (worldRayDirection * t);

        /*
        // TODO: re-enable bounding box check when we figure it out. Right now, it cuts out
        // portions of the polygon, so you end up with the hit test being an intersection between the
        // bounding box and polygon rather than just the polygon as expected.

        // Do a bounding-box test (early-out).
        // Convert the intersection into plane-space.
        glm::mat4 yawMatrix = glm::toMat4(angleAxis((float)planeData.yaw, glm::vec3(0, 1, 0)));
        glm::mat4 planeMatrixInv = glm::inverse(planeMatrix * yawMatrix);
        glm::vec3 planeIntersectionLocal = transformVec3ByMat4(
            glm::vec3(planeIntersection.x + planeData.center_x, planeIntersection.y, planeIntersection.z - planeData.center_y),
            planeMatrixInv);
        //glm::vec3 rotatedBounds = transformVec3ByMat4(glm::vec3(planeData.width, planeData.height, 0), yawMat);

        // Check if the intersection is outside of the extent of the plane.
        if (abs(planeIntersectionLocal.x) > planeData.width / 2) {
          continue;
        }
        if (abs(planeIntersectionLocal.y) > planeData.height / 2) {
          continue;
        }
        */

        // Transform all the points into world space using the plane matrix.
        glm::vec3 polygonPoints[planeData.boundary_point_num];
        for (int i = 0; i < planeData.boundary_point_num; ++i) {
          polygonPoints[i] = transformVec3ByMat4(
              glm::vec3((float)planeData.boundary_polygon[i * 2], 0, 
              -(float)planeData.boundary_polygon[(i * 2) + 1]),
            planeMatrix);
        }

        if(!isPointInPolygon(planeIntersection, polygonPoints, 
            planeData.boundary_point_num)) {
          // The intersection point lies outside the plane polygon, so skip it.
          continue;
        }

        glm::mat4 hitMatrix = glm::translate(glm::mat4(1.0f), planeIntersection);
        Hit hit;
        setFloat16FromMat4(hit.modelMatrix, hitMatrix);
        hits.push_back(hit);
      }

      // Sort the hits based on distance to the camera.
      auto sortFunc = [worldRayOrigin] (Hit a, Hit b) {
        glm::vec3 vA = glm::vec3(a.modelMatrix[12], a.modelMatrix[13], 
            a.modelMatrix[14]);
        float dA = glm::length2(vA - worldRayOrigin);

        glm::vec3 vB = glm::vec3(b.modelMatrix[12], b.modelMatrix[13], 
            b.modelMatrix[14]);
        float dB = glm::length2(vB - worldRayOrigin);
        return dA < dB;
      };

      std::sort(hits.begin(), hits.end(), sortFunc);
    }

    TangoPlaneData_free(planes, numberOfPlanes);

    result = hits.size() > 0;
  }

  return result;
}

bool TangoHandler::getPlanes(std::vector<Plane>& planes) {
  if (connected)
  {
    TangoPlaneData* planeDatas = 0;
    size_t numberOfPlanes = 0;
    TangoErrorType tangoResult = TangoService_Experimental_getPlanes(&planeDatas, 
        &numberOfPlanes);
    if (tangoResult == TANGO_SUCCESS)
    {
      if (numberOfPlanes == 0) {
        return false;
      }

      int count = 0;
      for (int i = 0; i < numberOfPlanes; i++) {
        TangoPlaneData planeData = planeDatas[i];
        if (planeData.is_valid && planeData.subsumed_by == -1) {
          count++;
        }
      }
      planes.resize(count);
      count = 0;

      // Gather all the planes.
      for (int i = 0; i < numberOfPlanes; i++) {
        TangoPlaneData planeData = planeDatas[i];

        if (!planeData.is_valid) {
          // This plane is no longer valid, so skip it.
          continue;
        }
        if (planeData.subsumed_by != -1) {
          // This plane has been subsumed by another plane, so skip it.
          continue;
        }

        Plane plane;

        // Set the plane's unique identifier.
        plane.identifier = (long)planeData.id;

        plane.timestamp = planeData.timestamp;

        // Set the transform values from the transformed plane matrix.
        glm::mat4 planeMatrix = getPlaneMatrixFromPlanePose(planeData.pose);
        glm::mat4 yawMatrix = glm::toMat4(glm::angleAxis((float)planeData.yaw, 
            glm::vec3(0, 1, 0)));
        planeMatrix = planeMatrix * yawMatrix;
        const float* fM = glm::value_ptr(planeMatrix);
        for (int j = 0; j < 16; j++) {
          plane.modelMatrix[j] = fM[j];
        }

        // Bake the center information into the plane matrix for simplicity.
        plane.modelMatrix[12] += planeData.center_x;
        // The y-value gets negated because we are moving from a left-handed to a right-handed coordinate system.
        plane.modelMatrix[14] += -planeData.center_y;

        // Set the extents.
        plane.extent[0] = planeData.width;
        plane.extent[1] = planeData.height;

        // Create the vertices array.
        plane.count = planeData.boundary_point_num;
        plane.vertices.resize(plane.count * 3);

        // Gather all the polygon vertices and add a y-value of zero.
        for (int j = 0; j < plane.count; j++) {
          // Create a glm::vec3 representing each point and subtract the center value (since it is baked into the plane matrix).
          glm::vec3 vertex = glm::vec3(
              (float)planeData.boundary_polygon[j * 2] - planeData.center_x,
              0,
              // The y-value gets negated because we are moving from a left-handed to a right-handed coordinate system.
              (float)-planeData.boundary_polygon[(j * 2) + 1] + planeData.center_y);
          // Transform the points by the inverse yaw matrix since we combined the yaw with the plane matrix
          // above and the yaw only applies to the bounding box.
          vertex = transformVec3ByMat4(vertex, glm::inverse(yawMatrix));
          plane.vertices[j * 3] = vertex.x;
          plane.vertices[(j * 3) + 1] = vertex.y;
          plane.vertices[(j * 3) + 2] = vertex.z;
        }

        planes[count] = plane;
        count++;
      }
    }

    TangoPlaneData_free(planeDatas, numberOfPlanes);
    return tangoResult == TANGO_SUCCESS;
  }

  return false;
}

bool TangoHandler::getPlaneDeltas(PlaneDeltas& planeDeltas) {
  if (!connected)
  {
    return false;
  }

  std::vector<Plane> planes;
  getPlanes(planes);

  // Find all the planes that have been added or changed this frame.
  std::unordered_map<long, Plane> planeMapNew;
  for (auto plane : planes) {
    planeMapNew[plane.identifier] = plane;
    // Check if this is a plane we already know about.
    auto prev = planeMap.find(plane.identifier);
    if (prev != planeMap.end()) {
      // This is an existing plane. Has it changed?
      Plane existing = prev->second;
      if (existing.timestamp != plane.timestamp) {
        // The plane has a new timestamp, thus it has changed.
        planeDeltas.updated.push_back(plane);
      }
    } else {
      // This is a new plane.
      planeDeltas.added.push_back(plane);
    }
  }

  // Check for all previous planes in the new map to see if they have been removed.
  for (auto it : planeMap) {
    if (planeMapNew.find(it.first) == planeMapNew.end()) {
      planeDeltas.removed.push_back(it.first);
    }
  }

  planeMap = planeMapNew;

  return true;
}

std::shared_ptr<Anchor> TangoHandler::addAnchor(
    const float* anchorModelMatrix) {
  std::shared_ptr<Anchor> anchor;

  // TODO: What happens when the anchor is created and the camera pose is 
  // incorrect either because the timestamp is incorrect or the pose retrieval 
  // fails?

  double timestamp = hasLastTangoImageBufferTimestampChangedLately() ? lastTangoImageBufferTimestamp : 0.0;

  TangoPoseData tangoPoseData;
  if (TangoSupport_getPoseAtTime(
      timestamp, TANGO_COORDINATE_FRAME_START_OF_SERVICE,
      TANGO_COORDINATE_FRAME_CAMERA_COLOR, TANGO_SUPPORT_ENGINE_OPENGL,
      TANGO_SUPPORT_ENGINE_OPENGL,
      static_cast<TangoSupport_Rotation>(activityOrientation), 
      &tangoPoseData) == TANGO_SUCCESS) {
    glm::mat4 cameraModelMatrix = mat4FromTranslationOrientation(
        tangoPoseData.translation, tangoPoseData.orientation);
    anchor = anchorManager.addAnchor(timestamp, 
        (const float*)&cameraModelMatrix, 
        anchorModelMatrix);

    // TODO: This is needed so there is not a crash foro calling an event from
    // the wrong thread in Chromium. I (Iker) am still trying to fix it with
    // the help of some Chromium experts but have not been able to make it
    // work. For some strange reason this hack of making the event call at
    // least once before the next, different thread calls are made seems to
    // fix it. Another possible solution would be to store a container of
    // updated anchors (checking that only storing the latest) and retrieve
    // them in the IDL layer by polling instead of event calling.
    // For now, just sending an empty container so no event is actually fired
    // to the final app.
    std::vector<std::shared_ptr<Anchor>> anchors;
    for (auto listener: listeners)
    {
      listener->anchorsUpdated(anchors);
    }

  }
  else 
  {
    LOGE("ERROR: Could not retrieve the new pose data while creating anchor.");
  }

  return anchor;
}

void TangoHandler::removeAnchor(uint32_t identifier) {
  anchorManager.removeAnchor(identifier);
}

void TangoHandler::resetPose() {
  TangoService_resetMotionTracking();
}

void TangoHandler::reset() {
  resetPose();
  planeMap.clear();
  anchorManager.removeAllAnchors();
  waitForAllMarkerDetectionThreads();
}

bool TangoHandler::updateCameraIntrinsics() {
  if (!connected) {
    LOGE("TangoHandler::updateCameraIntrinsics, is not connected.");
    return false;
  }

  int result = TangoSupport_getCameraIntrinsicsBasedOnDisplayRotation(
      TANGO_CAMERA_COLOR, 
      static_cast<TangoSupport_Rotation>(activityOrientation),
      &tangoCameraIntrinsics);

  if (result != TANGO_SUCCESS) {
    LOGE(
        "TangoHandler::updateCameraIntrinsics, failed to get camera intrinsics "
        "with error code: %d",
        result);
    return false;
  }

  /*
  LOGE("TangoHandler::updateCameraIntrinsics, success. fx: %f, fy: %f, width: %d, height: %d, cx: %f, cy: %f",
    tangoCameraIntrinsics.fx,
    tangoCameraIntrinsics.fy,
    tangoCameraIntrinsics.width,
    tangoCameraIntrinsics.height,
    tangoCameraIntrinsics.cx,
    tangoCameraIntrinsics.cy);
  */

  // Always subtract the height of the address bar since we cannot
  // get rid of it
  tangoCameraIntrinsics.height -= ANDROID_WEBVIEW_ADDRESS_BAR_HEIGHT;

  // Update the stored values for width and height
  cameraImageWidth = cameraImageTextureWidth = tangoCameraIntrinsics.width;
  cameraImageHeight = cameraImageTextureHeight = tangoCameraIntrinsics.height;

  return true;
}

bool TangoHandler::getCameraImageSize(uint32_t* width, uint32_t* height) {
  bool result = true;

  *width = cameraImageWidth;
  *height = cameraImageHeight;

  return result;
}

bool TangoHandler::getCameraImageTextureSize(uint32_t* width, 
    uint32_t* height) {
  bool result = true;

  *width = cameraImageTextureWidth;
  *height = cameraImageTextureHeight;

  return result;
}

bool TangoHandler::getCameraFocalLength(double* focalLengthX, 
    double* focalLengthY) {
  bool result = true;
  *focalLengthX = tangoCameraIntrinsics.fx;
  *focalLengthY = tangoCameraIntrinsics.fy;
  return result;
}

bool TangoHandler::getCameraPoint(double* x, double* y) {
  bool result = true;
  *x = tangoCameraIntrinsics.cx;
  *y = tangoCameraIntrinsics.cy;
  return result;
}

bool TangoHandler::updateCameraImageIntoTexture(uint32_t textureId) {
  if (!connected) return false;

  TangoErrorType result = TangoService_updateTextureExternalOes(
      TANGO_CAMERA_COLOR, textureId, &lastTangoImageBufferTimestamp);

  std::time(&lastTangoImagebufferTimestampTime);

  textureReaderMutex.lock();

  if (textureReadRequestCounter > 0) {

    // If there has been a request to read the texture and the texture has
    // not been initialized yet, do it. This resolves some issues with the
    // texture reader not working on some S8 models if marker detection is
    // not requested.
    // TODO(ijamardo): Integrate the new version of the texture reader that is meant
    // to resolve the issue once and for all.
    if (!textureReaderInitialized || textureReaderTextureId != textureId)
    {
      if (textureReaderInitialized)
      {
        TextureReader_release();
        cameraImageBuffer.data = 0;
      }
      TextureReader_initialize(textureId,
          GL_TEXTURE_EXTERNAL_OES,
          tangoCameraIntrinsics.width,
          tangoCameraIntrinsics.height, true);
      textureReaderInitialized = true;
      textureReaderTextureId = textureId;
    }

    int cameraImageBufferSize;
    cameraImageBuffer.data = TextureReader_readPixels(&cameraImageBufferSize, 
        IMAGE_FORMAT_YUV420);
    textureReaderMutex.unlock();
    // A texture read has been performed. Notify one of the requesters.
    textureReaderCV.notify_one();
  }
  else {
    textureReaderMutex.unlock();
  }


  // LOGI("JUDAX: TangoHandler::updateCameraImageIntoTexture lastTangoImageBufferTimestamp = %lf, result = %d, textureId = %d", lastTangoImageBufferTimestamp, result, textureId);

  return result == TANGO_SUCCESS;
}

int TangoHandler::getSensorOrientation() const{
  return sensorOrientation;
}

int TangoHandler::getActivityOrientation() const{
  return activityOrientation;
}

void TangoHandler::addTangoHandlerEventListener(
    TangoHandlerEventListener* listener) {
  std::vector<TangoHandlerEventListener*>::const_iterator it = 
    std::find(listeners.begin(), listeners.end(), listener);
  if (it == listeners.end())
  {
    listeners.push_back(listener);
  }
}

void TangoHandler::removeTangoHandlerEventListener(
    TangoHandlerEventListener* listener) {
  std::vector<TangoHandlerEventListener*>::const_iterator it = 
    std::find(listeners.begin(), listeners.end(), listener);
  if (it != listeners.end())
  {
    listeners.erase(it);
  }
}

void TangoHandler::removeAllTangoHandlerEventListeners() {
  listeners.clear();
}

bool TangoHandler::hasLastTangoImageBufferTimestampChangedLately() {
  std::time_t currentTime;
  std::time(&currentTime);
  return std::difftime(currentTime, lastTangoImagebufferTimestampTime) < 1.0;
}

void TangoHandler::removeThisMarkerDetectionThread() {
  if (joiningMarkerDetectionThreads) {
      markerDetectionThreadsMutex.unlock();
      return;
  }
  std::thread::id thisThreadId = std::this_thread::get_id();
  std::hash<std::thread::id> threadHasher;
  std::size_t thisThreadIdHash = threadHasher(thisThreadId);
  markerDetectionThreadsMutex.lock();
  auto iter = std::find_if(markerDetectionThreads.begin(), 
      markerDetectionThreads.end(), 
      [=](std::thread &t) { 
       return (t.get_id() == thisThreadId); 
      });
  if (iter != markerDetectionThreads.end())
  {
    iter->detach();
    markerDetectionThreads.erase(iter);
  }
  markerDetectionThreadsMutex.unlock();
}

void TangoHandler::waitForAllMarkerDetectionThreads() {
  // Disable any read texture.
  textureReaderMutex.lock();
  TextureReader_release();
  textureReaderInitialized = false;
  cameraImageBuffer.data = 0;
  textureReaderMutex.unlock();
  // Marker detection threads might be waiting for the texture to be read so
  // wake them all up.
  textureReaderCV.notify_all();
  // TODO: This is the only place where we can assume that the page has been
  // realoded so wait for all the marker detection threads to finish.
  // Using an ugly hack for now to tell the threads that they do not need
  // to detach because the main thread is joining them. Maybe we can use the
  // MarkerDetector and MarkerDetectorThreadPool concepts, but they need a 
  // bit more love.
  markerDetectionThreadsMutex.lock();
  joiningMarkerDetectionThreads = true;
  for (auto& markerDetectionThread: markerDetectionThreads) {
    if (markerDetectionThread.joinable()) {
      markerDetectionThread.join();
    }
  }
  // Clear all the threads.
  markerDetectionThreads.clear();
  markerDetectionThreadsMutex.unlock();
  // TODO: Reset the flag that indicates the marker detection threads that the
  // main thread is no longer waiting to join them.
  joiningMarkerDetectionThreads = false;
  // Do also clear all the detected markers.
  detectedMarkersMutex.lock();
  detectedMarkers.clear();
  detectedMarkersMutex.unlock();
}

bool TangoHandler::getMarkers(TangoMarkers_MarkerType markerType, 
    float markerSize, std::vector<Marker>& markers) {
  if (connected)
  {

    time_type currentTime = 
        std::chrono::time_point_cast<std::chrono::milliseconds>(clock::now());
    long elapsedTimeBetweenGetMarkerCalls = 
        (currentTime - lastGetMarkersCallTime).count();

    // Lock the use of he detectedMarkers vector
    detectedMarkersMutex.lock();

    // Make a copy of the currently detected markers to the structure that requested them.
    markers.insert(markers.begin(), detectedMarkers.begin(), 
        detectedMarkers.end());
    // Now that the detected markers have been copied, clear the list for future requests.
    detectedMarkers.clear();

    // We are done with the container so unlock the mutex.
    detectedMarkersMutex.unlock();      

    // Marker detection is a time-consuming process. This is to make sure marker
    // detection process runs at a frequency no higher than a pre-defined FPS.
    if (elapsedTimeBetweenGetMarkerCalls < (1000 / kMarkerDetectionFPS))
    {
      // If the getMarker call was made too fast, return whatever we've got.
      return true;
    }

    // Get the current time as the last time for future request elapsed time calculation.
    lastGetMarkersCallTime = currentTime;

    // Start a new detection query in a different thread.
    // Store the thread to be able to "stop" them if needed.
    std::thread t([this, markerType, markerSize]()
    {
    // Get latest image buffer.
    // TangoImageBuffer* imageBuffer = nullptr;
    // TangoErrorType status = TangoSupport_getLatestImageBuffer(
    //     imageBufferManager, &imageBuffer);
    // if (status == TANGO_SUCCESS)
    // {
      TangoMarkers_DetectParam param;
      param.type = markerType;
      param.marker_size = markerSize;
      TangoMarkers_MarkerList markerList;
      
      double translation[3];
      double orientation[4];
      // Get the translation and orientation from the last pose.
      poseForMarkerDetectionMutex.lock();
      memcpy(translation, poseForMarkerDetection.translation, 
          sizeof(double) * 3);
      memcpy(orientation, poseForMarkerDetection.orientation, 
          sizeof(double) * 4);
      poseForMarkerDetectionMutex.unlock();

      TangoErrorType errorType;

      // Make sure there is camera image buffer data to be able to detect 
      // markers. 
      // Both detecting markers and acquiring the camera frame (at least with
      // the camera read utility library) are very impactful on performance
      // so this thread will request a frame and wait until the frame is
      // acquired.
      // NOTE: The unique_lock locks the mutex in the constructor so we are
      // safe to work with some variables that should be guarded.
      std::unique_lock<std::mutex> lock(textureReaderMutex);
      // If the needed conditions are not met, wait.
      if (!textureReaderInitialized || !cameraImageBuffer.data) {
        textureReadRequestCounter++;
        // Wait until the frame is acquired.
        textureReaderCV.wait(lock);
        textureReadRequestCounter--;
        // If the condition was interrupted and we still do not have what we were
        // waiting for, just assume that the interruption came from the
        // disconnect event, and cancel the detection.
        if (!textureReaderInitialized || !cameraImageBuffer.data) {
          lock.unlock();
          removeThisMarkerDetectionThread();
          return;
        }
      }
      // Detect markers as we are certain we have the needed data.
      errorType = TangoMarkers_detectMarkers(
          &cameraImageBuffer, 
          TANGO_CAMERA_COLOR, 
          translation, 
          orientation, 
          &param, 
          &markerList);
      // Reset the image data so no more texture read are executed until needed.
      cameraImageBuffer.data = 0;
      // Finally unlock.
      lock.unlock();

      if (errorType == TANGO_SUCCESS)
      {
        detectedMarkersMutex.lock();
        detectedMarkers.clear();
        for (int i = 0; i < markerList.marker_count; ++i)
        {
          int id = 0;
          std::string content;
          switch(markerType)
          {
            case TANGO_MARKERS_MARKER_ARTAG:
              id = atoi(markerList.markers[i].content);
              break;
            case TANGO_MARKERS_MARKER_QRCODE:
              content = std::string(markerList.markers[i].content, 
                  markerList.markers[i].content_size);
              break;
          }
          mat4 modelMatrix = mat4FromTranslationOrientation(
              markerList.markers[i].translation,
              markerList.markers[i].orientation);
          detectedMarkers.push_back(Marker(markerType, id,
            content, (float*)&modelMatrix));
        }
        detectedMarkersMutex.unlock();
        TangoMarkers_freeMarkerList(&markerList);
      }
      // }
      // else 
      // {
      //   LOGE("ERROR: Could not retrieve the latest image buffer while trying to detect markers.");
      // }

      // Remove this thread from the markerDetectionThreads container.
      removeThisMarkerDetectionThread();
    });

    markerDetectionThreadsMutex.lock();
    markerDetectionThreads.push_back(std::move(t));
    markerDetectionThreadsMutex.unlock();
  }

  return connected;
}

}  // namespace tango_chromium
