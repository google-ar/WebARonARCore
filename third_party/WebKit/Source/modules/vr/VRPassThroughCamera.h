// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRPassThroughCamera_h
#define VRPassThroughCamera_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "device/vr/vr_service.mojom-blink.h"

namespace blink {

class VRPassThroughCamera final : public GarbageCollected<VRPassThroughCamera>, 
public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    VRPassThroughCamera();

    unsigned long width() const;
    unsigned long height() const;
    unsigned long textureWidth() const;
    unsigned long textureHeight() const;
    double focalLengthX() const;
    double focalLengthY() const;
    double pointX() const;
    double pointY() const;
    long orientation();

    void setPassThroughCamera(
            const device::mojom::blink::VRPassThroughCameraPtr&);

    DECLARE_VIRTUAL_TRACE()
private:
    unsigned long m_width;
    unsigned long m_height;
    unsigned long m_textureWidth;
    unsigned long m_textureHeight;
    double m_focalLengthX;
    double m_focalLengthY;
    double m_pointX;
    double m_pointY;
    long m_orientation;
};

} // namespace blink

#endif // VRPassThroughCamera_h
