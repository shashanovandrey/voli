/*
Simple sound volume (ALSA) indicator for system tray.
Thanks to everyone for their help and code examples.
Andrey Shashanov (2019)
Set your: SND_CTL_NAME, MIXER_PART_NAME
Dependences (Debian): libasound2, libasound2-dev
gcc -O2 -s -lasound -lm `pkg-config --cflags --libs gtk+-3.0` -o voli voli.c
*/

#define GDK_VERSION_MIN_REQUIRED (G_ENCODE_VERSION(3, 0))
#define GDK_VERSION_MAX_ALLOWED (G_ENCODE_VERSION(3, 12))

/* struct timespec */
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#else
#if (_POSIX_C_SOURCE < 200809L)
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <math.h>
#include <alsa/asoundlib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

#define SND_CTL_NAME "default" /* possible format "hw:0" */
#define MIXER_PART_NAME "Master"

typedef struct _applet
{
    snd_mixer_elem_t *elem;
    GPid child_pid_1;
    GPid child_pid_2;
} applet;

static GtkStatusIcon *status_icon;

static int elem_cb(snd_mixer_elem_t *elem,
                   unsigned int mask __attribute__((unused)))
{
    long min, max, value, range;
    int active, percentage;
    gchar tooltip_text[64];

    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &value);
    snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_MONO, &active);

    range = max - min;

    if (range == 0)
        percentage = 0;
    else
        percentage = (int)rint(((double)(value - min)) / (double)range * 100);

    if (!active || percentage == 0)
        gtk_status_icon_set_from_icon_name(status_icon, "audio-volume-muted");
    else if (percentage < 34)
        gtk_status_icon_set_from_icon_name(status_icon, "audio-volume-low");
    else if (percentage < 67)
        gtk_status_icon_set_from_icon_name(status_icon, "audio-volume-medium");
    else
        gtk_status_icon_set_from_icon_name(status_icon, "audio-volume-high");

    g_sprintf(tooltip_text, "%s: %d%%%s",
              MIXER_PART_NAME, percentage, active ? "" : " (Muted)");
    gtk_status_icon_set_tooltip_text(status_icon, tooltip_text);

    return 0;
}

static void child_watch_cb(GPid pid,
                           gint status __attribute__((unused)),
                           GPid *ppid)
{
    g_spawn_close_pid(pid);
    *ppid = 0;
}

static gboolean button_press_event_cb(GtkWidget *widget __attribute__((unused)),
                                      GdkEventButton *event,
                                      applet *data)
{
    gchar *argv_1[] = {"/usr/bin/x-terminal-emulator",
                       "-title",
                       "alsamixer",
                       "-e",
                       "/usr/bin/alsamixer",
                       NULL};
    gchar *argv_2[] = {"/usr/bin/pavucontrol",
                       NULL};
    int active;

    if (event->type != GDK_BUTTON_PRESS)
        return FALSE;

    switch (event->button)
    {
    case 1:
        if (data->child_pid_1 == 0 &&
            g_spawn_async_with_pipes(NULL, argv_1, NULL,
                                     G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL,
                                     &data->child_pid_1, NULL, NULL,
                                     NULL, NULL))
            g_child_watch_add(data->child_pid_1,
                              (GChildWatchFunc)child_watch_cb,
                              &data->child_pid_1);
        break;
    case 2:
        snd_mixer_selem_get_playback_switch(data->elem, SND_MIXER_SCHN_MONO,
                                            &active);
        snd_mixer_selem_set_playback_switch_all(data->elem, !active);
        elem_cb(data->elem, 0);
        break;
    case 3:
        if (data->child_pid_2 == 0 &&
            g_spawn_async_with_pipes(NULL, argv_2, NULL,
                                     G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL,
                                     &data->child_pid_2, NULL, NULL,
                                     NULL, NULL))
            g_child_watch_add(data->child_pid_2,
                              (GChildWatchFunc)child_watch_cb,
                              &data->child_pid_2);
    }
    return TRUE;
}

static gboolean poll_cb(GIOChannel *source __attribute__((unused)),
                        GIOCondition condition __attribute__((unused)),
                        gpointer mixer)
{
    if (snd_mixer_handle_events(mixer) < 0)
        gtk_main_quit();

    return TRUE;
}

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
    applet data;
    snd_mixer_t *mixer;
    snd_mixer_selem_id_t *sid;
    GIOChannel *giochannel;
    struct pollfd pfd;

    snd_mixer_open(&mixer, 0);
    snd_mixer_attach(mixer, SND_CTL_NAME);
    snd_mixer_selem_register(mixer, NULL, NULL);
    snd_mixer_load(mixer);

    snd_mixer_selem_id_malloc(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, MIXER_PART_NAME);

    data.elem = snd_mixer_find_selem(mixer, sid);
    snd_mixer_selem_id_free(sid);
    if (data.elem == NULL)
    {
        snd_mixer_close(mixer);
        return 1;
    }

    gtk_init(NULL, NULL);

    status_icon = gtk_status_icon_new();

    data.child_pid_1 = 0;
    data.child_pid_2 = 0;

    g_signal_connect(status_icon, "button-press-event",
                     G_CALLBACK(button_press_event_cb), &data);

    elem_cb(data.elem, 0);

    snd_mixer_elem_set_callback(data.elem, elem_cb);
    if (snd_mixer_poll_descriptors(mixer, &pfd, 1) != 1)
    {
        snd_mixer_close(mixer);
        return 1;
    }

    giochannel = g_io_channel_unix_new(pfd.fd);
    g_io_add_watch(giochannel, G_IO_IN, poll_cb, mixer);

    gtk_main();

    snd_mixer_close(mixer);
    snd_config_update_free_global();
    return 0;
}
