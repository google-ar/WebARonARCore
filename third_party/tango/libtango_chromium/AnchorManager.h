#ifndef _ANCHOR_MANAGER_H_
#define _ANCHOR_MANAGER_H_

#include "glm.hpp"
#include <unordered_map>
#include <vector>

// TODO: Move this to its own header. LogUtils.h?
#include <android/log.h>
#define LOG_TAG "LeTango Chromium"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace tango_chromium {

class AnchorManager;

class Anchor {
public:
  uint32_t getIdentifier() const;
  const float* getModelMatrix() const;

private:
  Anchor(double timestamp, const float* cameraModelMatrix,
         const float* anchorModelMatrix);
  Anchor(const Anchor&) = delete;
  Anchor& operator=(const Anchor&) = delete;
  /**
  * Return: A flag to indicate if the anchor has been updated.
  */
  bool update(double newTimestamp, const float* newCameraModelMatrix);

  static uint32_t identifierCounter;
  uint32_t identifier;
  double timestamp;
  glm::mat4 cameraModelMatrix;
  glm::mat4 anchorModelMatrix;
  glm::mat4 anchorModelMatrixRelativeToCamera;

  friend class AnchorManager;
};

class AnchorManager {
public:
  std::shared_ptr<Anchor> createAnchor(double timestamp, 
                                       const float* cameraModelMatrix,
                                       const float* anchorModelMatrix);
  void removeAnchor(uint32_t identifier);
  void removeAllAnchors();

  /**
  * Return: The list of updated anchors.
  */
  std::vector<std::shared_ptr<Anchor>> update(
      double newTimestamp, const float* newCameraModelMatrix);
  
private:
  std::unordered_map<uint32_t, std::shared_ptr<Anchor>> anchors;
};

}

#endif