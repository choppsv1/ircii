/*
 * ssl.c: Dealing with SSL connections to ircd.
 *
 * Written By Matthew R. Green
 *
 * Copyright (c) 2014 Matthew R. Green
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "irc.h"
IRCII_RCSID("@(#)$eterna: ssl.c,v 2.10 2014/08/25 04:45:38 mrg Exp $");

#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include "ssl.h"
#include "output.h"
#include "server.h"
#include "ircaux.h"
#include "debug.h"
#include "vars.h"

/*
 * We provide a backend that lets server.c use the same functions in
 * either normal or SSL mode.  So this really is a general IO routine
 * for normal or SSL as well as the SSL setup/etc handling.
 */

#ifdef USE_OPENSSL
static	void	ssl_print_error_queue(const char *);

static	SSL_CTX	*ssl_ctx;

/* SslInfo: private structure per-server connection. */
struct ssl_info_stru {
	SSL	*ssl;
};

static	void
ssl_print_error_queue(const char *msg)
{
	unsigned long sslcode = ERR_get_error();

	do {
		yell("--- %s: SSL error: %s:%s:%s",
		     msg,
		     ERR_lib_error_string(sslcode),
		     ERR_func_error_string(sslcode),
		     ERR_reason_error_string(sslcode));
		Debug(DB_SSL, "%s: SSL error: %s:%s:%s", 
		     msg,
		     ERR_lib_error_string(sslcode),
		     ERR_func_error_string(sslcode),
		     ERR_reason_error_string(sslcode));
	} while ((sslcode = ERR_get_error()));
}
#endif

/*
 * we keep local copies of each of the variables so we can properly determine
 * when something has changed.  if neither ca_path nor ca_file is set, then we
 * load the default paths.
 */
void
ssl_setup_certs(u_char *dummy)
{
#ifdef USE_OPENSSL
	if (!ssl_ctx)
		return;

	static	u_char	*ca_file;
	static	u_char	*ca_path;
	static	u_char	*chain_file;
	static	u_char	*private_key_file;
		u_char	*cur_ca_file = get_string_var(SSL_CA_FILE_VAR);
		u_char	*cur_ca_path = get_string_var(SSL_CA_PATH_VAR);
		u_char	*cur_chain_file = get_string_var(SSL_CA_CHAIN_FILE_VAR);
		u_char	*cur_private_key_file =
			 get_string_var(SSL_CA_PRIVATE_KEY_FILE_VAR);

	if (ca_file != cur_ca_file ||
	    ca_path != cur_ca_path)
	{
		ca_file = cur_ca_file;
		ca_path = cur_ca_path;

		Debug(DB_SSL, "calling SSL_CTX_load_verify_locations(%s, %s)",
		      ca_file, ca_path);
		SSL_CTX_load_verify_locations(ssl_ctx, CP(ca_file), CP(ca_path));
	}
	
	if (!ca_path && !ca_file)
		SSL_CTX_set_default_verify_paths(ssl_ctx);

	if (chain_file != cur_chain_file)
	{
		chain_file = cur_chain_file;
		Debug(DB_SSL, "calling SSL_CTX_use_certificate_chain_file(%s)",
		      chain_file);
		SSL_CTX_use_certificate_chain_file(ssl_ctx, CP(chain_file));
	}

	if (private_key_file != cur_private_key_file)
	{
		private_key_file = cur_private_key_file;
		Debug(DB_SSL, "calling SSL_CTX_use_PrivateKey_file(%s)",
		      private_key_file);
		SSL_CTX_use_PrivateKey_file(ssl_ctx, CP(private_key_file),
					    SSL_FILETYPE_PEM);
	}
#endif
}

/*
 * returns either OK, FAIL or PENDING.
 */
ssl_init_status
ssl_init_connection(int server, int fd, SslInfo **newp)
{
#ifdef USE_OPENSSL
	SslInfo	*new = NULL;
	static	int first = 1;
	ssl_init_status	status = SSL_INIT_OK;
	int	rv;
	server_ssl_level ssl_level;

	ssl_level = server_do_ssl(server);
	if (ssl_level == SSL_OFF)
	{
		Debug(DB_SSL, "no SSL this server");
		goto cleanup;
	}

	if (first)
	{
		first = 0;
		if (!SSL_library_init())
		{
			yell("Unable to initialise SSL");
			goto cleanup;
		}
		SSL_load_error_strings();
		OpenSSL_add_all_algorithms();
		Debug(DB_SSL, "init");

		ssl_ctx = SSL_CTX_new(SSLv23_client_method());
		if (!ssl_ctx)
		{
			yell("Unable to create SSL context");
			ssl_print_error_queue("failed to create SSL context");
			goto cleanup;
		}
		ssl_setup_certs(NULL);
	}

	if (*newp)
		new = *newp;
	else
	{
		new = new_malloc(sizeof *new);
		new->ssl = NULL;

		new->ssl = SSL_new(ssl_ctx);
		if (!new->ssl)
		{
			yell("Unable to create new SSL session");
			ssl_print_error_queue("failed to create SSL session");
			goto cleanup;
		}

		SSL_set_mode(new->ssl, SSL_MODE_AUTO_RETRY);
		SSL_set_fd(new->ssl, fd);

		if (ssl_level == SSL_VERIFY)
		{
			SSL_set_verify(new->ssl, SSL_VERIFY_PEER, NULL);
		}
		else
		{
			yell("SSL connection not verified, may be insecure");
			SSL_set_verify(new->ssl, SSL_VERIFY_NONE, NULL);
		}
	}

	rv = SSL_connect(new->ssl);
	if (rv != 1)
	{
		int ssl_err;

		Debug(DB_SSL, "SSL_connect failed: %d", rv);
		ssl_err = SSL_get_error(new->ssl, rv);

		/*
		 * Are we connected yet?  If not, return pending
		 * so that the caller can continue processing other
		 * events until we're ready again.  The traditional
		 * loop against SSL_connect() does not work so great
		 * for non-blocking sockets.
		 */
		if (ssl_err == SSL_ERROR_WANT_READ ||
		    ssl_err == SSL_ERROR_WANT_WRITE)
		{
			/* take the path out that writes *newp */
			status = SSL_INIT_PENDING;
			goto out;
		}
		yell("Unable to connect to SSL server");
		ssl_print_error_queue("SSL_connect failed");
		status = SSL_INIT_FAIL;
		goto cleanup;
	}

	yell("Connected to SSL successfully!");

	status = SSL_INIT_OK;
	Debug(DB_SSL, "success!");
out:
	*newp = new;

	return status;
cleanup:
	if (new)
	{
		if (new->ssl)
			SSL_free(new->ssl);
		new_free(&new);
	}
	goto out;
#else
	return SSL_INIT_OK;
#endif
}

void
ssl_close_connection(SslInfo **ssl_info)
{
#ifdef USE_OPENSSL
	if (!*ssl_info)
		return;

	if ((*ssl_info)->ssl)
		SSL_free((*ssl_info)->ssl);

	new_free(ssl_info);
#endif
}

ssize_t
ssl_write(SslInfo *info, int fd, const void *buf, size_t len)
{
#ifdef USE_OPENSSL
	if (info && info->ssl)
	{
		int rv;

		rv = SSL_write(info->ssl, buf, len);
		if (rv < 0)
		{
			int ssl_err = SSL_get_error(info->ssl, rv);
			
			if (ssl_err == SSL_ERROR_WANT_READ ||
			    ssl_err == SSL_ERROR_WANT_WRITE)
				return -2;

			ssl_print_error_queue("SSL_write failed");
		}

		return rv;
	}
	else
#endif
		return write(fd, buf, len);
}

ssize_t
ssl_read(SslInfo *info, int fd, void *buf, size_t len)
{
#ifdef USE_OPENSSL
	if (info && info->ssl)
	{
		int rv;

		rv = SSL_read(info->ssl, buf, len);
		if (rv < 0)
		{
			int ssl_err = SSL_get_error(info->ssl, rv);
			
			if (ssl_err == SSL_ERROR_WANT_READ ||
			    ssl_err == SSL_ERROR_WANT_WRITE)
				return -2;

			ssl_print_error_queue("SSL_read failed");
		}

		return rv;
	}
	else
#endif
		return read(fd, buf, len);
}
