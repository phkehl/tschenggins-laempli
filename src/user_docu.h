/*!
    \file
    \brief flipflip's Tschenggins Lämpli: documentation (see \ref mainpage)

    - Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>,
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/
#ifndef __USER_DOCU_H_
#define __USER_DOCU_H__

/*!

\mainpage
\anchor mainpage

\tableofcontents

\section P_INTRO Introduction

See https://oinkzwurgl.org/projeaggd/tschenggins-laempli for the project documentation.

See [source code](files.html) for copyrights, credits, sources and more.

\section P_API API Reference, Source Code

Original stuff:

- \ref USER_APP    (user_app.c)    the main application
- \ref USER_CFG    (user_cfg.c)    application configuration incl. web interface
- \ref USER_FS     (user_fs.c)     read-only filesystem incl. serving files on \ref USER_HTTPD
- \ref USER_HTML   (user_html.c)   a simple HTML templating system
- \ref USER_HTTPD  (user_httpd.c)  a HTTP webserver
- \ref USER_STATUS (user_status.c) a status LED with different blink patterns
- \ref USER_STUFF  (user_stuff.c)  various utility functions, buffered non-blocking (using interrupts) debug output on serial console, and more
- \ref USER_TONE   (user_tone.c)   tone and melody generation
- \ref USER_WGET   (user_wget.c)   HTTP requests with authentication and query parameters
- \ref USER_WIFI   (user_wifi.c)   WiFi station and access point functions incl. status web interface
- \ref USER_WS2801 (user_ws2801.c) buffered non-blocking (using interrupts) WS2801 RGB LED driver

3rd-party code modified for ESP8266 (see source files for copyright and credits):

- base64.c   [Base64](https://en.wikipedia.org/wiki/Base64) encoding and decoding
- captdns.c  "captive portal" DNS server (used by \ref USER_HTTPD)
- jsmn.c     JSON parser/tokenizer
- pgmspace.c functions for strings in ROM ("PROGMEM")
- rtttl.c    [RTTTL](https://en.wikipedia.org/wiki/Ring_Tone_Transfer_Language) melodies generation

Other 3rd-party stuff:

- jQuery from https://jquery.com/
- SDK API and programming documents Espressif (http://espressif.com/)

\section P_BUILD Building

- Use Linux.
- Install the ESP8266 SDK from https://github.com/pfalcon/esp-open-sdk.
- Install Doxygen and the usual Linux tools (Perl, sed, awk, etc.).
- Say `make help` for further instructions.


\section P_SETUP Setup

- Install the \c tools/tschenggins-status.pl as a CGI script on some web server. This will need \c
  Linux::Inotify2 (`sudo apt install liblinux-inotify2-perl`).
- Run the \c tools/tschenggins-watcher.pl script on the Jenkins server to monitor the Jenkins jobs
  and point it to the location of the CGI script.
- Connect to the Jenkins Lämpli and configure the CGI address.


\section P_WIRING Wiring

\verbatim
           +-----------------------+
           |                       |
           |      ..antenna..      |
           |                       |
           o A0        **GPIO16 D0 o--(WAKE)--
           |                       |
           o G            GPIO5 D1 o--(out)--> sound effect (Chewie) (1)
           |                       |
           o VU           GPIO4 D2 o--(out)--> speaker (2)
           |                       |
           o S3          *GPIO0 D3 o<--(in)--- built-in flash button (3)
           |                       |
           o S2          *GPIO2 D4 o--(out)--> built-in blue LED (4)
           |                       |
           o S1      LoLin      3V o
           |        NodeMCU        |
           o SC      ESP12       G o
           |                       |
           o SO          GPIO14 D5 o--(out)--(HSCLK)--> WS2801 CI (clock in)  \
           |                       |                                          |
           o SK          GPIO12 D6 o<--(in)--(HMISO)--- not connected         |
           |                       |                                           > HSPI (5)
           o G           GPIO13 D7 o--(out)--(HMOSI)--> WS2801 DI (data in)   |
           |                       |                                          |
           o 3V         *GPIO15 D8 o--(out)--(HCS)----> not connected         /
           |                       |
           o EN           GPIO3 RX o<--(in)--(RXD0)---- not connected  \
           |                       |                                    > UART0
           o RST          GPIO1 TX o--(out)--(TXD0)---> debug tx (6)   /
           |                       |
           o G                   G o
           |                       |
           o VIN                3V o
           |          USB          |
           +---------/===\---------+

* boot mode related:
  GPIO15/D8 must be low
  GPIO2/D4 must be high
  GPIO0/D3 selects boot mode (high=run, low=flash)

** sleep mode related,
   GPIO16/D0 must be connected to RST
   so that it can reset the system at wakeup time

(1) connected to Chewbacca sound module,
    pulled low for a moment to trigger the roaring (user_loop.c)

(2) connected to (piezo or other small) speaker,
    used for outputting tones and melodies (user_tone.c)

(3) connected to the flash button on the NodeMCU board,
    which pulls this low on press,
    used to force a Jenkins status update (user_loop.c)

(4) connected to the LED on the ESP12 module (it seems),
    used for status indication (user_status.c),
    inverted, the LED is lit when GPIO2/D4 is low

(5) only CLK and MOSI are connected to the WS2801 chain,
    but MOSI and CS are also configured (user_ws2801.c)
    (it's unclear if that is required or the pins could be used for something else)

(6) connected to the CH304G USB to UART chip,
    for debug output (user_stuff.c)
\endverbatim


\section P_FLASH Flash Memory Layout

The "NodeMCU" has a 32 Mbit (=4 mega bytes, 0x400000) flash. system_get_flash_size_map() says it's
FLASH_SIZE_32M_MAP_512_512. I think I'm using the "Non-FOTA" layout.

1k = 0x400, 4k = 0x1000, 1M = 0x100000

\verbatim
---------------------------------------------------------------------------
sect address  size             what
===========================================================================
   0 0x000000 0x010000  (64kb) eagle.flash.bin, firmware part 1
                               obj/tschenggins-laempli_0x00000.bin
---------------------------------------------------------------------------
  16 0x010000 0x05c000 (368kb) eagle.irom0text.bin, firmare part 2
                               obj/tschenggins-laempli_0x10000.bin
---------------------------------------------------------------------------
 108 0x06c000 0x094000 (592kb) more space for irom0 or user data
---------------------------------------------------------------------------
 256 0x100000           (~3MB) user data
     ...
---------------------------------------------------------------------------
1020 0x3fc000 ?        (128b?) esp_init_data_default.bin (at end - 4 sect)
1021
---------------------------------------------------------------------------
1022 0x3fe000 0x001000   (4kb) blank.bin (at end - 2 sect)
1023 0x3ff000 0x001000   (4kb) unused?
===========================================================================
1024 0x400000                  end of flash
---------------------------------------------------------------------------
\endverbatim


\page P_TODO Todo, Ideas

- stop HTTP protocol abuse when connecting to \c tschenggins-status.pl and create a separate, simple TCP server instead
- ping.h, sntp.h
- https://richard.burtons.org/2015/05/18/rboot-a-new-boot-loader-for-esp8266/, https://github.com/raburton
- https://github.com/SuperHouse/esp-open-rtos/blob/master/core/esp_iomux.c
- https://github.com/Spritetm/libesphttpd, https://github.com/Spritetm/esphttpd
- https://github.com/espressif/esp-gdbstub
- wifi scan in config form


\page P_DOXYGEN Doxygen

\section P_DOXYGEN_WARNINGS Warnings

\verbatim
%DOXYGEN_WARNINGS%
\endverbatim

\section P_DOXYGEN_LOG Log

\verbatim
%DOXYGEN_LOG%
\endverbatim


\defgroup USER USER

API documentation

*/

#endif // __USER_DOCU_H__
// eof
