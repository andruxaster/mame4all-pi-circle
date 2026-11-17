/* glib.h - tiny replacement for the GKeyFile subset used by config.cpp (Circle port).
   Reads an INI file: [section] lines and key=value pairs; '#' and ';' start comments. */
#ifndef CIRCLE_GLIB_SHIM_H
#define CIRCLE_GLIB_SHIM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct { int code; } GError;
typedef struct GKeyFileEntry { char *section, *key, *value; struct GKeyFileEntry *next; } GKeyFileEntry;
typedef struct { GKeyFileEntry *first; } GKeyFile;
#define G_KEY_FILE_NONE 0

static GError g_shim_error = { 1 };

static inline GKeyFile *g_key_file_new (void)
{
	return (GKeyFile *) calloc (1, sizeof (GKeyFile));
}

static inline char *g_shim_trim (char *s)
{
	char *e;
	while (*s && isspace ((unsigned char) *s)) s++;
	e = s + strlen (s);
	while (e > s && isspace ((unsigned char) e[-1])) *--e = 0;
	return s;
}

static inline int g_key_file_load_from_file (GKeyFile *kf, const char *file, int flags, GError **err)
{
	char line[512], section[128] = "";
	FILE *f = fopen (file, "r");
	(void) flags;
	if (!f) { if (err) *err = &g_shim_error; return 0; }
	while (fgets (line, sizeof line, f))
	{
		char *s = g_shim_trim (line), *eq;
		if (!*s || *s == '#' || *s == ';') continue;
		if (*s == '[')
		{
			char *e = strchr (s, ']');
			if (e) { *e = 0; strncpy (section, s + 1, sizeof section - 1); }
			continue;
		}
		eq = strchr (s, '=');
		if (!eq) continue;
		*eq = 0;
		GKeyFileEntry *ent = (GKeyFileEntry *) calloc (1, sizeof *ent);
		ent->section = strdup (section);
		ent->key = strdup (g_shim_trim (s));
		ent->value = strdup (g_shim_trim (eq + 1));
		ent->next = kf->first;
		kf->first = ent;
	}
	fclose (f);
	return 1;
}

static inline const char *g_shim_find (GKeyFile *kf, const char *section, const char *key)
{
	GKeyFileEntry *e;
	for (e = kf ? kf->first : 0; e; e = e->next)
		if (strcasecmp (e->section, section) == 0 && strcasecmp (e->key, key) == 0) return e->value;
	return 0;
}

static inline char *g_key_file_get_string (GKeyFile *kf, const char *section, const char *key, GError **err)
{
	const char *v = g_shim_find (kf, section, key);
	if (!v) { if (err) *err = &g_shim_error; return 0; }
	return strdup (v);
}

static inline int g_key_file_get_integer (GKeyFile *kf, const char *section, const char *key, GError **err)
{
	const char *v = g_shim_find (kf, section, key);
	if (!v) { if (err) *err = &g_shim_error; return 0; }
	return (int) strtol (v, 0, 0);
}

static inline double g_key_file_get_double (GKeyFile *kf, const char *section, const char *key, GError **err)
{
	const char *v = g_shim_find (kf, section, key);
	if (!v) { if (err) *err = &g_shim_error; return 0; }
	return strtod (v, 0);
}

static inline void g_key_file_free (GKeyFile *kf)
{
	GKeyFileEntry *e, *n;
	if (!kf) return;
	for (e = kf->first; e; e = n) { n = e->next; free (e->section); free (e->key); free (e->value); free (e); }
	free (kf);
}

#endif
