#include "Anchor.h"

#include "AnchorManager.h"

#include "gtc/type_ptr.hpp"

namespace tango_chromium {

uint32_t Anchor::identifierCounter = 0;

Anchor::Anchor(double timestamp, const float* cameraModelMatrix,
               const float* anchorModelMatrix): identifier(identifierCounter++), 
                                                timestamp(timestamp) {
  // TODO: Why store the cameraModelMatrix? A local variable would do it.
  glm::mat4 cameraModelMatrixMat4;
  memcpy(glm::value_ptr(cameraModelMatrixMat4), cameraModelMatrix,
         sizeof(glm::mat4));
  memcpy(glm::value_ptr(this->anchorModelMatrix), anchorModelMatrix,
         sizeof(glm::mat4));
  glm::mat4 inverseCameraModelMatrix = glm::inverse(cameraModelMatrixMat4);
  anchorModelMatrixRelativeToCamera =
    this->anchorModelMatrix * inverseCameraModelMatrix;
}

void Anchor::update(const float* newCameraModelMatrix) {
  glm::mat4 newCameraModelMatrixMat4;
  memcpy(glm::value_ptr(newCameraModelMatrixMat4), newCameraModelMatrix,
         sizeof(glm::mat4));
  anchorModelMatrix = anchorModelMatrixRelativeToCamera * 
                      newCameraModelMatrixMat4;
}

uint32_t Anchor::getIdentifier() const {
  return identifier;
}

const float* Anchor::getModelMatrix() const {
  return (const float*)(&anchorModelMatrix);
}

} // tango_chromium