#include "irc.h"
IRCII_RCSID("@(#)$eterna: scandir.c,v 1.47 2014/03/14 01:57:16 mrg Exp $");

#ifndef HAVE_SCANDIR

/*
 * Copyright (c) 1983 Regents of the University of California. All rights
 * reserved. 
 *
 * Redistribution and use in source and binary forms are permitted provided that
 * the above copyright notice and this paragraph are duplicated in all such
 * forms and that any documentation, advertising materials, and other
 * materials related to such distribution and use acknowledge that the
 * software was developed by the University of California, Berkeley.  The
 * name of the University may not be used to endorse or promote products
 * derived from this software without specific prior written permission. THIS
 * SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
 */

# if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)scandir.c	5.3 (Berkeley) 6/18/88";
# endif /* LIBC_SCCS and not lint */

/*
 * Scan the directory dirname calling select to make a list of selected
 * directory entries then sort using qsort and compare routine dcomp. Returns
 * the number of entries and a pointer to a list of pointers to struct direct
 * (through namelist). Returns -1 if there were any errors. 
 */

/*
 *  SCANDIR
 *  Scan a directory, collecting all (selected) items into a an array.
 */

#include <dirent.h>

#include "ircaux.h"
#include "scandir.h"
#include "newio.h"


/* Initial guess at directory size. */
# define INITIAL_SIZE	30

#ifndef DIRSIZ
# define DIRSIZ(d) (sizeof(struct dirent) + my_strlen(d->d_name) + 1) 
#endif /* !DIRSIZ */

int
irc_scandir(const char	*Name,
	struct dirent	***dirlist,
	int		(*Selector)(const struct dirent *),
	int		(*Sorter)(const void *, const void *))
{
	static struct dirent *E;
	struct dirent	**names;
	DIR		*Dp;
	int		i;
	int		size = INITIAL_SIZE;

	if (!(names = new_malloc(size * sizeof names[0])) ||
	     access(Name, R_OK | X_OK) || !(Dp = opendir(Name)))
		return -1;

	/* Read entries in the directory. */

	for (i = 0; (E = readdir(Dp)); )
		if (Selector == NULL || (*Selector)(E))
	{
		/* User wants them all, or he wants this one. */
		if (++i >= size)
		{
			size <<= 1;
			names = new_realloc(names, size * sizeof names[0]);
			if (names == NULL)
			{
				closedir(Dp);
				new_free(&names);
				return(-1);
			}
		}

		/* Copy the entry. */
		names[i - 1] = new_malloc(DIRSIZ(E));
		if (names[i - 1] == NULL)
		{ 
			closedir(Dp);
			new_free(&names);
			return(-1);
		}
#ifndef __QNX__
		names[i - 1]->d_ino = E->d_ino;
		names[i - 1]->d_reclen = E->d_reclen;
#endif /* !__QNX__ */
		my_strcpy(names[i - 1]->d_name, E->d_name);
	}

	/* Close things off. */
	names = new_realloc(names, (i + 1) * sizeof names[0]);
	names[i] = 0;
	*dirlist = names;
	closedir(Dp);

	/* Sort? */
	if (i && Sorter)
		qsort((char *)names, i, sizeof names[0], Sorter);

	return i;
}
#endif /* !HAVE_SCANDIR */
