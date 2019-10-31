/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZGetVariable, ZSetVariable, and ZUnsetVariable
 * functions.
 *
 *	Created by:	Robert French
 *
 *	Copyright (c) 1987 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h".
 */

#include "libpurple/internal.h"
#include "internal.h"
#include "util.h"

#include <ctype.h>
#ifndef WIN32
#include <pwd.h>
#endif

static char *get_localvarfile(void);
static const gchar *get_varval(const gchar *fn, const gchar *val);
static int varline(const gchar *bfr, const gchar *var);

const gchar *
ZGetVariable(const gchar *var)
{
	gchar *varfile;
	const gchar *ret;

	if ((varfile = get_localvarfile()) == NULL)
		return ((char *)0);

	ret = get_varval(varfile, var);
	g_free(varfile);
	if (ret != ZERR_NONE)
		return ret;

#ifdef _WIN32
	varfile = g_strdup("C:\\zephyr\\zephyr.var");
#else
	varfile = g_build_filename(PURPLE_SYSCONFDIR, "zephyr.vars", NULL);
#endif
	ret = get_varval(varfile, var);
	g_free(varfile);

	return ret;
}

Code_t ZSetVariable(var, value)
    char *var;
    char *value;
{
    int written;
    FILE *fpin, *fpout;
    char *varfile, *varfilebackup, varbfr[512];

    written = 0;

    if ((varfile = get_localvarfile()) == NULL)
	return (ZERR_INTERNAL);

    varfilebackup = g_strconcat(varfile, ".backup", NULL);

    if (!(fpout = fopen(varfilebackup, "w"))) {
	g_free(varfile);
	g_free(varfilebackup);
	return (errno);
    }
    if ((fpin = fopen(varfile, "r")) != NULL) {
	while (fgets(varbfr, sizeof varbfr, fpin) != (char *) 0) {
	    if (varbfr[strlen(varbfr)-1] < ' ')
		varbfr[strlen(varbfr)-1] = '\0';
	    if (varline(varbfr, var)) {
		fprintf(fpout, "%s = %s\n", var, value);
		written = 1;
	    }
	    else
		fprintf(fpout, "%s\n", varbfr);
	}
	(void) fclose(fpin);		/* don't care about errs on input */
    }
    if (!written)
	fprintf(fpout, "%s = %s\n", var, value);
    if (fclose(fpout) == EOF) {
    	g_free(varfilebackup);
    	g_free(varfile);
	return(EIO);		/* can't rely on errno */
    }
    if (g_rename(varfilebackup, varfile)) {
	g_free(varfilebackup);
	g_free(varfile);
	return (errno);
    }
    g_free(varfilebackup);
    g_free(varfile);
    return (ZERR_NONE);
}

Code_t ZUnsetVariable(var)
    char *var;
{
    FILE *fpin, *fpout;
    char *varfile, *varfilebackup, varbfr[512];

    if ((varfile = get_localvarfile()) == NULL)
	return (ZERR_INTERNAL);

    varfilebackup = g_strconcat(varfile, ".backup", NULL);

    if (!(fpout = fopen(varfilebackup, "w"))) {
	g_free(varfile);
	g_free(varfilebackup);
	return (errno);
    }
    if ((fpin = fopen(varfile, "r")) != NULL) {
	while (fgets(varbfr, sizeof varbfr, fpin) != (char *) 0) {
	    if (varbfr[strlen(varbfr)-1] < ' ')
		varbfr[strlen(varbfr)-1] = '\0';
	    if (!varline(varbfr, var))
		fprintf(fpout, "%s\n", varbfr);
	}
	(void) fclose(fpin);		/* don't care about read close errs */
    }
    if (fclose(fpout) == EOF) {
	g_free(varfilebackup);
	g_free(varfile);
	return(EIO);		/* errno isn't reliable */
    }
    if (g_rename(varfilebackup, varfile)) {
	g_free(varfilebackup);
	g_free(varfile);
	return (errno);
    }
    g_free(varfilebackup);
    g_free(varfile);
    return (ZERR_NONE);
}

static char *get_localvarfile(void)
{
    const char *base;
#ifndef WIN32
    struct passwd *pwd;
    base = purple_home_dir();
#else
    base = getenv("HOME");
    if (!base)
        base = getenv("HOMEPATH");
    if (!base)
        base = "C:\\";
#endif
    if (!base) {
#ifndef WIN32
	if (!(pwd = getpwuid((int) getuid()))) {
	    fprintf(stderr, "Zephyr internal failure: Can't find your entry in /etc/passwd\n");
	    return NULL;
	}
	base = pwd->pw_dir;
#endif
    }

    return g_strconcat(base, "/.zephyr.vars", NULL);
}

static const gchar *
get_varval(const gchar *fn, const gchar *var)
{
    FILE *fp;
    static gchar varbfr[512];
    int i;

    fp = fopen(fn, "r");
    if (!fp)
	return ((char *)0);

    while (fgets(varbfr, sizeof varbfr, fp) != (char *) 0) {
	if (varbfr[strlen(varbfr)-1] < ' ')
	    varbfr[strlen(varbfr)-1] = '\0';
	if (!(i = varline(varbfr, var)))
	    continue;
	(void) fclose(fp);		/* open read-only, don't care */
	return (varbfr+i);
    }
    (void) fclose(fp);			/* open read-only, don't care */
    return ((char *)0);
}

/* If the variable in the line bfr[] is the same as var, return index to
   the variable value, else return 0. */
static int
varline(const gchar *bfr, const gchar *var)
{
	register const gchar *cp;

	if (!bfr[0] || bfr[0] == '#') {
		/* comment or null line */
		return (0);
	}

	cp = bfr;
	while (*cp && !isspace(*cp) && (*cp != '=')) {
		cp++;
	}

	if (g_ascii_strncasecmp(bfr, var, MAX(strlen(var), (gsize)(cp - bfr)))) {
		/* var is not the var in bfr ==> no match */
		return 0;
	}

	cp = strchr(bfr, '=');
	if (!cp) {
		return (0);
	}
	cp++;
	while (*cp && isspace(*cp)) {
		/* space up to variable value */
		cp++;
	}

	return (cp - bfr); /* return index */
}
