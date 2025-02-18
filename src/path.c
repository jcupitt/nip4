// Search paths for files.

/*

	Copyright (C) 1991-2003 The National Gallery

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */

/* just load .defs/.wses from "."
#define DEBUG_LOCAL
 */

/* show path searches
#define DEBUG_SEARCH
 */

/* show path rewrites
#define DEBUG_REWRITE
 */

#include "nip4.h"

/* Default search paths if prefs fail.
 */
GSList *path_start_default = NULL;
GSList *path_search_default = NULL;
const char *path_tmp_default = NULL;

/* We rewrite paths to try to handle files referenced in workspaces in
 * directories that move.
 *
 * For example, suppose we have workspace.ws in /some/directory which loads
 * image.v in that directory. The workspace will include a line like
 * (Image_file "/some/directory/image"). Now if directory is moved to
 * /other/directory and workspace.ws reloaded, we want to rewrite the string
 * "/some/directory/image.v" to "/other/directory/image.v".
 *
 * Also consider picking ICC profiles in export/import: we want to avoid
 * putting the path into the ws file, we need to go back to "$VIPSHOME" again.
 *
 * Rewrite rules can be "locked". For example, we don't want the rewrite from
 * "/home/john" to "$HOME" to ever be removed.
 */

typedef struct _Rewrite {
	char *old;
	char *new;
	gboolean lock;
} Rewrite;

static GSList *rewrite_list = NULL;

static void
path_rewrite_free(Rewrite *rewrite)
{
	rewrite_list = g_slist_remove(rewrite_list, rewrite);

	VIPS_FREE(rewrite->old);
	VIPS_FREE(rewrite->new);
	VIPS_FREE(rewrite);
}

void
path_rewrite_free_all(void)
{
	while (rewrite_list) {
		Rewrite *rewrite = (Rewrite *) rewrite_list->data;

		VIPS_FREEF(path_rewrite_free, rewrite);
	}
}

static Rewrite *
path_rewrite_new(const char *old, const char *new, gboolean lock)
{
	Rewrite *rewrite;

	rewrite = g_new(Rewrite, 1);
	rewrite->old = g_strdup(old);
	rewrite->new = g_strdup(new);
	rewrite->lock = lock;
	rewrite_list = g_slist_prepend(rewrite_list, rewrite);

	return rewrite;
}

static gint
path_rewrite_sort_fn(Rewrite *a, Rewrite *b)
{
	return strlen(b->old) - strlen(a->old);
}

static Rewrite *
path_rewrite_lookup(const char *old)
{
	GSList *p;
	Rewrite *rewrite;

	for (p = rewrite_list; p; p = p->next) {
		rewrite = (Rewrite *) p->data;

		if (strcmp(old, rewrite->old) == 0)
			return rewrite;
	}

	return NULL;
}

/* Add a new rewrite pair to the rewrite list. @new can be NULL, meaning
 * remove a rewrite rule.
 */
void
path_rewrite_add(const char *old, const char *new, gboolean lock)
{
	char old_buf[VIPS_PATH_MAX + 1];
	char new_buf[VIPS_PATH_MAX + 1];

	Rewrite *rewrite;

#ifdef DEBUG_REWRITE
	printf("path_rewrite_add: old = %s, new = %s, lock = %d\n",
		old, new, lock);
#endif /*DEBUG_REWRITE*/

	g_return_if_fail(old);

	/* We want the old path in long form, with a trailing '/'. The
	 * trailing '/' will stop us rewriting filenames.
	 *
	 * If we keep all @old paths in long form we can avoid rewrite loops.
	 */
	g_strlcpy(old_buf, old, VIPS_PATH_MAX);
	strcat(old_buf, G_DIR_SEPARATOR_S);
	path_expand(old_buf);
	old = old_buf;

	if (new) {
		/* We must keep the new path in short (unexpanded) form,
		 * obviously.
		 */
		g_strlcpy(new_buf, new, VIPS_PATH_MAX);
		strcat(new_buf, G_DIR_SEPARATOR_S);
		new = new_buf;
	}

	/* If old is a prefix of new we will get endless expansion.
	 */
	if (new &&
			is_prefix(old, new))
		return;

	if ((rewrite = path_rewrite_lookup(old))) {
		if (!rewrite->lock &&
			(!new || strcmp(old, new) == 0)) {
#ifdef DEBUG_REWRITE
			printf("path_rewrite_add: removing\n");
#endif /*DEBUG_REWRITE*/

			VIPS_FREEF(path_rewrite_free, rewrite);
		}
		else if (!rewrite->lock && new) {
#ifdef DEBUG_REWRITE
			printf("path_rewrite_add: updating\n");
#endif /*DEBUG_REWRITE*/

			VIPS_SETSTR(rewrite->new, new);
		}
		else {
#ifdef DEBUG_REWRITE
			printf("path_rewrite_add: rewrite rule locked\n");
#endif /*DEBUG_REWRITE*/
		}
	}
	else if (new &&strcmp(old, new) != 0) {
#ifdef DEBUG_REWRITE
		printf("path_rewrite_add: adding\n");
#endif /*DEBUG_REWRITE*/

		(void) path_rewrite_new(old, new, lock);
	}

	/* Keep longest old first, in case one old is a prefix of
	 * another.
	 */
	rewrite_list = g_slist_sort(rewrite_list,
		(GCompareFunc) path_rewrite_sort_fn);

#ifdef DEBUG_REWRITE
	{
		GSList *p;

		printf("path_rewrite_add: state:\n");

		for (p = rewrite_list; p; p = p->next) {
			rewrite = (Rewrite *) p->data;

			printf("\told = %s, new = %s\n", rewrite->old, rewrite->new);
		}
	}
#endif /*DEBUG_REWRITE*/
}

/* Rewrite a string using the rewrite list. buf must be VIPS_PATH_MAX
 * characters.
 */
void
path_rewrite(char *buf)
{
	GSList *p;
	gboolean changed;

#ifdef DEBUG_REWRITE
	printf("path_rewrite: %s\n", buf);
#endif /*DEBUG_REWRITE*/

	do {
		changed = FALSE;

		for (p = rewrite_list; p; p = p->next) {
			Rewrite *rewrite = (Rewrite *) p->data;

			if (is_prefix(rewrite->old, buf)) {
				int olen = strlen(rewrite->old);
				int nlen = strlen(rewrite->new);
				int blen = strlen(buf);

				if (blen - olen + nlen > VIPS_PATH_MAX - 3)
					break;

				memmove(buf + nlen, buf + olen, blen - olen + 1);
				memcpy(buf, rewrite->new, nlen);

				changed = TRUE;

				break;
			}
		}
	} while (changed);

#ifdef DEBUG_REWRITE
	printf("\t-> %s\n", buf);
#endif /*DEBUG_REWRITE*/
}

/* The inverse: rewrite in long form ready for file ops.
 */
void
path_expand(char *path)
{
	char buf[VIPS_PATH_MAX];

	expand_variables(path, buf);
	nativeize_path(buf);
	absoluteize_path(buf);
	canonicalize_path(buf);
	g_strlcpy(path, buf, VIPS_PATH_MAX);
}

/* Rewite a path to compact form. @path must be VIPS_PATH_MAX characters.
 *
 * Examples:
 *
 * 	/home/john/../somefile 		-> $HOME/../somefile
 * 	/home/./john/../somefile 	-> $HOME/../somefile
 * 	fred				-> ./fred
 */
void
path_compact(char *path)
{
	path_expand(path);
	path_rewrite(path);
}

/* Turn a search path (eg. "/pics/lr:/pics/hr") into a list of directory names.
 */
GSList *
path_parse(const char *path)
{
	GSList *op = NULL;
	const char *p;
	const char *e;
	int len;
	char name[VIPS_PATH_MAX + 1];

	for (p = path; *p; p = e) {
		/* Find the start of the next component, or the NULL
		 * character.
		 */
		if (!(e = strchr(p, G_SEARCHPATH_SEPARATOR)))
			e = p + strlen(p);
		len = e - p + 1;

		/* Copy to our buffer, turn to string.
		 */
		g_strlcpy(name, p, VIPS_MIN(len, VIPS_PATH_MAX));

		/* Add to path list.
		 */
		op = g_slist_append(op, g_strdup(name));

		/* Skip G_SEARCHPATH_SEPARATOR.
		 */
		if (*e == G_SEARCHPATH_SEPARATOR)
			e++;
	}

	return op;
}

/* Sub-fn of below. Add length of this string + 1 (for ':').
 */
static int
path_add_component(const char *str, int c)
{
	return c + strlen(str) + 1;
}

/* Sub-fn of below. Copy string to buffer, append ':', return new end.
 */
static char *
path_add_string(const char *str, char *buf)
{
	strcpy(buf, str);
	strcat(buf, G_SEARCHPATH_SEPARATOR_S);

	return buf + strlen(buf);
}

/* Turn a list of directory names into a search path.
 */
char *
path_unparse(GSList *path)
{
	int len = GPOINTER_TO_INT(slist_fold(path, 0,
		(SListFoldFn) path_add_component, NULL));
	char *buf = imalloc(NULL, len + 1);

	/* Build new string.
	 */
	slist_fold(path, buf, (SListFoldFn) path_add_string, NULL);

	/* Fix '\0' to remove trailing G_SEARCHPATH_SEPARATOR.
	 */
	if (len > 0)
		buf[len - 1] = '\0';

	return buf;
}

void
path_print(GSList *path)
{
	g_autofree char *str = path_unparse(path);
	printf("%s", str);
}

/* Free a path. path_free() is reserved n OS X :(
 */
void
path_free2(GSList *path)
{
	slist_map(path, (SListMapFn) g_free, NULL);
	g_slist_free(path);
}

/* Track this stuff during a file search.
 */
typedef struct _Search {
	/* Pattern we search for, and it's compiled form. This does not
	 * include any directory components.
	 */
	char *basename;
	GPatternSpec *wild;

	/* Directory offset. If the original pattern is a relative path, eg.
	 * "poop/x*.v", we search every directory on path for a subdirectory
	 * called "poop" and then search all files within that.
	 */
	char *dirname;

	/* User function to call for every matching file.
	 */
	path_map_fn fn;
	void *a;

	/* Files we've previously offered to the user function: we remove
	 * duplicates. So "path1/wombat.def" hides "path2/wombat.def".
	 */
	GSList *previous;
} Search;

static void
path_search_free(Search *search)
{
	VIPS_FREEF(g_free, search->basename);
	VIPS_FREEF(g_free, search->dirname);
	g_slist_free_full(g_steal_pointer(&search->previous), g_free);
	VIPS_FREEF(g_pattern_spec_free, search->wild);
}

static gboolean
path_search_init(Search *search, const char *patt, path_map_fn fn, void *a)
{
	search->basename = g_path_get_basename(patt);
	search->dirname = g_path_get_dirname(patt);
	search->wild = NULL;
	search->fn = fn;
	search->a = a;
	search->previous = NULL;

	if (!(search->wild = g_pattern_spec_new(search->basename))) {
		path_search_free(search);
		return FALSE;
	}

	return TRUE;
}

static void *
path_str_eq(const char *s1, const char *s2)
{
	return strcmp(s1, s2) == 0 ? (void *) s1 : NULL;
}

/* Test for string matches pattern. If the match is successful, call a user
 * function.
 */
static void *
path_search_match(Search *search, const char *dir_name, const char *name)
{
	if (g_pattern_match_string(search->wild, name) &&
		!slist_map(search->previous,
			(SListMapFn) path_str_eq, (gpointer) name)) {
		char buf[VIPS_PATH_MAX + 10];
		void *result;

		/* Add to exclusion list.
		 */
		search->previous = g_slist_prepend(search->previous, g_strdup(name));

		g_snprintf(buf, VIPS_PATH_MAX, "%s/%s", dir_name, name);

		path_compact(buf);

#ifdef DEBUG_SEARCH
		printf("path_search_match: matched \"%s\"\n", buf);
#endif /*DEBUG_SEARCH*/

		if ((result = search->fn(buf, search->a, NULL, NULL)))
			return result;
	}

	return NULL;
}

/* Scan a directory, calling a function for every entry. Abort scan if
 * function returns non-NULL.
 */
static void *
path_scan_dir(const char *dir_name, Search *search)
{
	char buf[VIPS_PATH_MAX];
	const char *name;
	void *result;

	/* Add the pattern offset, if any. It's '.' for no offset.
	 */
	g_snprintf(buf, VIPS_PATH_MAX, "%s/%s", dir_name, search->dirname);

	g_autoptr(GDir) dir = (GDir *) callv_string_filename(
		(callv_string_fn) g_dir_open, buf, NULL, NULL, NULL);
	if (!dir)
		return NULL;

	while ((name = g_dir_read_name(dir)))
		if ((result = path_search_match(search, buf, name)))
			return result;

	return NULL;
}

/* Scan a search path, applying a function to every file name which matches a
 * pattern. If the user function returns NULL, keep looking, otherwise return
 * its result. We return NULL on error, or if the user function returns NULL
 * for all filenames which match.
 *
 * Remove duplicates: if fred.wombat is in the first and second dirs on the
 * path, only apply to the first occurence.

	FIXME ... speed up with a hash and a (date based) cache at some point

 */
void *
path_map(GSList *path, const char *patt, path_map_fn fn, void *a)
{
	Search search;
	void *result;

#ifdef DEBUG_SEARCH
	printf("path_map: searching for \"%s\"\n", patt);
#endif /*DEBUG_SEARCH*/

	if (!path_search_init(&search, patt, fn, a))
		return NULL;

	result = slist_map(path, (SListMapFn) path_scan_dir, &search);

	path_search_free(&search);

	return result;
}

/* As above, but scan a single directory.
 */
void *
path_map_dir(const char *dir, const char *patt, path_map_fn fn, void *a)
{
	Search search;
	void *result;

#ifdef DEBUG_SEARCH
	printf("path_map_dir: searching for \"%s\"\n", patt);
#endif /*DEBUG_SEARCH*/

	if (!path_search_init(&search, patt, fn, a))
		return NULL;

	if (!(result = path_scan_dir(dir, &search))) {
		/* Not found? Maybe - error message anyway.
		 */
		error_top(_("Not found"));
		error_sub(_("file \"%s\" not found"), patt);
	}

	path_search_free(&search);

	return result;
}

/* Search for a file on the search path.
 */
char *
path_find_file(const char *filename)
{
	char *fname;

#ifdef DEBUG_SEARCH
	printf("path_find_file: \"%s\"\n", filename);
#endif /*DEBUG_SEARCH*/

	/* Try file name exactly.
	 */
	if (existsf("%s", filename))
		return g_strdup(filename);

	/* Search everywhere.
	 */
	if ((fname = path_map(PATH_SEARCH, filename,
			 (path_map_fn) g_strdup, NULL)))
		return fname;

	error_top(_("Not found"));
	error_sub(_("file \"%s\" not found on path"), filename);

	return NULL;
}

void
path_init(void)
{
	char buf[VIPS_PATH_MAX];

	path_rewrite_add(get_prefix(), "$VIPSHOME", TRUE);
	path_rewrite_add(g_get_home_dir(), "$HOME", TRUE);
	path_rewrite_add(get_savedir(), "$SAVEDIR", TRUE);

	/* You might think we could add a rule to swap '.' for
	 * g_get_current_dir(), but that would then make workspaces depend on
	 * a certain value of cwd before they could work.
	 */

	/* And the expanded form too.
	 */
	expand_variables(get_savedir(), buf);
	path_rewrite_add(buf, "$SAVEDIR", TRUE);

#ifdef DEBUG_LOCAL
	printf("path_init: loading start from \".\" only\n");
	path_start_default = path_parse(".");
	path_search_default = path_parse(".");
	path_tmp_default = g_strdup(".");
#else  /*!DEBUG_LOCAL*/
	g_snprintf(buf, VIPS_PATH_MAX,
		"%s/start" G_SEARCHPATH_SEPARATOR_S
		"$VIPSHOME/share/$PACKAGE/start",
		get_savedir());
	path_start_default = path_parse(buf);

	g_snprintf(buf, VIPS_PATH_MAX,
		"%s/data" G_SEARCHPATH_SEPARATOR_S
		"$VIPSHOME/share/$PACKAGE/data" G_SEARCHPATH_SEPARATOR_S
		".",
		get_savedir());
	path_search_default = path_parse(buf);

	g_snprintf(buf, VIPS_PATH_MAX, "%s/tmp", get_savedir());
	path_tmp_default = g_strdup(buf);
#endif /*DEBUG_LOCAL*/

#ifdef DEBUG_SEARCH
	printf("path_start_default = ");
	path_print(path_start_default);
	printf("\n");

	printf("path_search_default = ");
	path_print(path_search_default);
	printf("\n");

	printf("path_tmp_default = %s\n", path_tmp_default);
#endif /*DEBUG_SEARCH*/

	// make libvips save temps to ~/.nip4-xxx/tmp etc.
	expand_variables(path_tmp_default, buf);
	g_setenv("TMPDIR", buf, TRUE);
}
