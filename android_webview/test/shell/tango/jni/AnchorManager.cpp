#include "AnchorManager.h"

#include "gtc/type_ptr.hpp"

using namespace tango_chromium;

uint32_t Anchor::identifierCounter = 0;

Anchor::Anchor(double timestamp, const float* cameraModelMatrix,
               const float* anchorModelMatrix):
                 identifier(identifierCounter++), timestamp(timestamp)
{
  // TODO: Why store the cameraModelMatrix? A local variable would do it.
  memcpy(glm::value_ptr(this->cameraModelMatrix), cameraModelMatrix,
         sizeof(glm::mat4));
  memcpy(glm::value_ptr(this->anchorModelMatrix), anchorModelMatrix,
         sizeof(glm::mat4));
  glm::mat4 inverseCameraModelMatrix = glm::inverse(this->cameraModelMatrix);
  anchorModelMatrixRelativeToCamera =
    inverseCameraModelMatrix * this->anchorModelMatrix;
}

bool Anchor::update(double newTimestamp, const float* newCameraModelMatrix)
{
  bool needsUpdate = newTimestamp <= timestamp;
  if (needsUpdate)
  {
    timestamp = newTimestamp;
    // TODO: Why store the cameraModelMatrix? A local variable would do it.
    memcpy(glm::value_ptr(cameraModelMatrix), newCameraModelMatrix, 
           sizeof(glm::mat4));
    anchorModelMatrix = cameraModelMatrix * anchorModelMatrixRelativeToCamera;
  }
  return needsUpdate;
}

uint32_t Anchor::getIdentifier() const
{
  return identifier;
}

const float* Anchor::getModelMatrix() const
{
  return (const float*)(&anchorModelMatrix);
}

// ===============================================

std::shared_ptr<Anchor> AnchorManager::createAnchor(double timestamp,
  const float* cameraModelMatrix, const float* anchorModelMatrix)
{
  std::shared_ptr<Anchor> anchor(new Anchor(timestamp,
                                            cameraModelMatrix,
                                            anchorModelMatrix));
  anchors[anchor->getIdentifier()] = anchor;
  return anchor;
}

void AnchorManager::removeAnchor(uint32_t identifier)
{
  std::unordered_map<uint32_t, std::shared_ptr<Anchor>>::iterator it = 
    anchors.find(identifier);
  if (it != anchors.end())
  {
    anchors.erase(it);
  }
}

void AnchorManager::removeAllAnchors()
{
  anchors.clear();
}

std::vector<std::shared_ptr<Anchor>> AnchorManager::update(
  double newTimestamp, const float* newCameraModelMatrix)
{
  std::vector<std::shared_ptr<Anchor>> updatedAnchors;
  updatedAnchors.reserve(anchors.size());
  std::unordered_map<uint32_t, std::shared_ptr<Anchor>>::const_iterator it =
    anchors.begin();
  for (; it != anchors.end(); it++) {
    std::shared_ptr<Anchor> anchor = it->second;
    if (anchor->update(newTimestamp, newCameraModelMatrix))
    {
      updatedAnchors.push_back(anchor);
    }
  }
  return updatedAnchors;
}
