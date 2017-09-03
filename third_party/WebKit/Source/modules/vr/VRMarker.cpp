// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRMarker.h"

namespace blink {

VRMarker::VRMarker(): m_type(0), m_id(0)
{
  m_position = DOMFloat32Array::create(3);
  m_orientation = DOMFloat32Array::create(4);
  m_modelMatrix = DOMFloat32Array::create(16);
}

unsigned VRMarker::type() const
{
  return m_type;
}

unsigned VRMarker::id() const
{
  return m_id;
}

String VRMarker::content() const
{
  return m_content;
}

DOMFloat32Array* VRMarker::position() const
{
  return m_position;
}

DOMFloat32Array* VRMarker::orientation() const
{
  return m_orientation;
}

DOMFloat32Array* VRMarker::modelMatrix() const
{
  return m_modelMatrix;
}

void VRMarker::setMarker(const device::mojom::blink::VRMarkerPtr& markerPtr)
{
  m_type = markerPtr->type;
  m_id = markerPtr->id;
  m_content = markerPtr->content;
  memcpy(m_position->data(), &(markerPtr->position.front()), 3 * sizeof(float));
  memcpy(m_orientation->data(), &(markerPtr->orientation.front()), 4 * sizeof(float));
  memcpy(m_modelMatrix->data(), &(markerPtr->modelMatrix.front()), 16 * sizeof(float));
}

DEFINE_TRACE(VRMarker)
{
  visitor->trace(m_position);
  visitor->trace(m_orientation);
  visitor->trace(m_modelMatrix);
}

} // namespace blink