// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device.h"

#include <math.h>
#include <algorithm>

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "device/vr/vr_device_manager.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

namespace {

static const uint64_t kPredictionTimeWithoutVsyncNanos = 50000000;

}  // namespace

GvrDevice::GvrDevice(GvrDeviceProvider* provider, GvrDelegate* delegate)
    : VRDevice(), delegate_(delegate), gvr_provider_(provider) {}

GvrDevice::~GvrDevice() {}

mojom::VRDisplayInfoPtr GvrDevice::GetVRDevice() {
  TRACE_EVENT0("input", "GvrDevice::GetVRDevice");

  mojom::VRDisplayInfoPtr device = mojom::VRDisplayInfo::New();

  device->index = id();

  device->capabilities = mojom::VRDisplayCapabilities::New();
  device->capabilities->hasOrientation = true;
  device->capabilities->hasPosition = false;
  device->capabilities->hasExternalDisplay = false;
  device->capabilities->canPresent = true;
  device->capabilities->hasPassThroughCamera = false;

  device->leftEye = mojom::VREyeParameters::New();
  device->rightEye = mojom::VREyeParameters::New();
  mojom::VREyeParametersPtr& left_eye = device->leftEye;
  mojom::VREyeParametersPtr& right_eye = device->rightEye;

  left_eye->fieldOfView = mojom::VRFieldOfView::New();
  right_eye->fieldOfView = mojom::VRFieldOfView::New();

  left_eye->offset.resize(3);
  right_eye->offset.resize(3);

  // Set the render target size to "invalid" to indicate that
  // we can't render into it yet. Other code uses this to check
  // for valid state.
  gvr::Sizei render_target_size = kInvalidRenderTargetSize;
  left_eye->renderWidth = render_target_size.width / 2;
  left_eye->renderHeight = render_target_size.height;

  right_eye->renderWidth = left_eye->renderWidth;
  right_eye->renderHeight = left_eye->renderHeight;

  gvr::GvrApi* gvr_api = GetGvrApi();
  if (!gvr_api) {
    // We may not be able to get an instance of GvrApi right away, so
    // stub in some data till we have one.
    device->displayName = "Unknown";

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
    left_eye->offset[2] = -0.03;

    right_eye->offset[0] = 0.0;
    right_eye->offset[1] = 0.0;
    right_eye->offset[2] = 0.03;

    // Tell the delegate not to draw yet, to avoid a race condition
    // (and visible wobble) on entering VR.
    if (delegate_) {
      delegate_->SetWebVRRenderSurfaceSize(kInvalidRenderTargetSize.width,
                                           kInvalidRenderTargetSize.height);
    }

    return device;
  }

  // In compositor mode, we have to use the current compositor window's
  // surface size. Would be nice to change it, but that needs more browser
  // internals to be modified. TODO(klausw,crbug.com/655722): remove this once
  // we can pick our own surface size.
  gvr::Sizei compositor_size = delegate_->GetWebVRCompositorSurfaceSize();
  left_eye->renderWidth = compositor_size.width / 2;
  left_eye->renderHeight = compositor_size.height;
  right_eye->renderWidth = left_eye->renderWidth;
  right_eye->renderHeight = left_eye->renderHeight;

  std::string vendor = gvr_api->GetViewerVendor();
  std::string model = gvr_api->GetViewerModel();
  device->displayName = vendor + " " + model;

  gvr::BufferViewportList gvr_buffer_viewports =
      gvr_api->CreateEmptyBufferViewportList();
  gvr_buffer_viewports.SetToRecommendedBufferViewports();

  gvr::BufferViewport eye_viewport = gvr_api->CreateBufferViewport();
  gvr_buffer_viewports.GetBufferViewport(GVR_LEFT_EYE, &eye_viewport);
  gvr::Rectf eye_fov = eye_viewport.GetSourceFov();
  left_eye->fieldOfView->upDegrees = eye_fov.top;
  left_eye->fieldOfView->downDegrees = eye_fov.bottom;
  left_eye->fieldOfView->leftDegrees = eye_fov.left;
  left_eye->fieldOfView->rightDegrees = eye_fov.right;

  eye_viewport = gvr_api->CreateBufferViewport();
  gvr_buffer_viewports.GetBufferViewport(GVR_RIGHT_EYE, &eye_viewport);
  eye_fov = eye_viewport.GetSourceFov();
  right_eye->fieldOfView->upDegrees = eye_fov.top;
  right_eye->fieldOfView->downDegrees = eye_fov.bottom;
  right_eye->fieldOfView->leftDegrees = eye_fov.left;
  right_eye->fieldOfView->rightDegrees = eye_fov.right;

  gvr::Mat4f left_eye_mat = gvr_api->GetEyeFromHeadMatrix(GVR_LEFT_EYE);
  left_eye->offset[0] = -left_eye_mat.m[0][3];
  left_eye->offset[1] = -left_eye_mat.m[1][3];
  left_eye->offset[2] = -left_eye_mat.m[2][3];

  gvr::Mat4f right_eye_mat = gvr_api->GetEyeFromHeadMatrix(GVR_RIGHT_EYE);
  right_eye->offset[0] = -right_eye_mat.m[0][3];
  right_eye->offset[1] = -right_eye_mat.m[1][3];
  right_eye->offset[2] = -right_eye_mat.m[2][3];

  if (delegate_) {
    delegate_->SetWebVRRenderSurfaceSize(2 * left_eye->renderWidth,
                                         left_eye->renderHeight);
  }

  return device;
}

mojom::VRPosePtr GvrDevice::GetPose() {
  TRACE_EVENT0("input", "GvrDevice::GetSensorState");

  mojom::VRPosePtr pose = mojom::VRPose::New();

  pose->timestamp = base::Time::Now().ToJsTime();
  
  // Increment pose frame counter always, even if it's a faked pose.
  pose->poseIndex = ++pose_index_;

  pose->orientation.emplace(4);

  gvr::GvrApi* gvr_api = GetGvrApi();
  if (!gvr_api) {
    // If we don't have a GvrApi instance return a static forward orientation.
    pose->orientation.value()[0] = 0.0;
    pose->orientation.value()[1] = 0.0;
    pose->orientation.value()[2] = 0.0;
    pose->orientation.value()[3] = 1.0;

    return pose;
  }

  if (!delegate_)
    return nullptr;

  gvr::ClockTimePoint target_time = gvr::GvrApi::GetTimePointNow();
  target_time.monotonic_system_time_nanos += kPredictionTimeWithoutVsyncNanos;

  gvr::Mat4f head_mat =
      gvr_api->GetHeadSpaceFromStartSpaceRotation(target_time);
  head_mat = gvr_api->ApplyNeckModel(head_mat, 1.0f);

  gfx::Transform inv_transform(
      head_mat.m[0][0], head_mat.m[0][1], head_mat.m[0][2], head_mat.m[0][3],
      head_mat.m[1][0], head_mat.m[1][1], head_mat.m[1][2], head_mat.m[1][3],
      head_mat.m[2][0], head_mat.m[2][1], head_mat.m[2][2], head_mat.m[2][3],
      head_mat.m[3][0], head_mat.m[3][1], head_mat.m[3][2], head_mat.m[3][3]);

  gfx::Transform transform;
  if (inv_transform.GetInverse(&transform)) {
    gfx::DecomposedTransform decomposed_transform;
    gfx::DecomposeTransform(&decomposed_transform, transform);

    pose->orientation.value()[0] = decomposed_transform.quaternion[0];
    pose->orientation.value()[1] = decomposed_transform.quaternion[1];
    pose->orientation.value()[2] = decomposed_transform.quaternion[2];
    pose->orientation.value()[3] = decomposed_transform.quaternion[3];

    pose->position.emplace(3);
    pose->position.value()[0] = decomposed_transform.translate[0];
    pose->position.value()[1] = decomposed_transform.translate[1];
    pose->position.value()[2] = decomposed_transform.translate[2];
  }

  // Save the underlying GVR pose for use by rendering. It can't use a
  // VRPosePtr since that's a different data type.
  delegate_->SetGvrPoseForWebVr(head_mat, pose_index_);

  return pose;
}

void GvrDevice::ResetPose() {
  gvr::GvrApi* gvr_api = GetGvrApi();

  // Should never call RecenterTracking when using with Daydream viewers. On
  // those devices recentering should only be done via the controller.
  if (gvr_api && gvr_api->GetViewerType() == GVR_VIEWER_TYPE_CARDBOARD)
    gvr_api->RecenterTracking();
}

mojom::VRPassThroughCameraPtr GvrDevice::GetPassThroughCamera()
{
  return nullptr;
}

std::vector<mojom::VRHitPtr> GvrDevice::HitTest(float x, float y)
{
  std::vector<mojom::VRHitPtr> hits;
  return hits;
}

mojom::VRPlaneDeltasPtr GvrDevice::GetPlaneDeltas() {
  mojom::VRPlaneDeltasPtr planes = nullptr;
  return planes;
}

mojom::VRAnchorPtr GvrDevice::AddAnchor(
    const std::vector<float>& modelMatrix) {
  return nullptr;
}

void GvrDevice::RemoveAnchor(uint32_t identifier) {
}

std::vector<mojom::VRMarkerPtr> GvrDevice::GetMarkers(unsigned markerType, float markerSize) {
  std::vector<mojom::VRMarkerPtr> markers;
  return markers;
}

void GvrDevice::RequestPresent(const base::Callback<void(bool)>& callback) {
  gvr_provider_->RequestPresent(callback);
}

void GvrDevice::SetSecureOrigin(bool secure_origin) {
  secure_origin_ = secure_origin;
  if (delegate_)
    delegate_->SetWebVRSecureOrigin(secure_origin_);
}

void GvrDevice::ExitPresent() {
  gvr_provider_->ExitPresent();
  OnExitPresent();
}

void GvrDevice::SubmitFrame(mojom::VRPosePtr pose) {
  if (delegate_)
    delegate_->SubmitWebVRFrame();
}

void GvrDevice::UpdateLayerBounds(mojom::VRLayerBoundsPtr left_bounds,
                                  mojom::VRLayerBoundsPtr right_bounds) {
  if (!delegate_)
    return;

  gvr::Rectf left_gvr_bounds;
  left_gvr_bounds.left = left_bounds->left;
  left_gvr_bounds.top = 1.0f - left_bounds->top;
  left_gvr_bounds.right = left_bounds->left + left_bounds->width;
  left_gvr_bounds.bottom = 1.0f - (left_bounds->top + left_bounds->height);

  gvr::Rectf right_gvr_bounds;
  right_gvr_bounds.left = right_bounds->left;
  right_gvr_bounds.top = 1.0f - right_bounds->top;
  right_gvr_bounds.right = right_bounds->left + right_bounds->width;
  right_gvr_bounds.bottom = 1.0f - (right_bounds->top + right_bounds->height);

  delegate_->UpdateWebVRTextureBounds(left_gvr_bounds, right_gvr_bounds);
}

void GvrDevice::SetDelegate(GvrDelegate* delegate) {
  delegate_ = delegate;

  // Notify the clients that this device has changed
  if (delegate_) {
    delegate_->SetWebVRSecureOrigin(secure_origin_);
    OnChanged();
  }
}

gvr::GvrApi* GvrDevice::GetGvrApi() {
  if (!delegate_)
    return nullptr;

  return delegate_->gvr_api();
}

}  // namespace device
