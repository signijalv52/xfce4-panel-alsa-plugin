# xfce4-panel-alsa-plugin
This is simle alsa plugin for xfce4-panel written on C using GTK3 library.
Written and tested in Debian 9 32/64 bit.
## compilation
gcc -shared -Wall -fPIC -o libalsa-plugin.so alsa-plugin.c `pkg-config --cflags --libs libxfce4panel-2.0` `pkg-config --cflags --libs alsa`
## dependencies
libxfce4panel-2.0-dev (libxfce4panel-2.0)
libasound2-dev
volumeicon-alsa
