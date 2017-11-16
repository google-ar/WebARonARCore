// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DEVICE_H
#define DEVICE_VR_ANDROID_GVR_DEVICE_H

#include "base/macros.h"
#include "device/vr/vr_device.h"

namespace gvr {
class GvrApi;
}  // namespace gvr

namespace device {

class GvrDeviceProvider;
class GvrDelegate;

class GvrDevice : public VRDevice {
 public:
  GvrDevice(GvrDeviceProvider* provider, GvrDelegate* delegate);
  ~GvrDevice() override;

  // VRDevice
  mojom::VRDisplayInfoPtr GetVRDevice() override;
  mojom::VRPosePtr GetPose() override;
  void ResetPose() override;

  mojom::VRPassThroughCameraPtr GetPassThroughCamera() override;
  std::vector<mojom::VRHitPtr> HitTest(float x, float y) override;
  mojom::VRPlaneDeltasPtr GetPlaneDeltas() override;
  mojom::VRAnchorPtr AddAnchor(
    const std::vector<float>& modelMatrix) override;
  void RemoveAnchor(uint32_t identifier) override;
  std::vector<mojom::VRMarkerPtr> GetMarkers(unsigned markerType, 
      float markerSize) override;

  void RequestPresent(const base::Callback<void(bool)>& callback) override;
  void SetSecureOrigin(bool secure_origin) override;
  void ExitPresent() override;

  void SubmitFrame(mojom::VRPosePtr pose) override;
  void UpdateLayerBounds(mojom::VRLayerBoundsPtr left_bounds,
      mojom::VRLayerBoundsPtr right_bounds) override;

  void SetDelegate(GvrDelegate* delegate);

 private:
  gvr::GvrApi* GetGvrApi();

  GvrDelegate* delegate_;
  GvrDeviceProvider* gvr_provider_;
  bool secure_origin_ = false;
  uint32_t pose_index_ = 1;

  DISALLOW_COPY_AND_ASSIGN(GvrDevice);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DEVICE_H
