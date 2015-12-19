/*
 * Global stuff for translation tables.
 *
 * Tomten, tomten@solace.hsh.se / tomten@lysator.liu.se
 *
 * @(#)$eterna: translat.h,v 1.20 2015/05/14 01:47:38 mrg Exp $
 */

#ifndef irc__translat_h_
# define irc__translat_h_

#include "defs.h"

#ifdef HAVE_ICONV_H
#include <iconv.h>
#endif

	void	enter_digraph(u_int, u_char *);
	u_char	get_digraph(u_int);
	void	digraph(u_char *, u_char *, u_char *);
	void	save_digraphs(FILE *);

#ifndef HAVE_ICONV_OPEN
	typedef void *iconv_t;
#endif

typedef struct mb_data
{
	unsigned input_bytes;    /* How many bytes we ate */
	unsigned output_bytes;   /* How many bytes we produced */
	unsigned num_columns;    /* How many columns does this result take */
#ifdef HAVE_ICONV_OPEN
	iconv_t conv_in;
	iconv_t conv_out;
	const char* enc;
#endif
} mb_data;

	/* Returns true(1)/false(0) whether the given unival is printable */
	char displayable_unival(unsigned unival, iconv_t conv_out);

	/* Sequence the given unicode value to the given utf-8 buffer */
	void utf8_sequence(unsigned unival, u_char *utfbuf);
	
	/* Return the unicode value of the first character in the given utf-8 string */
	unsigned calc_unival(const u_char *);
	/* Guess the width of the given unicode value in columns */
	unsigned calc_unival_width(unsigned unival);
	/* Calculate the length of the unicode sequence beginning at given pos */
	unsigned calc_unival_length(const u_char *);

	void set_irc_encoding(u_char *);
	void set_input_encoding(u_char *);
	void set_display_encoding(u_char *);

	void mbdata_init(struct mb_data *d, const char *enc);
	void mbdata_done(struct mb_data *d);
	void decode_mb(u_char *ptr, u_char *dest, size_t destlen, mb_data *data);

#ifdef HAVE_ICONV_OPEN
	u_char  *current_irc_encoding(void);
	u_char  *current_display_encoding(void);
	u_char  *current_input_encoding(void);
#endif

#endif /* irc__translat_h_ */
