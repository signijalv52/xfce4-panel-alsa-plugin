/*
создано на основе кода проекта ALSA Tray https://projects.flogisoft.com/alsa-tray/
лицензия GPLv3+

хорошая документация, только устаревшая:
https://wiki.xfce.org/dev/howto/panel_plugins
http://api.xfce.m8t.in/xfce-4.10/libxfce4panel-4.10.0/XfcePanelPlugin.html
=======================================
компиляция
gcc -shared -Wall -fPIC -o libalsa-plugin.so alsa-plugin.c `pkg-config --cflags --libs libxfce4panel-2.0` `pkg-config --cflags --libs alsa`
=======================================
установка в Debian:
sudo cp libalsa-plugin.so /usr/lib/x86_64-linux-gnu/xfce4/panel/plugins/libalsa-plugin.so
sudo cp alsa-plugin.desktop /usr/share/xfce4/panel/plugins/alsa-plugin.desktop
sudo chmod -x /usr/lib/x86_64-linux-gnu/xfce4/panel/plugins/libalsa-plugin.so

зависимости:
build-essential
libxfce4panel-2.0-dev (libxfce4panel-2.0)
libasound2-dev
volumeicon-alsa
=======================================
установка в ArchLinux:
sudo cp libalsa-plugin.so /usr/lib/xfce4/panel/plugins/libalsa-plugin.so
sudo cp alsa-plugin.desktop /usr/share/xfce4/panel/plugins/alsa-plugin.desktop
sudo chmod -x /usr/lib/xfce4/panel/plugins/libalsa-plugin.so

зависимости:
base-devel
xfce4-panel
alsa-lib
volumeicon
=======================================
отладка:
xfce4-panel -q && PANEL_DEBUG=1 xfce4-panel


недостатки:
(*) всплывающие подсказки не используются
(*) кнопка на панели не принимает сигналы колеса мыши (в libxfce4panel-1.0 принимала)
(*) панель должна находиться только внизу
(*) уровень звука и флаг mute снимается только с 1 канала FRONT_LEFT
(*) уровень звука устанавливается сразу одинаково на 2 каналах FRONT_LEFT и FRONT_RIGHT


alsa-plugin.desktop

[Xfce Panel]
Type=X-XFCE-PanelPlugin
Encoding=UTF-8
Name=alsa-plugin
Comment=alsa-plugin
Icon=multimedia-volume-control
X-XFCE-Module=alsa-plugin
X-XFCE-Module-Path=/usr/lib/xfce4/panel/plugins
X-XFCE-Internal=TRUE
X-XFCE-Unique=TRUE
X-XFCE-API=2.0

*/

#include <alsa/asoundlib.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4panel/xfce-panel-macros.h>

#define CARD "default"
#define CONTROL_NAME "Master"
#define SLIDER_HEIGHT 300
#define SLIDER_WIDTH 70

#define MIN_VOLUME 0
#define MAX_VOLUME 100
#define TIMER_INTERVAL 2
//============================================================================================
typedef struct {
  snd_mixer_t*          mixer;
  snd_mixer_selem_id_t* control;
  snd_mixer_elem_t*     channel;
} mixerStruct;
//--------------------------------------------------------------------------------------------
typedef struct {
  guint        timerId;
  mixerStruct* mixer;
  GtkWidget*   image;
  GdkPixbuf*   image0;
  GdkPixbuf*   image1;
  GdkPixbuf*   image2;
  GdkPixbuf*   image3;
  GtkWidget*   mainButton;
  GtkWidget*   window;
  GtkWidget*   slider;
  gulong       connection;
} pluginStruct;
//--------------------------------------------------------------------------------------------
const int volume_scale[101] =
{
  0,
  15,
  24,
  30,
  35,
  39,
  42,
  45,
  48,
  50,
  52,
  54,
  56,
  57,
  59,
  60,
  62,
  63,
  64,
  65,
  66,
  67,
  68,
  69,
  70,
  71,
  72,
  72,
  73,
  74,
  75,
  75,
  76,
  77,
  77,
  78,
  79,
  79,
  80,
  80,
  81,
  81,
  82,
  82,
  83,
  83,
  84,
  84,
  85,
  85,
  86,
  86,
  86,
  87,
  87,
  88,
  88,
  88,
  89,
  89,
  89,
  90,
  90,
  90,
  91,
  91,
  91,
  92,
  92,
  92,
  93,
  93,
  93,
  94,
  94,
  94,
  94,
  95,
  95,
  95,
  96,
  96,
  96,
  96,
  97,
  97,
  97,
  97,
  98,
  98,
  98,
  98,
  99,
  99,
  99,
  99,
  99,
  100,
  100,
  100,
  100
};
//============================================================================================
//к сожалению изменение уровня громкости нелинейное, поэтому городим костыли :)
static int percent_to_volume(const int percent)
{
  if (percent < 0)
    return 0;
  if (percent > 100)
    return 100;
  //return round(8.0 * exp(1.0) * log(percent + 1));
  return *(volume_scale + percent);
}
//============================================================================================
static int volume_to_percent(const int volume)
{
  if (volume < 0)
    return 0;
  if (volume > 100)
    return 100;
  //return round(pow(exp(1.0), volume / (8 * exp(1.0))) - 1);
  int i;
  for (i = 100; i >= 0; --i)
    if (*(volume_scale + i) == volume)
      break;
  return i;
}
//============================================================================================
static void setIcon(pluginStruct* widgets, const long volume, const int mute)
{
  if (mute)
    gtk_image_set_from_pixbuf(GTK_IMAGE(widgets->image), widgets->image0);
  else
    if (volume > 0)
    {
      if (volume > (long)(33 * MAX_VOLUME / 100))
      {
        if (volume > (long)(66 * MAX_VOLUME / 100))
          gtk_image_set_from_pixbuf(GTK_IMAGE(widgets->image), widgets->image3);
        else
          gtk_image_set_from_pixbuf(GTK_IMAGE(widgets->image), widgets->image2);
      }
      else
        gtk_image_set_from_pixbuf(GTK_IMAGE(widgets->image), widgets->image1);
    }
    else
      gtk_image_set_from_pixbuf(GTK_IMAGE(widgets->image), widgets->image1);
}
//============================================================================================
static void updateWidgets(pluginStruct* widgets)
{
  long volume;
  int  mute=1;
  //get info
  snd_mixer_selem_get_playback_volume(widgets->mixer->channel, SND_MIXER_SCHN_FRONT_LEFT, &volume);
  if (snd_mixer_selem_has_playback_switch(widgets->mixer->channel))
    snd_mixer_selem_get_playback_switch(widgets->mixer->channel, SND_MIXER_SCHN_FRONT_LEFT, &mute);

  mute = !mute;
  volume = volume_to_percent(volume);

  //update plugin icon
  setIcon(widgets, volume, mute);

  //set slider
  g_signal_handler_block(widgets->slider, widgets->connection);
  if (!gtk_widget_get_visible(widgets->window))
    gtk_range_set_value(GTK_RANGE(widgets->slider), volume);
  g_signal_handler_unblock(widgets->slider, widgets->connection);
}
//============================================================================================
static int volumeChanged(snd_mixer_elem_t* channel, unsigned int mask)
{
  pluginStruct* widgets;
  widgets = snd_mixer_elem_get_callback_private(channel);
  if (mask==SND_CTL_EVENT_MASK_VALUE)
    updateWidgets(widgets);
  return 0;
}
//============================================================================================
static gboolean timerFunction(snd_mixer_t* mixer)
{
  //int res;
  //res = snd_mixer_wait(mixer, 0);
  //if (res >= 0)
  snd_mixer_handle_events(mixer);
  return TRUE;
}
//============================================================================================
static void buttonClicked(GtkWidget* button, pluginStruct* widgets)
{
  if (gtk_widget_get_visible(widgets->window))
    gtk_widget_hide(widgets->window);
  else
  {
    GdkWindow* id;
    gint x, y;
    int w;

    id = gtk_widget_get_window (button);
    gdk_window_get_origin(id, &x, &y);
    w = gdk_window_get_width (id);
    y = y - SLIDER_HEIGHT;
    x = x - (int)((SLIDER_WIDTH - w) / 2);

    gtk_window_move(GTK_WINDOW(widgets->window), x, y);
    gtk_widget_show(widgets->window);
    gtk_widget_show(widgets->slider);
  }
}
//============================================================================================
static void windowDefocused(GtkWidget* window, pluginStruct* widgets)
{
  gtk_widget_hide(window);
}
//============================================================================================
static void sliderChanged(GtkRange* range, pluginStruct* widgets)
{
  int mute;
  long volume;
  volume = ((long)gtk_range_get_value(range));

  if (volume > 0)
    mute = 0;
  else
    mute = 1;

  //update plugin icon
  setIcon(widgets, volume, mute);

  volume = percent_to_volume(volume);

  //set alsa mute/unmute
  if (snd_mixer_selem_has_playback_switch(widgets->mixer->channel))
  {
    snd_mixer_selem_set_playback_switch(widgets->mixer->channel, SND_MIXER_SCHN_FRONT_LEFT, !mute);
    snd_mixer_selem_set_playback_switch(widgets->mixer->channel, SND_MIXER_SCHN_FRONT_RIGHT, !mute);
  }

  //set alsa volume level
  snd_mixer_selem_set_playback_volume(widgets->mixer->channel, SND_MIXER_SCHN_FRONT_LEFT,  volume);
  snd_mixer_selem_set_playback_volume(widgets->mixer->channel, SND_MIXER_SCHN_FRONT_RIGHT, volume);
}
//============================================================================================
static void pluginFree(XfcePanelPlugin* rootWidget, pluginStruct* widgets)
{
  snd_mixer_elem_set_callback(widgets->mixer->channel, NULL);
  g_source_destroy(g_main_context_find_source_by_id(NULL, widgets->timerId));

  snd_mixer_free(widgets->mixer->mixer);
  snd_mixer_selem_id_free(widgets->mixer->control);
  snd_mixer_close(widgets->mixer->mixer);
  g_slice_free(mixerStruct, widgets->mixer);

  gtk_widget_destroy(widgets->slider);
  gtk_widget_destroy(widgets->window);
  gtk_image_set_from_pixbuf (GTK_IMAGE(widgets->image), NULL);
  g_object_unref(widgets->image3);
  g_object_unref(widgets->image2);
  g_object_unref(widgets->image1);
  g_object_unref(widgets->image0);
  gtk_widget_destroy(widgets->image);
  gtk_widget_destroy(widgets->mainButton);
  g_slice_free(pluginStruct, widgets);
  //g_message("xfce4-panel-alsa-plugin stopped");
}
//============================================================================================
static void pluginConstruct(XfcePanelPlugin* rootWidget)
{
  //g_message("xfce4-panel-alsa-plugin started");
  pluginStruct* widgets;
  widgets = g_slice_new(pluginStruct);
  widgets->image0 = gdk_pixbuf_new_from_file("/usr/share/volumeicon/icons/tango/1.png", NULL);//  0%
  widgets->image1 = gdk_pixbuf_new_from_file("/usr/share/volumeicon/icons/tango/2.png", NULL);//  1%- 33%
  widgets->image2 = gdk_pixbuf_new_from_file("/usr/share/volumeicon/icons/tango/4.png", NULL);// 33%- 66%
  widgets->image3 = gdk_pixbuf_new_from_file("/usr/share/volumeicon/icons/tango/7.png", NULL);// 66%-100%

  //универсальный вариант из gtk, но выглядит, как инородный:
  //widgets->mainButton = gtk_button_new();
  //старый вариант:
  //widgets->mainButton = xfce_create_panel_button();
  //новый вариант из xfce-panel-convenience.h:
  widgets->mainButton = xfce_panel_create_button();
  widgets->image = gtk_image_new();
  gtk_button_set_image(GTK_BUTTON(widgets->mainButton), widgets->image);
  gtk_container_add(GTK_CONTAINER(rootWidget), widgets->mainButton);
  //в libxfce4panel-2.0 не надо, добавляет стандартное меню мыши
  //xfce_panel_plugin_add_action_widget(rootWidget, widgets->mainButton);

  widgets->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated (GTK_WINDOW(widgets->window), FALSE);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(widgets->window), TRUE);
  gtk_window_set_skip_pager_hint(GTK_WINDOW(widgets->window), TRUE);
  gtk_widget_set_size_request(widgets->window, SLIDER_WIDTH, SLIDER_HEIGHT);

  widgets->slider = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, MIN_VOLUME, MAX_VOLUME, 1);
  gtk_range_set_inverted(GTK_RANGE(widgets->slider), TRUE );
  gtk_range_set_increments (GTK_RANGE(widgets->slider), 1, 3);
  gtk_scale_set_draw_value (GTK_SCALE(widgets->slider), FALSE);
  gtk_range_set_value (GTK_RANGE(widgets->slider), 0);
  gtk_container_add(GTK_CONTAINER(widgets->window), widgets->slider);

  widgets->mixer = g_slice_new(mixerStruct);
  snd_mixer_open(&widgets->mixer->mixer, 0);
  snd_mixer_attach(widgets->mixer->mixer, CARD);
  snd_mixer_selem_register(widgets->mixer->mixer, NULL, NULL);
  snd_mixer_selem_id_malloc(&widgets->mixer->control);
  snd_mixer_selem_id_set_index(widgets->mixer->control, 0);
  snd_mixer_selem_id_set_name(widgets->mixer->control, CONTROL_NAME);
  snd_mixer_load(widgets->mixer->mixer);
  widgets->mixer->channel = snd_mixer_find_selem(widgets->mixer->mixer, widgets->mixer->control);
  snd_mixer_selem_set_playback_volume_range(widgets->mixer->channel, 0, 100);

  g_signal_connect(widgets->mainButton, "clicked", G_CALLBACK(buttonClicked), widgets);
  g_signal_connect(rootWidget, "free-data",  G_CALLBACK(pluginFree), widgets);
  g_signal_connect(widgets->window, "focus-out-event", G_CALLBACK(windowDefocused), widgets);
  widgets->connection = g_signal_connect(widgets->slider, "value-changed", G_CALLBACK(sliderChanged), widgets);
  //вызов volumeChanged происходит через раз, чем обусловлено- хз
  snd_mixer_elem_set_callback(widgets->mixer->channel, volumeChanged);
  snd_mixer_elem_set_callback_private(widgets->mixer->channel, widgets);
  widgets->timerId = g_timeout_add_seconds(TIMER_INTERVAL, (GSourceFunc)timerFunction, widgets->mixer->mixer);

  updateWidgets(widgets);

  gtk_widget_show(widgets->mainButton);
}

XFCE_PANEL_PLUGIN_REGISTER(pluginConstruct);
