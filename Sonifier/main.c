/* Copyright (c) 2017 LeafLabs, LLC.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Sonifier: The Willow stream analysis plugin for sonification
 *
 * When the "Start" Button is pressed, Sonifier plays the
 * selected datanode channel as audio to the system default audio
 * output, and when the Stop button is pressed after Start, audio
 * output stops.
 */

#define _POSIX_SOURCE
#include <gtk/gtk.h>
#include <glib.h>
#include <glib-unix.h>
#include <glib/gstdio.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

/* window parameters */
#define WINDOW_BORDER_WIDTH       10
#define WINDOW_CHILD_SPACING      5

/* channel adjustment parameters*/
#define CHANNEL_ADJ_MIN           0.0
#define CHANNEL_ADJ_INIT          CHANNEL_ADJ_MIN
#define CHANNEL_ADJ_MAX           1023.0
#define CHANNEL_ADJ_STEP          1.0
#define CHANNEL_ADJ_PAGE_STEP     32.0
#define CHANNEL_ADJ_PAGE_SIZE     0.0

/* channel spin button parameters */
#define CHANNEL_SPIN_CLIMB_RATE   1.0
#define CHANNEL_SPIN_DISPLAY_SIZE 100

/* volume scale parameters */
#define VOLUME_SCALE_MIN          1.0
#define VOLUME_SCALE_INIT         VOLUME_SCALE_MIN
#define VOLUME_SCALE_MAX          10.0
#define VOLUME_SCALE_STEP         0.1
#define VOLUME_SCALE_DISPLAY_SIZE 100

#define SAMPLE_RATE 30000

#define SAMPLE_RECV_HZ 20

#define MAX_SAMPLES_PER_RECV SAMPLE_RATE/SAMPLE_RECV_HZ

#define CHANNELS_PER_CHIP 32

static int channel_number = 0;
static int chip = 0;
static int chip_channel = 0;

static uint16_t recvbuf[MAX_SAMPLES_PER_RECV * CHANNELS_PER_CHIP];
static int16_t samples[MAX_SAMPLES_PER_RECV];

static const pa_sample_spec ss = {
  .format = PA_SAMPLE_S16LE,
  .rate = SAMPLE_RATE,
  .channels = 1,
};

static int quit_requested = 0;
static int sonifying = 0;

static gdouble digital_gain = VOLUME_SCALE_INIT;

static GPollFD gpollfd[1];
static gchar *protoargv[3];

static GPid subprocess_pid;

static void start_sonifying(void)
{
  GError *err = NULL;
  gint protostdout;

  if (!sonifying) {
    fprintf(stderr, "hwif_req: startStreaming_subsamples\n");
    if (g_spawn_async_with_pipes(NULL,
                                 protoargv,
                                 NULL,
                                 G_SPAWN_DEFAULT,
                                 NULL,
                                 NULL,
                                 &subprocess_pid,
                                 NULL,
                                 &protostdout,
                                 NULL,
                                 &err) == FALSE) {
      g_print(__FILE__
              ": failed to spawn \"%s %s\"\n",
              protoargv[0], protoargv[1]);
      if (err) {
        g_print(__FILE__
                ": glib error: %s\n",
                err->message);
        g_error_free(err);
      }
      exit(1);
    }

    if (g_unix_set_fd_nonblocking(protostdout, TRUE, &err) == FALSE) {
      g_print(__FILE__
              ": failed to set subprocess stdout %d nonblocking\n",
              protostdout);
      if (err) {
        g_print(__FILE__
                ": glib error: %s\n",
                err->message);
        g_error_free(err);
      }

      if (g_close(protostdout, &err) == FALSE) {
        g_print(__FILE__
                ": failed to close subprocess stdout %d\n",
                protostdout);
        if (err) {
          g_print(__FILE__
                  ": glib error: %s\n",
                  err->message);
          g_error_free(err);
        }
      }
      exit(1);
    }

    gpollfd[0].fd = protostdout;
    sonifying = 1;
  }
}

static void stop_sonifying(void)
{
  if (sonifying) {
    sonifying = 0;
    fprintf(stderr, "hwif_req: stopStreaming\n");
    kill(subprocess_pid, SIGKILL);
    g_spawn_close_pid(subprocess_pid);
  }
}

static void clicker(GtkWidget __attribute__((unused)) *widget,
                    gpointer data)
{
  if (!strcmp((const char *) data, "start")) {
    start_sonifying();
  }
  else if (!strcmp((const char *) data, "stop")) {
    stop_sonifying();
  }
}

static void channelchanged(GtkWidget __attribute__((unused)) *widget,
                           GtkSpinButton *spin)
{
  int new_channel_number;
  int new_chip;
  int new_chip_channel;

  new_channel_number = gtk_spin_button_get_value_as_int(spin);
  new_chip = new_channel_number / CHANNELS_PER_CHIP;
  new_chip_channel = new_channel_number % CHANNELS_PER_CHIP;

  if (new_chip != chip) {
    /* inform WillowGUI of the new chip number */
    fprintf(stderr, "hwif_req: setSubsamples_byChip, %d\n",
            new_chip);
  }

  channel_number = new_channel_number;
  chip = new_chip;
  chip_channel = new_chip_channel;
}

static void volumechanged(GtkWidget __attribute__((unused)) *widget,
                          GtkScale *scale)
{
  digital_gain = gtk_range_get_value((GtkRange *)scale);
}

static void destroy(GtkWidget __attribute__((unused)) *widget,
                    gpointer __attribute__((unused)) data)
{
  g_print("Window is being destroyed\n");
  quit_requested = 1;
}

int main(int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *main_vbox;
  GtkWidget *channelspinner;
  GtkWidget *startbutton;
  GtkWidget *stopbutton;
  GtkWidget *volumescale;
  GtkWidget *label;
  GtkWidget *volumelabel;
  GtkAdjustment *adj;
  pa_simple *s = NULL;
  int error;
  int ret = 1;

  if (argc != 2) {
    /*
     * in production use, stderr is dedicated
     * to the streaming command channel from the Sonifier
     * to WillowGUI.
     */
    g_print("usage: %s <path to proto2bytes>\n",
            argv[0]);
    exit(1);
  }


  protoargv[0] = argv[1];
  protoargv[1] = "-A";
  protoargv[2] = NULL;

  if (!(s = pa_simple_new(NULL, "Sonifier", PA_STREAM_PLAYBACK,
                          NULL, "Activity", &ss, NULL, NULL,
                          &error))) {

    g_print(__FILE__
            ": pa_simple_new() - playback failed: %s\n",
            pa_strerror(error));
    goto done;
  }

  gtk_init(&argc, &argv);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title(GTK_WINDOW(window), "Sonification Control");

  g_signal_connect(window, "destroy",
                    G_CALLBACK(destroy), NULL);


  gtk_container_set_border_width(GTK_CONTAINER(window),
                                 WINDOW_BORDER_WIDTH);

  main_vbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,
                          WINDOW_CHILD_SPACING);
  gtk_container_set_border_width(GTK_CONTAINER(main_vbox),
                                 WINDOW_BORDER_WIDTH);
  gtk_container_add(GTK_CONTAINER(window), main_vbox);

  label = gtk_label_new("Channel Number:");
  gtk_box_pack_start(GTK_BOX(main_vbox), label, FALSE, TRUE, 0);

  adj = (GtkAdjustment *) gtk_adjustment_new(CHANNEL_ADJ_INIT,
                                             CHANNEL_ADJ_MIN,
                                             CHANNEL_ADJ_MAX,
                                             CHANNEL_ADJ_STEP,
                                             CHANNEL_ADJ_PAGE_STEP,
                                             CHANNEL_ADJ_PAGE_SIZE);
  channelspinner = gtk_spin_button_new(adj,
                                       CHANNEL_SPIN_CLIMB_RATE,
                                       0);
  gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(channelspinner), TRUE);
  gtk_widget_set_size_request(channelspinner,
                              CHANNEL_SPIN_DISPLAY_SIZE,
                              -1);
  gtk_box_pack_start(GTK_BOX(main_vbox),
                     channelspinner, FALSE, TRUE, 0);

  /* inform WillowGUI iof the initial chip number */
  fprintf(stderr, "hwif_req: setSubsamples_byChip, %d\n", chip);

  startbutton = gtk_button_new_with_label("Start Sonifying");

  gtk_box_pack_start(GTK_BOX(main_vbox), startbutton, FALSE, TRUE, 0);

  stopbutton = gtk_button_new_with_label("End Sonifying");

  gtk_box_pack_start(GTK_BOX(main_vbox), stopbutton, FALSE, TRUE, 0);

  g_signal_connect(adj, "value_changed",
                   G_CALLBACK(channelchanged),
                   channelspinner);

  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (channelchanged),
                    channelspinner);

  volumelabel = gtk_label_new("Digital Gain:");
  gtk_box_pack_start(GTK_BOX(main_vbox), volumelabel, FALSE, TRUE, 0);

  volumescale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                         VOLUME_SCALE_MIN,
                                         VOLUME_SCALE_MAX,
                                         VOLUME_SCALE_STEP);

  gtk_widget_set_size_request(volumescale,
                              VOLUME_SCALE_DISPLAY_SIZE,
                              -1);

  gtk_box_pack_start(GTK_BOX(main_vbox), volumescale, FALSE, TRUE, 0);

  g_signal_connect(adj, "value_changed",
                   G_CALLBACK(channelchanged),
                   channelspinner);

  g_signal_connect(startbutton, "clicked",
                   G_CALLBACK(clicker), "start");

  g_signal_connect(stopbutton, "clicked",
                   G_CALLBACK(clicker), "stop");

  g_signal_connect(volumescale, "value_changed",
                   G_CALLBACK(volumechanged),
                   volumescale);

  gtk_widget_show_all(window);


  gpollfd[0].events = G_IO_IN | G_IO_HUP | G_IO_ERROR;

  while (!quit_requested) {
    int num_fds = 0;
    ssize_t bytes;

    while (gtk_events_pending ()) {
      gtk_main_iteration();
    }

    gpollfd[0].revents = 0;

    /*
     * while sonifying, poll the subprocess's stdout; otherwise specify
     * zero fds to use g_poll as a synchronous timer to limit churn.
     */
    num_fds = g_poll(&gpollfd[0], sonifying? 1 : 0, 1000/SAMPLE_RECV_HZ);

    if (num_fds > 0) {
      gint outfd = gpollfd[0].fd;

      if (gpollfd[0].revents & G_IO_HUP) {
        g_print(__FILE__
                ": Received HUP on subprocess stdout %d\n",
                outfd);
        stop_sonifying();
      } else if (gpollfd[0].revents & G_IO_ERR) {
        g_print(__FILE__
                ": Received ERR on subprocess stdout %d\n",
                outfd);;
        stop_sonifying();
      } else if (gpollfd[0].revents & G_IO_IN) {
        bytes = read(outfd, recvbuf, sizeof(recvbuf));

        if (bytes < 0) {
          g_print(__FILE__
                  ": read error on subprocess stdout %d: \"%s\"\n",
                  outfd, strerror(errno));
          continue;
        } else if (bytes < (ssize_t)(CHANNELS_PER_CHIP * sizeof(uint16_t))) {
          g_print(__FILE__
                  ": short read of %ld bytes from subprocess stdout %d\n",
                  bytes, outfd);
          continue;
        } else {
          int j;
          int num_samples = bytes / (CHANNELS_PER_CHIP * sizeof(uint16_t));

          for (j = 0; j < num_samples; j++) {
            gdouble scaled_value;

            /*
             * assemble samples from the channel of interest on the chip,
             * converting from unsigned to signed 16-bit representation,
             * and scaling as specified by the digital volume slider.
             */
            scaled_value = digital_gain *
              ((int) recvbuf[(j * CHANNELS_PER_CHIP) +
                             chip_channel] - (1 << 15));

            if (scaled_value > (gdouble) INT16_MAX) {
              samples[j] = INT16_MAX;
            } else if (scaled_value < (gdouble) INT16_MIN) {
              samples[j] = INT16_MIN;
            } else {
              samples[j] = (int16_t) scaled_value;
            }
          }
          if (pa_simple_write(s, samples, sizeof(int16_t) * num_samples,
                              &error) < 0) {
            g_print(__FILE__
                    ": pa_simple_write() failed: %s\n",
                    pa_strerror(error));
            goto done;
          }
        }
      } else {
        g_print(__FILE__
                ": Unknown event %d on subprocess stdout %d, exiting\n",
                gpollfd[0].revents, outfd);
        goto done;
      }
    }
  }

  if (pa_simple_drain(s, &error) < 0) {
    g_print(__FILE__
            ": pa_simple_drain() failed: %s\n",
            pa_strerror(error));
    goto done;
  }
  ret = 0;

 done:
  if (s) {
    pa_simple_free(s);
  }

  g_print(__FILE__
          ": main is exiting now with status %d\n",
          ret);

  return ret;
}
