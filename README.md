# ESPNow Proxy

Basic ESPNow proxy component for ESPHome supporting Unicast and Broadcast.

ESPNow is a protocol that allows for direct communication between ESP32 (and ESP8266) devices without needing a WiFi router or access point. It uses the same radio chip as WiFi to establish a direct peer-to-peer connection.

This custom component for esphome supports only the sending and receiving of commands using esphome / ESP32.

There is no support for sensor data just the basic send/recv functionality using a simple data protocol with command ack.

The code is inspired by Beethowen:
[Beethowen Transmitter](https://github.com/afarago/esphome_component_bthome/blob/master/components/docs/beethowen_transmitter.rst),
[Beethowen Receiver](https://github.com/afarago/esphome_component_bthome/blob/master/components/docs/beethowen_receiver.rst)

## Remarks

When SoftAP is being used, the mac address will be different. This will cause issues, when only using the base mac address.

More info:

- <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/misc_system_api.html>
- <https://forum.arduino.cc/t/question-about-mac-address-and-esp-now/1017002/5>
- <https://lastminuteengineers.com/esp32-mac-address-tutorial/>

## Unicast configuration

The unicast confiuration is being used, to only send to a specific device. This can be done by setting the mac address in `receiver`.

```yaml
espnow_proxy:
  id: espnow_send
  channel: 1
  receiver: AA:BB:CC:DD:EE:FF
  peers:
    - mac_address: AA:BB:CC:DD:EE:FF
      name_prefix: Peer1
```

## Multicast configuration

When the `receiver` is left out, broadcast address (FF:FF:FF:FF:FF:FF) will be used.

```yaml
espnow_proxy:
  id: espnow_send
  channel: 1
  peers:
    - mac_address: AA:BB:CC:DD:EE:FF
      name_prefix: Peer1
    - mac_address: AA:BB:CC:DD:EE:FF
      name_prefix: Peer2
```

## Component and Peer Events

```yaml

espnow_proxy:
  id: espnow_send
  channel: 1

  on_command:  # All commands
    - ...
  on_command_data:
    - ...
  on_send_started:
    - ...
  on_send_finished:
    - ...
  on_send_failed:
    - ...
  peers:
    - mac_address: AA:BB:CC:DD:EE:FF

      on_command:  # Only commands directed to this peer
        - ...
      on_command_data:
        - ...
      on_send_started:
        - ...
      on_send_finished:
        - ...
      on_send_failed:
```
