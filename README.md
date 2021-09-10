# xfce4-panel-alsa-plugin
This is simple xfce4-panel mixer plugin for alsa written on C using GTK3 library. Based on ALSA Tray project source code https://projects.flogisoft.com/alsa-tray/
## required
written and tested using:
- ArchLinux 2021-09-10
## compilation
gcc -shared -Wall -fPIC -o libalsa-plugin.so alsa-plugin.c \`pkg-config --cflags --libs libxfce4panel-2.0\` \`pkg-config --cflags --libs alsa\`
## build dependencies
- base-devel
- xfce4-panel
- alsa-lib
- volumeicon
## installation
- sudo cp libalsa-plugin.so /usr/lib/xfce4/panel/plugins/libalsa-plugin.so
- sudo cp alsa-plugin.desktop /usr/share/xfce4/panel/plugins/alsa-plugin.desktop
- sudo chmod -x /usr/lib/xfce4/panel/plugins/libalsa-plugin.so
## debug info
- xfce4-panel -q && PANEL_DEBUG=1 xfce4-panel
## known bugs
- tool tips not used
- mouse wheel not working on panel button
- panel must be placed on bottom only
- program read volume level from FRONT_LEFT channel only
- program set volume level in 2 channels FRONT_LEFT and FRONT_RIGHT only
## tuning
you can play with global variables CARD, CONTROL_NAME for using your alsa options and SLIDER_HEIGHT, SLIDER_WIDTH for slider window size
