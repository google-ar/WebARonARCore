#include "AnchorManager.h"

#include "Anchor.h"

#include "gtc/type_ptr.hpp"

#include "tango_client_api.h"   // NOLINT
// #include "tango_client_api2.h"   // NOLINT
#include "tango_support.h"  // NOLINT

#include "MathUtils.h"
#include "LogUtils.h"

namespace tango_chromium {

std::shared_ptr<Anchor> AnchorManager::createAnchor(double timestamp,
  const float* cameraModelMatrix, const float* anchorModelMatrix) {
  std::shared_ptr<Anchor> anchor(new Anchor(timestamp,
                                            cameraModelMatrix,
                                            anchorModelMatrix));
  anchors[anchor->getIdentifier()] = anchor;
  return anchor;
}

void AnchorManager::removeAnchor(uint32_t identifier) {
  std::unordered_map<uint32_t, std::shared_ptr<Anchor>>::iterator it = 
      anchors.find(identifier);
  if (it != anchors.end())
  {
    anchors.erase(it);
  }
}

void AnchorManager::removeAllAnchors() {
  anchors.clear();
}

std::vector<std::shared_ptr<Anchor>> AnchorManager::update(
    double historyChangeTimestamp, int activityOrientation) {
  std::vector<std::shared_ptr<Anchor>> updatedAnchors;
  updatedAnchors.reserve(anchors.size());
  std::unordered_map<uint32_t, std::shared_ptr<Anchor>>::const_iterator it =
    anchors.begin();
  for (; it != anchors.end(); it++) {
    std::shared_ptr<Anchor> anchor = it->second;
    if (historyChangeTimestamp < anchor->timestamp) {
      TangoPoseData newTangoPoseData;
      if (TangoSupport_getPoseAtTime(
            anchor->timestamp, TANGO_COORDINATE_FRAME_START_OF_SERVICE,
            TANGO_COORDINATE_FRAME_CAMERA_COLOR, TANGO_SUPPORT_ENGINE_OPENGL,
            TANGO_SUPPORT_ENGINE_OPENGL,
            static_cast<TangoSupport_Rotation>(activityOrientation), 
            &newTangoPoseData) == TANGO_SUCCESS) {
        glm::mat4 newCameraModelMatrix = mat4FromTranslationOrientation(
            newTangoPoseData.translation, newTangoPoseData.orientation);
        anchor->update(glm::value_ptr(newCameraModelMatrix));
        updatedAnchors.push_back(anchor);
      }
      else {
        LOGE("ERROR: Could not retrieve the new pose data from the pose " \
             "history change for anchor with identifier = %d.", 
             anchor->identifier);
      }
    }
  }
  return updatedAnchors;
}

} // tango_chromium