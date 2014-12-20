/*	$NetBSD: getcap.c,v 1.39 2003/10/27 00:12:42 lukem Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Casey Leedom of Lawrence Livermore National Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if !defined(__linux__)
#include <sys/cdefs.h>
#if !defined(lint)
#if 0
static char sccsid[] = "@(#)getcap.c	8.3 (Berkeley) 3/25/94";
#else
__RCSID("$NetBSD: getcap.c,v 1.39 2003/10/27 00:12:42 lukem Exp $");
#endif
#endif /* not lint */
#endif /* !__linux__ */

#include <db.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "bsd_compat.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN PATH_MAX
#endif

#ifndef MAX_RECURSION
#define MAX_RECURSION (unsigned long)2147483647 /* (2^31 - 1)  for Linux */
#endif

#define	BFRAG		1024
#define	SFRAG		100		/* cgetstr mallocs in SFRAG chunks */

#define TCERR	(char)1
#define	SHADOW	(char)2

static size_t topreclen;	/* toprec length */
static char *toprec;	/* Additional record specified by cgetset() */

static int cdbget(DB *capdbp, char **bp, const char *name)
{
	DBT key;
	DBT data;

	assert(capdbp != NULL);
	assert(bp != NULL);
	assert(name != NULL);

	/* LINTED key is not modified */
	key.data = (char *)name;
	key.size = strlen(name);

	for (;;) {
		/* Get the reference. */
		switch(capdbp->get(capdbp, NULL, &key, &data, 0)) {
		case -1:
			return (-2);
		case 1:
			return (-1);
		}

		/* If not an index to another record, leave. */
		if (((char *)data.data)[0] != SHADOW)
			break;

		key.data = (char *)data.data + 1;
		key.size = data.size - 1;
	}
	
	*bp = (char *)data.data + 1;
	return (((char *)(data.data))[0] == TCERR ? 1 : 0);
}

/*
 * Compare name field of record.
 */
static int nfcmp(char *nf, char *rec)
{
	char *cp, tmp;
	int ret;

	assert(nf != NULL);
	assert(rec != NULL);

	for (cp = rec; *cp != ':'; cp++)
		;
	
	tmp = *(cp + 1);
	*(cp + 1) = '\0';
	ret = strcmp(nf, rec);
	*(cp + 1) = tmp;

	return (ret);
}

/*
 * Cgetmatch will return 0 if name is one of the names of the capability
 * record buf, -1 if not.
 */
int cgetmatch(const char *buf, char *name)
{
	const char *np, *bp;

	assert(buf != NULL);
	assert(name != NULL);

	/*
	 * Start search at beginning of record.
	 */
	bp = buf;
	for (;;) {
		/*
		 * Try to match a record name.
		 */
		np = name;
		for (;;)
			if (*np == '\0') {
				if (*bp == '|' || *bp == ':' || *bp == '\0')
					return (0);
				else
					break;
			} else
				if (*bp++ != *np++)
					break;

		/*
		 * Match failed, skip to next name in record.
		 */
		if (bp > buf)
			bp--;	/* a '|' or ':' may have stopped the match */
		else
			return (-1);
		for (;;)
			if (*bp == '\0' || *bp == ':')
				return (-1);	/* match failed totally */
			else
				if (*bp++ == '|')
					break;	/* found next name */
	}
}

/*
 * Getent implements the functions of cgetent.  If fd is non-negative,
 * *db_array has already been opened and fd is the open file descriptor.  We
 * do this to save time and avoid using up file descriptors for tc=
 * recursions.
 *
 * Getent returns the same success/failure codes as cgetent.  On success, a
 * pointer to a malloc'ed capability record with all tc= capabilities fully
 * expanded and its length (not including trailing ASCII NUL) are left in
 * *cap and *len.
 *
 * Basic algorithm:
 *	+ Allocate memory incrementally as needed in chunks of size BFRAG
 *	  for capability buffer.
 *	+ Recurse for each tc=name and interpolate result.  Stop when all
 *	  names interpolated, a name can't be found, or depth exceeds
 *	  MAX_RECURSION.
 */
static int getent(char **cap, char **db_array, char *nfield, const char *name, size_t *len, int fd, int depth)
{
	DB *capdbp = NULL;
	char *r_end, *rp = NULL, **db_p;	/* pacify gcc */
	int myfd = 0, eof, foundit, retval;
	size_t clen;
	char *record, *cbuf, *newrecord;
	int tc_not_resolved;
	char pbuf[MAXPATHLEN];

	assert(cap != NULL);
	assert(len != NULL);
	assert(db_array != NULL);
	/* fd may be -1 */
	assert(name != NULL);
	/* nfield may be NULL */

	/*
	 * Return with ``loop detected'' error if we've recursed more than
	 * MAX_RECURSION times.
	 */
	if (depth > MAX_RECURSION)
		return (-3);

	/*
	 * Check if we have a top record from cgetset().
         */
	if (depth == 0 && toprec != NULL && cgetmatch(toprec, strdup(name)) == 0) {
		if ((record = malloc (topreclen + BFRAG)) == NULL) {
			errno = ENOMEM;
			return (-2);
		}
		(void)strcpy(record, toprec);	/* XXX: strcpy is safe */
		db_p = db_array;
		rp = record + topreclen + 1;
		r_end = rp + BFRAG;
		goto tc_exp;
	}
	/*
	 * Allocate first chunk of memory.
	 */
	if ((record = malloc(BFRAG)) == NULL) {
		errno = ENOMEM;
		return (-2);
	}
	r_end = record + BFRAG;
	foundit = 0;
	/*
	 * Loop through database array until finding the record.
	 */

	for (db_p = db_array; *db_p != NULL; db_p++) {
		eof = 0;

		/*
		 * Open database if not already open.
		 */

		if (fd >= 0) {
			(void)lseek(fd, (off_t)0, SEEK_SET);
		} else {
			(void)snprintf(pbuf, sizeof(pbuf), "%s.db", *db_p);
#if defined(__linux__)
			if ((capdbp->open(capdbp, NULL, pbuf, "", DB_HASH, DB_RDONLY, 0)) == 0) {
#else
			if ((capdbp = dbopen(pbuf, O_RDONLY, 0, DB_HASH, 0)) != NULL) {
#endif /* !__linux__ */
				free(record);
				retval = cdbget(capdbp, &record, name);
				if (retval < 0) {
					/* no record available */
					(void)capdbp->close(capdbp, 0);
					return (retval);
				}
				/* save the data; close frees it */
				clen = strlen(record);
				cbuf = malloc(clen + 1);
				memmove(cbuf, record, clen + 1);
				if (capdbp->close(capdbp, 0) < 0) {
					int serrno = errno;

					free(cbuf);
					errno = serrno;
					return (-2);
				}
				*len = clen;
				*cap = cbuf;
				return (retval);
			} else {
				fd = open(*db_p, O_RDONLY, 0);
				if (fd < 0) {
					/* No error on unfound file. */
					continue;
				}
				myfd = 1;
			}
		}
		/*
		 * Find the requested capability record ...
		 */
		{
		char buf[BUFSIZ];
		char *b_end, *bp, *cp;
		int c, slash;

		/*
		 * Loop invariants:
		 *	There is always room for one more character in record.
		 *	R_end always points just past end of record.
		 *	Rp always points just past last character in record.
		 *	B_end always points just past last character in buf.
		 *	Bp always points at next character in buf.
		 *	Cp remembers where the last colon was.
		 */
		b_end = buf;
		bp = buf;
		cp = 0;
		slash = 0;
		for (;;) {

			/*
			 * Read in a line implementing (\, newline)
			 * line continuation.
			 */
			rp = record;
			for (;;) {
				if (bp >= b_end) {
					int n;

					n = read(fd, buf, sizeof(buf));
					if (n <= 0) {
						if (myfd)
							(void)close(fd);
						if (n < 0) {
							int serrno = errno;

							free(record);
							errno = serrno;
							return (-2);
						} else {
							fd = -1;
							eof = 1;
							break;
						}
					}
					b_end = buf+n;
					bp = buf;
				}

				c = *bp++;
				if (c == '\n') {
					if (slash) {
						slash = 0;
						rp--;
						continue;
					} else
						break;
				}
				if (slash) {
					slash = 0;
					cp = 0;
				}
				if (c == ':') {
					/*
					 * If the field was `empty' (i.e.
					 * contained only white space), back up
					 * to the colon (eliminating the
					 * field).
					 */
					if (cp)
						rp = cp;
					else
						cp = rp;
				} else if (c == '\\') {
					slash = 1;
				} else if (c != ' ' && c != '\t') {
					/*
					 * Forget where the colon was, as this
					 * is not an empty field.
					 */
					cp = 0;
				}
				*rp++ = c;

				/*
				 * Enforce loop invariant: if no room 
				 * left in record buffer, try to get
				 * some more.
				 */
				if (rp >= r_end) {
					u_int pos;
					size_t newsize;

					pos = rp - record;
					newsize = r_end - record + BFRAG;
					newrecord = realloc(record, newsize);
					if (newrecord == NULL) {
						free(record);
						if (myfd)
							(void)close(fd);
						errno = ENOMEM;
						return (-2);
					}
					record = newrecord;
					r_end = record + newsize;
					rp = record + pos;
				}
			}
			/* Eliminate any white space after the last colon. */
			if (cp)
				rp = cp + 1;
			/* Loop invariant lets us do this. */
			*rp++ = '\0';

			/*
			 * If encountered eof check next file.
			 */
			if (eof)
				break;

			/*
			 * Toss blank lines and comments.
			 */
			if (*record == '\0' || *record == '#')
				continue;

			/*
			 * See if this is the record we want ...
			 */
			if (cgetmatch(record, strdup(name)) == 0) {
				if (nfield == NULL || !nfcmp(nfield, record)) {
					foundit = 1;
					break;	/* found it! */
				}
			}
		}
	}
		if (foundit)
			break;
	}

	if (!foundit)
		return (-1);

	/*
	 * Got the capability record, but now we have to expand all tc=name
	 * references in it ...
	 */
tc_exp:	{
		char *newicap, *s;
		size_t ilen, newilen;
		int diff, iret, tclen;
		char *icap, *scan, *tc, *tcstart, *tcend;

		/*
		 * Loop invariants:
		 *	There is room for one more character in record.
		 *	R_end points just past end of record.
		 *	Rp points just past last character in record.
		 *	Scan points at remainder of record that needs to be
		 *	scanned for tc=name constructs.
		 */
		scan = record;
		tc_not_resolved = 0;
		for (;;) {
			if ((tc = cgetcap(scan, "tc", '=')) == NULL)
				break;

			/*
			 * Find end of tc=name and stomp on the trailing `:'
			 * (if present) so we can use it to call ourselves.
			 */
			s = tc;
			for (;;)
				if (*s == '\0')
					break;
				else
					if (*s++ == ':') {
						*(s - 1) = '\0';
						break;
					}
			tcstart = tc - 3;
			tclen = s - tcstart;
			tcend = s;

			iret = getent(&icap, (char **)&ilen, (char *)db_p, (const char *)&fd, (size_t *)tc, depth+1, 0);
			newicap = icap;		/* Put into a register. */
			newilen = ilen;
			if (iret != 0) {
				/* an error */
				if (iret < -1) {
					if (myfd)
						(void)close(fd);
					free(record);
					return (iret);
				}
				if (iret == 1)
					tc_not_resolved = 1;
				/* couldn't resolve tc */
				if (iret == -1) {
					*(s - 1) = ':';
					scan = s - 1;
					tc_not_resolved = 1;
					continue;
				}
			}
			/* not interested in name field of tc'ed record */
			s = newicap;
			for (;;)
				if (*s == '\0')
					break;
				else
					if (*s++ == ':')
						break;
			newilen -= s - newicap;
			newicap = s;

			/* make sure interpolated record is `:'-terminated */
			s += newilen;
			if (*(s-1) != ':') {
				*s = ':';	/* overwrite NUL with : */
				newilen++;
			}

			/*
			 * Make sure there's enough room to insert the
			 * new record.
			 */
			diff = newilen - tclen;
			if (diff >= r_end - rp) {
				u_int pos, tcpos, tcposend;
				size_t newsize;

				pos = rp - record;
				newsize = r_end - record + diff + BFRAG;
				tcpos = tcstart - record;
				tcposend = tcend - record;
				newrecord = realloc(record, newsize);
				if (newrecord == NULL) {
					free(record);
					if (myfd)
						(void)close(fd);
					free(icap);
					errno = ENOMEM;
					return (-2);
				}
				record = newrecord;
				r_end = record + newsize;
				rp = record + pos;
				tcstart = record + tcpos;
				tcend = record + tcposend;
			}

			/*
			 * Insert tc'ed record into our record.
			 */
			s = tcstart + newilen;
			memmove(s, tcend,  (size_t)(rp - tcend));
			memmove(tcstart, newicap, newilen);
			rp += diff;
			free(icap);

			/*
			 * Start scan on `:' so next cgetcap works properly
			 * (cgetcap always skips first field).
			 */
			scan = s-1;
		}
	}
	/*
	 * Close file (if we opened it), give back any extra memory, and
	 * return capability, length and success.
	 */
	if (myfd)
		(void)close(fd);
	*len = rp - record - 1;	/* don't count NUL */
	if (r_end > rp) {
		if ((newrecord = realloc(record, (size_t)(rp - record))) == NULL) {
			free(record);
			errno = ENOMEM;
			return (-2);
		}
		record = newrecord;
	}

	*cap = record;
	if (tc_not_resolved)
		return (1);
	return (0);
}

/*
 * Cgetent extracts the capability record name from the NULL terminated file
 * array db_array and returns a pointer to a malloc'd copy of it in buf.
 * Buf must be retained through all subsequent calls to cgetcap, cgetnum,
 * cgetflag, and cgetstr, but may then be free'd.  0 is returned on success,
 * -1 if the requested record couldn't be found, -2 if a system error was
 * encountered (couldn't open/read a file, etc.), and -3 if a potential
 * reference loop is detected.
 */
int cgetent(char **buf, char **db_array, const char *name)
{
	size_t dummy;

	assert(buf != NULL);
	assert(db_array != NULL);
	assert(name != NULL);

	return (getent(buf, (char **)&dummy, (char *)db_array, (const char *)-1, (size_t *)name, 0, 0));
}

/*
 * Cgetcap searches the capability record buf for the capability cap with
 * type `type'.  A pointer to the value of cap is returned on success, NULL
 * if the requested capability couldn't be found.
 *
 * Specifying a type of ':' means that nothing should follow cap (:cap:).
 * In this case a pointer to the terminating ':' or NUL will be returned if
 * cap is found.
 *
 * If (cap, '@') or (cap, terminator, '@') is found before (cap, terminator)
 * return NULL.
 */
char *cgetcap(char *buf, const char *cap, int type)
{
	char *bp;
	const char *cp;

	assert(buf != NULL);
	assert(cap != NULL);

	bp = buf;
	for (;;) {
		/*
		 * Skip past the current capability field - it's either the
		 * name field if this is the first time through the loop, or
		 * the remainder of a field whose name failed to match cap.
		 */
		for (;;)
			if (*bp == '\0')
				return (NULL);
			else
				if (*bp++ == ':')
					break;

		/*
		 * Try to match (cap, type) in buf.
		 */
		for (cp = cap; *cp == *bp && *bp != '\0'; cp++, bp++)
			continue;
		if (*cp != '\0')
			continue;
		if (*bp == '@')
			return (NULL);
		if (type == ':') {
			if (*bp != '\0' && *bp != ':')
				continue;
			return(bp);
		}
		if (*bp != type)
			continue;
		bp++;
		return (*bp == '@' ? NULL : bp);
	}
	/* NOTREACHED */
}

/*
 * Cgetustr retrieves the value of the string capability cap from the
 * capability record pointed to by buf.  The difference between cgetustr()
 * and cgetstr() is that cgetustr does not decode escapes but rather treats
 * all characters literally.  A pointer to a  NUL terminated malloc'd 
 * copy of the string is returned in the char pointed to by str.  The 
 * length of the string not including the trailing NUL is returned on success,
 * -1 if the requested string capability couldn't be found, -2 if a system 
 * error was encountered (storage allocation failure).
 */
int cgetustr(char *buf, const char *cap, char **str)
{
	u_int m_room;
	const char *bp;
	char *mp;
	int len;
	char *mem, *newmem;

	assert(buf != NULL);
	assert(cap != NULL);
	assert(str != NULL);

	/*
	 * Find string capability cap
	 */
	if ((bp = cgetcap(buf, cap, '=')) == NULL)
		return (-1);

	/*
	 * Conversion / storage allocation loop ...  Allocate memory in
	 * chunks SFRAG in size.
	 */
	if ((mem = malloc(SFRAG)) == NULL) {
		errno = ENOMEM;
		return (-2);	/* couldn't even allocate the first fragment */
	}
	m_room = SFRAG;
	mp = mem;

	while (*bp != ':' && *bp != '\0') {
		/*
		 * Loop invariants:
		 *	There is always room for one more character in mem.
		 *	Mp always points just past last character in mem.
		 *	Bp always points at next character in buf.
		 */
		*mp++ = *bp++;
		m_room--;

		/*
		 * Enforce loop invariant: if no room left in current
		 * buffer, try to get some more.
		 */
		if (m_room == 0) {
			size_t size = mp - mem;

			if ((newmem = realloc(mem, size + SFRAG)) == NULL) {
				free(mem);
				return (-2);
			}
			mem = newmem;
			m_room = SFRAG;
			mp = mem + size;
		}
	}
	*mp++ = '\0';	/* loop invariant let's us do this */
	m_room--;
	len = mp - mem - 1;

	/*
	 * Give back any extra memory and return value and success.
	 */
	if (m_room != 0) {
		if ((newmem = realloc(mem, (size_t)(mp - mem))) == NULL) {
			free(mem);
			return (-2);
		}
		mem = newmem;
	}
	*str = mem;
	return (len);
}
