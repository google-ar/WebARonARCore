// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef VRAnchorEvent_h
#define VRAnchorEvent_h

#include "modules/EventModules.h"
#include "modules/vr/VRDisplay.h"
#include "modules/vr/VRAnchorEventInit.h"

namespace blink {

class VRAnchor;

class VRAnchorEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VRAnchorEvent* create() { return new VRAnchorEvent; }
  static VRAnchorEvent* create(const AtomicString& type,
                              bool canBubble,
                              bool cancelable,
                              VRDisplay* display,
                              HeapVector<Member<VRAnchor>> planes) {
    return new VRAnchorEvent(type, canBubble, cancelable, display, planes);
  }

  static VRAnchorEvent* create(const AtomicString& type,
                              const VRAnchorEventInit& initializer) {
    return new VRAnchorEvent(type, initializer);
  }

  ~VRAnchorEvent() override;

  VRDisplay* display() const { return m_display.get(); }
  HeapVector<Member<VRAnchor>> anchors() const { return m_anchors; }

  const AtomicString& interfaceName() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  VRAnchorEvent();
  VRAnchorEvent(const AtomicString& type,
               bool canBubble,
               bool cancelable,
               VRDisplay*,
               HeapVector<Member<VRAnchor>>);
  VRAnchorEvent(const AtomicString&, const VRAnchorEventInit&);

  Member<VRDisplay> m_display;
  HeapVector<Member<VRAnchor>> m_anchors;
};

}  // namespace blink

#endif  // VRAnchorEvent_h
