/*
 * augeas.c: the core data structure for storing key/value pairs
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

#include "augeas.h"
#include "internal.h"

#include <fnmatch.h>

/* Two special entries: they are always on the main list
   so that we don't need to worry about some corner cases in dealing
   with empty lists */

#define P_ROOT   "system"
#define P_SYSTEM_CONFIG "system/config"

/* An entry in the global config tree. The data structure allows associating
 * values with interior nodes, but the API currently marks that as an error.
 */
struct tree {
    struct tree *next;
    const char  *label;      /* Last component of PATH */
    struct tree *children;   /* List of children through NEXT */
    const char  *value;
    int          dirty;
};

static struct tree *aug_tree = NULL;

/* Hardcoded list of existing providers. Ultimately, they should be created
 * from a metadata description, not in code
 */

/* Provider for parsed files (prov_spec.c) */
extern const struct aug_provider augp_spec;

static const struct aug_provider *providers[] = {
    &augp_spec,
    NULL
};

static struct tree *aug_tree_find(struct tree *tree, const char *path) {
    if (path == NULL)
        return NULL;
    if (*path == SEP)
        path += 1;

    while (tree != NULL) {
        if (tree->label != NULL) {
            if (STREQ(tree->label, path))
                return tree;
            if (tree->children != NULL && pathprefix(tree->label, path)) {
                struct tree *t = 
                    aug_tree_find(tree->children, pathstrip(path));
                if (t != NULL)
                    return t;
            }
        }
        tree = tree->next;
    }
    return NULL;
}

static struct tree *aug_tree_create(const char *path) {
    struct tree *tree;

    CALLOC(tree, 1);
    
    const char *end = pathstrip(path);
    if (end == NULL) {
        tree->label = strdup(path);
        char *l = (char *) tree->label + strlen(path) - 1;
        if (*l == SEP)
            *l = '\0';
    } else {
        tree->label = strndup(path, end - path - 1);
        tree->children = aug_tree_create(end);
    }
    
    return tree;
}

static struct tree *aug_tree_find_or_create(const char *path, 
                                            struct tree *tree) {
    if (*path == SEP) path += 1;

    list_for_each(t, tree) {
        if (STREQ(t->label, path))
            return t;
        if (pathprefix(t->label, path)) {
            if (t->children == NULL) {
                t->children = aug_tree_create(pathstrip(path));
                while (t->children != NULL)
                    t = t->children;
                return t;
            }
            else
                return aug_tree_find_or_create(pathstrip(path), t->children);
        }
    }
    
    struct tree *new = aug_tree_create(path);
    list_append(tree, new);
    while (new->children != NULL)
        new = new->children;
    return new;
}

static void aug_tree_free(struct tree *tree) {
    if (tree != NULL) {
        free((void *) tree->label);
        free(tree);
    }
}

int aug_init(void) {
    if (aug_tree == NULL) {
        CALLOC(aug_tree, 1);
        if (aug_tree == NULL)
            return -1;

        aug_tree->label = strdup(P_ROOT);
        aug_tree_find_or_create(P_SYSTEM_CONFIG, aug_tree);
    }

    for (int i=0; providers[i] != NULL; i++) {
        const struct aug_provider *prov = providers[i];
        int r;
        r = prov->init();
        if (r == -1)
            return -1;
        r = prov->load();
        if (r == -1)
            return -1;
    }
    return 0;
}

const char *aug_get(const char *path) {
    struct tree *tree;

    tree = aug_tree_find(aug_tree, path);
    if (tree != NULL)
        return tree->value;

    return NULL;
}

int aug_set(const char *path, const char *value) {
    struct tree *tree;

    tree = aug_tree_find(aug_tree, path);
    if (tree == NULL) {
        tree = aug_tree_find_or_create(path, aug_tree);
        if (tree == NULL)
            return -1;
    }
    if (tree->value != NULL) {
        free((char *) tree->value);
    }
    tree->value = strdup(value);
    tree->dirty = 1;
    if (tree->value == NULL)
        return -1;
    return 0;
}

int aug_exists(const char *path) {
    return (aug_tree_find(aug_tree, path) != NULL);
}

int aug_insert(const char *path, const char *sibling) {
    struct tree *parent, *prev = NULL, *new = NULL;
    char *pathdup = NULL, *label;

    if (path == NULL || sibling == NULL || STREQ(path, sibling))
        goto error;

    pathdup = strdup(path);
    if (pathdup == NULL)
        goto error;

    label = strrchr(pathdup, SEP);
    if (label == NULL)
        goto error;
    *label = '\0';
    
    if (STRNEQLEN(pathdup, sibling, strlen(pathdup)))
        goto error;
    sibling = sibling + strlen(pathdup) + 1;

    parent = aug_tree_find(aug_tree, pathdup);
    *label = SEP;

    if (parent == NULL)
        goto error;
    
    label += 1;
    list_for_each(t, parent->children) {
        if (STREQ(t->label, label))
            goto error;
        if (t->next != NULL && STREQ(t->next->label, sibling))
            prev = t;
    }

    if (prev == NULL)
        return -1;

    CALLOC(new, 1);
    new->label = strdup(label);
    new->next = prev->next;
    prev->next = new;
    parent->dirty = 1;

    free(pathdup);
    return 0;
 error:
    free(pathdup);
    free(new);
    return -1;
}

static int del_rec(struct tree *tree) {
    int cnt = 0;

    while (tree != NULL) {
        struct tree *del = tree;
        tree = del->next;
        cnt += del_rec(del->children);
        aug_tree_free(del);
        cnt += 1;
    }

    return cnt;
}

int aug_rm(const char *path) {
    char *ppath = strdupa(path);
    char *pend = strrchr(ppath, SEP);
    char *label = NULL;

    if (ppath[strlen(ppath)-1] == SEP)
        ppath[strlen(ppath)] = '\0';

    if (pend == NULL)
        return -1;
    *pend = '\0';
    label = pend + 1;

    struct tree *parent = aug_tree_find(aug_tree, ppath);
    if (parent == NULL)
        return 0;

    struct tree *del;
    if (STREQ(label, parent->children->label)) {
        del = parent->children;
        parent->children = del->next;
    } else {
        struct tree *prev;
        for (prev=parent->children;
             prev->next != NULL && STRNEQ(label, prev->next->label);
             prev = prev->next);
        if (prev->next == NULL)
            return 0;
        del = prev->next;
        prev->next = del->next;
    }

    int cnt = del_rec(del->children);
    aug_tree_free(del);
    parent->dirty = 1;
    return cnt + 1;
}

int aug_ls(const char *path, const char ***children) {
    struct tree *tree = NULL;
    int cnt = 0;

    tree = aug_tree_find(aug_tree, path);
    if (tree == NULL || tree->children == NULL)
        return 0;

    tree = tree->children;
    for (struct tree *t = tree; t != NULL; cnt++, t = t->next);

    if (children == NULL)
        return cnt;

    *children = calloc(cnt, sizeof(char *));
    if (*children == NULL)
        return -1;

    for (int i=0; i < cnt; i++, tree = tree->next) {
        int len = strlen(path) + 1 + strlen(tree->label) + 1;
        char *p = malloc(len);
        snprintf(p, len, "%s/%s", path, tree->label);
        (*children)[i] = p;
    }
    return cnt;
}

static int match_rec(struct tree *tree, const char *pattern,
                     char **path,
                     const char **matches, int size) {

    int cnt = 0;
    int end = strlen(*path);

    while (tree != NULL) {
        *path = realloc(*path, end + 1 + strlen(tree->label) + 1);
        (*path)[end] = SEP;
        strcpy(*path + end + 1, tree->label);

        if (fnmatch(pattern, *path, FNM_NOESCAPE) == 0) {
            if (size > 0) {
                *matches = strdup(*path);
                matches += 1;
                size -= 1;
            }
            cnt += 1;
        }
        int n = match_rec(tree->children, pattern, path, matches, size);
        cnt += n;
        matches += n;
        size -= n;
        tree = tree->next;
    }

    (*path)[end] = '\0';
    return cnt;
}

int aug_match(const char *pattern, const char **matches, int size) {
    char *path = calloc(10, 1);
    int n = match_rec(aug_tree, pattern, &path, matches, size);
    free(path);
    return n;
}

int aug_save(void) {
    int r;

    for (int i=0; providers[i] != NULL; i++) {
        r = providers[i]->save();
        if (r == -1)
            return -1;
    }
    return 0;
}

static void print_rec(FILE *out, struct tree *tree, char **path) {
    int end = strlen(*path);

    while (tree != NULL) {
        *path = realloc(*path, strlen(*path) + 1 + strlen(tree->label) + 1);
        (*path)[end] = SEP;
        strcpy(*path + end + 1, tree->label);

        fprintf(out, *path);
        if (tree->value != NULL)
            fprintf(out, " = %s", tree->value);
        fputc('\n', out);
        print_rec(out, tree->children, path);
        tree = tree->next;
    }
    
    (*path)[end] = '\0';
}

void aug_print(FILE *out, const char *path) {
    char *pbuf = calloc(10, 1);
    struct tree *tree = aug_tree_find(aug_tree, path);
    if (tree == NULL)
        tree = aug_tree;
    print_rec(out, tree, &pbuf);
    free(pbuf);
}

/*
 * Local variables:
 *  indent-tabs-mode: nil
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
