/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-queue.c: Asyncronous Callback-based SOAP Request Queue.
 *
 * Authors:
 *      Alex Graveley (alex@ximian.com)
 *
 * Copyright (C) 2001, Ximian, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>

#include "soup-ssl.h"
#include "soup-misc.h"

static gboolean 
soup_ssl_idle_waitpid (gpointer ppid)
{
	int pid = waitpid (GPOINTER_TO_INT (ppid), NULL, WNOHANG);

	if (pid == 0) return TRUE;
	return FALSE;
}

GIOChannel *
soup_ssl_get_iochannel (GIOChannel *sock)
{
	GIOChannel *new_chan;
	int sock_fd;
	int pid;
	int pair[2], flags;

	g_return_val_if_fail (sock != NULL, NULL);

	g_io_channel_ref (sock);

	if (!(sock_fd = g_io_channel_unix_get_fd (sock))) goto ERROR_ARGS;
	flags = fcntl(sock_fd, F_GETFD, 0);
	fcntl (sock_fd, F_SETFD, flags & ~FD_CLOEXEC);
	
	if (socketpair (PF_UNIX, SOCK_STREAM, 0, pair) != 0) goto ERROR_ARGS;

	fflush (stdin);
	fflush (stdout);

	pid = fork ();

	switch (pid) {
	case -1:
		goto ERROR;
	case 0:
		close (pair [1]);

		dup2 (pair [0], STDIN_FILENO);
		dup2 (pair [0], STDOUT_FILENO);

		close (pair [0]);

		putenv (g_strdup_printf ("SOCKFD=%d", sock_fd));
		putenv (g_strdup_printf ("SECURITY_POLICY=%d", 
					 soup_get_security_policy ()));

		execl (BINDIR G_DIR_SEPARATOR_S "soup-ssl-proxy", 
		       BINDIR G_DIR_SEPARATOR_S "soup-ssl-proxy", 
		       NULL);

		execlp ("soup-ssl-proxy", "soup-ssl-proxy", NULL);

		g_error ("Error executing SSL Proxy\n");
	}

	close (pair [0]);

	g_idle_add (soup_ssl_idle_waitpid, GINT_TO_POINTER (pid));

	flags = fcntl(pair [1], F_GETFL, 0);
	fcntl (pair [1], F_SETFL, flags & O_NONBLOCK);

	new_chan = g_io_channel_unix_new (pair [1]);
	
	/* FIXME: Why is this needed?? */
	g_io_channel_ref (new_chan);
	return new_chan;

 ERROR:
	close (pair [0]);
	close (pair [1]);
 ERROR_ARGS:
	g_io_channel_unref (sock);
	return NULL;
}
