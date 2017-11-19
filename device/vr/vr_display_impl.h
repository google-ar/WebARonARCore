// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DISPLAY_IMPL_H
#define DEVICE_VR_VR_DISPLAY_IMPL_H

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_export.h"
#include "device/vr/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace device {

class VRServiceImpl;

class VRDisplayImpl : public mojom::VRDisplay {
 public:
  VRDisplayImpl(device::VRDevice* device, VRServiceImpl* service);
  ~VRDisplayImpl() override;

  mojom::VRDisplayClient* client() { return client_.get(); }

 private:
  friend class VRDisplayImplTest;
  friend class VRServiceImpl;

  void GetPose(const GetPoseCallback& callback) override;
  void ResetPose() override;

  void HitTest(float x, float y, const HitTestCallback& callback) override;
  void GetPassThroughCamera(const GetPassThroughCameraCallback& callback) override;
  void GetPlaneDeltas(const GetPlaneDeltasCallback& callback) override;
  void AddAnchor(const std::vector<float>& modelMatrix,
      const AddAnchorCallback& callback) override;
  void RemoveAnchor(uint32_t identifier,
      const RemoveAnchorCallback& callback) override;
  void GetMarkers(unsigned markerType, float markerSize, 
      const GetMarkersCallback& callback) override;
  void RequestPresent(bool secure_origin,
      const RequestPresentCallback& callback) override;
  void ExitPresent() override;
  void SubmitFrame(mojom::VRPosePtr pose) override;

  void UpdateLayerBounds(mojom::VRLayerBoundsPtr left_bounds,
      mojom::VRLayerBoundsPtr right_bounds) override;

  void RequestPresentResult(const RequestPresentCallback& callback,
      bool secure_origin, bool success);

  mojo::Binding<mojom::VRDisplay> binding_;
  mojom::VRDisplayClientPtr client_;
  device::VRDevice* device_;
  VRServiceImpl* service_;

  base::WeakPtrFactory<VRDisplayImpl> weak_ptr_factory_;
};

}  // namespace device

#endif  //  DEVICE_VR_VR_DISPLAY_IMPL_H
