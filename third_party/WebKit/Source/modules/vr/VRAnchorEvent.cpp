// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRAnchorEvent.h"

#include "modules/vr/VRAnchor.h"

namespace blink {

VRAnchorEvent::VRAnchorEvent() {}

VRAnchorEvent::VRAnchorEvent(const AtomicString& type,
                             bool canBubble,
                             bool cancelable,
                             VRDisplay* display,
                             HeapVector<Member<VRAnchor>> anchors)
    : Event(type, canBubble, cancelable),
      m_display(display),
      m_anchors(anchors) {}

VRAnchorEvent::VRAnchorEvent(const AtomicString& type,
                             const VRAnchorEventInit& initializer)
    : Event(type, initializer) {
  if (initializer.hasDisplay())
    m_display = initializer.display();

  if (initializer.hasAnchors())
    m_anchors = initializer.anchors();
}

VRAnchorEvent::~VRAnchorEvent() {}

const AtomicString& VRAnchorEvent::interfaceName() const {
  return EventNames::VRAnchorEvent;
}

DEFINE_TRACE(VRAnchorEvent) {
  visitor->trace(m_display);
  visitor->trace(m_anchors);
  Event::trace(visitor);
}

}  // namespace blink
