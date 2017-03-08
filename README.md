# Example Deployments - Soap Dispenser firmware

Soap dispenser firmware for xDot.

## How to provision devices

Before you can use the firmware you need authentication keys for the network. For this you need your device EUI. To find the EUI:

1. Flash this application on an xDot.
1. Inspect the serial logs, and look for a line that starts with `[INFO] device ID/EUI`.

Next, provision the device in LORIOT - our LoRa network service provider:

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

See `RadioEvent.h` for a hook on which to receive data.

## Troubleshooting

* Verify that messages are received by the gateway via the *Tap into data stream* link in LORIOT.
* All communication between device and the network (downlink and uplink) can be found in the [application log](https://eu1.loriot.io/home/application.html?app=BE7A0393#application/log).
