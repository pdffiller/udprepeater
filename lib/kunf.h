/*** This file is copyrighted(c) and may only be used in terms of the LGPL ***/

#ifndef _kunf_h_
#define _kunf_h_

/*****************************************************************************/
/* Needed for the FILE structure *********************************************/

#include <stdio.h>

/*****************************************************************************/
/* Version information *******************************************************/

#define KUNFIG_VERSION "1.0"

/*****************************************************************************/
/* Modes of opening the kunf database ****************************************/

#define KUNFIG_OPEN_NOENV          0x0001  /* don't ever look at env var     */
#define KUNFIG_OPEN_DEFAULTINGENV  0x0002  /* look at env if arg fails       */
#define KUNFIG_OPEN_OVERRIDINGENV  0x0004  /* look at env before arg         */

#define KUNFIG_OPEN_SIMPLEACCESS   0x0010  /* no write, raw nor comment      */
#define KUNFIG_OPEN_COMPLEXACCESS  0x0020  /* write, raw and comment         */

#define KUNFIG_OPEN_PRINTERRORS    0x0100  /* print errors to stderr         */
#define KUNFIG_OPEN_LOGERRORS      0x0200  /* log errors to syslog           */
#define KUNFIG_OPEN_PRINTWARNINGS  0x0400  /* print warnings to stderr       */
#define KUNFIG_OPEN_LOGWARNINGS    0x0800  /* log warnings to syslog         */

#define KUNFIG_OPEN_EXITONERRORS   0x1000  /* do exit() if we get errors     */
#define KUNFIG_OPEN_EXITONWARNINGS 0x2000  /* do exit() on warnings          */

#define KUNFIG_OPEN_STANDARD       0x1511  /* sensible options               */
#define KUNFIG_OPEN_STANDALONE     0x0514  /* no /etc/root.kunf              */

/*****************************************************************************/
/* Return error codes: In most cases you should not need these ***************/

#define KUNFIG_ERROR_NONE          0x0000  /* everything peachy              */

#define KUNFIG_WARNING_ANY         0x0010  /* any warning                    */

#define KUNFIG_WARNING             0x00ff  /* warning mask                   */


#define KUNFIG_ERROR_MALLOC        0x0100  /* malloc/realloc failed          */
#define KUNFIG_ERROR_EOF           0x0200  /* read error in fgetc()          */
#define KUNFIG_ERROR_SYNTAX        0x0400  /* syntax error                   */
#define KUNFIG_ERROR_ACCESS        0x0800  /* could not open a file          */
#define KUNFIG_ERROR_CALL          0x1000  /* other functions failed         */
#define KUNFIG_ERROR_ANY           0x1000  /* any other error                */

#define KUNFIG_ERROR               0xff00  /* error mask                     */

/*****************************************************************************/
/* Make sure that C++ does not mangle the names ******************************/

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************/
/* You should not access these structures directly ***************************/

struct kunfig_item{
  char *name;
  char *rawname;

  char *value;
  char *rawvalue;

  char *desc;

  unsigned int flags;

  struct kunfig_item *next;
  struct kunfig_item *down;
};

typedef struct kunfig_item KITEM;

struct kunfig_state{
  unsigned int       error;
  unsigned int       flags;

  KITEM              *root;

  char            *scanbuf;
  unsigned int scanbufsize;

  int          esclen[256];
  char        *escstr[256];
  char             *escbuf;

  int                depth;
};

typedef struct kunfig_state KSTATE;

/*****************************************************************************/
/* The 3 functions below are all you need for a simple applications **********/

KSTATE       *kunfig_open        (char *fname, unsigned int flags);

extern int    kunfig_close       (KSTATE *k);

extern char  *kunfig_findvalue   (KSTATE *k, int number, char *first, ...);

/*****************************************************************************/
/* These functions could be required for a more complex applications *********/

extern KITEM *kunfig_findentry(KSTATE *k, int number, char *first, ...);

extern KITEM *kunfig_next     (KSTATE *k, KITEM *prev);
extern KITEM *kunfig_down     (KSTATE *k, KITEM *up);

extern KITEM *kunfig_start    (KSTATE *k);        /* get very first/top item */

extern int    kunfig_isentry  (KSTATE *k, KITEM *ptr); /* call before getvalue */
extern int    kunfig_isheader (KSTATE *k, KITEM *ptr); /* call this before down() */

extern char  *kunfig_getname  (KSTATE *k, KITEM *ptr); /* get current name   */
extern char  *kunfig_getvalue (KSTATE *k, KITEM *ptr); /* get current value  */

/*****************************************************************************/
/* The functions below should only be neccessary for a browser ***************/

extern int kunfig_isincludestart(KSTATE *k, KITEM *ptr); /* include entry    */

extern int kunfig_isincludeend  (KSTATE *k, KITEM *ptr); /* EOF in an included file */

extern int kunfig_isredirected  (KSTATE *k, KITEM *ptr); /* a section in a new file */

extern char *kunfig_getrawname  (KSTATE *k, KITEM *ptr); /* no % escapes in name*/

extern char *kunfig_getrawvalue (KSTATE *k, KITEM *ptr); /* no % escapes in value */

extern char *kunfig_getdescription(KSTATE *k, KITEM *ptr); /* get description*/

extern char *kunfig_getfilename (KSTATE *k, KITEM *ptr); /* filename of inc/redir */

/*****************************************************************************/
/* The next set of functions should only be required by an editor ************/

extern int kunfig_setrawvalue(KSTATE *k, KITEM *ptr, char *rawvalue); /* set value */

extern int kunfig_writeback  (KSTATE *k);           /* flush changes to disk */

/*****************************************************************************/
/* These 2 functions can be used to figure out errors ************************/

extern int kunfig_printerror(KSTATE *k, FILE *stream);

extern int kunfig_waserror  (KSTATE *k);         /* true if an error occured */

/*****************************************************************************/
/* Make sure that C++ does not mangle the names ******************************/

#ifdef __cplusplus
}
#endif

#endif
