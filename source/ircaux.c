/*
 * ircaux.c: some extra routines... not specific to irc... that I needed 
 *
 * Written By Michael Sandrof
 *
 * Copyright (c) 1990, 1991 Michael Sandrof.
 * Copyright (c) 1991, 1992 Troy Rollo.
 * Copyright (c) 1992-2014 Matthew R. Green.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "irc.h"
IRCII_RCSID("@(#)$eterna: ircaux.c,v 1.114 2014/08/06 19:49:01 mrg Exp $");

#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif /* HAVE_SYS_UN_H */

#include <pwd.h>

#include "ircaux.h"
#include "output.h"
#include "ircterm.h"
#include "newio.h"
#include "server.h"

static int bind_local_addr(u_char *, int, int);


/*
 * new_free:  Why do this?  Why not?  Saves me a bit of trouble here and
 * there 
 */
void
new_free(void *iptr)
{
	void	**ptr = (void **) iptr;
#ifdef DO_USER2
	int	oldmask;
#endif /* DO_USER2 */

	/* cheap hack. */
	if (*ptr == empty_string() || *ptr == zero() || *ptr == one())
		*ptr = NULL;
	else if (*ptr)
	{
#ifdef DO_USER2
		oldmask = sigblock(sigmask(SIGUSR2));
#endif /* DO_USER2 */
#ifdef FREE_DEBUG
		if (free(*ptr) < 0)
			put_it("*debug* free failed '%s'", (u_char *) ptr);
#else
		free(*ptr);
#endif /* FREE_DEBUG */
#ifdef DO_USER2
		sigblock(oldmask);
#endif /* DO_USER2 */
		*ptr = NULL;
	}
}

#define WAIT_BUFFER 2048
static u_char * wait_pointers[WAIT_BUFFER] = {0}, **current_wait_ptr = wait_pointers;

/*
 * wait_new_free: same as new_free() except that free() is postponed.
 */
void
wait_new_free(u_char **ptr)
{
	if (*current_wait_ptr)
		new_free(current_wait_ptr);
	*current_wait_ptr++ = *ptr;
	if (current_wait_ptr >= wait_pointers + WAIT_BUFFER)
		current_wait_ptr = wait_pointers;
	*ptr = NULL;
}

/*
 * really_free: really free the data if level == 0
 */
void
really_free(int level)
{
	if (level != 0)
		return;
	for (current_wait_ptr = wait_pointers; current_wait_ptr < wait_pointers + WAIT_BUFFER; current_wait_ptr++)
		if (*current_wait_ptr)
			new_free(current_wait_ptr);
	current_wait_ptr = wait_pointers;
}

void	*
new_realloc(void *ptr, size_t size)
{
	void	*new_ptr;

	if ((new_ptr = realloc(ptr, size)) == NULL)
	{
		fprintf(stderr, "realloc failed (%d): %s\nIrc Aborted!\n",
			(int)size, strerror(errno));
		exit(1);
	}
	return (new_ptr);
}

void	*
new_malloc(size_t size)
{
	void	*ptr;

	if ((ptr = malloc(size)) == NULL)
	{
		static	char	error[] = "Malloc failed: \nIrc Aborted!\n";

		(void)write(2, error, my_strlen(error));
		(void)write(2, strerror(errno), my_strlen(strerror(errno)));
		term_reset();
		exit(1);
	}
	return (ptr);
}

/*
 * malloc_strcpy:  Mallocs enough space for src to be copied in to where
 * ptr points to.
 *
 * Never call this with ptr pointing to an uninitialised string, as the
 * call to new_free() might crash the client... - phone, jan, 1993.
 */
void
malloc_strcpy(u_char **ptr, u_char *src)
{
	malloc_strncpy(ptr, src, 0);
}

void
malloc_strncpy(u_char **ptr, u_char *src, size_t extra)
{
	/* no point doing anything else */
	if (src == *ptr)
		return;

	new_free(ptr);
	/* cheap hack. */
	if ((src == empty_string() || src == zero() || src == one()) && extra == 0)
		*ptr = src;
	else if (src)
	{
		*ptr = new_malloc(my_strlen(src) + 1 + extra);
		my_strcpy(*ptr, src);
	}
	else
		*ptr = NULL;
}

/* malloc_strcat: Yeah, right */
void
malloc_strcat(u_char **ptr, u_char *src)
{
	malloc_strncat(ptr, src, 0);
}

/* malloc_strncat: Yeah, right */
void
malloc_strncat(u_char **ptr, u_char *src, size_t extra)
{
	u_char	*new;

	if (*ptr)
	{
		new = new_malloc(my_strlen(*ptr) + my_strlen(src) + 1 + extra);
		my_strcpy(new, *ptr);
		my_strcat(new, src);
		new_free(ptr);
		*ptr = new;
	}
	else
		malloc_strcpy(ptr, src);
}

void
malloc_strcat_ue(u_char **ptr, u_char *src)
{
	u_char	*new;

	if (*ptr)
	{
		size_t len = my_strlen(*ptr) + my_strlen(src) + 1;

		new = new_malloc(len);
		my_strcpy(new, *ptr);
		strmcat_ue(new, src, len);
		new_free(ptr);
		*ptr = new;
	}
	else
		malloc_strcpy(ptr, src);
}

u_char	*
upper(u_char *s)
{
	u_char	*t = NULL;

	if (s)
		for (t = s; *s; s++)
			if (islower(*s))
				*s = toupper(*s);
	return (t);
}

u_char *
lower(u_char *s)
{
	u_char	*t = NULL;

	if (s)
		for (t = s; *s; s++)
			if (isupper(*s))
				*s = tolower(*s);
	return t;
}

/*
 * connect_by_number(): Performs a connecting to socket 'service' on host
 * 'host'.  Host can be a hostname or ip-address.  If 'host' is null, the
 * local host is assumed.   The parameter full_hostname will, on return,
 * contain the expanded hostname (if possible).  Note that full_hostname is a
 * pointer to a u_char *, and is allocated by connect_by_numbers() 
 *
 * The following special values for service exist:
 *
 * 0  Create a socket for accepting connections
 *
 * -2 Connect to the address passed in place of the hostname parameter
 *
 * Errors: 
 *
 * -1 get service failed 
 *
 * -2 get host failed 
 *
 * -3 socket call failed 
 *
 * -4 connect call failed 
 */
int
connect_by_number(int service, u_char *host, int nonblocking, struct addrinfo **oldres, struct addrinfo **oldres0)
{
	int	s = -1;
	u_char	buf[100];
	int	err = -1;
	u_char	strhost[NI_MAXHOST], strservice[NI_MAXSERV], *serv;
	SOCKADDR_STORAGE *server;
	struct addrinfo hints, *res, *res0;
	u_char	*cur_source_host = get_irchost();
	int warned_bind = 0;
	int	saved_errno = -1;

	if (service == -2)
	{
		server = (SOCKADDR_STORAGE *) host;
		if (getnameinfo((struct sockaddr *)server,
		    SA_LEN((struct sockaddr *)server),
		    CP(strhost), sizeof(strhost),
		    CP(strservice), sizeof(strservice),
		    NI_NUMERICHOST|NI_NUMERICSERV))
			return -1;

		serv = strservice;
		host = strhost;
	}
	else
	{
		snprintf(CP(strservice), sizeof strservice, "%d", service);
		serv = strservice;

		if (service > 0)
		{
			if (host == NULL)
			{
				gethostname(CP(buf), sizeof(buf));
				host = buf;
			}
		}
	}

	if (oldres && *oldres && oldres0 && *oldres0)
	{
		res = *oldres;
		res0 = *oldres0;
		*oldres = 0;
		*oldres0 = 0;
	}
	else
	{
		memset(&hints, 0, sizeof hints);
		hints.ai_flags = 0;
		hints.ai_protocol = 0;
		hints.ai_addrlen = 0;
		hints.ai_canonname = NULL;
		hints.ai_addr = NULL;
		hints.ai_next = NULL;
		if (service == -1)
			hints.ai_socktype = SOCK_DGRAM;
		else
			hints.ai_socktype = SOCK_STREAM;
		hints.ai_family = AF_UNSPEC;
		errno = 0;
		err = getaddrinfo(CP(host), CP(serv), &hints, &res0);
		if (err != 0) 
			return (-2);
		res = res0;
	}
	for (; res; res = res->ai_next) {
		err = 0;
		if ((s = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
		{	
			saved_errno = errno;
			continue;
		}
		set_socket_options(s);
		if (cur_source_host)
		{
			if (bind_local_addr(cur_source_host, s, res->ai_family) < 0)
			{
				saved_errno = errno;
				if (warned_bind == 0)
				{
					say("Couldn't bind to IRCHOST");
					warned_bind = 1;
					cur_source_host = NULL;
				}
			}
			else if (service <= 0 && service != -2 && listen(s, 1) == -1)
				err = -4;
			if (err)
				goto again;
		}
#ifdef NON_BLOCKING_CONNECTS
		if (nonblocking && set_non_blocking(s) < 0)
		{
			err = -4;
			goto again;
		}
#endif /* NON_BLOCKING_CONNECTS */
		err = connect(s, res->ai_addr, res->ai_addrlen);
		if (err < 0)
		{
			if (!(errno == EINPROGRESS && nonblocking))
			{
				err = -4;
				goto again;
			}
			/*
			 * if we're non blocking, and we got an EINPROGRESS
			 * save the res0 away so we can re-continue if this
			 * fails to connect.
			 */
			if (oldres && oldres0)
			{
				*oldres = res->ai_next;
				*oldres0 = res0;
				res0 = 0;
			}
			err = 0;
		}
again:
		if (err == 0)
			break;
		new_close(s);
	}
	if (res0)
	{
		freeaddrinfo(res0);
		res0 = 0;
	}
	if (err < 0) {
		new_close(s);
		return err;
	}
	if (s == -1 && saved_errno != -1)
		errno = saved_errno;

	return s;
}

/*
 * Binds to specified socket, host, port using specified family.
 * Returns:
 * 0   if everythins is OK
 * -2  if host wasn't found
 * -10 if family type wasn't supported for specified host
 */
static int
bind_local_addr(u_char *localhost, int fd, int family)
{
	u_char	lbuf[NI_MAXHOST];
	u_char	*end;
	u_char	*localport = NULL;
	struct  addrinfo hintsx, *resx, *res0x;
	int     err = -1;

	/* We often modify this */
	my_strmcpy(lbuf, localhost, sizeof lbuf);
	localhost = lbuf;

	memset(&hintsx, 0, sizeof(hintsx));
	hintsx.ai_family = family;
	hintsx.ai_socktype = SOCK_STREAM;
	hintsx.ai_flags = AI_PASSIVE;

	/* Look for IPv6 */
	if (*localhost == '[')
	{
		end = my_index(localhost + 1, ']');
		if (end)
		{
			*end++ = '\0';
			localhost++;
			hintsx.ai_flags |= AI_NUMERICHOST;
		}
	}
	else
		end = my_index(localhost, ':');

	/* find port if given */
	if (end && end[0] == ':' && end[1])
	{
		end[0] = '\0';
		localport = &end[1];
	}

	err = getaddrinfo(CP(localhost), CP(localport), &hintsx, &res0x);
	if (err != 0)
	{
# ifndef EAI_ADDRFAMILY
#  ifdef EAI_FAMILY
#   define EAI_ADDRFAMILY EAI_FAMILY
#  else
#   error "no EAI_ADDRFAMILY or EAI_FAMILY"
#  endif
# endif
		if (err == EAI_ADDRFAMILY)
			return -10;
		else
			return -2;
	}
	err = -1;
	for (resx = res0x; resx; resx = resx->ai_next)
	{
		if (bind(fd, resx->ai_addr, resx->ai_addrlen) == 0)
		{
			err = 0;
			break;
		}
	}
	freeaddrinfo(res0x);
	if (err < 0)
		return -2;
	return 0;
}

u_char	*
next_arg(u_char *str, u_char **new_ptr)
{
	u_char	*ptr;

	if ((ptr = sindex(str, UP("^ "))) != NULL)
	{
		if ((str = my_index(ptr, ' ')) != NULL)
			*str++ = (u_char) 0;
		else
			str = empty_string();
	}
	else
		str = empty_string();
	if (new_ptr)
		*new_ptr = str;
	return ptr;
}

u_char	*
new_next_arg(u_char *str, u_char **new_ptr)
{
	u_char	*ptr,
		*start;

	if ((ptr = sindex(str, UP("^ \t"))) != NULL)
	{
		if (*ptr == '"')
		{
			start = ++ptr;
			while ((str = sindex(ptr, UP("\"\\"))) != NULL)
			{
				switch (*str)
				{
				case '"':
					*str++ = '\0';
					if (*str == ' ')
						str++;
					if (new_ptr)
						*new_ptr = str;
					return (start);
				case '\\':
					if (*(str + 1) == '"')
						my_strcpy(str, str + 1);
					ptr = str + 1;
				}
			}
			str = empty_string();
		}
		else
		{
			if ((str = sindex(ptr, UP(" \t"))) != NULL)
				*str++ = '\0';
			else
				str = empty_string();
		}
	}
	else
		str = empty_string();
	if (new_ptr)
		*new_ptr = str;
	return ptr;
}

/* my_stricmp: case insensitive version of strcmp */
int
my_stricmp(const u_char *str1, const u_char *str2)
{
	int	xor;

	if (!str1)
		return -1;
	if (!str2)
		return 1; 
	for (; *str1 || *str2 ; str1++, str2++)
	{
		if (!*str1 || !*str2)
			return (*str1 - *str2);
		if (isalpha(*str1) && isalpha(*str2))
		{
			xor = *str1 ^ *str2;
			if (xor != 32 && xor != 0)
				return (*str1 - *str2);
		}
		else
		{
			if (*str1 != *str2)
				return (*str1 - *str2);
		}
	}
	return 0;
}

/* my_strnicmp: case insensitive version of strncmp */
int	
my_strnicmp(const u_char *str1, const u_char *str2, size_t n)
{
	size_t	i;
	int	xor;

	for (i = 0; i < n; i++, str1++, str2++)
	{
		if (isalpha(*str1) && isalpha(*str2))
		{
			xor = *str1 ^ *str2;
			if (xor != 32 && xor != 0)
				return (*str1 - *str2);
		}
		else
		{
			if (*str1 != *str2)
				return (*str1 - *str2);
		}
	}
	return 0;
}

/*
 * strmcpy: Well, it's like this, strncpy doesn't append a trailing null if
 * strlen(str) == maxlen.  strmcpy always makes sure there is a trailing null 
 */
void
strmcpy(u_char *dest, u_char *src, size_t maxlen)
{
	my_strncpy(dest, src, maxlen);
	dest[maxlen-1] = '\0';
}

/*
 * strmcat: like strcat, but truncs the dest string to maxlen (thus the dest
 * should be able to handle maxlen+1 (for the null)) 
 */
void
strmcat(u_char *dest, u_char *src, size_t maxlen)
{
	size_t	srclen,
		len;

	srclen = my_strlen(src);
	if ((len = my_strlen(dest) + srclen) > maxlen)
		my_strncat(dest, src, srclen - (len - maxlen));
	else
		my_strcat(dest, src);
}

/*
 * strmcat_ue: like strcat, but truncs the dest string to maxlen (thus the dest
 * should be able to handle maxlen + 1 (for the null)). Also unescapes
 * backslashes.
 */
void
strmcat_ue(u_char *dest, u_char *src, size_t maxlen)
{
	size_t	dstlen;

	dstlen = my_strlen(dest);
	dest += dstlen;
	maxlen -= dstlen;
	while (*src && maxlen > 0)
	{
		if (*src == '\\')
		{
			if (my_index("npr0", src[1]))
				*dest++ = '\020';
			else if (*(src + 1))
				*dest++ = *++src;
			else
				*dest++ = '\\';
		}
		else
			*dest++ = *src;
		src++;
	}
	*dest = '\0';
}

/*
 * scanstr: looks for an occurrence of str in source.  If not found, returns
 * 0.  If it is found, returns the position in source (1 being the first
 * position).  Not the best way to handle this, but what the hell 
 */
int
scanstr(u_char *source, u_char *str)
{
	int	i,
		max;
	size_t	len;

	len = my_strlen(str);
	max = my_strlen(source) - len;
	for (i = 0; i <= max; i++, source++)
	{
		if (!my_strnicmp(source, str, len))
			return (i + 1);
	}
	return (0);
}

/* expand_twiddle: expands ~ in pathnames. */
u_char	*
expand_twiddle(u_char *str)
{
	u_char	lbuf[BIG_BUFFER_SIZE];

	if (*str == '~')
	{
		str++;
		if (*str == '/' || *str == '\0')
		{
			my_strmcpy(lbuf, my_path(), sizeof lbuf);
			my_strmcat(lbuf, str, sizeof lbuf);
		}
		else
		{
			u_char	*rest;
			struct	passwd *entry;

			if ((rest = my_index(str, '/')) != NULL)
				*rest++ = '\0';
			if ((entry = getpwnam(CP(str))) != NULL)
			{
				my_strmcpy(lbuf, entry->pw_dir, sizeof lbuf);
				if (rest)
				{
					my_strmcat(lbuf, "/", sizeof lbuf);
					my_strmcat(lbuf, rest, sizeof lbuf);
				}
			}
			else
				return NULL;
		}
	}
	else
		my_strmcpy(lbuf, str, sizeof lbuf);
	str = '\0';
	malloc_strcpy(&str, lbuf);
	return (str);
}

/*
 * sindex: much like strchr(), but it looks for a match of any character in
 * the group, and returns that position.  If the first character is a ^, then
 * this will match the first occurence not in that group.
 */
u_char	*
sindex(u_char *string, u_char *group)
{
	u_char	*ptr;

	if (!string || !group)
		return NULL;
	if (*group == '^')
	{
		group++;
		for (; *string; string++)
		{
			for (ptr = group; *ptr; ptr++)
			{
				if (*ptr == *string)
					break;
			}
			if (*ptr == '\0')
				return string;
		}
	}
	else
	{
		for (; *string; string++)
		{
			for (ptr = group; *ptr; ptr++)
			{
				if (*ptr == *string)
					return string;
			}
		}
	}
	return NULL;
}

/*
 * srindex: much like strrchr(), but it looks for a match of any character in
 * the group, and returns that position.  If the first character is a ^, then
 * this will match the first occurence not in that group.
 */
u_char	*
srindex(u_char *string, u_char *group)
{
	u_char	*ptr, *str;

	if (!string || !group)
		return NULL;
	str = string + my_strlen(string);
	if (*group == '^')
	{
		group++;
		for (; str != (string-1); str--)
		{
			for (ptr = group; *ptr; ptr++)
			{
				if (*ptr == *str)
					break;
			}
			if (*ptr == '\0')
				return str;
		}
	}
	else
	{
		for (; str != (string-1); str--)
		{
			for (ptr = group; *ptr; ptr++)
			{
				if (*ptr == *str)
					return str;
			}
		}
	}
	return NULL;
}

/* is_number: returns true if the given string is a number, false otherwise */
int
is_number(u_char *str)
{
	while (*str == ' ')
		str++;
	if (*str == '-')
		str++;
	if (*str)
	{
		for (; *str; str++)
		{
			if (!isdigit((*str)))
				return (0);
		}
		return 1;
	}
	else
		return 0;
}

/* rfgets: exactly like fgets, cept it works backwards through a file!  */
char	*
rfgets(char *lbuf, int size, FILE *file)
{
	char	*ptr;
	off_t	pos;

	if (fseek(file, -2L, 1))
		return NULL;
	do
	{
		switch (fgetc(file))
		{
		case EOF:
			return NULL;
		case '\n':
			pos = ftell(file);
			ptr = fgets(lbuf, size, file);
			fseek(file, (long)pos, 0);
			return ptr;
		}
	}
	while (fseek(file, -2L, 1) == 0);
	rewind(file);
	pos = 0L;
	ptr = fgets(lbuf, size, file);
	fseek(file, (long)pos, 0);
	return ptr;
}

/*
 * path_search: given a file called name, this will search each element of
 * the given path to locate the file.  If found in an element of path, the
 * full path name of the file is returned in a static string.  If not, null
 * is returned.  Path is a colon separated list of directories 
 */
u_char	*
path_search(u_char *name, u_char *path)
{
	static	u_char	lbuf[BIG_BUFFER_SIZE] = "";
	u_char	*ptr,
		*free_path = NULL;

	malloc_strcpy(&free_path, path);
	path = free_path;
	while (path)
	{
		if ((ptr = my_index(path, ':')) != NULL)
			*(ptr++) = '\0';
		lbuf[0] = 0;
		if (path[0] == '~')
		{
			my_strmcat(lbuf, my_path(), sizeof lbuf);
			path++;
		}
		my_strmcat(lbuf, path, sizeof lbuf);
		my_strmcat(lbuf, "/", sizeof lbuf);
		my_strmcat(lbuf, name, sizeof lbuf);
		if (access(CP(lbuf), F_OK) == 0)
			break;
		path = ptr;
	}
	new_free(&free_path);
	return (path) ? lbuf : NULL;
}

/*
 * double_quote: Given a str of text, this will quote any character in the
 * set stuff with the QUOTE_CHAR. It returns a malloced quoted, null
 * terminated string 
 */
u_char	*
double_quote(u_char *str, u_char *stuff)
{
	u_char	lbuf[BIG_BUFFER_SIZE];
	u_char	*ptr = NULL;
	u_char	c;
	int	pos;

	if (str && stuff)
	{
		for (pos = 0; (c = *str); str++)
		{
			if (my_index(stuff, c))
			{
				if (c == '$')
					lbuf[pos++] = '$';
				else
					lbuf[pos++] = '\\';
			}
			lbuf[pos++] = c;
		}
		lbuf[pos] = '\0';
		malloc_strcpy(&ptr, lbuf);
	}
	else
		malloc_strcpy(&ptr, str);
	return ptr;
}

#ifdef ZCAT
/* Here another interesting stuff:
 * it handle zcat of compressed files
 * You can manage compressed files in this way:
 *
 * IN: u_char *name, the compressed file FILENAME
 * OUT: a FILE *, from which read the expanded file
 *
 */
FILE	*
zcat(u_char *name)
{
	FILE	*fp;
	int	in[2];

	in[0] = -1;
	in[1] = -1;
	if (pipe(in))
	{
		say("Unable to start decompression process: %s", strerror(errno));
		if(in[0] != -1)
		{
			new_close(in[0]);
			new_close(in[1]);
		}
		return(NULL);
	}
	switch(fork())
	{
	case -1:
		say("Unable to start decompression process: %s", strerror(errno));
		return(NULL);
	case 0:
		(void) MY_SIGNAL(SIGINT, (sigfunc *) SIG_IGN, 0);
		dup2(in[1], 1);
		new_close(in[0]);
		(void)setuid(getuid());
		(void)setgid(getgid());
#ifdef ZARGS
		execlp(ZCAT, ZCAT, ZARGS, name, NULL);
#else
		execlp(ZCAT, ZCAT, name, NULL);
#endif /* ZARGS */
		exit(0);
	default:
		new_close(in[1]);
		if ((fp = fdopen(in[0], "r")) == NULL)
		{
			say("Cannot open pipe file descriptor: %s", strerror(errno));
			return(NULL);
		}
		break;
	}
	return(fp);
}
#endif /* ZCAT */

#ifdef NON_BLOCKING_CONNECTS
int
set_non_blocking(int fd)
{
	int	res;

	if ((res = fcntl(fd, F_GETFL, 0)) == -1)
		return -1;
	else if (fcntl(fd, F_SETFL, res | O_NONBLOCK) == -1)
		return -1;
	return 0;
}

int
set_blocking(int fd)
{
	int	res;

	if ((res = fcntl(fd, F_GETFL, 0)) == -1)
		return -1;
	else if (fcntl(fd, F_SETFL, res & ~O_NONBLOCK) == -1)
		return -1;
	return 0;
}
#endif /* NON_BLOCKING_CONNECTS */
