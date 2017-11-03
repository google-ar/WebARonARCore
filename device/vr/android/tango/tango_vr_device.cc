// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/tango/tango_vr_device.h"

#include "base/bind.h"
#include "base/task_scheduler/post_task.h"

#include "tango_support.h"
#include "Anchor.h"

#include "base/trace_event/trace_event.h"

#define THIS_VALUE_NEEDS_TO_BE_OBTAINED_FROM_THE_TANGO_API -1

using base::android::AttachCurrentThread;
using tango_chromium::TangoHandler;
using tango_chromium::Hit;
using tango_chromium::Plane;
using tango_chromium::PlaneDeltas;
using tango_chromium::Marker;

const float RAD_2_DEG = 180.0 / M_PI;

namespace device {

TangoVRDevice::TangoVRDevice(TangoVRDeviceProvider* provider)
    : tangoVRDeviceProvider(provider) {
  tangoCoordinateFramePair.base = TANGO_COORDINATE_FRAME_START_OF_SERVICE;
  tangoCoordinateFramePair.target = TANGO_COORDINATE_FRAME_DEVICE;

  TangoHandler::getInstance()->addTangoHandlerEventListener(this);
}

TangoVRDevice::~TangoVRDevice() {
}

mojom::VRDisplayInfoPtr TangoVRDevice::GetVRDevice() {
  TRACE_EVENT0("input", "TangoVRDevice::GetVRDevice");
  mojom::VRDisplayInfoPtr device = mojom::VRDisplayInfo::New();

  device->displayName = "Tango VR Device";

  device->capabilities = mojom::VRDisplayCapabilities::New();
  device->capabilities->hasOrientation = true;
  device->capabilities->hasPosition = true;
  device->capabilities->hasExternalDisplay = false;
  device->capabilities->canPresent = false;
  device->capabilities->hasPassThroughCamera = true;

  device->leftEye = mojom::VREyeParameters::New();
  device->rightEye = mojom::VREyeParameters::New();
  mojom::VREyeParametersPtr& left_eye = device->leftEye;
  mojom::VREyeParametersPtr& right_eye = device->rightEye;
  left_eye->fieldOfView = mojom::VRFieldOfView::New();
  right_eye->fieldOfView = mojom::VRFieldOfView::New();

  left_eye->offset.resize(3);
  right_eye->offset.resize(3);

  TangoHandler* tangoHandler = TangoHandler::getInstance();
  if (!tangoHandler->isConnected()) {
    // We may not be able to get an instance of TangoHandler right away, so
    // stub in some data till we have one.
    left_eye->fieldOfView->upDegrees = 45;
    left_eye->fieldOfView->downDegrees = 45;
    left_eye->fieldOfView->leftDegrees = 45;
    left_eye->fieldOfView->rightDegrees = 45;
    right_eye->fieldOfView->upDegrees = 45;
    right_eye->fieldOfView->downDegrees = 45;
    right_eye->fieldOfView->leftDegrees = 45;
    right_eye->fieldOfView->rightDegrees = 45;

    left_eye->offset[0] = -0.0;
    left_eye->offset[1] = -0.0;
    left_eye->offset[2] = -0.0;

    right_eye->offset[0] = 0.0;
    right_eye->offset[1] = 0.0;
    right_eye->offset[2] = 0.0;

    left_eye->renderWidth = 
        THIS_VALUE_NEEDS_TO_BE_OBTAINED_FROM_THE_TANGO_API / 2.0;
    left_eye->renderHeight = 
        THIS_VALUE_NEEDS_TO_BE_OBTAINED_FROM_THE_TANGO_API;

    right_eye->renderWidth = 
        THIS_VALUE_NEEDS_TO_BE_OBTAINED_FROM_THE_TANGO_API / 2.0;
    right_eye->renderHeight = 
        THIS_VALUE_NEEDS_TO_BE_OBTAINED_FROM_THE_TANGO_API;

    return device;
  }

  uint32_t iw = THIS_VALUE_NEEDS_TO_BE_OBTAINED_FROM_THE_TANGO_API;
  uint32_t ih = THIS_VALUE_NEEDS_TO_BE_OBTAINED_FROM_THE_TANGO_API;
  double fx = THIS_VALUE_NEEDS_TO_BE_OBTAINED_FROM_THE_TANGO_API;
  double fy = THIS_VALUE_NEEDS_TO_BE_OBTAINED_FROM_THE_TANGO_API;
  double cx = THIS_VALUE_NEEDS_TO_BE_OBTAINED_FROM_THE_TANGO_API;
  double cy = THIS_VALUE_NEEDS_TO_BE_OBTAINED_FROM_THE_TANGO_API;

  tangoHandler->getCameraImageSize(&iw, &ih);
  tangoHandler->getCameraFocalLength(&fx, &fy);
  tangoHandler->getCameraPoint(&cx, &cy);

  float vDegrees = atan(ih / (2.0 * fy)) * RAD_2_DEG;
  float hDegrees = atan(iw / (2.0 * fx)) * RAD_2_DEG;

  left_eye->fieldOfView->upDegrees = vDegrees;
  left_eye->fieldOfView->downDegrees = vDegrees;
  left_eye->fieldOfView->leftDegrees = hDegrees;
  left_eye->fieldOfView->rightDegrees = hDegrees;

  right_eye->fieldOfView->upDegrees = vDegrees;
  right_eye->fieldOfView->downDegrees = vDegrees;
  right_eye->fieldOfView->leftDegrees = hDegrees;
  right_eye->fieldOfView->rightDegrees = hDegrees;

  left_eye->offset[0] = 0.0f;
  left_eye->offset[1] = 0.0f;
  left_eye->offset[2] = 0.0f;

  right_eye->offset[0] = 0.0f;
  right_eye->offset[1] = 0.0f;
  right_eye->offset[2] = 0.0f;

  left_eye->renderWidth = iw;
  left_eye->renderHeight = ih;

  right_eye->renderWidth = iw;
  right_eye->renderHeight = ih;

  // Store the orientation values so we can check in future GetPose()
  // calls if we need to update camera intrinsics and regenerate the
  // VRDeviceInfoPtr
  lastSensorOrientation = tangoHandler->getSensorOrientation();
  lastActivityOrientation = tangoHandler->getActivityOrientation();

  return device;
}

mojom::VRPosePtr TangoVRDevice::GetPose() {

  // Check to see if orientation has changed, and if so, fire
  // an OnChanged() so that the VRFieldOfView can be updated,
  // with the up-to-date VRDeviceInfoPtr sent to WebKit for correct
  // projection matrix calculations.
  TangoHandler* tangoHandler = TangoHandler::getInstance();
  if (tangoHandler->isConnected() &&
      (lastSensorOrientation  != tangoHandler->getSensorOrientation() ||
      lastActivityOrientation != tangoHandler->getActivityOrientation())) {
    VRDevice::OnChanged();
  }

  TangoPoseData tangoPoseData;

  mojom::VRPosePtr pose = nullptr;

  if (tangoHandler->isConnected() && tangoHandler->getPose(&tangoPoseData))
  {
    pose = mojom::VRPose::New();

    pose->timestamp = base::Time::Now().ToJsTime();

    pose->orientation.emplace(4);
    pose->position.emplace(3);

    pose->orientation.value()[0] = tangoPoseData.orientation[0];
    pose->orientation.value()[1] = tangoPoseData.orientation[1];
    pose->orientation.value()[2] = tangoPoseData.orientation[2];
    pose->orientation.value()[3] = tangoPoseData.orientation[3];

    pose->position.value()[0] = tangoPoseData.translation[0];
    pose->position.value()[1] = tangoPoseData.translation[1];
    pose->position.value()[2] = tangoPoseData.translation[2];
  }

  return pose;
}

void TangoVRDevice::ResetPose() {
  TangoHandler::getInstance()->resetPose();
}

mojom::VRPassThroughCameraPtr TangoVRDevice::GetPassThroughCamera() {
  TangoHandler* tangoHandler = TangoHandler::getInstance();
  mojom::VRPassThroughCameraPtr seeThroughCameraPtr = nullptr;
  if (tangoHandler->isConnected())
  {
    seeThroughCameraPtr = mojom::VRPassThroughCamera::New();
    tangoHandler->getCameraImageSize(&(seeThroughCameraPtr->width), 
        &(seeThroughCameraPtr->height));
    tangoHandler->getCameraImageTextureSize(&(seeThroughCameraPtr->textureWidth), 
        &(seeThroughCameraPtr->textureHeight));
    tangoHandler->getCameraFocalLength(&(seeThroughCameraPtr->focalLengthX), 
        &(seeThroughCameraPtr->focalLengthY));
    tangoHandler->getCameraPoint(&(seeThroughCameraPtr->pointX), 
        &(seeThroughCameraPtr->pointY));
    seeThroughCameraPtr->orientation = tangoHandler->getSensorOrientation();
  }
  return seeThroughCameraPtr;
}

std::vector<mojom::VRHitPtr> TangoVRDevice::HitTest(float x, float y) {
  std::vector<mojom::VRHitPtr> mojomHits;
  if (TangoHandler::getInstance()->isConnected())
  {
    std::vector<Hit> hits;
    if (TangoHandler::getInstance()->hitTest(x, y, hits) && hits.size() > 0)
    {
      std::vector<Hit>::size_type size = hits.size();
      mojomHits.resize(size);
      for (std::vector<Hit>::size_type i = 0; i < size; i++)
      {
        mojomHits[i] = mojom::VRHit::New();
        mojomHits[i]->modelMatrix.resize(16);
        for (int j = 0; j < 16; j++)
        {
          mojomHits[i]->modelMatrix[j] = hits[i].modelMatrix[j];
        }
      }
    }
  }
  return mojomHits;
}

static mojom::VRPlanePtr CreateMojomPlane(Plane& plane) {
  mojom::VRPlanePtr result = mojom::VRPlane::New();

  result->identifier = plane.identifier;

  result->modelMatrix.resize(16);
  for (int i = 0; i < 16; i++)
  {
    result->modelMatrix[i] = plane.modelMatrix[i];
  }

  result->extent.resize(2);
  for (int i = 0; i < 2; i++) {
    result->extent[i] = plane.extent[i];
  }

  result->vertices.resize(plane.count * 3);
  for (uint i = 0; i < plane.count * 3; i++) {
    result->vertices[i] = plane.vertices[i];
  }
  result->count = plane.count;

  return result;
}

static mojom::VRAnchorPtr CreateMojomAnchor(
    const std::shared_ptr<Anchor>& anchor) {
  const float* modelMatrix = anchor->getModelMatrix(); 
  mojom::VRAnchorPtr mojomAnchor = mojom::VRAnchor::New();
  mojomAnchor->identifier = anchor->getIdentifier();
  mojomAnchor->modelMatrix.resize(16);
  for (size_t i = 0; i < 16; i++) {
    mojomAnchor->modelMatrix[i] = modelMatrix[i];
  }
  return mojomAnchor;
}

static void PopulateMojomPlanes(std::vector<mojom::VRPlanePtr>& mojomPlanes, 
    std::vector<Plane>& planes) {
  std::vector<Plane>::size_type size = planes.size();
  mojomPlanes.resize(size);
  for (std::vector<Plane>::size_type i = 0; i < size; i++)
  {
    mojomPlanes[i] = CreateMojomPlane(planes[i]);
  }
}

mojom::VRPlaneDeltasPtr TangoVRDevice::GetPlaneDeltas() {
  if (!TangoHandler::getInstance()->isConnected()) {
    return nullptr;
  }

  mojom::VRPlaneDeltasPtr mojomPlaneDeltas = mojom::VRPlaneDeltas::New();
  PlaneDeltas planeDeltas;
  if (TangoHandler::getInstance()->getPlaneDeltas(planeDeltas))
  {
    PopulateMojomPlanes(mojomPlaneDeltas->added, planeDeltas.added);
    PopulateMojomPlanes(mojomPlaneDeltas->updated, planeDeltas.updated);
    mojomPlaneDeltas->removed.resize(planeDeltas.removed.size());
    for (size_t i = 0; i < planeDeltas.removed.size(); i++) {
     mojomPlaneDeltas->removed[i] = planeDeltas.removed[i];
    }
  }
  
  return mojomPlaneDeltas;
}

mojom::VRAnchorPtr TangoVRDevice::CreateAnchor(
    const std::vector<float>& modelMatrix)  {
  if (!TangoHandler::getInstance()->isConnected()) 
  {
    return nullptr;
  }
  mojom::VRAnchorPtr mojomAnchor = nullptr;
  std::shared_ptr<Anchor> anchor = TangoHandler::getInstance()->
    createAnchor((const float*)(&(modelMatrix[0])));
  if (!anchor)
  {
    return nullptr;
  }
  mojomAnchor = CreateMojomAnchor(anchor);
  return mojomAnchor;
}

void TangoVRDevice::RemoveAnchor(uint32_t identifier) {
  if (!TangoHandler::getInstance()->isConnected()) {
    TangoHandler::getInstance()->removeAnchor(identifier);
  }
}

std::vector<mojom::VRMarkerPtr> TangoVRDevice::GetMarkers(unsigned markerType, 
    float markerSize) {
  std::vector<mojom::VRMarkerPtr> mojomMarkers;
  if (TangoHandler::getInstance()->isConnected())
  {
    TangoMarkers_MarkerType mt;
    switch(markerType)
    {
      case 0x1:
        mt = TANGO_MARKERS_MARKER_ARTAG;
        break;
      case 0x2:
        mt = TANGO_MARKERS_MARKER_QRCODE;
        break;
      default:
        VLOG(0) << "ERROR: Incorrect marker type value. Currently supported" \
            "values are VRDipslay.MARKER_TYPE_AR and " \
            "VRDisplay.MARKER_TYPE_QRCODE.";
        return mojomMarkers;
    }
    std::vector<Marker> markers;
    if (TangoHandler::getInstance()->getMarkers(mt, markerSize, markers))
    {
      std::vector<Marker>::size_type size = markers.size();
      mojomMarkers.resize(size);
      for (std::vector<Marker>::size_type i = 0; i < size; i++)
      {
        mojomMarkers[i] = mojom::VRMarker::New();
        mojomMarkers[i]->type = markers[i].getType();
        mojomMarkers[i]->id = markers[i].getId();
        mojomMarkers[i]->content = markers[i].getContent();
        mojomMarkers[i]->modelMatrix.resize(16);
        const float* modelMatrix = markers[i].getModelMatrix();
        for (int j = 0; j < 16; j++)
        {
          mojomMarkers[i]->modelMatrix[j] = modelMatrix[j];
        }
      }
    }
  }
  return mojomMarkers;
}

void TangoVRDevice::RequestPresent(const base::Callback<void(bool)>& callback) {
  // gvr_provider_->RequestPresent(callback);
}

void TangoVRDevice::SetSecureOrigin(bool secure_origin) {
  // secure_origin_ = secure_origin;
  // if (delegate_)
  //   delegate_->SetWebVRSecureOrigin(secure_origin_);
}

void TangoVRDevice::ExitPresent() {
  // gvr_provider_->ExitPresent();
  // OnExitPresent();
}

void TangoVRDevice::SubmitFrame(mojom::VRPosePtr pose) {
  // if (delegate_)
  //   delegate_->SubmitWebVRFrame();
}

void TangoVRDevice::UpdateLayerBounds(mojom::VRLayerBoundsPtr left_bounds,
                                  mojom::VRLayerBoundsPtr right_bounds) {
  // if (!delegate_)
  //   return;

  // gvr::Rectf left_gvr_bounds;
  // left_gvr_bounds.left = left_bounds->left;
  // left_gvr_bounds.top = 1.0f - left_bounds->top;
  // left_gvr_bounds.right = left_bounds->left + left_bounds->width;
  // left_gvr_bounds.bottom = 1.0f - (left_bounds->top + left_bounds->height);

  // gvr::Rectf right_gvr_bounds;
  // right_gvr_bounds.left = right_bounds->left;
  // right_gvr_bounds.top = 1.0f - right_bounds->top;
  // right_gvr_bounds.right = right_bounds->left + right_bounds->width;
  // right_gvr_bounds.bottom = 1.0f - (right_bounds->top + right_bounds->height);

  // delegate_->UpdateWebVRTextureBounds(left_gvr_bounds, right_gvr_bounds);
}

void TangoVRDevice::anchorsUpdated(
  const std::vector<std::shared_ptr<Anchor>>& anchors) {

  anchorsUpdatedInternal(anchors);
  
  // TODO: Tried these 2 options and none worked. One compiles/links and the
  // other just compiles.

  // base::PostTask(FROM_HERE, 
  //                base::Bind(&TangoVRDevice::anchorsUpdatedInternal, 
  //                           base::Unretained(this), anchors));

  // content::BrowserThread::PostTask(
  //     content::BrowserThread::UI, FROM_HERE, 
  //     base::Bind(&TangoVRDevice::anchorsUpdatedInternal, 
  //                base::Unretained(this), anchors));
}

void TangoVRDevice::anchorsUpdatedInternal(
  const std::vector<std::shared_ptr<Anchor>>& anchors) {
  std::vector<mojom::VRAnchorPtr> mojomAnchors;
  mojomAnchors.resize(anchors.size());
  for (size_t i = 0; i < anchors.size(); i++)
  {
    mojomAnchors[i] = CreateMojomAnchor(anchors[i]);
  }

  // VLOG(0) << "JUDAX: TangoVRDevice::anchorsUpdated -> mojomAnchors.size() = " 
          // << mojomAnchors.size(); 

  VRDevice::OnAnchorsUpdated(std::move(mojomAnchors));
}

}  // namespace device
