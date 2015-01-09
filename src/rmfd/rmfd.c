/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 * rmfd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2013 Zodiac Inflight Innovations
 *
 * Author: Aleksander Morgado <aleksander@lanedo.com>
 */

#include "config.h"

#include <signal.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <gio/gio.h>
#include <glib-unix.h>

#include <libqmi-glib.h>

#include "rmfd-manager.h"
#include "rmfd-syslog.h"

#define PROGRAM_NAME    "rmfd"
#define PROGRAM_VERSION PACKAGE_VERSION

/* Globals */
static GMainLoop *loop;
RmfdManager *manager;

/* Context */
static gboolean verbose_flag;
static gboolean version_flag;

static GOptionEntry main_entries[] = {
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose_flag,
      "Run action with verbose logs",
      NULL
    },
    { "version", 'V', 0, G_OPTION_ARG_NONE, &version_flag,
      "Print version",
      NULL
    },
    { NULL }
};

static gboolean
quit_cb (gpointer user_data)
{
    g_debug (PROGRAM_NAME " caught signal: shutting down...");
    g_idle_add ((GSourceFunc) g_main_loop_quit, loop);
    return FALSE;
}

static void
log_handler (const gchar    *log_domain,
             GLogLevelFlags  log_level,
             const gchar    *message,
             gpointer        user_data)
{
    const gchar *log_level_str;
	time_t now;
	gchar time_str[64];
	struct tm    *local_time;

	now = time ((time_t *) NULL);
	local_time = localtime (&now);
	strftime (time_str, 64, "%d %b %Y, %H:%M:%S", local_time);

	switch (log_level) {
	case G_LOG_LEVEL_WARNING:
		log_level_str = "[Warning] ";
		break;
	case G_LOG_LEVEL_CRITICAL:
		log_level_str = "[Critical]";
		break;
    case G_LOG_FLAG_FATAL:
		log_level_str = "[Fatal]  ";
		break;
	case G_LOG_LEVEL_ERROR:
		log_level_str = "[Error]  ";
		break;
	case G_LOG_LEVEL_DEBUG:
		log_level_str = "[Debug]  ";
		break;
    default:
		log_level_str = "";
		break;
    }

    g_print ("[%s] %s %s\n", time_str, log_level_str, message);
}

static void
print_version_and_exit (void)
{
    g_print ("\n"
             PROGRAM_NAME " " PROGRAM_VERSION "\n"
             "Copyright (2013) Zodiac Inflight Innovations\n"
             "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl-3.0.html>\n"
             "This is free software: you are free to change and redistribute it.\n"
             "There is NO WARRANTY, to the extent permitted by law.\n"
             "\n");
    exit (EXIT_SUCCESS);
}

int
main (int argc, char *argv[])
{
    GOptionContext *context;

#if !GLIB_CHECK_VERSION (2,36,0)
    g_type_init ();
#endif

    /* Setup option context, process it and destroy it */
    context = g_option_context_new ("- Rave Modem Factory Daemon");
    g_option_context_add_main_entries (context, main_entries, NULL);
    g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

    if (version_flag)
        print_version_and_exit ();

    /* Setup logging if running in verbose mode */
    if (verbose_flag) {
        g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_MASK, log_handler, NULL);
        qmi_utils_set_traces_enabled (TRUE);
        g_log_set_handler ("Qmi", G_LOG_LEVEL_MASK, log_handler, NULL);
    }

    /* Initialize syslog if needed */
    rmfd_syslog_setup ();

    g_debug (PROGRAM_NAME " starting...");

    /* Setup signals */
    g_unix_signal_add (SIGTERM, quit_cb, NULL);
    g_unix_signal_add (SIGINT, quit_cb, NULL);

    /* Create manager */
    manager = rmfd_manager_new ();

    /* Go into the main loop */
    loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);

    g_debug (PROGRAM_NAME " is shut down");

    g_main_loop_unref (loop);
    g_object_unref (manager);

    rmfd_syslog_teardown ();

    return 0;
}
