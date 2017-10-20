// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRAnchor_h
#define VRAnchor_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMTypedArray.h"
#include "device/vr/vr_service.mojom-blink.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"

namespace blink {

class VRAnchor final : public GarbageCollected<VRAnchor>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    VRAnchor();

    long identifier() const { return m_identifier; }
    DOMFloat32Array* modelMatrix() const { return m_modelMatrix; }

    void setAnchor(const device::mojom::blink::VRAnchorPtr&);

    DECLARE_VIRTUAL_TRACE();

private:
    long m_identifier;
    Member<DOMFloat32Array> m_modelMatrix;
};

} // namespace blink

#endif // VRAnchor_h
