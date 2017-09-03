// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRDisplay.h"

#include "core/css/StylePropertySet.h"
#include "core/dom/DOMException.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/FrameRequestCallback.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/ScriptedAnimationController.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/DocumentLoader.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/EventTargetModules.h"
#include "modules/vr/NavigatorVR.h"
#include "modules/vr/VRController.h"
#include "modules/vr/VRDisplayCapabilities.h"
#include "modules/vr/VREyeParameters.h"
#include "modules/vr/VRFrameData.h"
#include "modules/vr/VRLayer.h"
#include "modules/vr/VRPose.h"
#include "modules/vr/VRStageParameters.h"
#include "modules/vr/VRHit.h"
#include "modules/vr/VRPassThroughCamera.h"
#include "modules/vr/VRPlane.h"
#include "modules/vr/VRPlaneEvent.h"
#include "modules/vr/VRAnchor.h"
#include "modules/vr/VRAnchorEvent.h"
#include "modules/vr/VRMarker.h"
#include "modules/webgl/WebGLRenderingContextBase.h"
#include "platform/Histogram.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/Platform.h"
#include "wtf/AutoReset.h"

#include <array>

namespace blink {

namespace {

// Magic numbers used to mark valid pose index values encoded in frame
// data. Must match the magic numbers used in vr_shell.cc.
static constexpr std::array<uint8_t, 2> kWebVrPosePixelMagicNumbers{{42, 142}};

VREye stringToVREye(const String& whichEye) {
  if (whichEye == "left")
    return VREyeLeft;
  if (whichEye == "right")
    return VREyeRight;
  return VREyeNone;
}

class VRDisplayFrameRequestCallback : public FrameRequestCallback {
 public:
  VRDisplayFrameRequestCallback(VRDisplay* vrDisplay) : m_vrDisplay(vrDisplay) {
    m_useLegacyTimeBase = true;
  }
  ~VRDisplayFrameRequestCallback() override {}
  void handleEvent(double highResTimeMs) override {
    Document* doc = m_vrDisplay->document();
    if (!doc)
      return;

    // Need to divide by 1000 here because serviceScriptedAnimations expects
    // time to be given in seconds.
    m_vrDisplay->serviceScriptedAnimations(
        doc->loader()->timing().pseudoWallTimeToMonotonicTime(highResTimeMs /
                                                              1000.0));
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_vrDisplay);

    FrameRequestCallback::trace(visitor);
  }

  Member<VRDisplay> m_vrDisplay;
};

}  // namespace

VRDisplay::VRDisplay(NavigatorVR* navigatorVR,
                     device::mojom::blink::VRDisplayPtr display,
                     device::mojom::blink::VRDisplayClientRequest request)
    : ContextLifecycleObserver(navigatorVR->document()),
      m_navigatorVR(navigatorVR),
      m_isConnected(false),
      m_isPresenting(false),
      m_isValidDeviceForPresenting(true),
      m_canUpdateFramePose(true),
      m_capabilities(new VRDisplayCapabilities()),
      m_eyeParametersLeft(new VREyeParameters()),
      m_eyeParametersRight(new VREyeParameters()),
      m_depthNear(0.01),
      m_depthFar(10000.0),
      m_fullscreenCheckTimer(this, &VRDisplay::onFullscreenCheck),
      m_contextGL(nullptr),
      m_animationCallbackRequested(false),
      m_inAnimationFrame(false),
      m_display(std::move(display)),
      m_binding(this, std::move(request)) {}

VRDisplay::~VRDisplay() {}

VRController* VRDisplay::controller() {
  return m_navigatorVR->controller();
}

void VRDisplay::update(const device::mojom::blink::VRDisplayInfoPtr& display) {
  m_displayId = display->index;
  m_displayName = display->displayName;
  m_isConnected = true;

  m_capabilities->setHasOrientation(display->capabilities->hasOrientation);
  m_capabilities->setHasPosition(display->capabilities->hasPosition);
  m_capabilities->setHasExternalDisplay(
      display->capabilities->hasExternalDisplay);
  m_capabilities->setCanPresent(display->capabilities->canPresent);
  m_capabilities->setMaxLayers(display->capabilities->canPresent ? 1 : 0);
  m_capabilities->setHasPassThroughCamera(display->capabilities->hasPassThroughCamera);

  // Ignore non presenting delegate
  bool isValid = display->leftEye->renderWidth > 0;
  bool needOnPresentChange = false;
  if (m_isPresenting && isValid && !m_isValidDeviceForPresenting) {
    needOnPresentChange = true;
  }
  m_isValidDeviceForPresenting = isValid;
  m_eyeParametersLeft->update(display->leftEye);
  m_eyeParametersRight->update(display->rightEye);

  if (!display->stageParameters.is_null()) {
    if (!m_stageParameters)
      m_stageParameters = new VRStageParameters();
    m_stageParameters->update(display->stageParameters);
  } else {
    m_stageParameters = nullptr;
  }

  if (display->capabilities->hasPassThroughCamera) {
    if (!m_passThroughCamera) {
      m_passThroughCamera = new VRPassThroughCamera();
    }
  }

  if (needOnPresentChange) {
    OnPresentChange();
  }
}

void VRDisplay::disconnected() {
  if (m_isConnected)
    m_isConnected = !m_isConnected;
}

bool VRDisplay::getFrameData(VRFrameData* frameData) {
  updatePose();

  if (!m_framePose)
    return false;

  if (!frameData)
    return false;

  if (m_depthNear == m_depthFar)
    return false;

  return frameData->update(m_framePose, m_eyeParametersLeft,
                           m_eyeParametersRight, m_depthNear, m_depthFar);
}

VRPose* VRDisplay::getPose() {
  updatePose();

  if (!m_framePose)
    return nullptr;

  VRPose* pose = VRPose::create();
  pose->setPose(m_framePose);
  return pose;
}

void VRDisplay::updatePose() {
  if (m_displayBlurred) {
    // WebVR spec says to return a null pose when the display is blurred.
    m_framePose = nullptr;
    return;
  }
  if (m_canUpdateFramePose) {
    if (!m_display) {
      return;
    }
    device::mojom::blink::VRPosePtr pose;
    m_display->GetPose(&pose);
    m_framePose = std::move(pose);

    // We are making this call here because ARCore doesn't have events
    // for planes and so we need to update planes each frame. We are assuming
    // that update pose is called each frame on all AR apps.
    updatePlanes();

    if (m_isPresenting) {
      m_canUpdateFramePose = false;
    }
  }
}

void VRDisplay::resetPose() {
  if (!m_display)
    return;

  m_display->ResetPose();
}

HeapVector<Member<VRHit>> VRDisplay::hitTest(float x, float y) {
  HeapVector<Member<VRHit>> hits;

  if (!m_display) {
    return hits;
  }

  Vector<device::mojom::blink::VRHitPtr> hitPtrs;
  m_display->HitTest(x, y, &hitPtrs);
  if (hitPtrs.size() > 0)
  {
    hits.resize(hitPtrs.size());
    for (size_t i = 0; i < hitPtrs.size(); i++)
    {
      VRHit* hit = new VRHit();
      hit->setHit(hitPtrs[i]);
      hits[i] = hit;
    }
  }
  return hits;
}

HeapVector<Member<VRPlane>> VRDisplay::getPlanes() {
  return m_planes;
}

void VRDisplay::dispatchPlaneEvent(const AtomicString& eventName, HeapVector<Member<VRPlane>>& vrPlanes) {
  if (vrPlanes.size() > 0 && hasEventListeners(eventName)) {
    dispatchEvent(VRPlaneEvent::create(
        eventName, true, false, this, vrPlanes));
  }
}

void VRDisplay::dispatchPlaneEvent(const AtomicString& eventName, WTF::Vector<device::mojom::blink::VRPlanePtr>& mojomPlanes) {
  if (mojomPlanes.size() > 0 && hasEventListeners(eventName)) {
    HeapVector<Member<VRPlane>> vrPlanes;
    for (auto& mojomPlane : mojomPlanes)
    {
      VRPlane* vrPlane = new VRPlane();
      vrPlane->setPlane(mojomPlane);
      vrPlanes.push_back(vrPlane);
    }
    dispatchPlaneEvent(eventName, vrPlanes);
  }
}

void VRDisplay::updatePlanes() {
  device::mojom::blink::VRPlaneDeltasPtr planeDeltas;
  m_display->GetPlaneDeltas(&planeDeltas);
  if (!planeDeltas) {
    return;
  }

  if (planeDeltas->removed.size() > 0) {
    HeapVector<Member<VRPlane>> vrPlanesRemoved;
    vrPlanesRemoved.reserveCapacity(planeDeltas->removed.size());
    for (auto removed : planeDeltas->removed) {
      for (size_t i = 0; i < m_planes.size();) {
        if (m_planes[i]->identifier() == removed) {
          vrPlanesRemoved.push_back(m_planes[i]);
          m_planes.remove(i);
        } else {
          i++;
        }
      }
    }
    dispatchPlaneEvent(EventTypeNames::planesremoved, vrPlanesRemoved);
  }
  
  for (auto& updated : planeDeltas->updated) {
    for (auto plane : m_planes) {
      if (plane->identifier() == updated->identifier) {
        plane->setPlane(updated);
      }
    }
  }
  dispatchPlaneEvent(EventTypeNames::planesupdated, planeDeltas->updated);

  m_planes.reserveCapacity(planeDeltas->added.size() + m_planes.size());
  for (auto& added : planeDeltas->added) {
    VRPlane* plane = new VRPlane();
    plane->setPlane(added);
    m_planes.push_back(plane);
  }
  dispatchPlaneEvent(EventTypeNames::planesadded, planeDeltas->added);
}

VRPassThroughCamera* VRDisplay::getPassThroughCamera() {
  if (!m_display || !m_passThroughCamera)
    return nullptr;

  device::mojom::blink::VRPassThroughCameraPtr passThroughCamera;
  m_display->GetPassThroughCamera(&passThroughCamera);
  if (passThroughCamera.is_null()) {
    return nullptr;
  }
  else {
    m_passThroughCamera->setPassThroughCamera(passThroughCamera);
  }
  return m_passThroughCamera;
}

VRAnchor* VRDisplay::createAnchor(WTF::Vector<float>& modelMatrix) {
  if (!m_display)
    return nullptr;

  device::mojom::blink::VRAnchorPtr mojomAnchor;
  m_display->CreateAnchor(modelMatrix, &mojomAnchor);
  if (mojomAnchor.is_null()) {
    return nullptr;
  }
  VRAnchor* anchor = new VRAnchor();
  anchor->setAnchor(mojomAnchor);
  m_anchors.push_back(anchor);

  VLOG(0) << "JUDAX: VRDisplay::createAnchor -> Anchor created with identifier = " <<
             anchor->identifier();

  return anchor;
}

VRAnchor* VRDisplay::createAnchor(DOMFloat32Array* modelMatrix) {
  if (!m_display)
    return nullptr;

  // Copy the DOMFloat32Array to a Vector<float>
  WTF::Vector<float> modelMatrixVector;
  size_t modelMatrixSize = modelMatrix->length();
  modelMatrixVector.resize(modelMatrixSize);
  for (size_t i = 0; i < modelMatrixSize; i++) {
    modelMatrixVector[i] = modelMatrix->data()[i];
  }

  // Use the other overloaded method to create an anchor using the Vector.
  return createAnchor(modelMatrixVector);
}

void VRDisplay::removeAnchor(VRAnchor* anchor) {
  if (!m_display)
    return;

  bool removed = false;
  for (size_t i = 0; !removed && i < m_anchors.size(); i++) {
    // NOTE: Should we compare the pointers directly?
    if (m_anchors[i]->identifier() == anchor->identifier()) {
      m_display->RemoveAnchor(anchor->identifier());
      m_anchors.remove(i);
      removed = true;
    }
  }
}

HeapVector<Member<VRAnchor>> VRDisplay::getAnchors() {
  return m_anchors;
}

HeapVector<Member<VRMarker>> VRDisplay::getMarkers(unsigned markerType, float markerSize)
{
  HeapVector<Member<VRMarker>> markers;
  if (!m_display)
    return markers;
  Vector<device::mojom::blink::VRMarkerPtr> mojomMarkers;
  if (m_display->GetMarkers(markerType, markerSize, &mojomMarkers) && !mojomMarkers.isEmpty())
  {
    markers.resize(mojomMarkers.size());
    for (size_t i = 0; i < mojomMarkers.size(); i++)
    {
      VRMarker* marker = new VRMarker();
      marker->setMarker(mojomMarkers[i]);
      markers[i] = marker;
    }
  }
  return markers;
}

VREyeParameters* VRDisplay::getEyeParameters(const String& whichEye) {
  switch (stringToVREye(whichEye)) {
    case VREyeLeft:
      return m_eyeParametersLeft;
    case VREyeRight:
      return m_eyeParametersRight;
    default:
      return nullptr;
  }
}

int VRDisplay::requestAnimationFrame(FrameRequestCallback* callback) {
  Document* doc = this->document();
  if (!doc)
    return 0;

  if (!m_animationCallbackRequested) {
    doc->requestAnimationFrame(new VRDisplayFrameRequestCallback(this));
    m_animationCallbackRequested = true;
  }

  callback->m_useLegacyTimeBase = false;
  return ensureScriptedAnimationController(doc).registerCallback(callback);
}

void VRDisplay::cancelAnimationFrame(int id) {
  if (!m_scriptedAnimationController)
    return;
  m_scriptedAnimationController->cancelCallback(id);
}

void VRDisplay::OnBlur() {
  m_displayBlurred = true;

  m_navigatorVR->enqueueVREvent(VRDisplayEvent::create(
      EventTypeNames::vrdisplayblur, true, false, this, ""));
}

void VRDisplay::OnFocus() {
  m_displayBlurred = false;
  // Restart our internal doc requestAnimationFrame callback, if it fired while
  // the display was blurred.
  // TODO(bajones): Don't use doc->requestAnimationFrame() at all. Animation
  // frames should be tied to the presenting VR display (e.g. should be serviced
  // by GVR library callbacks on Android), and not the doc frame rate.
  if (!m_animationCallbackRequested) {
    Document* doc = this->document();
    if (!doc)
      return;
    doc->requestAnimationFrame(new VRDisplayFrameRequestCallback(this));
  }
  m_navigatorVR->enqueueVREvent(VRDisplayEvent::create(
      EventTypeNames::vrdisplayfocus, true, false, this, ""));
}

void VRDisplay::serviceScriptedAnimations(double monotonicAnimationStartTime) {
  if (!m_scriptedAnimationController)
    return;
  AutoReset<bool> animating(&m_inAnimationFrame, true);
  m_animationCallbackRequested = false;

  // We use an internal rAF callback to run the animation loop at the display
  // speed, and run the user's callback after our internal callback fires.
  // However, when the display is blurred, we want to pause the animation loop,
  // so we don't fire the user's callback until the display is focused.
  if (m_displayBlurred)
    return;
  m_scriptedAnimationController->serviceScriptedAnimations(
      monotonicAnimationStartTime);
}

void ReportPresentationResult(PresentationResult result) {
  // Note that this is called twice for each call to requestPresent -
  // one to declare that requestPresent was called, and one for the
  // result.
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, vrPresentationResultHistogram,
      ("VRDisplayPresentResult",
       static_cast<int>(PresentationResult::PresentationResultMax)));
  vrPresentationResultHistogram.count(static_cast<int>(result));
}

ScriptPromise VRDisplay::requestPresent(ScriptState* scriptState,
                                        const HeapVector<VRLayer>& layers) {
  ExecutionContext* executionContext = scriptState->getExecutionContext();
  UseCounter::count(executionContext, UseCounter::VRRequestPresent);
  if (!executionContext->isSecureContext()) {
    UseCounter::count(executionContext,
                      UseCounter::VRRequestPresentInsecureOrigin);
  }

  ReportPresentationResult(PresentationResult::Requested);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  // If the VRDisplay does not advertise the ability to present reject the
  // request.
  if (!m_capabilities->canPresent()) {
    DOMException* exception =
        DOMException::create(InvalidStateError, "VRDisplay cannot present.");
    resolver->reject(exception);
    ReportPresentationResult(PresentationResult::VRDisplayCannotPresent);
    return promise;
  }

  bool firstPresent = !m_isPresenting;

  // Initiating VR presentation is only allowed in response to a user gesture.
  // If the VRDisplay is already presenting, however, repeated calls are
  // allowed outside a user gesture so that the presented content may be
  // updated.
  if (firstPresent && !UserGestureIndicator::utilizeUserGesture()) {
    DOMException* exception = DOMException::create(
        InvalidStateError, "API can only be initiated by a user gesture.");
    resolver->reject(exception);
    ReportPresentationResult(PresentationResult::NotInitiatedByUserGesture);
    return promise;
  }

  // A valid number of layers must be provided in order to present.
  if (layers.size() == 0 || layers.size() > m_capabilities->maxLayers()) {
    forceExitPresent();
    DOMException* exception =
        DOMException::create(InvalidStateError, "Invalid number of layers.");
    resolver->reject(exception);
    ReportPresentationResult(PresentationResult::InvalidNumberOfLayers);
    return promise;
  }

  // If what we were given has an invalid source, need to exit fullscreen with
  // previous, valid source, so delay m_layer reassignment
  if (!layers[0].source()) {
    forceExitPresent();
    DOMException* exception =
        DOMException::create(InvalidStateError, "Invalid layer source.");
    resolver->reject(exception);
    ReportPresentationResult(PresentationResult::InvalidLayerSource);
    return promise;
  }
  m_layer = layers[0];

  CanvasRenderingContext* renderingContext =
      m_layer.source()->renderingContext();

  if (!renderingContext || !renderingContext->is3d()) {
    forceExitPresent();
    DOMException* exception = DOMException::create(
        InvalidStateError, "Layer source must have a WebGLRenderingContext");
    resolver->reject(exception);
    ReportPresentationResult(
        PresentationResult::LayerSourceMissingWebGLContext);
    return promise;
  }

  // Save the WebGL script and underlying GL contexts for use by submitFrame().
  m_renderingContext = toWebGLRenderingContextBase(renderingContext);
  m_contextGL = m_renderingContext->contextGL();

  if ((m_layer.leftBounds().size() != 0 && m_layer.leftBounds().size() != 4) ||
      (m_layer.rightBounds().size() != 0 &&
       m_layer.rightBounds().size() != 4)) {
    forceExitPresent();
    DOMException* exception = DOMException::create(
        InvalidStateError,
        "Layer bounds must either be an empty array or have 4 values");
    resolver->reject(exception);
    ReportPresentationResult(PresentationResult::InvalidLayerBounds);
    return promise;
  }

  if (!m_pendingPresentResolvers.isEmpty()) {
    // If we are waiting on the results of a previous requestPresent call don't
    // fire a new request, just cache the resolver and resolve it when the
    // original request returns.
    m_pendingPresentResolvers.append(resolver);
  } else if (firstPresent) {
    bool secureContext = scriptState->getExecutionContext()->isSecureContext();
    if (!m_display) {
      forceExitPresent();
      DOMException* exception = DOMException::create(
          InvalidStateError, "The service is no longer active.");
      resolver->reject(exception);
      return promise;
    }

    m_pendingPresentResolvers.append(resolver);
    m_display->RequestPresent(secureContext, convertToBaseCallback(WTF::bind(
                                                 &VRDisplay::onPresentComplete,
                                                 wrapPersistent(this))));
  } else {
    updateLayerBounds();
    resolver->resolve();
    ReportPresentationResult(PresentationResult::SuccessAlreadyPresenting);
  }

  return promise;
}

void VRDisplay::onPresentComplete(bool success) {
  if (success) {
    this->beginPresent();
  } else {
    this->forceExitPresent();
    DOMException* exception = DOMException::create(
        NotAllowedError, "Presentation request was denied.");

    while (!m_pendingPresentResolvers.isEmpty()) {
      ScriptPromiseResolver* resolver = m_pendingPresentResolvers.takeFirst();
      resolver->reject(exception);
    }
  }
}

ScriptPromise VRDisplay::exitPresent(ScriptState* scriptState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  if (!m_isPresenting) {
    // Can't stop presenting if we're not presenting.
    DOMException* exception =
        DOMException::create(InvalidStateError, "VRDisplay is not presenting.");
    resolver->reject(exception);
    return promise;
  }

  if (!m_display) {
    DOMException* exception =
        DOMException::create(InvalidStateError, "VRService is not available.");
    resolver->reject(exception);
    return promise;
  }
  m_display->ExitPresent();

  resolver->resolve();

  forceExitPresent();

  return promise;
}

void VRDisplay::beginPresent() {
  Document* doc = this->document();
  std::unique_ptr<UserGestureIndicator> gestureIndicator;
  if (m_capabilities->hasExternalDisplay()) {
    forceExitPresent();
    DOMException* exception = DOMException::create(
        InvalidStateError,
        "VR Presentation not implemented for this VRDisplay.");
    while (!m_pendingPresentResolvers.isEmpty()) {
      ScriptPromiseResolver* resolver = m_pendingPresentResolvers.takeFirst();
      resolver->reject(exception);
    }
    ReportPresentationResult(
        PresentationResult::PresentationNotSupportedByDisplay);
    return;
  } else {
    // TODO(klausw,crbug.com/655722): Need a proper VR compositor, but
    // for the moment on mobile we'll just make the canvas fullscreen
    // so that VrShell can pick it up through the standard (high
    // latency) compositing path.
    auto canvas = m_layer.source();
    auto inlineStyle = canvas->inlineStyle();
    if (inlineStyle) {
      // THREE.js's VREffect sets explicit style.width/height on its rendering
      // canvas based on the non-fullscreen window dimensions, and it keeps
      // those unchanged when presenting. Unfortunately it appears that a
      // fullscreened canvas just gets centered if it has explicitly set a
      // size smaller than the fullscreen dimensions. Manually set size to
      // 100% in this case and restore it when exiting fullscreen. This is a
      // stopgap measure since THREE.js's usage appears legal according to the
      // WebVR API spec. This will no longer be necessary once we can get rid
      // of this fullscreen hack.
      m_fullscreenOrigWidth = inlineStyle->getPropertyValue(CSSPropertyWidth);
      if (!m_fullscreenOrigWidth.isNull()) {
        canvas->setInlineStyleProperty(CSSPropertyWidth, "100%");
      }
      m_fullscreenOrigHeight = inlineStyle->getPropertyValue(CSSPropertyHeight);
      if (!m_fullscreenOrigHeight.isNull()) {
        canvas->setInlineStyleProperty(CSSPropertyHeight, "100%");
      }
    } else {
      m_fullscreenOrigWidth = String();
      m_fullscreenOrigHeight = String();
    }

    if (doc) {
      // Since the callback for requestPresent is asynchronous, we've lost our
      // UserGestureToken, and need to create a new one to enter fullscreen.
      gestureIndicator = WTF::wrapUnique(
          new UserGestureIndicator(DocumentUserGestureToken::create(
              doc, UserGestureToken::Status::PossiblyExistingGesture)));
    }
    Fullscreen::requestFullscreen(*canvas);

    // Check to see if the canvas is still the current fullscreen
    // element once every 2 seconds.
    m_fullscreenCheckTimer.startRepeating(2.0, BLINK_FROM_HERE);
    m_reenteredFullscreen = false;
  }

  if (doc) {
    Platform::current()->recordRapporURL("VR.WebVR.PresentSuccess",
                                         WebURL(doc->url()));
  }

  m_isPresenting = true;
  ReportPresentationResult(PresentationResult::Success);

  updateLayerBounds();

  while (!m_pendingPresentResolvers.isEmpty()) {
    ScriptPromiseResolver* resolver = m_pendingPresentResolvers.takeFirst();
    resolver->resolve();
  }
  OnPresentChange();
}

void VRDisplay::forceExitPresent() {
  if (m_isPresenting) {
    if (!m_capabilities->hasExternalDisplay()) {
      auto canvas = m_layer.source();
      Fullscreen::fullyExitFullscreen(canvas->document());
      m_fullscreenCheckTimer.stop();
      if (!m_fullscreenOrigWidth.isNull()) {
        canvas->setInlineStyleProperty(CSSPropertyWidth, m_fullscreenOrigWidth);
        m_fullscreenOrigWidth = String();
      }
      if (!m_fullscreenOrigHeight.isNull()) {
        canvas->setInlineStyleProperty(CSSPropertyWidth,
                                       m_fullscreenOrigHeight);
        m_fullscreenOrigHeight = String();
      }
    } else {
      // Can't get into this presentation mode, so nothing to do here.
    }
    m_isPresenting = false;
    OnPresentChange();
  }

  m_renderingContext = nullptr;
  m_contextGL = nullptr;
}

void VRDisplay::updateLayerBounds() {
  if (!m_display)
    return;

  // Set up the texture bounds for the provided layer
  device::mojom::blink::VRLayerBoundsPtr leftBounds =
      device::mojom::blink::VRLayerBounds::New();
  device::mojom::blink::VRLayerBoundsPtr rightBounds =
      device::mojom::blink::VRLayerBounds::New();

  if (m_layer.leftBounds().size() == 4) {
    leftBounds->left = m_layer.leftBounds()[0];
    leftBounds->top = m_layer.leftBounds()[1];
    leftBounds->width = m_layer.leftBounds()[2];
    leftBounds->height = m_layer.leftBounds()[3];
  } else {
    // Left eye defaults
    leftBounds->left = 0.0f;
    leftBounds->top = 0.0f;
    leftBounds->width = 0.5f;
    leftBounds->height = 1.0f;
    m_layer.setLeftBounds({0.0f, 0.0f, 0.5f, 1.0f});
  }

  if (m_layer.rightBounds().size() == 4) {
    rightBounds->left = m_layer.rightBounds()[0];
    rightBounds->top = m_layer.rightBounds()[1];
    rightBounds->width = m_layer.rightBounds()[2];
    rightBounds->height = m_layer.rightBounds()[3];
  } else {
    // Right eye defaults
    rightBounds->left = 0.5f;
    rightBounds->top = 0.0f;
    rightBounds->width = 0.5f;
    rightBounds->height = 1.0f;
    m_layer.setRightBounds({0.5f, 0.0f, 0.5f, 1.0f});
  }

  m_display->UpdateLayerBounds(std::move(leftBounds), std::move(rightBounds));
}

HeapVector<VRLayer> VRDisplay::getLayers() {
  HeapVector<VRLayer> layers;

  if (m_isPresenting) {
    layers.push_back(m_layer);
  }

  return layers;
}

void VRDisplay::submitFrame() {
  if (!m_display)
    return;

  Document* doc = this->document();
  if (!m_isPresenting) {
    if (doc) {
      doc->addConsoleMessage(ConsoleMessage::create(
          RenderingMessageSource, WarningMessageLevel,
          "submitFrame has no effect when the VRDisplay is not presenting."));
    }
    return;
  }

  if (!m_inAnimationFrame) {
    if (doc) {
      doc->addConsoleMessage(
          ConsoleMessage::create(RenderingMessageSource, WarningMessageLevel,
                                 "submitFrame must be called within a "
                                 "VRDisplay.requestAnimationFrame callback."));
    }
    return;
  }

  if (!m_contextGL) {
    // Something got confused, we can't submit frames without a GL context.
    return;
  }

  // Write the frame number for the pose used into a bottom left pixel block.
  // It is read by chrome/browser/android/vr_shell/vr_shell.cc to associate
  // the correct corresponding pose for submission.
  auto gl = m_contextGL;

  // We must ensure that the WebGL app's GL state is preserved. We do this by
  // calling low-level GL commands directly so that the rendering context's
  // saved parameters don't get overwritten.

  gl->Enable(GL_SCISSOR_TEST);
  // Use a few pixels to ensure we get a clean color. The resolution for the
  // WebGL buffer may not match the final rendered destination size, and
  // texture filtering could interfere for single pixels. This isn't visible
  // since the final rendering hides the edges via a vignette effect.
  gl->Scissor(0, 0, 4, 4);
  gl->ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  int idx = m_framePose->poseIndex;
  // Careful with the arithmetic here. Float color 1.f is equivalent to int 255.
  // Use the low byte of the index as the red component, and store an arbitrary
  // magic number in green/blue. This number must match the reading code in
  // vr_shell.cc. Avoid all-black/all-white.
  gl->ClearColor((idx & 255) / 255.0f, kWebVrPosePixelMagicNumbers[0] / 255.0f,
                 kWebVrPosePixelMagicNumbers[1] / 255.0f, 1.0f);
  gl->Clear(GL_COLOR_BUFFER_BIT);

  // Set the GL state back to what was set by the WebVR application.
  m_renderingContext->restoreScissorEnabled();
  m_renderingContext->restoreScissorBox();
  m_renderingContext->restoreColorMask();
  m_renderingContext->restoreClearColor();

  m_display->SubmitFrame(m_framePose.Clone());
  m_canUpdateFramePose = true;
}

Document* VRDisplay::document() {
  return m_navigatorVR->document();
}

void VRDisplay::OnPresentChange() {
  if (m_isPresenting && !m_isValidDeviceForPresenting) {
    VLOG(1) << __FUNCTION__ << ": device not valid, not sending event";
    return;
  }
  m_navigatorVR->enqueueVREvent(VRDisplayEvent::create(
      EventTypeNames::vrdisplaypresentchange, true, false, this, ""));
}

void VRDisplay::OnChanged(device::mojom::blink::VRDisplayInfoPtr display) {
  update(display);
}

void VRDisplay::OnAnchorsUpdated(
    WTF::Vector<device::mojom::blink::VRAnchorPtr> mojomAnchors) {

  // TODO: WTF::Vector does not have empty or IsEmpty?
  // if (mojomAnchors.size() == 0) {
  //   return;
  // }

  VLOG(0) << "JUDAX: VRDisplay::OnAnchorsUpdated -> mojomAnchors.size() = " << mojomAnchors.size();

  HeapVector<Member<VRAnchor>> anchors;
  anchors.reserveCapacity(mojomAnchors.size());
  for (size_t i = 0; i < mojomAnchors.size(); i++) {
    int32_t identifier = mojomAnchors[i]->identifier;
    bool found = false;
    for (size_t j = 0; !found && j < m_anchors.size(); j++) {
      if (m_anchors[j]->identifier() == identifier) {
        m_anchors[j]->setAnchor(mojomAnchors[i]);
        anchors.push_back(m_anchors[j]);
        found = true;
      }
    }
    if (!found) {
      VLOG(1) << __FUNCTION__ << ": ERROR! An updated mojom anchor " \
          "identifier has not been found among the cached anchors. This "\
          "shouldn't happen!";
    }
  }

  VLOG(0) << "JUDAX: VRDisplay::OnAnchorsUpdated -> anchors.size() = " << anchors.size();

  // TODO: HeapVector does not have empty or IsEmpty?
  // if (anchors.size() != 0) {
    dispatchEvent(VRAnchorEvent::create(
      EventTypeNames::anchorsupdated, true, false, this, anchors));
  // }
}

void VRDisplay::OnExitPresent() {
  forceExitPresent();
}

void VRDisplay::onConnected() {
  m_navigatorVR->enqueueVREvent(VRDisplayEvent::create(
      EventTypeNames::vrdisplayconnect, true, false, this, "connect"));
}

void VRDisplay::onDisconnected() {
  m_navigatorVR->enqueueVREvent(VRDisplayEvent::create(
      EventTypeNames::vrdisplaydisconnect, true, false, this, "disconnect"));
}

void VRDisplay::OnActivate(device::mojom::blink::VRDisplayEventReason reason) {
  m_navigatorVR->dispatchVRGestureEvent(VRDisplayEvent::create(
      EventTypeNames::vrdisplayactivate, true, false, this, reason));
}

void VRDisplay::OnDeactivate(
    device::mojom::blink::VRDisplayEventReason reason) {
  m_navigatorVR->enqueueVREvent(VRDisplayEvent::create(
      EventTypeNames::vrdisplaydeactivate, true, false, this, reason));
}

void VRDisplay::onFullscreenCheck(TimerBase*) {
  if (!m_isPresenting) {
    m_fullscreenCheckTimer.stop();
    return;
  }
  // TODO: This is a temporary measure to track if fullscreen mode has been
  // exited by the UA. If so we need to end VR presentation. Soon we won't
  // depend on the Fullscreen API to fake VR presentation, so this will
  // become unnessecary. Until that point, though, this seems preferable to
  // adding a bunch of notification plumbing to Fullscreen.
  if (!Fullscreen::isFullscreenElement(*m_layer.source())) {
    // TODO(mthiesse): Due to asynchronous resizing, we might get kicked out of
    // fullscreen when changing display parameters upon entering WebVR. So one
    // time only, we reenter fullscreen after having left it; otherwise we exit
    // presentation.
    if (m_reenteredFullscreen) {
      m_isPresenting = false;
      OnPresentChange();
      m_fullscreenCheckTimer.stop();
      if (m_display)
        m_display->ExitPresent();
      return;
    }
    m_reenteredFullscreen = true;
    auto canvas = m_layer.source();
    Document* doc = this->document();
    std::unique_ptr<UserGestureIndicator> gestureIndicator;
    if (doc) {
      gestureIndicator = WTF::wrapUnique(
          new UserGestureIndicator(DocumentUserGestureToken::create(
              doc, UserGestureToken::Status::PossiblyExistingGesture)));
    }
    Fullscreen::requestFullscreen(*canvas);
  }
}

ScriptedAnimationController& VRDisplay::ensureScriptedAnimationController(
    Document* doc) {
  if (!m_scriptedAnimationController)
    m_scriptedAnimationController = ScriptedAnimationController::create(doc);

  return *m_scriptedAnimationController;
}

void VRDisplay::dispose() {
  m_binding.Close();
}

ExecutionContext* VRDisplay::getExecutionContext() const {
  return ContextLifecycleObserver::getExecutionContext();
}

const AtomicString& VRDisplay::interfaceName() const {
  return EventTargetNames::VRDisplay;
}

void VRDisplay::contextDestroyed(ExecutionContext*) {
  forceExitPresent();
  m_scriptedAnimationController.clear();
}

bool VRDisplay::hasPendingActivity() const {
  // Prevent V8 from garbage collecting the wrapper object if there are
  // event listeners attached to it.
  return getExecutionContext() && hasEventListeners();
}

DEFINE_TRACE(VRDisplay) {
  EventTargetWithInlineData::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
  visitor->trace(m_navigatorVR);
  visitor->trace(m_capabilities);
  visitor->trace(m_stageParameters);
  visitor->trace(m_eyeParametersLeft);
  visitor->trace(m_eyeParametersRight);
  visitor->trace(m_layer);
  visitor->trace(m_renderingContext);
  visitor->trace(m_scriptedAnimationController);
  visitor->trace(m_pendingPresentResolvers);
  visitor->trace(m_passThroughCamera);
  visitor->trace(m_planes);
  visitor->trace(m_anchors);
}

}  // namespace blink
