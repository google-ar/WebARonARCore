// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRAnchor.h"

namespace blink {

VRAnchor::VRAnchor() {
    m_modelMatrix = DOMFloat32Array::create(16);
}

void VRAnchor::setAnchor(const device::mojom::blink::VRAnchorPtr& anchorPtr) {
    if (anchorPtr.is_null()) {
        return;
    }

    m_identifier = anchorPtr->identifier;

    for (size_t i = 0; i < 16; i++) {
      m_modelMatrix->data()[i] = (float)anchorPtr->modelMatrix[i];
    }
}

DEFINE_TRACE(VRAnchor) {
    visitor->trace(m_modelMatrix);
}

} // namespace blink
