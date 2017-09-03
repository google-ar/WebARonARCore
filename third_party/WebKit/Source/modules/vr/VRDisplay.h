// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRDisplay_h
#define VRDisplay_h

#include "core/dom/Document.h"
#include "core/dom/FrameRequestCallback.h"
#include "core/events/EventTarget.h"
#include "core/dom/DOMTypedArray.h"
#include "device/vr/vr_service.mojom-blink.h"
#include "modules/vr/VRDisplayCapabilities.h"
#include "modules/vr/VRLayer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

namespace gpu {
namespace gles2 {

class GLES2Interface;
}
}

namespace blink {

class NavigatorVR;
class ScriptedAnimationController;
class VRController;
class VREyeParameters;
class VRFrameData;
class VRStageParameters;
class VRPose;
class VRHit;
class VRPassThroughCamera;
class VRPlane;
class VRAnchor;
class VRMarker;
class WebGLRenderingContextBase;

enum VREye { VREyeNone, VREyeLeft, VREyeRight };

class VRDisplay final : public EventTargetWithInlineData,
                        public ActiveScriptWrappable<VRDisplay>,
                        public ContextLifecycleObserver,
                        public device::mojom::blink::VRDisplayClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(VRDisplay);
  USING_PRE_FINALIZER(VRDisplay, dispose);

 public:
  ~VRDisplay();

  unsigned displayId() const { return m_displayId; }
  const String& displayName() const { return m_displayName; }

  VRDisplayCapabilities* capabilities() const { return m_capabilities; }
  VRStageParameters* stageParameters() const { return m_stageParameters; }

  bool isConnected() const { return m_isConnected; }
  bool isPresenting() const { return m_isPresenting; }

  bool getFrameData(VRFrameData*);
  VRPose* getPose();
  void resetPose();

  HeapVector<Member<VRHit>> hitTest(float x, float y);
  HeapVector<Member<VRPlane>> getPlanes();
  VRPassThroughCamera* getPassThroughCamera();
  VRAnchor* createAnchor(WTF::Vector<float>& modelMatrix);
  VRAnchor* createAnchor(DOMFloat32Array* modelMatrix);
  void removeAnchor(VRAnchor* anchor);
  HeapVector<Member<VRAnchor>> getAnchors();
  HeapVector<Member<VRMarker>> getMarkers(unsigned markerType, 
                                          float markerSize);

  double depthNear() const { return m_depthNear; }
  double depthFar() const { return m_depthFar; }

  void setDepthNear(double value) { m_depthNear = value; }
  void setDepthFar(double value) { m_depthFar = value; }

  VREyeParameters* getEyeParameters(const String&);

  int requestAnimationFrame(FrameRequestCallback*);
  void cancelAnimationFrame(int id);
  void serviceScriptedAnimations(double monotonicAnimationStartTime);

  ScriptPromise requestPresent(ScriptState*, const HeapVector<VRLayer>& layers);
  ScriptPromise exitPresent(ScriptState*);

  HeapVector<VRLayer> getLayers();

  void submitFrame();

  Document* document();

  // EventTarget overrides:
  ExecutionContext* getExecutionContext() const override;
  const AtomicString& interfaceName() const override;

  // ContextLifecycleObserver implementation.
  void contextDestroyed(ExecutionContext*) override;

  // ScriptWrappable implementation.
  bool hasPendingActivity() const final;

  DECLARE_VIRTUAL_TRACE();

 protected:
  friend class VRController;

  VRDisplay(NavigatorVR*,
            device::mojom::blink::VRDisplayPtr,
            device::mojom::blink::VRDisplayClientRequest);

  void update(const device::mojom::blink::VRDisplayInfoPtr&);

  void updatePose();

  void beginPresent();
  void forceExitPresent();

  void updateLayerBounds();
  void disconnected();

  VRController* controller();

 private:
  void onFullscreenCheck(TimerBase*);
  void onPresentComplete(bool);

  void onConnected();
  void onDisconnected();

  void OnPresentChange();

  // VRDisplayClient
  void OnChanged(device::mojom::blink::VRDisplayInfoPtr) override;
  void OnAnchorsUpdated(
    WTF::Vector<device::mojom::blink::VRAnchorPtr> anchors) override;
  void OnExitPresent() override;
  void OnBlur() override;
  void OnFocus() override;
  void OnActivate(device::mojom::blink::VRDisplayEventReason) override;
  void OnDeactivate(device::mojom::blink::VRDisplayEventReason) override;

  void dispatchPlaneEvent(const AtomicString& eventName, HeapVector<Member<VRPlane>>& vrPlanes);
  void dispatchPlaneEvent(const AtomicString& eventName, WTF::Vector<device::mojom::blink::VRPlanePtr>& planes);
  void updatePlanes();

  ScriptedAnimationController& ensureScriptedAnimationController(Document*);

  Member<NavigatorVR> m_navigatorVR;
  unsigned m_displayId;
  String m_displayName;
  bool m_isConnected;
  bool m_isPresenting;
  bool m_isValidDeviceForPresenting;
  bool m_canUpdateFramePose;
  Member<VRDisplayCapabilities> m_capabilities;
  Member<VRStageParameters> m_stageParameters;
  Member<VREyeParameters> m_eyeParametersLeft;
  Member<VREyeParameters> m_eyeParametersRight;
  device::mojom::blink::VRPosePtr m_framePose;

  Member<VRPassThroughCamera> m_passThroughCamera;
  
  VRLayer m_layer;
  double m_depthNear;
  double m_depthFar;

  void dispose();

  Timer<VRDisplay> m_fullscreenCheckTimer;
  String m_fullscreenOrigWidth;
  String m_fullscreenOrigHeight;
  gpu::gles2::GLES2Interface* m_contextGL;
  Member<WebGLRenderingContextBase> m_renderingContext;

  Member<ScriptedAnimationController> m_scriptedAnimationController;
  bool m_animationCallbackRequested;
  bool m_inAnimationFrame;
  bool m_displayBlurred;
  bool m_reenteredFullscreen;

  device::mojom::blink::VRDisplayPtr m_display;

  mojo::Binding<device::mojom::blink::VRDisplayClient> m_binding;

  HeapDeque<Member<ScriptPromiseResolver>> m_pendingPresentResolvers;

  HeapVector<Member<VRPlane>> m_planes;

  HeapVector<Member<VRAnchor>> m_anchors;
};

using VRDisplayVector = HeapVector<Member<VRDisplay>>;

enum class PresentationResult {
  Requested = 0,
  Success = 1,
  SuccessAlreadyPresenting = 2,
  VRDisplayCannotPresent = 3,
  PresentationNotSupportedByDisplay = 4,
  VRDisplayNotFound = 5,
  NotInitiatedByUserGesture = 6,
  InvalidNumberOfLayers = 7,
  InvalidLayerSource = 8,
  LayerSourceMissingWebGLContext = 9,
  InvalidLayerBounds = 10,
  ServiceInactive = 11,
  RequestDenied = 12,
  PresentationResultMax,  // Must be last member of enum.
};

void ReportPresentationResult(PresentationResult);

}  // namespace blink

#endif  // VRDisplay_h
