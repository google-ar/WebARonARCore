// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DEVICE_H
#define DEVICE_VR_VR_DEVICE_H

#include "base/callback.h"
#include "base/macros.h"
#include "device/vr/vr_export.h"
#include "device/vr/vr_service.mojom.h"

namespace device {

class VRDisplayImpl;

const unsigned int VR_DEVICE_LAST_ID = 0xFFFFFFFF;

class DEVICE_VR_EXPORT VRDevice {
 public:
  VRDevice();
  virtual ~VRDevice();

  unsigned int id() const { return id_; }

  virtual mojom::VRDisplayInfoPtr GetVRDevice() = 0;
  virtual mojom::VRPosePtr GetPose() = 0;
  virtual void ResetPose() = 0;
  virtual mojom::VRPassThroughCameraPtr GetPassThroughCamera() = 0;
  virtual std::vector<mojom::VRHitPtr> HitTest(float x, float y) = 0;
  virtual mojom::VRPlaneDeltasPtr GetPlaneDeltas() = 0;
  virtual mojom::VRAnchorPtr CreateAnchor(
    const std::vector<float>& modelMatrix) = 0;
  virtual void RemoveAnchor(uint32_t identifier) = 0;
  virtual std::vector<mojom::VRMarkerPtr> GetMarkers(unsigned markerType, float markerSize) = 0;

  virtual void RequestPresent(const base::Callback<void(bool)>& callback) = 0;
  virtual void SetSecureOrigin(bool secure_origin) = 0;
  virtual void ExitPresent() = 0;
  virtual void SubmitFrame(mojom::VRPosePtr pose) = 0;
  virtual void UpdateLayerBounds(mojom::VRLayerBoundsPtr left_bounds,
                                 mojom::VRLayerBoundsPtr right_bounds) = 0;

  virtual void AddDisplay(VRDisplayImpl* display);
  virtual void RemoveDisplay(VRDisplayImpl* display);

  virtual bool IsAccessAllowed(VRDisplayImpl* display);
  virtual bool CheckPresentingDisplay(VRDisplayImpl* display);

  virtual void OnChanged();
  virtual void OnAnchorsUpdated(std::vector<mojom::VRAnchorPtr> anchors);
  virtual void OnExitPresent();
  virtual void OnBlur();
  virtual void OnFocus();
  virtual void OnActivate(mojom::VRDisplayEventReason reason);
  virtual void OnDeactivate(mojom::VRDisplayEventReason reason);

 protected:
  friend class VRDisplayImpl;
  friend class VRDisplayImplTest;

  void SetPresentingDisplay(VRDisplayImpl* display);

 private:
  std::set<VRDisplayImpl*> displays_;

  VRDisplayImpl* presenting_display_;

  unsigned int id_;

  static unsigned int next_id_;

  DISALLOW_COPY_AND_ASSIGN(VRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_VR_DEVICE_H
