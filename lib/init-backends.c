/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */

/* $Id$ */

/* Copyright (C) 1998-99 Martin Baulig
   This file is part of LibGTop 1.0.

   Contributed by Martin Baulig <martin@home-of-linux.org>, April 1998.

   LibGTop is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License,
   or (at your option) any later version.

   LibGTop is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with LibGTop; see the file COPYING. If not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <glibtop.h>
#include <glibtop/global.h>
#include <glibtop/xmalloc.h>

#include <glibtop/backend.h>

#if HAVE_LIBXML

#define LIBGTOP_XML_NAMESPACE	"http://www.home-of-linux.org/libgtop/1.1"

#include <gnome-xml/parser.h>

#include <dirent.h>

static void _glibtop_init_gmodule_backends (const char *);

#endif /* HAVE_LIBXML */

void
glibtop_init_backends (void)
{
    static int backends_initialized = 0;

    if (backends_initialized)
	return;
    backends_initialized = 1;

#if HAVE_LIBXML
    _glibtop_init_gmodule_backends (LIBGTOP_BACKEND_DIR);
#endif
}

#if HAVE_LIBXML

static gchar *
_get_library_filename (xmlDocPtr doc, xmlNodePtr cur, const char *directory)
{
    char *filename = xmlNodeListGetString (doc, cur->childs, 1);
    gchar *retval;

    if (!filename)
	return NULL;

    /* already absolute */
    if (filename [0] == '/')
	retval = g_strdup (filename);
    else
	retval = g_strdup_printf ("%s/%s", directory, filename);

    return retval;
}

static GSList *
_parse_extra_libs (xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr cur, const char *dir)
{
    GSList *list = NULL;

    /* We don't care what the top level element name is */
    cur = cur->childs;
    while (cur != NULL) {
        if ((!strcmp (cur->name, "ExtraLib")) && (cur->ns == ns)) {
	    xmlNodePtr sub = cur->childs;

	    while (sub != NULL) {
		if ((!strcmp (sub->name, "ShlibName")) && (sub->ns == ns))
		    list = g_slist_append
			(list, _get_library_filename (doc, sub, dir));

		sub = sub->next;
	    }
	}

        cur = cur->next;
    }

    return list;
}

static glibtop_backend_entry *
_parseBackend (xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr cur, const char *dir)
{
    glibtop_backend_entry *ret = NULL;

    /*
     * allocate the struct
     */
    ret = g_new0 (glibtop_backend_entry, 1);

    /* We don't care what the top level element name is */
    cur = cur->childs;
    while (cur != NULL) {
        if ((!strcmp (cur->name, "Name")) && (cur->ns == ns))
            ret->name = xmlNodeListGetString
		(doc, cur->childs, 1);

        if ((!strcmp (cur->name, "Location")) && (cur->ns == ns)) {
	    xmlNodePtr sub = cur->childs;

	    while (sub != NULL) {
		if ((!strcmp (sub->name, "LibtoolName")) && (sub->ns == ns))
		    ret->libtool_name = _get_library_filename (doc, sub, dir);
		if ((!strcmp (sub->name, "ShlibName")) && (sub->ns == ns))
		    ret->shlib_name = _get_library_filename (doc, sub, dir);

		if ((!strcmp (sub->name, "ExtraLibs")) && (sub->ns == ns))
		    ret->extra_libs = _parse_extra_libs (doc, ns, sub, dir);

		sub = sub->next;
	    }
	}

        cur = cur->next;
    }

    return ret;
}

static void
_glibtop_init_gmodule_backends (const char *directory)
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir (directory);
    if (!dir) return;

    while ((entry = readdir (dir)) != NULL) {
	size_t len = strlen (entry->d_name);
	gchar *filename;
	xmlDocPtr doc;
	xmlNsPtr ns;
	xmlNodePtr cur;

	if (len < 8)
	    continue;

	if (strcmp (entry->d_name+len-8, ".backend"))
	    continue;

	filename = g_strdup_printf ("%s/%s", directory, entry->d_name);

	doc = xmlParseFile (filename);

	if (!doc) {
	    g_warning ("Cannot parse %s", filename);
	    g_free (filename);
	    continue;
	}

	/* Make sure the document is of the right kind */

	cur = xmlDocGetRootElement (doc);
	if (!cur) {
	    xmlFreeDoc (doc);
	    g_free (filename);
	    continue;
	}

	ns = xmlSearchNsByHref (doc, cur, LIBGTOP_XML_NAMESPACE);
	if (!ns) {
	    g_warning ("File %s of wrong type; LibGTop Namespace not found",
		       filename);
	    g_free (filename);
	    xmlFreeDoc (doc);
	    continue;
	}

	if (strcmp (cur->name, "Backends")) {
	    g_warning ("File %s of the wrong type, root node != 'Backends'",
		       filename);
	    g_free (filename);
	    xmlFreeDoc (doc);
	    continue;
	}

	cur = cur->childs;
	while (cur != NULL) {
	    glibtop_backend_entry *backend;

	    if ((!strcmp(cur->name, "Backend")) && (cur->ns == ns)) {
		backend = _parseBackend (doc, ns, cur, directory);
		if (!backend) {
		    g_warning ("File %s of wrong type; cannot parse",
			       filename);
		    continue;
		}

		glibtop_register_backend (backend);
	    }
	    cur = cur->next;
	}

	g_free (filename);
	xmlFreeDoc (doc);
    }

    closedir (dir);
}

#endif /* HAVE_LIBXML */