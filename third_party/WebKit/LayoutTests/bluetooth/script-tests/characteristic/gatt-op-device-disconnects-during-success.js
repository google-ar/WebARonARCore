'use strict';
promise_test(() => {
  let val = new Uint8Array([1]);
  return setBluetoothFakeAdapter('DisconnectingHealthThermometerAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['health_thermometer']}],
      optionalServices: [request_disconnection_service_uuid]
    }))
    .then(device => device.gatt.connect())
    .then(gattServer => {
      let measurement_interval;
      return gattServer
        .getPrimaryService('health_thermometer')
        .then(ht=> ht.getCharacteristic('measurement_interval'))
        .then(mi => measurement_interval = mi)
        .then(() => get_request_disconnection(gattServer))
        .then(requestDisconnection => {
          requestDisconnection();
          return assert_promise_rejects_with_message(
            measurement_interval.CALLS([
              getDescriptor(user_description.name)|
              getDescriptors(user_description.name)[UUID]|
              getDescriptors()|
              readValue()|
              writeValue(val)|
              startNotifications()|
              stopNotifications()]),
            new DOMException(
              'GATT Server disconnected while performing a GATT operation.',
              'NetworkError'));
        });
    });
}, 'Device disconnects during a FUNCTION_NAME call that succeeds. ' +
   'Reject with NetworkError.');
