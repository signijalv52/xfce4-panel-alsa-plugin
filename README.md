# xfce4-panel-alsa-plugin
This is simle alsa plugin for xfce4-panel written on C using GTK3 library. Written and tested in Debian 9 32/64 bit.
## compilation
gcc -shared -Wall -fPIC -o libalsa-plugin.so alsa-plugin.c \`pkg-config --cflags --libs libxfce4panel-2.0\` \`pkg-config --cflags --libs alsa\`
## dependencies
- libxfce4panel-2.0-dev
- libasound2-dev
- volumeicon-alsa
## instalation
- change in file alsa-plugin.desktop i386-linux-gnu to x86_64-linux-gnu if you use Debian 64bit
- sudo cp alsa-plugin.desktop /usr/share/xfce4/panel-plugins/alsa-plugin.desktop
- sudo cp libalsa-plugin.so /usr/lib/i386-linux-gnu/xfce4/panel-plugins/libalsa-plugin.so (or .../x86_64-linux-gnu/... for 64 bit system)
## debug info
- xfce4-panel -q
- PANEL_DEBUG=1 xfce4-panel
