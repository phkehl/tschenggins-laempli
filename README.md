# Tschenggins Lämpli (ESP8266 IoT Jenkins CI Lights/Beacon/Build Status Indicator)

by Philippe Kehl <flipflip at oinkzwurgl dot org>,
https://oinkzwurgl.org/projaeggd/tschenggins-laempli

## Introduction

![Tschenggins Lämpli Model 1](fs/laempli.png)
![Tschenggins Lämpli Model 3](doc/laempli3.jpg)

The colours indicate the result (success, warning, failure, unknown) and the the
LEDs pulsate while jobs are running. Chewie roars if something goes wrong (red,
failure) and he whistles the Indiana Jones theme when things go back to green
(success) again.

## Building

This uses the [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos). An
old version using [esp-open-sdk](https://github.com/pfalcon/esp-open-sdk) can be
found in the `old/` sub-directory.

Copy the `cfg-sample.txt` to `cfg-somename.txt` and adjust the parameters.

Say `make CFGFILE=cfg-somename.txt` to build the firmware.

Or you can put `CFGFILE = cfg-somename.txt` into a `local.mk` file and just say `make`.

Say `make help` for esp-open-rtos help on building and flashing the firmware.

Say `make debug` to connect to the ESP8266 module serial port and pretty-print
the debug output.

## Setup

- Install the `tools/tschenggins-status.pl` as a CGI script on some web server. This will need
  `Linux::Inotify2` (`sudo apt install liblinux-inotify2-perl`).
- Run the `tools/tschenggins-watcher.pl` script on the Jenkins server to monitor the Jenkins jobs
  and point it to the location of the CGI script.

