/* ---------------------------------------------------------------------- *
 * fileutil.c
 * This file is part of lincity.
 * Lincity is copyright (c) I J Peters 1995-1997, (c) Greg Sharp 1997-2001.
 * ---------------------------------------------------------------------- */
#include "lcconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include "lcintl.h"
#include "lcstring.h"

/* this is for OS/2 - RVI */
#ifdef __EMX__
#include <sys/select.h>
#include <X11/Xlibint.h>      /* required for __XOS2RedirRoot */
#define chown(x,y,z)
#define OS2_DEFAULT_LIBDIR "/XFree86/lib/X11/lincity"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined (TIME_WITH_SYS_TIME)
#include <time.h>
#include <sys/time.h>
#else
#if defined (HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#if defined (WIN32)
#include <winsock.h>
#if defined (__BORLANDC__)
#include <dir.h>
#include <dirent.h>
#include <dos.h>
#endif
#include <io.h>
#include <direct.h>
#include <process.h>
#endif

#if defined (HAVE_DIRENT_H)
#include <dirent.h>
#define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#define dirent direct
#define NAMLEN(dirent) (dirent)->d_namlen
#if defined (HAVE_SYS_NDIR_H)
#include <sys/ndir.h>
#endif
#if defined (HAVE_SYS_DIR_H)
#include <sys/dir.h>
#endif
#if defined (HAVE_NDIR_H)
#include <ndir.h>
#endif
#endif

#if defined (HAVE_POPEN)
FILE *popen(const char *command, const char *type);
int pclose(FILE *stream);
#endif

#include <ctype.h>
#include "common.h"
#ifdef LC_X11
#include <X11/cursorfont.h>
#endif
#include "lctypes.h"
#include "lin-city.h"
#include "cliglobs.h"
#include "engglobs.h"
#include "fileutil.h"

/* GCS: This is from dcgettext.c in the gettext package.      */
/* XPG3 defines the result of `setlocale (category, NULL)' as:
   ``Directs `setlocale()' to query `category' and return the current
     setting of `local'.''
   However it does not specify the exact format.  And even worse: POSIX
   defines this not at all.  So we can use this feature only on selected
   system (e.g. those using GNU C Library).  */
#ifdef _LIBC
# define HAVE_LOCALE_NULL
#endif

/* ---------------------------------------------------------------------- *
 * Private Fn Prototypes
 * ---------------------------------------------------------------------- */
void dump_screen (void);
void verify_package (void);
static const char *guess_category_value (int category, 
					 const char *categoryname);

/* ---------------------------------------------------------------------- *
 * Public Global Variables
 * ---------------------------------------------------------------------- */
#if defined (WIN32)
char LIBDIR[_MAX_PATH];
#elif defined (__EMX__)
#ifdef LIBDIR
#undef LIBDIR   /* yes, I know I shouldn't ;-) */
#endif
char LIBDIR[256];
#endif

char *lc_save_dir;
int lc_save_dir_len;
static char *lc_temp_filename;


/* ---------------------------------------------------------------------- *
 * Public Functions
 * ---------------------------------------------------------------------- */
#if defined (__BORLANDC__)
int
_chdir (const char *dirname)
{
    return chdir (dirname);
}

int 
_access (const char *path, int mode)
{
    return access (path, mode)
}
#endif

/* Executes a system command */
int
execute_command (char *cmd, char *p1, char *p2, char *p3)
{
  char *sys_cmd = (char *) malloc (strlen (cmd) + strlen (p1) + strlen (p2)
				   + strlen (p3) + 4);
  int ret_value;

  if (sys_cmd == 0) {
    malloc_failure ();
  }
  sprintf (sys_cmd, "%s %s %s %s", cmd, p1, p2, p3);
  ret_value = system (sys_cmd);
  free (sys_cmd);
  return ret_value;
}

void
copy_file (char *f1, char *f2)
{
  int ret_value = execute_command ("cp", f1, f2, "");
  if (ret_value != 0)
    {
      /* GCS FIX:  Need to make do_error into var_args fn? */
      printf ("Tried to cp %s %s\n", f1, f2);
      do_error ("Can't copy requested file");
    }
}

void
gunzip_file (char *f1, char *f2)
{
  int ret_value = execute_command ("gzip -c -d", f1, ">", f2);
  if (ret_value != 0)
    {
      /* GCS FIX:  Need to make do_error into var_args fn? */
      printf ("Tried to gzip -c -d %s > %s\n", f1, f2);
      do_error ("Can't gunzip requested file");
    }
}

FILE* 
fopen_read_gzipped (char* fn)
{
    FILE* fp;

#if defined (HAVE_GZIP) && defined (HAVE_POPEN)
    const char* cmd_str = "gzip -d -c < %s 2> /dev/null";
    char *cmd = (char*) malloc (strlen (cmd_str) + strlen (fn) + 1);
    sprintf (cmd, cmd_str, fn);
    fp=popen(cmd,"r");
    free(cmd);

#elif defined (HAVE_GZIP) && !defined (HAVE_POPEN)
    gunzip_file (fn, lc_temp_filename);
    fp = fopen (lc_temp_filename, "rb");

#else /* No gzip */
    fp = fopen (fn, "rb");
#endif

    return fp;
}

void 
fclose_read_gzipped (FILE* fp)
{
#if defined (HAVE_GZIP) && defined (HAVE_POPEN)
    pclose (fp);
#elif defined (HAVE_GZIP) && !defined (HAVE_POPEN)
    fclose (fp);
    remove (lc_temp_filename);
#else
    fclose (fp);
#endif
}

int
directory_exists (char *dir)
{
#if defined (WIN32)
    if (_chdir (dir) == -1) {
	_chdir (LIBDIR);		/* go back... */
	return 0;
    }
    _chdir (LIBDIR);		/* go back... */
#else /* UNIX */
    DIR *dp;
    if ((dp = opendir (dir)) == NULL) {
	return 0;
    }
    closedir (dp);
#endif
    return 1;
}

int
file_exists (char *filename)
{
    FILE* fp;
    fp = fopen (filename,"rb");
    if (fp == NULL) {
	return 0;
    }
    fclose (fp);
    return 1;
}

#if defined (WIN32)
void
find_libdir (void)
{
    const char searchfile[] = "Colour.pal";
    /* default_dir will be something like "C:\\LINCITY1.11" */
    const char default_dir[] = "C:\\LINCITY" VERSION;

    /* Check 1: environment variable */
    _searchenv (searchfile, "LINCITY_HOME", LIBDIR);
    if (*LIBDIR != '\0') {
	int endofpath_offset = strlen (LIBDIR) - strlen (searchfile) - 1;
	LIBDIR[endofpath_offset] = '\0';
	return;
    }

    /* Check 2: default location */
    if ((_access (default_dir, 0)) != -1) {
	strcpy (LIBDIR, default_dir);
	return;
    }

    /* Finally give up */
    HandleError ("Error. Can't find LINCITY_HOME", FATAL);
}
#endif


/* GCS:  This function comes from dcgettext.c in the gettext package.      */
/* Guess value of current locale from value of the environment variables.  */
static const char *
guess_category_value (int category, const char *categoryname)
{
    const char *retval;

    /* The highest priority value is the `LANGUAGE' environment
       variable.  This is a GNU extension.  */
    retval = getenv ("LANGUAGE");
    if (retval != NULL && retval[0] != '\0')
	return retval;

    /* `LANGUAGE' is not set.  So we have to proceed with the POSIX
       methods of looking to `LC_ALL', `LC_xxx', and `LANG'.  On some
       systems this can be done by the `setlocale' function itself.  */
#if defined HAVE_SETLOCALE && defined HAVE_LC_MESSAGES && defined HAVE_LOCALE_NULL
    return setlocale (category, NULL);
#else
    /* Setting of LC_ALL overwrites all other.  */
    retval = getenv ("LC_ALL");
    if (retval != NULL && retval[0] != '\0')
	return retval;

    /* Next comes the name of the desired category.  */
    retval = getenv (categoryname);
    if (retval != NULL && retval[0] != '\0')
	return retval;

    /* Last possibility is the LANG environment variable.  */
    retval = getenv ("LANG");
    if (retval != NULL && retval[0] != '\0')
	return retval;

    /* We use C as the default domain.  POSIX says this is implementation
       defined.  */
    return "C";
#endif
}

void
init_path_strings (void)
{
    char* homedir = NULL;
    const char* intl_suffix = "";

#if defined (WIN32)
    find_libdir ();
    homedir = LIBDIR;
#elif defined (__EMX__)
    strcpy(LIBDIR, __XOS2RedirRoot(OS2_DEFAULT_LIBDIR));
#else
    homedir = getenv ("HOME");
#endif

#if defined (ENABLE_NLS)
#if defined (HAVE_LC_MESSAGES)
    intl_suffix = guess_category_value(LC_MESSAGES,"LC_MESSAGES");
#else
    intl_suffix = guess_category_value(0,"LC_MESSAGES");
#endif
#endif

    printf ("intl_suffix is %s\n", intl_suffix);

    lc_save_dir_len = strlen (homedir) + strlen (LC_SAVE_DIR) + 1;
    if ((lc_save_dir = (char *) malloc (lc_save_dir_len + 1)) == 0)
	malloc_failure ();
    sprintf (lc_save_dir, "%s%c%s", homedir, PATH_SLASH, LC_SAVE_DIR);
    sprintf (colour_pal_file, "%s%c%s", LIBDIR, PATH_SLASH, "colour.pal");
    sprintf (opening_path, "%s%c%s", LIBDIR, PATH_SLASH, "opening");
#if defined (WIN32)
    sprintf (opening_pic, "%s%c%s",opening_path,PATH_SLASH,"open.tga");
#else
    sprintf (opening_pic, "%s%c%s",opening_path,PATH_SLASH,"open.tga.gz");
#endif
    sprintf (graphic_path, "%s%c%s%c", LIBDIR, PATH_SLASH, "icons",
	     PATH_SLASH);
    if (strcmp(intl_suffix,"C") && strcmp(intl_suffix,"")) {
	sprintf (message_path, "%s%c%s%c%s%c", LIBDIR, PATH_SLASH, "messages",
		 PATH_SLASH, intl_suffix, PATH_SLASH);
	sprintf (help_path, "%s%c%s%c%s%c", LIBDIR, PATH_SLASH, "help",
		 PATH_SLASH, intl_suffix, PATH_SLASH);
	printf ("Trying Message Path %s\n", message_path);
	if (!directory_exists(message_path)) {
	    sprintf (message_path, "%s%c%s%c", LIBDIR, PATH_SLASH, "messages",
		     PATH_SLASH);
	    printf ("Settling for message Path %s\n", message_path);
	}
	if (!directory_exists(help_path)) {
	    sprintf (help_path, "%s%c%s%c", LIBDIR, PATH_SLASH, "help",
		     PATH_SLASH);
	}
    } else {
	sprintf (message_path, "%s%c%s%c", LIBDIR, PATH_SLASH, "messages",
		 PATH_SLASH);
	sprintf (help_path, "%s%c%s%c", LIBDIR, PATH_SLASH, "help",
		 PATH_SLASH);
	printf ("Default message path %s\n", message_path);
    }
    sprintf (fontfile, "%s%c%s", opening_path, PATH_SLASH,
	     "iso8859-1-8x8.raw");
#if defined (WIN32)
    /* GCS: Use windows font for extra speed */
    strcpy (windowsfontfile, LIBDIR);
    if (!pix_double)
	strcat (windowsfontfile, "\\opening\\iso8859-1-8x8.fnt");
    else
	strcat (windowsfontfile, "\\opening\\iso8859-1-9x15.fnt");
#endif
    lc_temp_filename = (char *) malloc (lc_save_dir_len + 16);
    if (lc_temp_filename == 0) {
	malloc_failure ();
    }
    sprintf (lc_temp_filename, "%s%c%s", lc_save_dir, PATH_SLASH, "tmp-file");
}

void
verify_package (void)
{
    FILE *fp = fopen (colour_pal_file,"rb");
    if (!fp) {
	do_error ("Error. Can't find colour.pal.  "
		  "Did you forget `make install`?");
    }
    fclose (fp);
}

void
malloc_failure (void)
{
  printf ("Out of memory: malloc failure\n");
  exit (1);
}


char*
load_graphic(char *s)
{
    int x,l;
    char ss[100],*graphic;
    FILE *inf;
    strcpy(ss,graphic_path);
    strcat(ss,s);
    if ((inf=fopen(ss,"rb"))==NULL)
    {
	strcat(ss," -- UNABLE TO LOAD");
	do_error(ss);
    }
    fseek(inf,0L,SEEK_END);
    l=ftell(inf);
    fseek(inf,0L,SEEK_SET);
    graphic=(char *)malloc(l);
    for (x=0;x<l;x++)
	*(graphic+x)=fgetc(inf);
    fclose(inf);
    return(graphic);
}