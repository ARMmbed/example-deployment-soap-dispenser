# Example Deployments - Soap Dispenser firmware

Soap dispenser firmware for xDot.

## How to provision devices

Before you can use the firmware you need authentication keys for the network. For this you need your device EUI. To find the EUI:

1. Flash this application on an xDot.
1. Inspect the serial logs, and look for a line that starts with '[INFO] device ID/EUI' and '[INFO] network KEY'.

Next, provision the device in the LoRa bridge:

1. Go to the [LORIOT bridge](http://apm-lora-eu2.cloudapp.net:5101).
1. Click *Create new device*.
1. Fill in the device ID/EUI and an mbed Device Connector certificate.
1. Navigate to LORIOT.io->Applications/HPE/Devices/ImportOTAA
1. Copy 'device ID/EUI' and 'network KEY' into DevEUI and APPKEY respectively (Must be UPPERCASE).

Build and flash your application, the device now joins the network.

Next, set up LWM2M rules for the device.EUI

**Resources**

```json
{
    "soap/0/presses_left": {
        "mode": "r",
        "defaultValue": "0"
    },
    "lora/0/interval": {
        "mode": "w"
    },
    "lora/0/timestamp": {
        "mode": "r",
        "defaultValue": "0"
    }
}
```

**Process Data**

```js
function (bytes) {
    return {
        "soap/0/presses_left": (bytes[0] << 8) + bytes[1],
        "lora/0/timestamp": Date.now()
    };
}
```

**Write Data**

```js
{
    "lora/0/interval": function (v) {
        var b = Number(v);
        return { port: 5, data: [ (b >> 8) & 0xff, b & 0xff ] };
    }
}
```

Save the configuration. The device now shows up in mbed Device Connector.

### Alternative: provision directly in LORIOT

This does not expose the device to mbed Device Connector, but is useful for troubleshooting.

1. Go to the [ARM LORIOT account](http://eu1.loriot.io/home/) - for credentials see one of the contributors.
1. Under *Applications*, select *HPE*.
1. Click *Enroll device*.
1. Enter the Device EUI.
1. On the device page find the `APPKEY`, and enter this in `main.cpp` as the value for `network_key`.

Build and flash the firmware, the device should now join the network.

## How to build

1. Import this program using mbed CLI:

    ```
    $ mbed import git@github.com:ARMmbed/example-deployment-soap-dispenser.git example-deployment-soap-dispenser
    ```

1. Configure your target and toolchain, and build the project:

    ```
    $ mbed target xdot_l151cc
    $ mbed toolchain GCC_ARM
    $ mbed compile
    ```

## Receiving data

See `RadioEvent.h` for a hook on which to receive data. This hook is already used to process the interval frequency frames (on port 5).

## Troubleshooting

* Verify that messages are received by the gateway via the *Tap into data stream* link in LORIOT.
* All communication between device and the network (downlink and uplink) can be found in the [application log](https://eu1.loriot.io/home/application.html?app=BE7A0393#application/log).
