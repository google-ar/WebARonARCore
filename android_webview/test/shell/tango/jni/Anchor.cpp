#include "Anchor.h"
#include "AnchorManager.h"
#include "MathUtils.h"
#include "gtc/type_ptr.hpp"

namespace tango_chromium {

uint32_t Anchor::identifierCounter = 0;

Anchor::Anchor(double timestamp, const float* cameraModelMatrix,
    const float* anchorModelMatrix): identifier(identifierCounter++), 
    timestamp(timestamp) {
  // Convert the passed camera model matrix to a glm matrix.
  glm::mat4 cameraModelMatrixMat4;
  memcpy(glm::value_ptr(cameraModelMatrixMat4), cameraModelMatrix,
         sizeof(glm::mat4));
  // Convert the passed anchor model matrix to a glm matrix
  memcpy(glm::value_ptr(this->anchorModelMatrix), anchorModelMatrix,
         sizeof(glm::mat4));

  // Calculate the inverse of the camera model matrix and multiply the anchor
  // model matrix by it to calculate the anchor's model matrix relative to
  // the camera. This process assumed that the anchor model matrix has no scale
  // (1, 1, 1)
  glm::mat4 cameraModelMatrixMat4Inverse = glm::inverse(cameraModelMatrixMat4);
  anchorModelMatrixRelativeToCamera = 
      this->anchorModelMatrix * cameraModelMatrixMat4Inverse;

  // This is a different way to calculate the anchor relative pose.
  // The advantage of this code is that it makes sure that the original
  // anchor model matrix scale is not used (forces to use 1, 1, 1).
  // Calculate the relative anchor model matrix to the camera model matrix.
  // anchorModelMatrixRelativeToCamera =
  //     getMatrixRelativeToMatrix(this->anchorModelMatrix, cameraModelMatrixMat4,
  //     false);
}

void Anchor::update(const float* newCameraModelMatrix) {
  // Create a glm matrix of the new camera model matrix
  glm::mat4 newCameraModelMatrixMat4;
  memcpy(glm::value_ptr(newCameraModelMatrixMat4), newCameraModelMatrix,
      sizeof(glm::mat4));
  // Calculate the new anchor model matrix using the stored anchor relative
  // model matrix and the new camera model matrix.
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