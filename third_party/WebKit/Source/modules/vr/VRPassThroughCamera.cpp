// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRPassThroughCamera.h"

namespace blink {

VRPassThroughCamera::VRPassThroughCamera(): m_width(0)
  , m_height(0)
  , m_textureWidth(0)
  , m_textureHeight(0)
  , m_focalLengthX(0)
  , m_focalLengthY(0)
  , m_pointX(0)
  , m_pointY(0)
  , m_orientation(0) {
}

unsigned long VRPassThroughCamera::width() const {
  return m_width;
}

unsigned long VRPassThroughCamera::height() const {
  return m_height;
}

unsigned long VRPassThroughCamera::textureWidth() const {
  return m_textureWidth;
}

unsigned long VRPassThroughCamera::textureHeight() const {
  return m_textureHeight;
}

double VRPassThroughCamera::focalLengthX() const {
  return m_focalLengthX;
}

double VRPassThroughCamera::focalLengthY() const {
  return m_focalLengthY;
}

double VRPassThroughCamera::pointX() const {
  return m_pointX;
}

double VRPassThroughCamera::pointY() const {
  return m_pointY;
}

long VRPassThroughCamera::orientation() {
  return m_orientation;
}

void VRPassThroughCamera::setPassThroughCamera(
    const device::mojom::blink::VRPassThroughCameraPtr& passThroughCameraPtr) {
  m_width = passThroughCameraPtr->width;
  m_height = passThroughCameraPtr->height;
  m_textureWidth = passThroughCameraPtr->textureWidth;
  m_textureHeight = passThroughCameraPtr->textureHeight;
  m_focalLengthX = passThroughCameraPtr->focalLengthX;
  m_focalLengthY = passThroughCameraPtr->focalLengthY;
  m_pointX = passThroughCameraPtr->pointX;
  m_pointY = passThroughCameraPtr->pointY;
  m_orientation = passThroughCameraPtr->orientation;
}

DEFINE_TRACE(VRPassThroughCamera) {
}

} // namespace blink
