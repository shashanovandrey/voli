# voli
Simple sound volume (ALSA) indicator for system tray. GNU/Linux

Set your: `SND_CTL_NAME, MIXER_PART_NAME`

Dependences (Debian): libasound2, libasound2-dev

Build:

    gcc -O2 -s -lasound -lm `pkg-config --cflags --libs gtk+-3.0` -o voli voli.c
