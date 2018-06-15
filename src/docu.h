/*!
    \file
    \brief flipflip's Tschenggins Lämpli: documentation (see \ref mainpage)

    - Copyright (c) 2017-2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/
#ifndef __DOCU_H__
#define __DOCU_H__

/*!

\mainpage
\anchor mainpage

\tableofcontents

\section P_INTRO Introduction

See https://oinkzwurgl.org/projeaggd/tschenggins-laempli for the project documentation.

See [source code](files.html) for copyrights, credits, sources and more.


\section P_SETUP Setup

- Install the \c tools/tschenggins-status.pl as a CGI script on some web server. This will need \c
  Linux::Inotify2 (`sudo apt install liblinux-inotify2-perl`).
- Run the \c tools/tschenggins-watcher.pl script on the Jenkins server to monitor the Jenkins jobs
  and point it to the location of the CGI script.


\section P_WIRING Wiring

\verbatim
           +-----------------------+
           |                       |
           |      ..antenna..      |
           |                       |
           O A0        **GPIO16 D0 o--(WAKE)--
           |                       |
           o G            GPIO5 D1 O--(out)--> sound effect (Chewie) (1)
           |                       |
           o VU           GPIO4 D2 O--(out)--> speaker (2)
           |                       |
           o S3          *GPIO0 D3 O<--(in)--- built-in flash button (3)
           |                       |
           o S2          *GPIO2 D4 O--(out)--> built-in blue LED (4)
           |                       |
           o S1      LoLin      3V o
           |        NodeMCU        |
           o SC      ESP12       G o
           |                       |
           o SO          GPIO14 D5 O--(out)--(HSCLK)--> WS2801 CI (clock in)  \
           |                       |                                          |
           o SK          GPIO12 D6 O<--(in)--(HMISO)--- not connected         |
           |                       |                                           > HSPI (5)
           o G           GPIO13 D7 O--(out)--(HMOSI)--> WS2801 DI (data in)   |
           |                       |                                          |
           o 3V         *GPIO15 D8 O--(out)--(HCS)----> not connected         /
           |                       |
           o EN           GPIO3 RX O<--(in)--(RXD0)---- not connected  \
           |                       |                                    > UART0
           O RST          GPIO1 TX O--(out)--(TXD0)---> debug tx (6)   /
           |                       |
           o G                   G o
           |                       |
           o VIN                3V o
           |          USB          |
           +---------/===\---------+

O = pins also available on Wemos D1 mini and clones

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

\page P_DOXYGEN Doxygen

\section P_DOXYGEN_WARNINGS Warnings

\verbatim
%DOXYGEN_WARNINGS%
\endverbatim

\section P_DOXYGEN_LOG Log

\verbatim
%DOXYGEN_LOG%
\endverbatim

*/

#endif // __DOCU_H__
// eof
