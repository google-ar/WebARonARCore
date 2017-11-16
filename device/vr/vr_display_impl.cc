// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_service_impl.h"

namespace device {

VRDisplayImpl::VRDisplayImpl(device::VRDevice* device, VRServiceImpl* service)
    : binding_(this),
      device_(device),
      service_(service),
      weak_ptr_factory_(this) {
  mojom::VRDisplayInfoPtr display_info = device->GetVRDevice();
  if (service->client()) {
    service->client()->OnDisplayConnected(binding_.CreateInterfacePtrAndBind(),
        mojo::MakeRequest(&client_),
        std::move(display_info));
  }
}

VRDisplayImpl::~VRDisplayImpl() {
  device_->RemoveDisplay(this);
}

void VRDisplayImpl::GetPose(const GetPoseCallback& callback) {
  if (!device_->IsAccessAllowed(this)) {
    callback.Run(nullptr);
    return;
  }

  callback.Run(device_->GetPose());
}

void VRDisplayImpl::ResetPose() {
  if (!device_->IsAccessAllowed(this))
    return;

  device_->ResetPose();
}

void VRDisplayImpl::HitTest(float x, float y, const HitTestCallback& callback)
{
  if (!device_->IsAccessAllowed(this)) {
    callback.Run(std::vector<mojom::VRHitPtr>());
    return;
  }

  callback.Run(device_->HitTest(x, y));
}

void VRDisplayImpl::GetPassThroughCamera(
    const GetPassThroughCameraCallback& callback) {
  if (!device_->IsAccessAllowed(this)) {
    callback.Run(nullptr);
    return;
  }

  callback.Run(device_->GetPassThroughCamera());
}

void VRDisplayImpl::GetPlaneDeltas(const GetPlaneDeltasCallback& callback) {
  if (!device_->IsAccessAllowed(this)) {
    callback.Run(nullptr);
    return;
  }

  callback.Run(device_->GetPlaneDeltas());
}

void VRDisplayImpl::AddAnchor(const std::vector<float>& modelMatrix,
    const AddAnchorCallback& callback) {
  if (!device_->IsAccessAllowed(this)) {
    callback.Run(nullptr);
    return;
  }

  callback.Run(device_->AddAnchor(modelMatrix));
}

void VRDisplayImpl::RemoveAnchor(uint32_t identifier) {
  if (device_->IsAccessAllowed(this)) {
    device_->RemoveAnchor(identifier);
  }
}

void VRDisplayImpl::GetMarkers(unsigned markerType, float markerSize, 
    const GetMarkersCallback& callback) {
  if (!device_->IsAccessAllowed(this)) {
    callback.Run(std::vector<mojom::VRMarkerPtr>());
    return;
  }

  callback.Run(device_->GetMarkers(markerType, markerSize));
}

void VRDisplayImpl::RequestPresent(bool secure_origin,
    const RequestPresentCallback& callback) {
  if (!device_->IsAccessAllowed(this)) {
    callback.Run(false);
    return;
  }

  device_->RequestPresent(base::Bind(&VRDisplayImpl::RequestPresentResult,
      weak_ptr_factory_.GetWeakPtr(), callback,
      secure_origin));
}

void VRDisplayImpl::RequestPresentResult(const RequestPresentCallback& callback,
    bool secure_origin,
    bool success) {
  if (success) {
    device_->SetPresentingDisplay(this);
    device_->SetSecureOrigin(secure_origin);
  }
  callback.Run(success);
}

void VRDisplayImpl::ExitPresent() {
  if (device_->CheckPresentingDisplay(this))
    device_->ExitPresent();
}

void VRDisplayImpl::SubmitFrame(mojom::VRPosePtr pose) {
  if (!device_->CheckPresentingDisplay(this))
    return;
  device_->SubmitFrame(std::move(pose));
}

void VRDisplayImpl::UpdateLayerBounds(mojom::VRLayerBoundsPtr left_bounds,
    mojom::VRLayerBoundsPtr right_bounds) {
  if (!device_->IsAccessAllowed(this))
    return;

  device_->UpdateLayerBounds(std::move(left_bounds), std::move(right_bounds));
}
}
