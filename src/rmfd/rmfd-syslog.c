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
 * Copyright (C) 2015 Safran Passenger Innovations
 *
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <syslog.h>
#include <malloc.h>
#include "rmfd-syslog.h"

static gboolean syslog_open;

void
rmfd_syslog_setup (void)
{
    syslog_open = TRUE;
    openlog ("rmfd", LOG_CONS | LOG_PID | LOG_PERROR, LOG_DAEMON);
}

void
rmfd_syslog_teardown (void)
{
    if (syslog_open) {
        closelog ();
        syslog_open = FALSE;
    }
}

void
rmfd_syslog (gint type, const gchar *fmt, ...)
{
    char    *message;
    va_list  args;
    int      ret;

    if (!syslog_open)
        return;

    va_start (args, fmt);
    ret = vasprintf (&message, fmt, args);
    va_end (args);

    if (ret < 0)
        return;

    syslog (type, "%s", message);
    free (message);
}
