// some basic util functions

/*

	Copyright (C) 1991-2003 The National Gallery
	Copyright (C) 2004-2023 libvips.org

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

/*
#define DEBUG
 */

/* Handy for tracing errors.
#define DEBUG_ERROR
 */

#include "nip4.h"

/* Track errors messages in this thing. We keep two messages: a principal
 * error, and extra informative text. For example "Unable to load file.",
 * "Read failed for file blah, permission denied.".
 */
static char error_top_text[MAX_STRSIZE];
static char error_sub_text[MAX_STRSIZE];

VipsBuf error_top_buf = VIPS_BUF_STATIC(error_top_text);
VipsBuf error_sub_buf = VIPS_BUF_STATIC(error_sub_text);

/* Useful: Error message and quit. Emergencies only ... we don't tidy up
 * properly.
 */
void
error(const char *fmt, ...)
{
	va_list args;

	fprintf(stderr, PACKAGE ": ");

	va_start(args, fmt);
	(void) vfprintf(stderr, fmt, args);
	va_end(args);

	fprintf(stderr, "\n");

#ifdef DEBUG
	/* Make a coredump.
	 */
	abort();
#endif /*DEBUG*/

	exit(1);
}

/* Set this to block error messages. Useful if we've found an error, we want
 * to clean up, but we don't want any of the clean-up code to touch the error
 * buffer.
 */
static int error_level = 0;

void
error_block(void)
{
	error_level++;
}

void
error_unblock(void)
{
	g_assert(error_level);

	error_level--;
}

/* Set an error buffer.
 */
static void
error_set(VipsBuf *buf, const char *fmt, va_list ap)
{
	if (!error_level) {
		char txt[MAX_STRSIZE];
		VipsBuf tmp = VIPS_BUF_STATIC(txt);

		/* The string we write may contain itself ... write to an
		 * intermediate, then copy to main.
		 */
		vips_buf_vappendf(&tmp, fmt, ap);

		vips_buf_rewind(buf);
		(void) vips_buf_appends(buf, vips_buf_all(&tmp));

#ifdef DEBUG_ERROR
		printf("error: %p %s\n", buf, vips_buf_all(buf));
#endif /*DEBUG_ERROR*/
	}
}

void
error_clear_nip(void)
{
	if (!error_level) {
		vips_buf_rewind(&error_top_buf);
		vips_buf_rewind(&error_sub_buf);

#ifdef DEBUG_ERROR
		printf("error_clear_nip\n");
#endif /*DEBUG_ERROR*/
	}
}

void
error_clear(void)
{
	if (!error_level) {
		error_clear_nip();
		vips_error_clear();
	}
}

void
error_top(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	error_set(&error_top_buf, fmt, ap);
	va_end(ap);

	/* We could use error_clear_nip() before calling error_set(), but that
	 * fails if the arg to us uses the contents of either error buffer.
	 */
	if (!error_level)
		vips_buf_rewind(&error_sub_buf);
}

void
error_sub(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	error_set(&error_sub_buf, fmt, ap);
	va_end(ap);
}

/* Append any VIPS errors to sub buffer.
 */
void
error_vips(void)
{
	if (!error_level && strlen(vips_error_buffer()) > 0) {
		if (!vips_buf_is_empty(&error_sub_buf))
			vips_buf_appendf(&error_sub_buf, "\n");
		vips_buf_appendf(&error_sub_buf, "%s", vips_error_buffer());
		vips_error_clear();
	}
}

void
error_vips_all(void)
{
	error_top(_("VIPS library error"));
	error_vips();
}

const char *
error_get_top(void)
{
	return vips_buf_all(&error_top_buf);
}

const char *
error_get_sub(void)
{
	return vips_buf_all(&error_sub_buf);
}

void
error_alert(GtkWindow *window)
{
	if (!window) {
		// just bail out in batch mode
		printf("Error: %s\n", error_get_top());
		printf("\t%s\n", error_get_sub());
		vips_error_exit(NULL);
	}

	GtkAlertDialog *alert;

	alert = gtk_alert_dialog_new("alert");
	gtk_alert_dialog_set_message(alert, error_get_top());
	gtk_alert_dialog_set_detail(alert, error_get_sub());
	error_clear();
	gtk_alert_dialog_set_modal(alert, TRUE);
	gtk_alert_dialog_show(alert, window);
}

/* Set an xml property printf() style.
 */
gboolean
set_prop(xmlNode *xnode, const char *name, const char *fmt, ...)
{
	va_list ap;
	char value[MAX_STRSIZE];

	va_start(ap, fmt);
	(void) g_vsnprintf(value, MAX_STRSIZE, fmt, ap);
	va_end(ap);

	if (!xmlSetProp(xnode, (xmlChar *) name, (xmlChar *) value)) {
		error_top(_("Unable to set XML property"));
		error_sub(_("unable to set property \"%s\" to value \"%s\""),
			name, value);
		return FALSE;
	}

	return TRUE;
}

/* Set an xml property from an optionally NULL string.
 */
gboolean
set_sprop(xmlNode *xnode, const char *name, const char *value)
{
	if (value && !set_prop(xnode, name, "%s", value))
		return FALSE;

	return TRUE;
}

/* Set an xml property from an optionally NULL string.
 */
gboolean
set_iprop(xmlNode *xnode, const char *name, int value)
{
	return set_prop(xnode, name, "%d", value);
}

/* Save a list of strings. For name=="fred" and n strings in list, save as
 * "fredn" == n, "fred0" == list[0], etc.
 */
gboolean
set_slprop(xmlNode *xnode, const char *name, GSList *labels)
{
	if (labels) {
		char buf[256];
		int i;

		(void) g_snprintf(buf, 256, "%sn", name);
		if (!set_prop(xnode, buf, "%d", g_slist_length(labels)))
			return FALSE;

		for (i = 0; labels; i++, labels = labels->next) {
			const char *label = (const char *) labels->data;

			(void) g_snprintf(buf, 256, "%s%d", name, i);
			if (!set_sprop(xnode, buf, label))
				return FALSE;
		}
	}

	return TRUE;
}

/* Set a double ... use non-localisable conversion, rather than %g.
 */
gboolean
set_dprop(xmlNode *xnode, const char *name, double value)
{
	char buf[G_ASCII_DTOSTR_BUF_SIZE];

	g_ascii_dtostr(buf, sizeof(buf), value);

	return set_sprop(xnode, name, buf);
}

/* Save an array of double. For name=="fred" and n doubles in array, save as
 * "fredn" == n, "fred0" == array[0], etc.
 */
gboolean
set_dlprop(xmlNode *xnode, const char *name, double *values, int n)
{
	char buf[256];
	int i;

	(void) g_snprintf(buf, 256, "%sn", name);
	if (!set_prop(xnode, buf, "%d", n))
		return FALSE;

	for (i = 0; i < n; i++) {
		(void) g_snprintf(buf, 256, "%s%d", name, i);
		if (!set_dprop(xnode, buf, values[i]))
			return FALSE;
	}

	return TRUE;
}

gboolean
get_sprop(xmlNode *xnode, const char *name, char *buf, int sz)
{
	char *value = (char *) xmlGetProp(xnode, (xmlChar *) name);

	if (!value)
		return FALSE;

	g_strlcpy(buf, value, sz);
	VIPS_FREEF(xmlFree, value);

	return TRUE;
}

gboolean
get_spropb(xmlNode *xnode, const char *name, VipsBuf *buf)
{
	char *value = (char *) xmlGetProp(xnode, (xmlChar *) name);

	if (!value)
		return FALSE;

	vips_buf_appends(buf, value);
	VIPS_FREEF(xmlFree, value);

	return TRUE;
}

gboolean
get_iprop(xmlNode *xnode, const char *name, int *out)
{
	char buf[256];

	if (!get_sprop(xnode, name, buf, 256))
		return FALSE;

	*out = atoi(buf);

	return TRUE;
}

gboolean
get_dprop(xmlNode *xnode, const char *name, double *out)
{
	char buf[256];

	if (!get_sprop(xnode, name, buf, 256))
		return FALSE;

	*out = g_ascii_strtod(buf, NULL);

	return TRUE;
}

gboolean
get_bprop(xmlNode *xnode, const char *name, gboolean *out)
{
	char buf[256];

	if (!get_sprop(xnode, name, buf, 256))
		return FALSE;

	*out = g_ascii_strcasecmp(buf, "true") == 0;

	return TRUE;
}

/* Load a list of strings. For name=="fred", look for "fredn" == number of
 * strings to load, "fred0" == list[0], etc.
 */
gboolean
get_slprop(xmlNode *xnode, const char *name, GSList **out)
{
	char buf[256];
	int n, i;

	(void) g_snprintf(buf, 256, "%sn", name);
	if (!get_iprop(xnode, buf, &n))
		return FALSE;

	*out = NULL;
	for (i = n - 1; i >= 0; i--) {
		(void) g_snprintf(buf, 256, "%s%d", name, i);
		if (!get_sprop(xnode, buf, buf, 256))
			return FALSE;

		*out = g_slist_prepend(*out, g_strdup(buf));
	}

	return TRUE;
}

/* Load an array of double. For name=="fred", look for "fredn" == length of
 * array, "fred0" == array[0], etc.
 */
gboolean
get_dlprop(xmlNode *xnode, const char *name, double **out)
{
	char buf[256];
	int n, i;

	(void) g_snprintf(buf, 256, "%sn", name);
	if (!get_iprop(xnode, buf, &n))
		return FALSE;

	if (!(*out = IARRAY(NULL, n, double)))
		return FALSE;

	for (i = 0; i < n; i++) {
		(void) g_snprintf(buf, 256, "%s%d", name, i);
		if (!get_dprop(xnode, buf, *out + i))
			return FALSE;
	}

	return TRUE;
}

/* Find the first child node with a name.
 */
xmlNode *
get_node(xmlNode *base, const char *name)
{
	xmlNode *i;

	for (i = base->children; i; i = i->next)
		if (strcmp((char *) i->name, name) == 0)
			return i;

	return NULL;
}

static int rect_n_rects = 0;

/* Allocate and free rects.
 */
VipsRect *
rect_dup(VipsRect *init)
{
	VipsRect *new_rect;

	if (!(new_rect = INEW(NULL, VipsRect)))
		return NULL;

	*new_rect = *init;

	rect_n_rects += 1;

	return new_rect;
}

void *
rect_free(VipsRect *rect)
{
	g_free(rect);
	rect_n_rects -= 1;

	return NULL;
}

/* Test two lists for eqality.
 */
gboolean
slist_equal(GSList *l1, GSList *l2)
{
	while (l1 && l2) {
		if (l1->data != l2->data)
			return FALSE;

		l1 = l1->next;
		l2 = l2->next;
	}

	if (l1 || l2)
		return FALSE;

	return TRUE;
}

/* Map over an slist.
 */
void *
slist_map(GSList *list, SListMapFn fn, gpointer a)
{
	void *result;

	g_autoptr(GSList) copy = g_slist_copy(list);
	result = NULL;
	for (GSList *p = copy; p && !(result = fn(p->data, a)); p = p->next)
		;

	return result;
}

void *
slist_map2(GSList *list, SListMap2Fn fn, gpointer a, gpointer b)
{
	GSList *i;
	void *result;

	g_autoptr(GSList) copy = g_slist_copy(list);
	result = NULL;
	for (i = copy; i && !(result = fn(i->data, a, b)); i = i->next)
		;

	return result;
}

void *
slist_map3(GSList *list, SListMap3Fn fn, gpointer a, gpointer b, gpointer c)
{
	GSList *i;
	void *result;

	g_autoptr(GSList) copy = g_slist_copy(list);
	result = NULL;
	for (i = copy; i && !(result = fn(i->data, a, b, c)); i = i->next)
		;

	return result;
}

void *
slist_map4(GSList *list, SListMap4Fn fn,
	gpointer a, gpointer b, gpointer c, gpointer d)
{
	GSList *i;
	void *result;

	g_autoptr(GSList) copy = g_slist_copy(list);
	result = NULL;
	for (i = copy; i && !(result = fn(i->data, a, b, c, d)); i = i->next)
		;

	return result;
}

void *
slist_map5(GSList *list, SListMap5Fn fn,
	gpointer a, gpointer b, gpointer c, gpointer d, gpointer e)
{
	GSList *i;
	void *result;

	g_autoptr(GSList) copy = g_slist_copy(list);
	result = NULL;
	for (i = copy; i && !(result = fn(i->data, a, b, c, d, e)); i = i->next)
		;

	return result;
}

/* Map backwards.
 */
void *
slist_map_rev(GSList *list, SListMapFn fn, gpointer a)
{
	GSList *i;
	void *result;

	g_autoptr(GSList) copy = g_slist_copy(list);
	copy = g_slist_reverse(copy);
	result = NULL;
	for (i = copy; i && !(result = fn(i->data, a)); i = i->next)
		;

	return result;
}

void *
slist_map2_rev(GSList *list, SListMap2Fn fn, gpointer a, gpointer b)
{
	GSList *i;
	void *result;

	g_autoptr(GSList) copy = g_slist_copy(list);
	copy = g_slist_reverse(copy);
	result = NULL;
	for (i = copy; i && !(result = fn(i->data, a, b)); i = i->next)
		;

	return result;
}

void *
slist_map3_rev(GSList *list, SListMap3Fn fn, void *a, void *b, void *c)
{
	GSList *i;
	void *result;

	g_autoptr(GSList) copy = g_slist_copy(list);
	copy = g_slist_reverse(copy);
	result = NULL;
	for (i = copy; i && !(result = fn(i->data, a, b, c)); i = i->next)
		;

	return result;
}

void *
map_equal(void *a, void *b)
{
	if (a == b)
		return a;

	return NULL;
}

void *
slist_fold(GSList *list, void *start, SListFoldFn fn, void *a)
{
	void *c;
	GSList *ths, *next;

	for (c = start, ths = list; ths; ths = next) {
		next = ths->next;

		if (!(c = fn(ths->data, c, a)))
			return NULL;
	}

	return c;
}

void *
slist_fold2(GSList *list, void *start, SListFold2Fn fn, void *a, void *b)
{
	void *c;
	GSList *ths, *next;

	for (c = start, ths = list; ths; ths = next) {
		next = ths->next;

		if (!(c = fn(ths->data, c, a, b)))
			return NULL;
	}

	return c;
}

/* Remove all occurences of an item from a list.
 */
GSList *
slist_remove_all(GSList *list, gpointer data)
{
	GSList *tmp;
	GSList *prev;

	prev = NULL;
	tmp = list;

	while (tmp) {
		if (tmp->data == data) {
			GSList *next = tmp->next;

			if (prev)
				prev->next = next;
			if (list == tmp)
				list = next;

			tmp->next = NULL;
			g_slist_free(tmp);
			tmp = next;
		}
		else {
			prev = tmp;
			tmp = tmp->next;
		}
	}

	return list;
}

Queue *
queue_new(void)
{
	Queue *q = INEW(NULL, Queue);

	q->list = NULL;
	q->tail = NULL;
	q->length = 0;

	return q;
}

void *
queue_head(Queue *q)
{
	void *data;

	g_assert(q);

	if (!q->list)
		return NULL;

	data = q->list->data;

	q->list = g_slist_remove(q->list, data);
	if (!q->list)
		q->tail = NULL;

	q->length -= 1;

	return data;
}

void
queue_add(Queue *q, void *data)
{
	if (!q->tail) {
		g_assert(!q->list);

		q->list = q->tail = g_slist_append(q->list, data);
	}
	else {
		g_assert(q->list);

		q->tail = g_slist_append(q->tail, data);
		q->tail = q->tail->next;
	}

	q->length += 1;
}

/* Not very fast! But used infrequently. TRUE if the data was in the queue and
 * has now been removed.
 */
gboolean
queue_remove(Queue *q, void *data)
{
	GSList *ele;

	if (!(ele = g_slist_find(q->list, data)))
		return FALSE;

	q->list = g_slist_remove(q->list, data);

	if (ele == q->tail)
		q->tail = g_slist_last(q->list);

	q->length -= 1;

	return TRUE;
}

int
queue_length(Queue *q)
{
	return q->length;
}

/* Make an info string about an image.
 */
void
vips_buf_appendi(VipsBuf *buf, VipsImage *im)
{
	if (!im) {
		vips_buf_appends(buf, _("(no image)"));
		return;
	}

	/* Coded? Special warning.
	 */
	if (im->Coding != VIPS_CODING_NONE)
		vips_buf_appendf(buf, "%s, ",
			vips_enum_nick(VIPS_TYPE_CODING, im->Coding));

	/* Format string expands to (eg.)
	 * "2000x3000 128-bit complex, 3 bands, Lab"
	 */
	vips_buf_appendf(buf,
		ngettext("%dx%d %s, %d band, %s",
			"%dx%d %s, %d bands, %s", im->Bands),
		im->Xsize, im->Ysize,
		vips_enum_nick(VIPS_TYPE_BAND_FORMAT, im->BandFmt),
		im->Bands,
		vips_enum_nick(VIPS_TYPE_INTERPRETATION, im->Type));
}

/* Append a string, escaping C stuff. Escape double quotes if quote is set.
 */
gboolean
vips_buf_appendsc(VipsBuf *buf, gboolean quote, const char *str)
{
	char buffer[VIPS_PATH_MAX];
	char buffer2[VIPS_PATH_MAX];

	/* /2 to leave a bit of space.
	 */
	g_strlcpy(buffer, str, VIPS_PATH_MAX / 2);

	/* FIXME ... possible buffer overflow :-(
	 */
	my_strecpy(buffer2, buffer, quote);

	return vips_buf_appends(buf, buffer2);
}

/* Test for string a ends string b.
 */
gboolean
is_postfix(const char *a, const char *b)
{
	int n = strlen(a);
	int m = strlen(b);

	if (m < n)
		return FALSE;

	return !strcmp(a, b + m - n);
}

/* Test for string a ends string b, case independent.
 */
gboolean
is_casepostfix(const char *a, const char *b)
{
	int n = strlen(a);
	int m = strlen(b);

	if (m < n)
		return FALSE;

	return !g_ascii_strcasecmp(a, b + m - n);
}

/* Test for string a starts string b.
 */
gboolean
is_prefix(const char *a, const char *b)
{
	int n = strlen(a);
	int m = strlen(b);
	int i;

	if (m < n)
		return FALSE;
	for (i = 0; i < n; i++)
		if (a[i] != b[i])
			return FALSE;

	return TRUE;
}

/* Test for string a starts string b ... case insensitive.
 */
gboolean
is_caseprefix(const char *a, const char *b)
{
	int n = strlen(a);
	int m = strlen(b);
	int i;

	if (m < n)
		return FALSE;
	for (i = 0; i < n; i++)
		if (toupper(a[i]) != toupper(b[i]))
			return FALSE;

	return TRUE;
}

/* Like strstr(), but case-insensitive.
 */
char *
my_strcasestr(const char *haystack, const char *needle)
{
	int i;
	int hlen = strlen(haystack);
	int nlen = strlen(needle);

	for (i = 0; i <= hlen - nlen; i++)
		if (is_caseprefix(needle, haystack + i))
			return (char *) (haystack + i);

	return NULL;
}

/* Copy a string, interpreting (a few) C-language escape codes.

	FIXME ... yuk! dangerous and not that useful, needs a proper function
	to do this

 */
char *
my_strccpy(char *output, const char *input)
{
	const char *p;
	char *q;

	for (p = input, q = output; *p; p++, q++)
		if (p[0] == '\\') {
			switch (p[1]) {
			case 'n':
				q[0] = '\n';
				break;

			case 't':
				q[0] = '\t';
				break;

			case 'r':
				q[0] = '\r';
				break;

			case '"':
				q[0] = '\"';
				break;

			case '\'':
				q[0] = '\'';
				break;

			case '\\':
				q[0] = '\\';
				break;

			default:
				/* Leave uninterpreted.
				 */
				q[0] = p[0];
				q[1] = p[1];
				q++;
				break;
			}

			p++;
		}
		else
			q[0] = p[0];
	q[0] = '\0';

	return output;
}

/* Copy a string, expanding escape characters into C-language escape codes.
 * Escape double quotes if quote is set.
 */
char *
my_strecpy(char *output, const char *input, gboolean quote)
{
	const char *p;
	char *q;

	for (p = input, q = output; *p; p++, q++)
		switch (p[0]) {
		case '\n':
			q[0] = '\\';
			q[1] = 'n';
			q++;
			break;

		case '\t':
			q[0] = '\\';
			q[1] = 't';
			q++;
			break;

		case '\r':
			q[0] = '\\';
			q[1] = 'r';
			q++;
			break;

		case '\"':
			if (quote) {
				q[0] = '\\';
				q[1] = '\"';
				q++;
			}
			else
				q[0] = p[0];
			break;

		case '\'':
			q[0] = '\\';
			q[1] = '\'';
			q++;
			break;

		case '\\':
			q[0] = '\\';
			q[1] = '\\';
			q++;
			break;

		default:
			q[0] = p[0];
			break;
		}
	q[0] = '\0';

	return output;
}

/* Is a character in a string?
 */
static gboolean
instr(char c, const char *spn)
{
	const char *p;

	for (p = spn; *p; p++)
		if (*p == c)
			return TRUE;

	return FALSE;
}

/* Doh ... not everyone has strrspn(), define one. Return a pointer to the
 * start of the trailing segment of p which contains only chars in spn.
 */
const char *
my_strrspn(const char *p, const char *spn)
{
	const char *p1;

	for (p1 = p + strlen(p) - 1; p1 >= p && instr(*p1, spn); p1--)
		;
	p1++;

	return p1;
}

/* Find a pointer to the start of the trailing segment of p which contains
 * only chars not in spn.
 */
const char *
my_strrcspn(const char *p, const char *spn)
{
	const char *p1;

	for (p1 = p + strlen(p) - 1; p1 >= p && !instr(*p1, spn); p1--)
		;
	p1++;

	return p1;
}

/* Find the rightmost occurence of string a in string b.
 */
const char *
findrightmost(const char *a, const char *b)
{
	int la = strlen(a);
	int lb = strlen(b);
	int i;

	if (lb < la)
		return NULL;

	for (i = lb - la; i > 0; i--)
		if (strncmp(a, &b[i], la) == 0)
			return &b[i];

	return NULL;
}

/* Useful transformation: strip off a set of suffixes (eg. ".v", ".icon",
 * ".hr"), add a single new suffix (eg. ".hr.v").
 */
void
change_suffix(const char *name, char *out,
	const char *new, const char **olds, int nolds)
{
	char *p;
	int i;

	/* Copy start string.
	 */
	strcpy(out, name);

	/* Drop all matching suffixes.
	 */
	while ((p = strrchr(out, '.'))) {
		/* Found suffix - test against list of alternatives. Ignore
		 * case.
		 */
		for (i = 0; i < nolds; i++)
			if (g_ascii_strcasecmp(p, olds[i]) == 0) {
				*p = '\0';
				break;
			}

		/* Found match? If not, break from loop.
		 */
		if (*p)
			break;
	}

	/* Add new suffix.
	 */
	strcat(out, new);
}

/* Drop leading and trim trailing non-alphanum characters. NULL if nothing
 * left. The result can be a variable name.
 */
char *
trim_nonalpha(char *text)
{
	char *p, *q;

	/* Skip any initial non-alpha characters.
	 */
	for (q = text; *q && !isalpha((int) (*q)); q++)
		;

	/* Find next non-alphanumeric character.
	 */
	for (p = q; *p && isalnum((int) (*p)); p++)
		;
	*p = '\0';

	return strlen(q) == 0 ? NULL : q;
}

/* Drop leading and trim trailing whitespace characters. NULL if nothing
 * left.
 */
char *
trim_white(char *text)
{
	char *p, *q;

	/* Skip any initial whitespace characters.
	 */
	for (q = text; *q && isspace((int) (*q)); q++)
		;

	/* Find rightmost non-space char.
	 */
	for (p = q + strlen(q) - 1; p > q && isspace((int) (*p)); p--)
		;
	p[1] = '\0';

	return strlen(q) == 0 ? NULL : q;
}

/* Get a pointer to a band element in a region.
 */
void *
get_element(VipsRegion *ireg, int x, int y, int b)
{
	/* Return a pointer to this on error.
	 */
	static VipsPel empty[50] = { 0 };

	VipsImage *im = ireg->im;
	int es = VIPS_IMAGE_SIZEOF_ELEMENT(im);

	/* Make sure we can read from this descriptor.
	 */
	if (vips_image_pio_input(im))
		return empty;

	VipsRect iarea;
	iarea.left = x;
	iarea.top = y;
	iarea.width = 1;
	iarea.height = 1;
	if (vips_region_prepare(ireg, &iarea))
		return empty;

	return VIPS_REGION_ADDR(ireg, x, y) + b * es;
}

/* Make an info string about a file.
 */
void
get_image_info(VipsBuf *buf, const char *name)
{
	char name2[VIPS_PATH_MAX];
	struct stat st;

	expand_variables(name, name2);
	char *basename = g_filename_display_basename(name2);
	vips_buf_appendf(buf, "%s, ", basename);
	g_free(basename);

	/* Read size and file/dir.
	 */
	if (stat(name2, &st) == -1) {
		vips_buf_appendf(buf, "%s", g_strerror(errno));
		return;
	}

	if (S_ISDIR(st.st_mode))
		vips_buf_appends(buf, _("directory"));
	else if (S_ISREG(st.st_mode)) {
		VipsImage *im;

		/* Spot workspace files from the filename. These are XML files
		 * and if imagemagick sees them it'll try to load them as SVG
		 * or somethiing awful like that.
		 */
		if (vips_iscasepostfix(name2, ".ws"))
			vips_buf_appends(buf, _("workspace"));
		else if ((im = vips_image_new_from_file(name2, NULL))) {
			vips_buf_appendi(buf, im);
			VIPS_UNREF(im);
		}
		else
			/* No idea wtf this is, just put the size in.
			 */
			vips_buf_append_size(buf, st.st_size);
	}
}

/* A char that can be part of an environment variable name? A-Za-z0-9_
 */
static gboolean
isvariable(int ch)
{
	return isalnum(ch) || ch == '_';
}

/* Expand environment variables from in to out. Return true if we performed an
 * expansion, false for no variables there.
 */
static gboolean
expand_once(char *in, char *out)
{
	char *p, *q;
	gboolean have_substituted;

	/* Scan and copy.
	 */
	have_substituted = FALSE;
	for (p = in, q = out; (*q = *p) && (q - out) < VIPS_PATH_MAX; p++, q++)
		/* Did we just copy a '$'?
		 */
		if (*p == '$') {
			char vname[VIPS_PATH_MAX];
			char *r;
			const char *subst;
			const char *s;

			/* Extract the variable name.
			 */
			p++;
			for (r = vname;
				 isvariable((int) (*r = *p)) &&
				 (r - vname) < VIPS_PATH_MAX;
				 p++, r++)
				;
			*r = '\0';
			p--;

			/* Look up variable.
			 */
			subst = g_getenv(vname);

			/* Copy variable contents.
			 */
			if (subst) {
				for (s = subst;
					 (*q = *s) && (q - out) < VIPS_PATH_MAX;
					 s++, q++)
					;
			}
			q--;

			/* Remember we have performed a substitution.
			 */
			have_substituted = TRUE;
		}
		/* Or a '~' at the start of the string?
		 */
		else if (*p == '~' && p == in) {
			const char *subst = g_getenv("HOME");
			const char *r;

			/* Copy variable contents.
			 */
			if (subst) {
				for (r = subst;
					 (*q = *r) && (q - out) < VIPS_PATH_MAX;
					 r++, q++)
					;
			}
			q--;

			/* Remember we have performed a substitution.
			 */
			have_substituted = TRUE;
		}

	return have_substituted;
}

/* Expand all variables! Don't touch in, assume out[] is at least
 * VIPS_PATH_MAX bytes. in and out must not point to the same place!
 */
void
expand_variables(const char *in, char *out)
{
	char buf[VIPS_PATH_MAX];
	char *p1 = (char *) in; /* Discard const, but safe */
	char *p2 = out;

	g_assert(in != out);

	/* Expand any environment variables in component.
	 */
	while (expand_once(p1, p2))
		/* We have expanded --- swap the buffers and try
		 * again.
		 */
		if (p2 == out) {
			p1 = out;
			p2 = buf;
		}
		else {
			p1 = buf;
			p2 = out;
		}
}

static void
swap_chars(char *buf, char from, char to)
{
	int i;

	for (i = 0; buf[i]; i++)
		if (buf[i] == from)
			buf[i] = to;
}

/* If we use '/' seps, swap all '\' for '/' ... likewise vice versa. Only in
 * the filename part, though. We don't want to junk '\,', for example.
 */
void
nativeize_path(char *buf)
{
	char filename[VIPS_PATH_MAX];
	char mode[VIPS_PATH_MAX];

	vips__filename_split8(buf, filename, mode);

	if (G_DIR_SEPARATOR == '/')
		swap_chars(filename, '\\', '/');
	else
		swap_chars(filename, '/', '\\');

	if (strcmp(mode, "") != 0)
		g_snprintf(buf, VIPS_PATH_MAX, "%s[%s]", filename, mode);
	else
		g_snprintf(buf, VIPS_PATH_MAX, "%s", filename);
}

/* Change all occurences of "from" into "to". This will loop if "to" contains
 * "from", beware.
 */
static void
swap_string(char *buffer, const char *from, const char *to)
{
	char *p;

	while ((p = strstr(buffer, from))) {
		int off = p - buffer;
		char buf2[VIPS_PATH_MAX];

		g_strlcpy(buf2, buffer, VIPS_PATH_MAX);
		buf2[off] = '\0';
		g_snprintf(buffer, VIPS_PATH_MAX,
			"%s%s%s", buf2, to, buf2 + off + strlen(from));
	}
}

/* Remove "." and ".." from an absolute path (if we can).
 */
void
canonicalize_path(char *path)
{
	gboolean found;

	g_assert(is_absolute(path));

	/* Any "/./" can go.
	 */
	swap_string(path, "/./", "/");

	/* Any "//" can go.
	 */
	swap_string(path, "//", "/");

	/* Repeatedly search for "/[^/]+/../" and remove it.

		FIXME ... yuk, should search backwards from the end of the
		string

	 */
	do {
		found = FALSE;

		for (char *p = path; (p = strchr(p, '/')); p++) {
			char *q;

			q = strchr(p + 1, '/');

			if (q && is_prefix("/..", q)) {
				memmove(p, q + 3, strlen(q + 3) + 1);
				found = TRUE;
				break;
			}
		}
	} while (found);

	/* We may have the empty string ... that's just '/'.
	 */
	if (strcmp(path, "") == 0)
		strcpy(path, "/");
}

/* Absoluteize a path. Must be VIPS_PATH_MAX chars available.
 */
void
absoluteize_path(char *path)
{
	if (!is_absolute(path)) {
		char buf[VIPS_PATH_MAX];

		g_strlcpy(buf, path, VIPS_PATH_MAX);
		g_autofree char *cwd = g_get_current_dir();
		g_snprintf(path, VIPS_PATH_MAX, "%s%s%s", cwd, "/", buf);
		canonicalize_path(path);
	}
}

/* Call a void*-valued function, building a string arg. We expand env.
 * variables, but that's all.
 */
void *
callv_string(callv_string_fn fn, const char *arg, void *a, void *b, void *c)
{
	char buf[VIPS_PATH_MAX];

	expand_variables(arg, buf);

	return fn(buf, a, b, c);
}

void *
callv_stringva(callv_string_fn fn,
	const char *fmt, va_list ap, void *a, void *b, void *c)
{
	char buf[VIPS_PATH_MAX];

	(void) g_vsnprintf(buf, VIPS_PATH_MAX, fmt, ap);

	return callv_string(fn, buf, a, b, c);
}

void *
callv_stringf(callv_string_fn fn, const char *fmt, ...)
{
	va_list ap;
	void *res;

	va_start(ap, fmt);
	res = callv_stringva(fn, fmt, ap, NULL, NULL, NULL);
	va_end(ap);

	return res;
}

/* Call a function, building a filename arg. Nativize and absoluteize too.
 */
void *
callv_string_filename(callv_string_fn fn,
	const char *filename, void *a, void *b, void *c)
{
	char buf[VIPS_PATH_MAX];

	expand_variables(filename, buf);
	nativeize_path(buf);
	absoluteize_path(buf);

	return fn(buf, a, b, c);
}

void *
callv_string_filenameva(callv_string_fn fn,
	const char *fmt, va_list ap, void *a, void *b, void *c)
{
	char buf[VIPS_PATH_MAX];

	g_vsnprintf(buf, VIPS_PATH_MAX, fmt, ap);

	return callv_string_filename(fn, buf, a, b, c);
}

void *
callv_string_filenamef(callv_string_fn fn, const char *fmt, ...)
{
	va_list ap;
	void *res;

	va_start(ap, fmt);
	res = callv_string_filenameva(fn, fmt, ap, NULL, NULL, NULL);
	va_end(ap);

	return res;
}

/* Call an int-valued function, building a string arg. We expand env.
 * variables, but that's all.
 */
int
calli_string(calli_string_fn fn, const char *arg, void *a, void *b, void *c)
{
	char buf[VIPS_PATH_MAX];

	expand_variables(arg, buf);

	return fn(buf, a, b, c);
}

int
calli_stringva(calli_string_fn fn,
	const char *fmt, va_list ap, void *a, void *b, void *c)
{
	char buf[VIPS_PATH_MAX];

	(void) g_vsnprintf(buf, VIPS_PATH_MAX, fmt, ap);

	return calli_string(fn, buf, a, b, c);
}

int
calli_stringf(calli_string_fn fn, const char *fmt, ...)
{
	va_list ap;
	int res;

	va_start(ap, fmt);
	res = calli_stringva(fn, fmt, ap, NULL, NULL, NULL);
	va_end(ap);

	return res;
}

/* Call a function, building a filename arg. Nativize and absoluteize too.
 */
int
calli_string_filename(calli_string_fn fn,
	const char *filename, void *a, void *b, void *c)
{
	char buf[VIPS_PATH_MAX];

	expand_variables(filename, buf);
	nativeize_path(buf);
	absoluteize_path(buf);

	return fn(buf, a, b, c);
}

int
calli_string_filenameva(calli_string_fn fn,
	const char *fmt, va_list ap, void *a, void *b, void *c)
{
	char buf[VIPS_PATH_MAX];

	g_vsnprintf(buf, VIPS_PATH_MAX, fmt, ap);

	return calli_string_filename(fn, buf, a, b, c);
}

int
calli_string_filenamef(calli_string_fn fn, const char *fmt, ...)
{
	va_list ap;
	int res;

	va_start(ap, fmt);
	res = calli_string_filenameva(fn, fmt, ap, NULL, NULL, NULL);
	va_end(ap);

	return res;
}

/* Convert a filename to utf8 ... g_free the result.
 */
char *
f2utf8(const char *filename)
{
	char *utf8;

	if (!(utf8 = g_filename_to_utf8(filename, -1, NULL, NULL, NULL)))
		utf8 = g_strdup(_("<charset conversion error>"));

	return utf8;
}

void
setenvf(const char *name, const char *fmt, ...)
{
	va_list ap;
	char buf[VIPS_PATH_MAX];

	va_start(ap, fmt);
	(void) g_vsnprintf(buf, VIPS_PATH_MAX, fmt, ap);
	va_end(ap);

	g_setenv(name, buf, TRUE);
}

int
check(const char *filename)
{
	/* Need to work on filenames containing %.
	 */
	return vips_existsf("%s", filename);
}

/* File exists?
 */
gboolean
existsf(const char *name, ...)
{
	va_list ap;
	gboolean res;

	va_start(ap, name);
	res = calli_string_filenameva(
		(calli_string_fn) check, name, ap, NULL, NULL, NULL);
	va_end(ap);

	return res;
}

int
isdir_sub(const char *filename)
{
	struct stat st;

	return stat(filename, &st) != -1 && S_ISDIR(st.st_mode);
}

gboolean
isdir(const char *filename, ...)
{
	va_list ap;
	gboolean res;

	va_start(ap, filename);
	res = calli_string_filenameva(
		(calli_string_fn) isdir_sub, filename, ap, NULL, NULL, NULL);
	va_end(ap);

	return res;
}

static void *
mtime_sub(const char *filename, time_t *time)
{
	struct stat st;

	if (stat(filename, &st) == -1)
		return NULL;
#ifdef HAVE_GETEUID
	if (st.st_uid != geteuid())
		return NULL;
#endif /*HAVE_GETEUID*/
	*time = st.st_mtime;

	return NULL;
}

time_t
mtime(const char *filename, ...)
{
	va_list ap;
	time_t time;

	time = 0;
	va_start(ap, filename);
	(void) callv_string_filenameva(
		(callv_string_fn) mtime_sub, filename, ap, &time, NULL, NULL);
	va_end(ap);

	return time;
}

gboolean
mkdirf(const char *name, ...)
{
	va_list ap;
	gboolean res;

	va_start(ap, name);
	res = calli_string_filenameva(
			  (calli_string_fn) g_mkdir,
			  name, ap, GINT_TO_POINTER(0755), NULL, NULL) == 0;
	va_end(ap);

	return res;
}

/* system(), with printf() args and $xxx expansion.
 */
int
systemf(const char *fmt, ...)
{
	va_list ap;
	int res;

	va_start(ap, fmt);
	res = calli_stringva(
		(calli_string_fn) system, fmt, ap, NULL, NULL, NULL);
	va_end(ap);

	return res;
}

gboolean
touchf(const char *fmt, ...)
{
	va_list ap;
	int fd;

	va_start(ap, fmt);
	fd = calli_string_filenameva(
		(calli_string_fn) creat,
		fmt, ap, GINT_TO_POINTER(S_IRUSR | S_IWUSR),
		NULL, NULL);
	va_end(ap);
	(void) close(fd);

	return fd != -1;
}

int
unlinkf(const char *fmt, ...)
{
	va_list ap;
	int res;

	va_start(ap, fmt);
	res = calli_string_filenameva(
		(calli_string_fn) unlink, fmt, ap, NULL, NULL, NULL);
	va_end(ap);

	return res;
}

/* Relative or absolute dir path? Have to expand env vars to see.
 */
gboolean
is_absolute(const char *fname)
{
	char buf[VIPS_PATH_MAX];

	expand_variables(fname, buf);
	nativeize_path(buf);

	/* We can't use g_path_is_absolute(), we might be given a Windows path
	 * including a drive specifier, and g_path_is_absolute() on unix does
	 * not know about Windows paths.
	 *
	 * We should probably look out for whitespace.
	 */
	return buf[0] == '/' || (buf[0] != '\0' && buf[1] == ':');
}

/* Are two filenames pointng at the same file? Expand, absoluteize, nativise.
 */
gboolean
filenames_equal(const char *f1, const char *f2)
{
	char b1[VIPS_PATH_MAX];
	expand_variables(f1, b1);
	nativeize_path(b1);
	absoluteize_path(b1);

	char b2[VIPS_PATH_MAX];
	expand_variables(f2, b2);
	nativeize_path(b2);
	absoluteize_path(b2);

	return g_str_equal(b1, b2);
}

/* Free an iOpenFile.
 */
void
ifile_close(iOpenFile *of)
{
	VIPS_FREEF(fclose, of->fp);
	VIPS_FREE(of->fname);
	VIPS_FREE(of->fname_real);
	VIPS_FREE(of);
}

/* Make an iOpenFile*.
 */
static iOpenFile *
ifile_build(const char *fname)
{
	g_autoptr(iOpenFile) of = INEW(NULL, iOpenFile);
	if (!of)
		return NULL;

	of->fp = NULL;
	of->fname = NULL;
	of->fname_real = NULL;
	of->last_errno = 0;

	VIPS_SETSTR(of->fname, fname);
	if (!of->fname)
		return NULL;

	return g_steal_pointer(&of);
}

/* Find and open for read.
 */
iOpenFile *
ifile_open_read(const char *name, ...)
{
	va_list ap;
	char buf[VIPS_PATH_MAX];

	va_start(ap, name);
	(void) g_vsnprintf(buf, VIPS_PATH_MAX, name, ap);
	va_end(ap);

	g_autoptr(iOpenFile) of = ifile_build(buf);
	if (!of)
		return NULL;

	if (!(of->fname_real = path_find_file(of->fname))) {
		error_top(_("Unable to open"));
		error_sub(_("unable to open file \"%s\" for reading\n%s"),
			of->fname, g_strerror(errno));
		return NULL;
	}

	if (!(of->fp = (FILE *) callv_string_filename((callv_string_fn) fopen,
			  of->fname_real, "r", NULL, NULL))) {
		error_top(_("Unable to open"));
		error_sub(_("unable to open file \"%s\" for reading\n%s"),
			of->fname_real, g_strerror(errno));
		return NULL;
	}

	of->read = TRUE;

	return g_steal_pointer(&of);
}

/* Open stdin for read.
 */
iOpenFile *
ifile_open_read_stdin()
{
	g_autoptr(iOpenFile) of = ifile_build("stdin");
	if (!of)
		return NULL;

	VIPS_SETSTR(of->fname_real, of->fname);
	if (!of->fname_real)
		return NULL;
	of->fp = stdin;
	of->read = TRUE;

	return g_steal_pointer(&of);
}

/* Find and open for write.
 */
iOpenFile *
ifile_open_write(const char *name, ...)
{
	va_list ap;
	char buf[VIPS_PATH_MAX];

	va_start(ap, name);
	(void) g_vsnprintf(buf, VIPS_PATH_MAX, name, ap);
	va_end(ap);

	g_autoptr(iOpenFile) of = ifile_build("stdin");
	if (!of)
		return NULL;

	VIPS_SETSTR(of->fname_real, of->fname);
	if (!of->fname_real)
		return NULL;

	if (!(of->fp = (FILE *) callv_string_filename((callv_string_fn) fopen,
			  of->fname_real, "w", NULL, NULL))) {
		error_top(_("Unable to open."));
		error_sub(_("unable to open file \"%s\" for writing\n%s"),
			of->fname_real, g_strerror(errno));
		return NULL;
	}

	of->read = FALSE;

	return g_steal_pointer(&of);
}

/* fprintf() to a file, checking result.
 */
gboolean
ifile_write(iOpenFile *of, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (vfprintf(of->fp, fmt, ap) == EOF) {
		of->last_errno = errno;
		error_top(_("Unable to write"));
		error_sub(_("unable to write to file \"%s\"\n%s"),
			of->fname_real, g_strerror(of->last_errno));
		return FALSE;
	}
	va_end(ap);

	return TRUE;
}

/* Save a string ... if non-NULL. Eg.
 *	fred="boink!"
 */
gboolean
ifile_write_var(iOpenFile *of, const char *name, const char *value)
{
	if (value && !ifile_write(of, " %s=\"%s\"", name, value))
		return FALSE;

	return TRUE;
}

/* Load up a file as a string.
 */
char *
ifile_read(iOpenFile *of)
{
	long len;
	size_t len2;
	char *str;

	/* Find length.
	 */
	fseek(of->fp, 0L, 2);
	len = ftell(of->fp);
	rewind(of->fp);
	if (len < 0 || len > 1024 * 1024) {
		error_top(_("Unable to read"));
		error_sub(_("file \"%s\" too large"), of->fname_real);
		return NULL;
	}

	/* Allocate memory and fill.
	 */
	if (!(str = imalloc(NULL, len + 1)))
		return NULL;

	/* We can't check len2 against len, since we may be reading a text
	 * file on Windows, in which case this fread will change CRLF to LF
	 * and len2 will be less than len.
	 */
	len2 = fread(str, sizeof(char), (size_t) len, of->fp);

	str[len2] = '\0';

	return str;
}

/* Load a file into a buffer. Useful for OpenFiles we can't seek in, like
 * stdin.
 */
char *
ifile_read_buffer(iOpenFile *of, char *buffer, size_t max)
{
	size_t len;

	/* -1 off max to leave space for the '\0'.
	 */
	len = fread(buffer, sizeof(char), max - 1, of->fp);
	if (!feof(of->fp)) {
		/* File too large for buffer.
		 */
		of->last_errno = errno;
		error_top(_("Unable to read"));
		error_sub(_("unable to read from file \"%s\"\n%s"),
			of->fname_real, g_strerror(of->last_errno));
		return NULL;
	}
	buffer[len] = '\0';

	return buffer;
}

/* Return '\0' for EOF, -1 for error.
 */
int
ifile_getc(iOpenFile *of)
{
	int ch;

	ch = fgetc(of->fp);

	if (ch == EOF && feof(of->fp))
		return 0;
	else if (ch == EOF)
		return -1;
	else
		return ch;
}

static off_t
statf(const char *fmt, ...)
{
	va_list ap;
	struct stat st;
	int result;

	va_start(ap, fmt);
	result = calli_string_filenameva(
		(calli_string_fn) stat, fmt, ap, &st, NULL, NULL);
	va_end(ap);

	return result == -1 || S_ISDIR(st.st_mode) ? 0 : st.st_size;
}

static void *
directory_size_sub(const char *filename, double *total)
{
	*total += statf("%s", filename);

	return NULL;
}

/* Find the amount of 'stuff' in a directory. Result in bytes. Don't look in
 * sub-dirs.
 */
double
directory_size(const char *dirname)
{
	double total;

	total = 0;
	path_map_dir(dirname, "*", (path_map_fn) directory_size_sub, &total);

	return total;
}

/* Escape "%" characters in a string.
 */
char *
escape_percent(const char *in, char *out, int len)
{
	const char *p;
	char *q;

	for (p = in, q = out; *p && q - out < len - 3; p++, q++)
		if (*p == '%') {
			q[0] = '%';
			q[1] = '%';
			q++;
		}
		else
			*q = *p;

	*q = '\0';

	return out;
}

// FIXME ... swap this for g_markup_escape_text()?
char *
escape_markup(const char *in, char *out, int len)
{
	const char *p;
	char *q;

	for (p = in, q = out; *p && q - out < len - 5; p++, q++)
		if (*p == '<') {
			strcpy(q, "&lt;");
			q += 3;
		}
		else if (*p == '>') {
			strcpy(q, "&gt;");
			q += 3;
		}
		else if (*p == '&') {
			strcpy(q, "&amp;");
			q += 4;
		}
		else
			*q = *p;

	*q = '\0';

	return out;
}

/* Like strtok(), but better. Give a string and a list of break characters;
 * write a '\0' into the string over the first break character and return a
 * pointer to the next non-break character. If there are no break characters,
 * then return a pointer to the end of the string. If passed a pointer to an
 * empty string or NULL, then return NULL.
 */
char *
break_token(char *str, const char *brk)
{
	char *p;

	/* Is the string empty? If yes, return NULL immediately.
	 */
	if (!str || !*str)
		return NULL;

	/* Skip initial break characters.
	 */
	p = str + strspn(str, brk);

	/* Search for the first break character after the token.
	 */
	p += strcspn(p, brk);

	/* Is there string left?
	 */
	if (*p) {
		/* Write in an end-of-string mark and return the start of the
		 * next token.
		 */
		*p++ = '\0';
		p += strspn(p, brk);
	}

	return p;
}

/* Turn a number to a string. 0 is "A", 1 is "B", 25 is "Z", 26 is "AA", 27 is
 * "AB", etc.
 */
void
number_to_string(int n, char *buf)
{
	do {
		int rem = n % 26;

		*buf++ = (char) (rem + (int) 'A');
		n /= 26;
	} while (n > 0);

	*buf = '\0';
}

/* Find the space remaining in a directory, in bytes. A double for >32bit
 * problem avoidance. <0 for error.
 */
#ifdef HAVE_SYS_STATVFS_H

double
find_space(const char *name)
{
	struct statvfs st;
	double sz;

	if (calli_string_filename((calli_string_fn) statvfs,
			(gpointer) name, &st, NULL, NULL))
		/* Set to error value.
		 */
		sz = -1;
	else
		sz = VIPS_MAX(0, (double) st.f_frsize * st.f_bavail);

	return sz;
}

#elif (HAVE_SYS_VFS_H || HAVE_SYS_MOUNT_H)

double
find_space(const char *name)
{
	struct statfs st;
	double sz;

	if (calli_string_filename((calli_string_fn) statfs,
			(gpointer) name, &st, NULL, NULL))
		sz = -1;
	else
		sz = VIPS_MAX(0, (double) st.f_bsize * st.f_bavail);

	return sz;
}

#elif defined G_OS_WIN32

double
find_space(const char *name)
{
	ULARGE_INTEGER avail;
	ULARGE_INTEGER total;
	ULARGE_INTEGER free;
	double sz;
	char name2[VIPS_PATH_MAX];

	expand_variables(name, name2);

	/* Truncate to just the drive letter.
	 */
	if (name2[1] == ':')
		name2[3] = 0;

	if (!GetDiskFreeSpaceEx(name2, &avail, &total, &free))
		sz = -1;
	else
		sz = VIPS_MAX(0, (double) free.QuadPart);

	return sz;
}

#else

double
find_space(const char *name)
{
	return -1;
}

#endif /*HAVE_SYS_STATVFS_H*/

/* Make a name for a temp file. Add the object name (can be NULL) and
 * specified extension.
 */
gboolean
temp_name(char *filename, const char *name, const char *type)
{
	/* Some mkstemp() require files to actually exist before they don't
	 * reuse the filename :-( add an extra field.
	 */
	static int n = 0;

	const char *dir;
	int fd;
	char buf[VIPS_PATH_MAX];
	char buf2[VIPS_PATH_MAX];

	dir = PATH_TMP;
	if (!existsf("%s", dir))
		dir = G_DIR_SEPARATOR_S;

	g_snprintf(buf2, VIPS_PATH_MAX,
		"%s/" PACKAGE "-%s-%" G_PID_FORMAT "-%d-XXXXXXX",
		dir, name ? name : "untitled", get_gpid(), n++);
	expand_variables(buf2, buf);

	fd = g_mkstemp(buf);
	if (fd == -1) {
		error_top(_("Unable to create temporary file"));
		error_sub(_("unable to make file \"%s\"\n%s"), buf, g_strerror(errno));
		return FALSE;
	}
	close(fd);
	unlinkf("%s", buf);

	g_snprintf(filename, VIPS_PATH_MAX, "%s.%s", buf, type);

	return TRUE;
}

/* Max/min of an area.
 */
int
findmaxmin(VipsImage *in,
	int left, int top, int width, int height, double *min, double *max)
{
	VipsImage *msk;
	VipsImage *t1;

	if (vips_extract_area(in, &t1, left, top, width, height, NULL))
		return -1;
	if (vips_stats(t1, &msk, NULL)) {
		VIPS_UNREF(t1);
		return -1;
	}
	VIPS_UNREF(t1);

	*min = *VIPS_MATRIX(msk, 0, 0);
	*max = *VIPS_MATRIX(msk, 1, 0);

	VIPS_UNREF(msk);

#ifdef DEBUG
	printf("findmaxmin: left = %d, top = %d, width = %d, height = %d\n",
		left, top, width, height);
	printf("findmaxmin: max = %g, min = %g\n", *max, *min);
#endif /*DEBUG*/

	return 0;
}

gboolean
char_to_bool(char *str, void *out)
{
	gboolean *t = (gboolean *) out;

	if (g_ascii_strcasecmp("TRUE", str) == 0)
		*t = TRUE;
	else
		*t = FALSE;

	return TRUE;
}

char *
bool_to_char(gboolean b)
{
	return b ? "true" : "false";
}

/* Increment a name ... strip any trailing numbers, add one, put numbers back.
 * Start at 1 if no number there. buf should be at least namelen chars. Keep
 * leading zeros, if any.
 */
void
increment_name(char *buf)
{
	char *p;
	int n;
	char fmt[256];

	/* If there's no number, p will point at the '\0'.
	 */
	p = (char *) my_strrspn(buf, NUMERIC);
	if (*p) {
		n = atoi(p);
		g_snprintf(fmt, 256, "%%0%dd", (int) strlen(p));
	}
	else {
		strcpy(fmt, "%d");
		n = 0;
	}

	g_snprintf(p, MAX_STRSIZE - (p - buf), fmt, n + 1);
}

/* Increment filename. Eg. "/home/jim/fred_00_tn.tif" becomes
 * "/home/jim/fred_01_tn.tif"
 */
void
increment_filename(char *filename)
{
	char buf[VIPS_PATH_MAX];
	char suf[VIPS_PATH_MAX];
	char tail[VIPS_PATH_MAX];
	char *p;

	g_strlcpy(buf, filename, VIPS_PATH_MAX);

	/* Save and replace the suffix around an increment_name.
	 */
	char *basename = g_path_get_basename(buf);
	if (!(p = strrchr(basename, '.')))
		p = basename + strlen(basename);
	g_strlcpy(suf, p, VIPS_PATH_MAX);
	*p = '\0';

	/* Also save any chars to the right of the number component (if any) of
	 * the filename.
	 */
	p = (char *) my_strrcspn(basename, NUMERIC);

	/* No numbers there? Take nothing as the tail and put the number at
	 * the end.
	 */
	if (p == basename)
		p = basename + strlen(basename);

	g_strlcpy(tail, p, VIPS_PATH_MAX);
	*p = '\0';

	increment_name(buf);

	strcpy(filename, buf);
	strcat(filename, tail);
	strcat(filename, suf);

	g_free(basename);
}

/* Extract the first line of a string in to buf, extract no more than len
 * chars.
 */
int
extract_first_line(char *buf, char *str, int len)
{
	char *p;
	int n;

	/* Find next '\n' or '\0'.
	 */
	if ((p = strchr(str, '\n')))
		n = p - str;
	else
		n = strlen(str);
	n = VIPS_MIN(len - 1, n);

	/* Copy those characters and make sure we have a '\0'.
	 */
	g_strlcpy(buf, str, n);
	buf[n] = '\0';

	return n;
}

/* Make a valid ip name from a filename.
 */
void
name_from_filename(const char *in, char *out)
{
	g_autofree char *basename = g_path_get_basename(in);

	const char *p;

	/* Skip leading path prefix, and any non-alpha.
	 * Don't use isalnum(), since we don't want leading digits.
	 */
	p = basename;
	while (*p && !(isalpha(*p) || *p == '_'))
		p += 1;

	if (!*p)
		strcpy(out, "untitled");
	else {
		char *q;

		/* Filter non-identifier chars. Stop at the first '.'
		 * character, so we don't get the suffix in there too.
		 */
		for (q = out; *p && *p != '.'; p++)
			if (isalnum(*p) || *p == '_' || *p == '\'')
				*q++ = *p;
		*q = '\0';
	}
}

/* Do any leak checking we can.
 */
void
util_check_all_destroyed(void)
{
	if (rect_n_rects)
		printf("rect_n_rects == %d\n", rect_n_rects);
}

/* Like vips_malloc(), but set our error stuff.
 */
void *
imalloc(VipsImage *im, size_t len)
{
	void *mem;

	if (!(mem = VIPS_MALLOC(im, len))) {
		char txt[256];
		VipsBuf buf = VIPS_BUF_STATIC(txt);

		vips_buf_append_size(&buf, len);
		error_top(_("Out of memory"));
		error_sub(_("request for %s of RAM triggered memory "
					"allocation failure"),
			vips_buf_all(&buf));
		error_vips();

		return NULL;
	}

	return mem;
}

/* Return the name of the save dir we use ... eg. "$HOME/.nip2-7.10.8",
 * or maybe "C:\Documents and Settings\john\Application Data"
 */
const char *
get_savedir(void)
{
#ifdef G_OS_WIN32

	/* If APPDATA is not defined, default to HOME, we know that will
	 * exist (since we make it if necessary in main()).
	 */
	if (g_getenv("APPDATA") && existsf("%s", g_getenv("APPDATA")))
		return "$APPDATA/" PACKAGE "-" VERSION;
	else
		return "$HOME/." PACKAGE "-" VERSION;

#elif G_OS_DARWIN

	/* Darwin ... in ~/Library
	 */
	return "$HOME/Library/" PACKAGE "-" VERSION;

#else /**nix*/

	/* *nix-style system .. .dot file in home area.
	 */
	return "$HOME/." PACKAGE "-" VERSION;

#endif
}

/* Turn an slist into a null-terminated array.
 */
void **
slist_to_array(GSList *list)
{
	g_autofree void **array = g_new(void *, g_slist_length(list) + 1);

	int i;

	for (i = 0; list; list = list->next, i++)
		array[i] = list->data;
	array[i] = NULL;

	return g_steal_pointer(&array);
}

/* Length of a NULL-terminated array.
 */
int
array_len(void **array)
{
	int i;

	for (i = 0; array[i]; i++)
		;

	return i;
}

double *
ink_to_vector(const char *domain, VipsImage *im, VipsPel *ink, int *n)
{
	VipsImage **t = (VipsImage **) vips_object_local_array(VIPS_OBJECT(im), 6);

	double *result;

#ifdef VIPS_DEBUG
	printf("ink_to_vector: starting\n");
#endif /*VIPS_DEBUG*/

	/* Wrap a VipsImage around ink.
	 */
	t[0] = vips_image_new_from_memory(ink, VIPS_IMAGE_SIZEOF_PEL(im),
		1, 1, VIPS_IMAGE_SIZEOF_PEL(im), VIPS_FORMAT_UCHAR);
	if (vips_copy(t[0], &t[1],
			"bands", im->Bands,
			"format", im->BandFmt,
			"coding", im->Coding,
			"interpretation", im->Type,
			NULL))
		return NULL;

	/* The image may be coded .. unpack to double.
	 */
	if (vips_image_decode(t[1], &t[2]) ||
		vips_cast(t[2], &t[3], VIPS_FORMAT_DOUBLE, NULL))
		return NULL;

	/* To a mem buffer, then copy to out.
	 */
	if (!(t[4] = vips_image_new_memory()) ||
		vips_image_write(t[3], t[4]))
		return NULL;

	if (!(result = VIPS_ARRAY(im, t[4]->Bands, double)))
		return NULL;
	memcpy(result, t[4]->data, VIPS_IMAGE_SIZEOF_PEL(t[4]));
	*n = t[4]->Bands;

#ifdef VIPS_DEBUG
	{
		int i;

		printf("vips__ink_to_vector:\n");
		printf("\tink = ");
		for (i = 0; i < n; i++)
			printf("%d ", ink[i]);
		printf("\n");

		printf("\tvec = ");
		for (i = 0; i < *n; i++)
			printf("%g ", result[i]);
		printf("\n");
	}
#endif /*VIPS_DEBUG*/

	return result;
}

void
draw_line(VipsImage *image, int x1, int y1, int x2, int y2,
	DrawPoint draw_point, void *client)
{
	int dx, dy;
	int x, y, err;

	dx = x2 - x1;
	dy = y2 - y1;

	/* Swap endpoints to reduce number of cases.
	 */
	if (abs(dx) >= abs(dy) &&
		dx < 0) {
		/* Swap to get all x greater or equal cases going to the
		 * right. Do diagonals here .. just have up and right and down
		 * and right now.
		 */
		VIPS_SWAP(int, x1, x2);
		VIPS_SWAP(int, y1, y2);
	}
	else if (abs(dx) < abs(dy) &&
		dy < 0) {
		/* Swap to get all y greater cases going down the screen.
		 */
		VIPS_SWAP(int, x1, x2);
		VIPS_SWAP(int, y1, y2);
	}

	dx = x2 - x1;
	dy = y2 - y1;

	x = x1;
	y = y1;

	/* Special case: zero width and height is single point.
	 */
	if (dx == 0 &&
		dy == 0)
		draw_point(image, x, y, client);
	/* Special case vertical and horizontal lines for speed.
	 */
	else if (dx == 0) {
		/* Vertical line going down.
		 */
		for (; y <= y2; y++)
			draw_point(image, x, y, client);
	}
	else if (dy == 0) {
		/* Horizontal line to the right.
		 */
		for (; x <= x2; x++)
			draw_point(image, x, y, client);
	}
	/* Special case diagonal lines.
	 */
	else if (abs(dy) == abs(dx) &&
		dy > 0) {
		/* Diagonal line going down and right.
		 */
		for (; x <= x2; x++, y++)
			draw_point(image, x, y, client);
	}
	else if (abs(dy) == abs(dx) &&
		dy < 0) {
		/* Diagonal line going up and right.
		 */
		for (; x <= x2; x++, y--)
			draw_point(image, x, y, client);
	}
	else if (abs(dy) < abs(dx) &&
		dy > 0) {
		/* Between -45 and 0 degrees.
		 */
		for (err = 0; x <= x2; x++) {
			draw_point(image, x, y, client);

			err += dy;
			if (err >= dx) {
				err -= dx;
				y++;
			}
		}
	}
	else if (abs(dy) < abs(dx) &&
		dy < 0) {
		/* Between 0 and 45 degrees.
		 */
		for (err = 0; x <= x2; x++) {
			draw_point(image, x, y, client);

			err -= dy;
			if (err >= dx) {
				err -= dx;
				y--;
			}
		}
	}
	else if (abs(dy) > abs(dx) &&
		dx > 0) {
		/* Between -45 and -90 degrees.
		 */
		for (err = 0; y <= y2; y++) {
			draw_point(image, x, y, client);

			err += dx;
			if (err >= dy) {
				err -= dy;
				x++;
			}
		}
	}
	else if (abs(dy) > abs(dx) &&
		dx < 0) {
		/* Between -90 and -135 degrees.
		 */
		for (err = 0; y <= y2; y++) {
			draw_point(image, x, y, client);

			err -= dx;
			if (err >= dy) {
				err -= dy;
				x--;
			}
		}
	}
	else
		g_assert_not_reached();
}

GPid
get_gpid(void)
{
	GPid gpid;

#ifdef G_OS_WIN32
	// pid_t is pointer-sized
	gpid = GINT_TO_POINTER(GetCurrentProcessId());
#else /*!G_OS_WIN32*/
	gpid = getpid();
#endif /*G_OS_WIN32*/

	return gpid;
}
