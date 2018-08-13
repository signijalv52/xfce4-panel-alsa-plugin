/*
создано на основе кода проекта ALSA Tray https://projects.flogisoft.com/alsa-tray/
лицензия GPLv3+


gcc -shared -Wall -fPIC -o libalsa-plugin.so alsa-plugin.c `pkg-config --cflags --libs libxfce4panel-2.0` `pkg-config --cflags --libs alsa`
sudo cp libalsa-plugin.so /usr/lib/i386-linux-gnu/xfce4/panel-plugins/libalsa-plugin.so
sudo cp alsa-plugin.desktop /usr/share/xfce4/panel-plugins/alsa-plugin.desktop


зависимости:
libxfce4panel-2.0-dev (libxfce4panel-2.0)
libasound2-dev
volumeicon-alsa


отладка:
xfce4-panel -q
PANEL_DEBUG=1 xfce4-panel


недостатки:
(*) всплывающие подсказки не используются
(*) "mute/unmute" не ипользуется
(*) кнопка на панели не принимает сигналы колеса мыши (в libxfce4panel-1.0 принимала)
(*) панель должна находиться только внизу
(*) уровень звука снимается только с 1 канала FRONT_LEFT
(*) уровень звука устанавливается сразу одинаково на 2 каналах FRONT_LEFT и FRONT_RIGHT


alsa-plugin.desktop

[Xfce Panel]
Type=X-XFCE-PanelPlugin
Encoding=UTF-8
Name=alsa-plugin
Comment=alsa-plugin
Icon=preferences-desktop-keyboard
X-XFCE-Module=alsa-plugin
X-XFCE-Module-Path=/usr/lib/i386-linux-gnu/xfce4/panel-plugins
X-XFCE-Internal=false
X-XFCE-Unique=true
X-XFCE-API=2.0



*/

#include <alsa/asoundlib.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#define __CARD "default"
#define __CONTROL_NAME "Master"
#define __SLIDER_HEIGHT 300
#define __SLIDER_WIDTH 70


//============================================================================================
typedef struct {
  snd_mixer_t*          mixer;
  snd_mixer_selem_id_t* control;
  snd_mixer_elem_t*     channel;
  long                  maxVolume;
  long                  minVolume;
  long                  prevVolume;
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
} pluginStruct;
//============================================================================================
static void loadMixer(mixerStruct* mixer)
{
  snd_mixer_load(mixer->mixer);
  mixer->channel = snd_mixer_find_selem(mixer->mixer, mixer->control);
}
//============================================================================================
static long getVolume(mixerStruct* mixer)
{
  loadMixer(mixer);
  long volume;
  snd_mixer_selem_get_playback_volume_range(mixer->channel, &mixer->minVolume, &mixer->maxVolume);
  snd_mixer_selem_get_playback_volume(mixer->channel, SND_MIXER_SCHN_FRONT_LEFT, &volume);
  snd_mixer_free(mixer->mixer);
  return volume;
}
//============================================================================================
static void setVolume(mixerStruct* mixer, long volume)
{
  loadMixer(mixer);
  snd_mixer_selem_set_playback_volume(mixer->channel, SND_MIXER_SCHN_FRONT_LEFT,  volume);
  snd_mixer_selem_set_playback_volume(mixer->channel, SND_MIXER_SCHN_FRONT_RIGHT, volume);
  snd_mixer_free(mixer->mixer);
}
//============================================================================================
static gboolean updateWidgets(pluginStruct* widgets)
{
  const long volume = getVolume(widgets->mixer);
  if (widgets->mixer->prevVolume == volume)
    {}
  else
  {
    widgets->mixer->prevVolume = volume;
    //-----------------image------------------
    if (volume > 0)
    {
      if (volume > (long)(33 * widgets->mixer->maxVolume / 100))
      {
        if (volume > (long)(66 * widgets->mixer->maxVolume / 100))
          gtk_image_set_from_pixbuf((GtkImage*)widgets->image, widgets->image3);
        else
          gtk_image_set_from_pixbuf((GtkImage*)widgets->image, widgets->image2);
      }
      else
        gtk_image_set_from_pixbuf((GtkImage*)widgets->image, widgets->image1);
    }
    else
      gtk_image_set_from_pixbuf((GtkImage*)widgets->image, widgets->image0);
    //-----------------slider------------------
    gtk_range_set_value((GtkRange*)widgets->slider, volume);
  }
  return TRUE;
}
//============================================================================================
static void windowShow(GtkWidget* button, pluginStruct* widgets)
{
  GdkWindow* id;
  gint x, y;
  int w;

  id = gtk_widget_get_window (button);
  gdk_window_get_origin(id, &x, &y);
  w = gdk_window_get_width (id);
  y = y - __SLIDER_HEIGHT;
  x = x - (int)((__SLIDER_WIDTH - w) / 2);

  gtk_window_move(GTK_WINDOW(widgets->window), x, y);
  gtk_widget_show(widgets->window);
  gtk_widget_show(widgets->slider);
}
//============================================================================================
static void windowHide(GtkWidget* window, gpointer data)
{
  gtk_widget_hide(window);
}
//============================================================================================
static void sliderChanged(GtkRange* range, pluginStruct* widgets)
{
  setVolume(widgets->mixer, (long)gtk_range_get_value(range));
}
//============================================================================================
static void pluginFree(XfcePanelPlugin* rootWidget, pluginStruct* widgets)
{
  g_source_destroy(g_main_context_find_source_by_id(NULL, widgets->timerId));

  snd_mixer_selem_id_free(widgets->mixer->control);
  snd_mixer_close(widgets->mixer->mixer);
  g_slice_free(mixerStruct, widgets->mixer);

  gtk_widget_destroy(widgets->slider);
  gtk_widget_destroy(widgets->window);
  gtk_image_set_from_pixbuf ((GtkImage*)widgets->image, NULL);
  g_object_unref(widgets->image3);
  g_object_unref(widgets->image2);
  g_object_unref(widgets->image1);
  g_object_unref(widgets->image0);
  gtk_widget_destroy(widgets->image);
  gtk_widget_destroy(widgets->mainButton);
  g_slice_free(pluginStruct, widgets);
}
//============================================================================================
static void pluginConstruct(XfcePanelPlugin* rootWidget)
{
  pluginStruct* widgets;
  widgets = g_slice_new(pluginStruct);
  widgets->image0 = gdk_pixbuf_new_from_file("/usr/share/volumeicon/icons/tango/1.png", NULL);//  0%
  widgets->image1 = gdk_pixbuf_new_from_file("/usr/share/volumeicon/icons/tango/2.png", NULL);//  1%- 33%
  widgets->image2 = gdk_pixbuf_new_from_file("/usr/share/volumeicon/icons/tango/4.png", NULL);// 33%- 66%
  widgets->image3 = gdk_pixbuf_new_from_file("/usr/share/volumeicon/icons/tango/7.png", NULL);// 66%-100%
  //widgets->image0 = gdk_pixbuf_scale_simple(widgets->image0, 25, 25, GDK_INTERP_BILINEAR);

  widgets->mainButton = xfce_create_panel_button();
  widgets->image = gtk_image_new();
  gtk_button_set_image(GTK_BUTTON(widgets->mainButton), widgets->image);
  gtk_container_add(GTK_CONTAINER(rootWidget), widgets->mainButton);

  //в libxfce4panel-2.0 не надо, добавляет стандартное меню мыши
  xfce_panel_plugin_add_action_widget(rootWidget, widgets->mainButton);

  widgets->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated (GTK_WINDOW(widgets->window), FALSE);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(widgets->window), TRUE);
  gtk_window_set_skip_pager_hint(GTK_WINDOW(widgets->window), TRUE);
  gtk_widget_set_size_request(widgets->window, __SLIDER_WIDTH, __SLIDER_HEIGHT);

  widgets->slider = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0, 100, 10);
  gtk_range_set_inverted((GtkRange*)widgets->slider, TRUE );
  gtk_range_set_increments ((GtkRange*)widgets->slider, 1, 3);
  gtk_scale_set_draw_value ((GtkScale*)widgets->slider, FALSE);
  gtk_range_set_value ((GtkRange*)widgets->slider, 0);
  gtk_container_add(GTK_CONTAINER(widgets->window), widgets->slider);

  widgets->mixer = g_slice_new(mixerStruct);
  widgets->mixer->prevVolume = -1;
  snd_mixer_open(&widgets->mixer->mixer, 0);
  snd_mixer_attach(widgets->mixer->mixer, __CARD);
  snd_mixer_selem_register(widgets->mixer->mixer, NULL, NULL);
  snd_mixer_selem_id_malloc(&widgets->mixer->control);
  snd_mixer_selem_id_set_index(widgets->mixer->control, 0);
  snd_mixer_selem_id_set_name(widgets->mixer->control, __CONTROL_NAME);

  updateWidgets(widgets);
  gtk_range_set_range((GtkRange*)widgets->slider, widgets->mixer->minVolume, widgets->mixer->maxVolume);

  g_signal_connect(widgets->mainButton, "clicked", G_CALLBACK(windowShow), widgets);
  g_signal_connect(rootWidget, "free-data",  G_CALLBACK(pluginFree), widgets);
  g_signal_connect(widgets->window, "focus-out-event", G_CALLBACK(windowHide), NULL);
  g_signal_connect(widgets->slider, "value-changed", G_CALLBACK(sliderChanged), widgets);


  widgets->timerId = g_timeout_add_seconds(2, (GSourceFunc)updateWidgets, widgets);


  gtk_widget_show(widgets->mainButton);
}
XFCE_PANEL_PLUGIN_REGISTER(pluginConstruct);