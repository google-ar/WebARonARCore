#ifndef _ANCHOR_H_
#define _ANCHOR_H_

#include "glm.hpp"

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
  void update(const float* newCameraModelMatrix);

  static uint32_t identifierCounter;
  uint32_t identifier;
  double timestamp;
  glm::mat4 anchorModelMatrix;
  glm::mat4 anchorModelMatrixRelativeToCamera;

  friend class AnchorManager;
};

} // tango_chromium

#endif // _ANCHOR_H_