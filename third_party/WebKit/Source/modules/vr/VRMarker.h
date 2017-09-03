// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRMarker_h
#define VRMarker_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "device/vr/vr_service.mojom-blink.h"
#include "core/dom/DOMTypedArray.h"
#include "wtf/text/WTFString.h"

namespace blink {

class VRMarker final : public GarbageCollected<VRMarker>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    VRMarker();

    unsigned type() const;
    unsigned id() const;
    String content() const;
    DOMFloat32Array* position() const;
    DOMFloat32Array* orientation() const;
    DOMFloat32Array* modelMatrix() const;

    void setMarker(const device::mojom::blink::VRMarkerPtr&);

    DECLARE_VIRTUAL_TRACE()
private:
    unsigned m_type;
    unsigned m_id;
    String m_content;
    Member<DOMFloat32Array> m_position;
    Member<DOMFloat32Array> m_orientation;
    Member<DOMFloat32Array> m_modelMatrix;
};

} // namespace blink

#endif // VRMarker_h