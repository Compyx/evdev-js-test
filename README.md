# evdev-js-test

Playground for testing [libevdev](https://www.freedesktop.org/wiki/Software/libevdev/)
for joystick input in VICE, replacing the old Linux joystick API currently used
VICE's Gtk3 port.

## Building

### Requirements

This code depends on Gtk+ 3.x and libevdev. Installation of the dependencies on
Debian should work using:
```
sudo apt install libevdev-dev libgtk-3-dev
```

GNU make, pgk-config and GCC are also required (install the `build-essential`
package if not already installed.

### Compiling

Just run `make`.

## Running the test

Run `./evdev-js-test`.
