#ifndef _ANCHOR_MANAGER_H_
#define _ANCHOR_MANAGER_H_

#include <unordered_map>
#include <vector>
#include <mutex>

namespace tango_chromium {

class Anchor;

class AnchorManager {
public:
  std::shared_ptr<Anchor> addAnchor(double timestamp, 
      const float* cameraModelMatrix,
      const float* anchorModelMatrix);
  void removeAnchor(uint32_t identifier);
  void removeAllAnchors();

  /**
  * Return: The list of updated anchors.
  */
  std::vector<std::shared_ptr<Anchor>> update(
      double historyChangeTimestamp, int activityOrientation);
  
private:
  std::unordered_map<uint32_t, std::shared_ptr<Anchor>> anchors;
  std::mutex anchorsMutex;
};

}

#endif