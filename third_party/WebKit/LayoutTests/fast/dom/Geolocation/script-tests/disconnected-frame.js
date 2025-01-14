description("Tests that when a request is made on a Geolocation object and its Frame is disconnected before a callback is made, no callbacks are made.");

var error;
var iframe = document.createElement('iframe');

function onIframeLoaded() {
    iframeGeolocation = iframe.contentWindow.navigator.geolocation;
    iframe.src = 'data:text/html,This frame should be visible when the test completes';
}

function onIframeUnloaded() {
    iframeGeolocation.getCurrentPosition(function () {
        testFailed('Success callback invoked unexpectedly');
        finishJSTest();
    }, function(e) {
        testFailed('Error callback invoked unexpectedly');
        finishJSTest();
    });
    setTimeout(function() {
        testPassed('No callbacks invoked');
        finishJSTest();
    }, 100);
}

geolocationServiceMock.then(mock => {
    mock.setGeolocationPermission(true);
    mock.setGeolocationPosition(51.478, -0.166, 100);

    iframe.src = 'resources/disconnected-frame-inner.html';
    document.body.appendChild(iframe);
});

window.jsTestIsAsync = true;
