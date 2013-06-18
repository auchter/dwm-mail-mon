/*
 * Copyright (c) 2013 Michael Auchter <michael.auchter@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <X11/Xlib.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define FOREACH(x, arr) \
	for (typeof(*(arr)) *(x) = (arr); (x) != ((arr) + ARRAY_SIZE(arr)); (x)++)

static struct {
	const char *name;
	const char *path;
	int wd;
	int nfiles;
} watches[] = {
	{
		.name = "misc",
		.path =	"/home/mauchter/mail/misc/new"
	},
	{
		.name = "group",
		.path = "/home/mauchter/mail/group/new"
	},
	{
		.name = "reviews",
		.path = "/home/mauchter/mail/reviews/new"
	},
};

static void fatal(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(1);
}

static int nfiles_in_dir(const char *path)
{
	DIR *dir;
	struct dirent *dirent;
	int n = 0;

	dir = opendir(path);
	if (!dir)
		fatal("cannot open \"%s\": %s\n", path, strerror(errno));

	while ((dirent = readdir(dir)))
		if (strcmp(".", dirent->d_name) && strcmp("..", dirent->d_name))
			n++;

	closedir(dir);
	return n;
}

static void display_watches(Display *d)
{
	int rv;
	char *cur, *prev;

	rv = asprintf(&cur, "%s", "");
	if (rv < 0)
		fatal("asprintf\n");

	FOREACH(w, watches) {
		prev = cur;
		rv = asprintf(&cur, "%s[%s: %d] ", prev, w->name, w->nfiles);
		if (rv < 0)
			fatal("asprintf\n");
		free(prev);
	}

	XStoreName(d, DefaultRootWindow(d), cur);
	XSync(d, False);

	free(cur);
}

int main(int argc, char **argv)
{
	Display *d;
	int fd;
	union {
		struct inotify_event event;
		char buf[sizeof(struct inotify_event) + NAME_MAX + 1];
	} e;

	d = XOpenDisplay(NULL);
	if (!d)
		fatal("cannot open display\n");

	fd = inotify_init();
	if (fd < 0)
		fatal("inotify_init() failed: %s", strerror(errno));

	FOREACH(w, watches) {
		w->nfiles = nfiles_in_dir(w->path);
		w->wd = inotify_add_watch(fd, w->path, IN_CREATE|IN_DELETE|IN_MOVED_FROM);
		if (w->wd < 0)
			fatal("failed to add watch for \"%s\": %s\n",
					w->path, strerror(errno));
	}

	display_watches(d);

	while (read(fd, &e, sizeof(e)) > 0) {
		FOREACH(w, watches)
			if (w->wd == e.event.wd)
				w->nfiles = nfiles_in_dir(w->path);
		display_watches(d);
	}

	XCloseDisplay(d);
	return 0;
}

