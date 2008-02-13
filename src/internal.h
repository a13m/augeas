/*
 * internal.h: Useful definitions
 *
 * Copyright (C) 2007 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 * Author: David Lutterkort <dlutter@redhat.com>
 */

#ifndef __INTERNAL_H
#define __INTERNAL_H

#define DEBUG

#include "list.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#ifdef __GNUC__
#ifdef HAVE_ANSIDECL_H
#include <ansidecl.h>
#endif

/* String equality tests, suggested by Jim Meyering. */
#define STREQ(a,b) (strcmp((a),(b)) == 0)
#define STRCASEEQ(a,b) (strcasecmp((a),(b)) == 0)
#define STRNEQ(a,b) (strcmp((a),(b)) != 0)
#define STRCASENEQ(a,b) (strcasecmp((a),(b)) != 0)
#define STREQLEN(a,b,n) (strncmp((a),(b),(n)) == 0)
#define STRNEQLEN(a,b,n) (strncmp((a),(b),(n)) != 0)

/* Path length and comparison */

#define SEP '/'

/* Length of PATH without any trailing '/' */
__attribute__((pure))
static inline int pathlen(const char *path) {
    int len = strlen(path);

    if (len > 0 && path[len-1] == SEP)
        len--;

    return len;
}

/* Return 1 if P1 is a prefix of P2. P1 as a string must have length <= P2 */
__attribute__((pure))
static inline int pathprefix(const char *p1, const char *p2) {
    int l1 = pathlen(p1);

    return STREQLEN(p1, p2, l1) && (p2[l1] == '\0' || p2[l1] == SEP);
}

/* Strip the first component from P and return the part of P after the
   first SEP. If P contains no SEP, or the next occurence of SEP in P is at
   the end of P, return NULL
*/
__attribute__((pure))
static inline const char *pathstrip(const char *p) {
    const char *c = strchr(p, SEP);
    if (c != NULL) {
        c += 1;
        return (*c == '\0') ? NULL : c;
    } else {
        return NULL;
    }
}

/* Call calloc to allocate an array of N instances of *VAR */
#define CALLOC(Var,N) do { (Var) = calloc ((N), sizeof (*(Var))); } while (0)

/**
 * ATTRIBUTE_UNUSED:
 *
 * Macro to flag conciously unused parameters to functions
 */
#ifndef ATTRIBUTE_UNUSED
#define ATTRIBUTE_UNUSED __attribute__((__unused__))
#endif

/**
 * ATTRIBUTE_FORMAT
 *
 * Macro used to check printf/scanf-like functions, if compiling
 * with gcc.
 */
#ifndef ATTRIBUTE_FORMAT
#define ATTRIBUTE_FORMAT(args...) __attribute__((__format__ (args)))
#endif

#else
#define ATTRIBUTE_UNUSED
#define ATTRIBUTE_FORMAT(...)
#endif

/**
 * TODO:
 *
 * macro to flag unimplemented blocks
 */
#define TODO 								\
    fprintf(stderr, "%s:%d Unimplemented block\n",			\
            __FILE__, __LINE__);

#define FIXME(msg, args ...)                            \
    do {                                                \
      fprintf(stderr, "%s:%d Unhandled error ",			\
              __FILE__, __LINE__);                      \
      fprintf(stderr, msg, ## args);                    \
      fputc('\n', stderr);                              \
    } while(0)

/*
 * Internal data structures
 */

/*
 * File tokenizing
 */
struct aug_file {
    const char *name;  // The absolute file name
    const char *node;  // The node in the tree for this file
    struct ast *ast;
    struct aug_file  *next;
};

// internal.c
void aug_file_free(struct aug_file *af);

/* Allocate a new file. NAME and NODE are dup'd */
struct aug_file *aug_make_file(const char *name, const char *node);

/* Read the contents of file PATH and return them as one long string. The
 * caller must free the result. Return NULL if any error occurs.
 */
const char* aug_read_file(const char *path);

/*
 * Provider. Should eventually be the main interface between the tree
 * and the handling of individual config files
 */

struct aug_provider {
    const char *name;
    int (*init)(void);
    int (*load)(void);
    int (*save)(void);
};

#endif


/*
 * Local variables:
 *  indent-tabs-mode: nil
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
