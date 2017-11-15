// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_TANGO_VR_DEVICE_H
#define DEVICE_VR_TANGO_VR_DEVICE_H

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "device/vr/vr_device.h"

#include "tango_client_api.h"

#include "TangoHandler.h"

namespace tango_chromium {
  class Anchor;
}

namespace device {

using tango_chromium::TangoHandlerEventListener;
using tango_chromium::Anchor;

class TangoVRDeviceProvider;

class TangoVRDevice : public VRDevice, public TangoHandlerEventListener {
 public:
  explicit TangoVRDevice(TangoVRDeviceProvider* provider);
  ~TangoVRDevice() override;

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

  // Override from TangoHandlerEventListener
  void anchorsUpdated(
      const std::vector<std::shared_ptr<Anchor>>& anchors) override;

 private:

  void anchorsUpdatedInternal(
      const std::vector<std::shared_ptr<Anchor>>& anchors);

  TangoCoordinateFramePair tangoCoordinateFramePair;
  TangoVRDeviceProvider* tangoVRDeviceProvider;

  int lastSensorOrientation;
  int lastActivityOrientation;
  DISALLOW_COPY_AND_ASSIGN(TangoVRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_TANGO_VR_DEVICE_H
