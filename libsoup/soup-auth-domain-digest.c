/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-auth-domain-digest.c: HTTP Digest Authentication (server-side)
 *
 * Copyright (C) 2007 Novell, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include "soup-auth-domain-digest.h"
#include "soup-auth-digest.h"
#include "soup-headers.h"
#include "soup-marshal.h"
#include "soup-message.h"
#include "soup-uri.h"

enum {
	PROP_0,

	PROP_AUTH_CALLBACK,
	PROP_AUTH_DATA,

	LAST_PROP
};

typedef struct {
	SoupAuthDomainDigestAuthCallback auth_callback;
	gpointer auth_data;
	GDestroyNotify auth_dnotify;

} SoupAuthDomainDigestPrivate;

#define SOUP_AUTH_DOMAIN_DIGEST_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SOUP_TYPE_AUTH_DOMAIN_DIGEST, SoupAuthDomainDigestPrivate))

G_DEFINE_TYPE (SoupAuthDomainDigest, soup_auth_domain_digest, SOUP_TYPE_AUTH_DOMAIN)

static char *accepts   (SoupAuthDomain *domain,
			SoupMessage    *msg,
			const char     *header);
static char *challenge (SoupAuthDomain *domain,
			SoupMessage    *msg);

static void set_property (GObject *object, guint prop_id,
			  const GValue *value, GParamSpec *pspec);
static void get_property (GObject *object, guint prop_id,
			  GValue *value, GParamSpec *pspec);

static void
soup_auth_domain_digest_init (SoupAuthDomainDigest *digest)
{
}

static void
finalize (GObject *object)
{
	SoupAuthDomainDigestPrivate *priv =
		SOUP_AUTH_DOMAIN_DIGEST_GET_PRIVATE (object);

	if (priv->auth_dnotify)
		priv->auth_dnotify (priv->auth_data);

	G_OBJECT_CLASS (soup_auth_domain_digest_parent_class)->finalize (object);
}

static void
soup_auth_domain_digest_class_init (SoupAuthDomainDigestClass *digest_class)
{
	SoupAuthDomainClass *auth_domain_class =
		SOUP_AUTH_DOMAIN_CLASS (digest_class);
	GObjectClass *object_class = G_OBJECT_CLASS (digest_class);

	g_type_class_add_private (digest_class, sizeof (SoupAuthDomainDigestPrivate));

	auth_domain_class->accepts   = accepts;
	auth_domain_class->challenge = challenge;

	object_class->finalize     = finalize;
	object_class->set_property = set_property;
	object_class->get_property = get_property;

	g_object_class_install_property (
		object_class, PROP_AUTH_CALLBACK,
		g_param_spec_pointer (SOUP_AUTH_DOMAIN_DIGEST_AUTH_CALLBACK,
				      "Authentication callback",
				      "Password-finding callback",
				      G_PARAM_READWRITE));
	g_object_class_install_property (
		object_class, PROP_AUTH_DATA,
		g_param_spec_pointer (SOUP_AUTH_DOMAIN_DIGEST_AUTH_DATA,
				      "Authentication callback data",
				      "Data to pass to authentication callback",
				      G_PARAM_READWRITE));
}

static void
set_property (GObject *object, guint prop_id,
	      const GValue *value, GParamSpec *pspec)
{
	SoupAuthDomainDigestPrivate *priv =
		SOUP_AUTH_DOMAIN_DIGEST_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_AUTH_CALLBACK:
		priv->auth_callback = g_value_get_pointer (value);
		break;
	case PROP_AUTH_DATA:
		if (priv->auth_dnotify) {
			priv->auth_dnotify (priv->auth_data);
			priv->auth_dnotify = NULL;
		}
		priv->auth_data = g_value_get_pointer (value);
		break;
	default:
		break;
	}
}

static void
get_property (GObject *object, guint prop_id,
	      GValue *value, GParamSpec *pspec)
{
	SoupAuthDomainDigestPrivate *priv =
		SOUP_AUTH_DOMAIN_DIGEST_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_AUTH_CALLBACK:
		g_value_set_pointer (value, priv->auth_callback);
		break;
	case PROP_AUTH_DATA:
		g_value_set_pointer (value, priv->auth_data);
		break;
	default:
		break;
	}
}

SoupAuthDomain *
soup_auth_domain_digest_new (const char *optname1, ...)
{
	SoupAuthDomain *domain;
	va_list ap;

	va_start (ap, optname1);
	domain = (SoupAuthDomain *)g_object_new_valist (SOUP_TYPE_AUTH_DOMAIN_DIGEST,
							optname1, ap);
	va_end (ap);

	g_return_val_if_fail (soup_auth_domain_get_realm (domain) != NULL, NULL);

	return domain;
}

/**
 * SoupAuthDomainDigestAuthCallback:
 * @domain: the domain
 * @msg: the message being authenticated
 * @username: the username provided by the client
 * @user_data: the data passed to soup_auth_domain_digest_set_auth_callback()
 *
 * Callback used by #SoupAuthDomainDigest for authentication purposes.
 * The application should look up @username in its password database,
 * and return the corresponding encoded password (see
 * soup_auth_domain_digest_encode_password()).
 *
 * Return value: the encoded password, or %NULL if @username is not a
 * valid user. @domain will free the password when it is done with it.
 **/

/**
 * soup_auth_domain_digest_set_auth_callback:
 * @domain: the domain
 * @auth_callback: the callback
 * @user_data: data to pass to @auth_callback
 * @dnotify: destroy notifier to free @user_data when @domain
 * is destroyed
 *
 * Sets the callback that @domain will use to authenticate incoming
 * requests. For each request containing authorization, @domain will
 * invoke the callback, and then either accept or reject the request
 * based on @auth_callback's return value.
 *
 * You can also set the auth callback by setting the
 * %SOUP_AUTH_DOMAIN_DIGEST_AUTH_CALLBACK and
 * %SOUP_AUTH_DOMAIN_DIGEST_AUTH_DATA properties, which can also be
 * used to set the callback at construct time.
 **/
void
soup_auth_domain_digest_set_auth_callback (SoupAuthDomain *domain,
					   SoupAuthDomainDigestAuthCallback auth_callback,
					   gpointer        user_data,
					   GDestroyNotify  dnotify)
{
	SoupAuthDomainDigestPrivate *priv =
		SOUP_AUTH_DOMAIN_DIGEST_GET_PRIVATE (domain);

	if (priv->auth_dnotify)
		priv->auth_dnotify (priv->auth_data);

	priv->auth_callback = auth_callback;
	priv->auth_data = user_data;
	priv->auth_dnotify = dnotify;
}

static char *
accepts (SoupAuthDomain *domain, SoupMessage *msg, const char *header)
{
	SoupAuthDomainDigestPrivate *priv =
		SOUP_AUTH_DOMAIN_DIGEST_GET_PRIVATE (domain);
	GHashTable *params;
	const char *uri, *qop, *realm, *username;
	const char *nonce, *nc, *cnonce, *response;
	char *hex_urp, hex_a1[33], computed_response[33], *ret_user;
	int nonce_count;
	SoupURI *dig_uri, *req_uri;
	gboolean accept = FALSE;

	if (!priv->auth_callback)
		return NULL;

	if (strncmp (header, "Digest ", 7) != 0)
		return NULL;

	params = soup_header_parse_param_list (header + 7);
	if (!params)
		return NULL;

	/* Check uri */
	uri = g_hash_table_lookup (params, "uri");
	if (!uri)
		goto DONE;

	req_uri = soup_message_get_uri (msg);
	dig_uri = soup_uri_new (uri);
	if (dig_uri) {
		if (!soup_uri_equal (dig_uri, req_uri)) {
			soup_uri_free (dig_uri);
			goto DONE;
		}
		soup_uri_free (dig_uri);
	} else {	
		char *req_path;

		req_path = soup_uri_to_string (req_uri, TRUE);
		if (strcmp (uri, req_path) != 0) {
			g_free (req_path);
			goto DONE;
		}
		g_free (req_path);
	}

	/* Check qop; we only support "auth" for now */
	qop = g_hash_table_lookup (params, "qop");
	if (!qop || strcmp (qop, "auth") != 0)
		goto DONE;

	/* Check realm */
	realm = g_hash_table_lookup (params, "realm");
	if (!realm || strcmp (realm, soup_auth_domain_get_realm (domain)) != 0)
		goto DONE;

	username = g_hash_table_lookup (params, "username");
	if (!username)
		goto DONE;
	nonce = g_hash_table_lookup (params, "nonce");
	if (!nonce)
		goto DONE;
	nc = g_hash_table_lookup (params, "nc");
	if (!nc)
		goto DONE;
	nonce_count = atoi (nc);
	if (nonce_count <= 0)
		goto DONE;
	cnonce = g_hash_table_lookup (params, "cnonce");
	if (!cnonce)
		goto DONE;
	response = g_hash_table_lookup (params, "response");
	if (!response)
		goto DONE;

	hex_urp = priv->auth_callback (domain, msg, username, priv->auth_data);
	if (hex_urp) {
		soup_auth_digest_compute_hex_a1 (hex_urp,
						 SOUP_AUTH_DIGEST_ALGORITHM_MD5,
						 nonce, cnonce, hex_a1);
		g_free (hex_urp);
		soup_auth_digest_compute_response (msg->method, uri, hex_a1,
						   SOUP_AUTH_DIGEST_QOP_AUTH,
						   nonce, cnonce, nonce_count,
						   computed_response);
		accept = (strcmp (response, computed_response) == 0);
	}

 DONE:
	ret_user = accept ? g_strdup (username) : NULL;
	soup_header_free_param_list (params);
	return ret_user;
}

static char *
challenge (SoupAuthDomain *domain, SoupMessage *msg)
{
	GString *str;

	str = g_string_new ("Digest ");

	/* FIXME: escape realm */
	g_string_append_printf (str, "realm=\"%s\", ", 
				soup_auth_domain_get_realm (domain));

	g_string_append_printf (str, "nonce=\"%lu%lu\", ", 
				(unsigned long) msg,
				(unsigned long) time (0));

	g_string_append_printf (str, "qop=\"auth\", ");
	g_string_append_printf (str, "algorithm=\"MD5\"");

	return g_string_free (str, FALSE);
}

/**
 * soup_auth_domain_digest_encode_password:
 * @username: a username
 * @realm: an auth realm name
 * @password: the password for @username in @realm
 *
 * Encodes the username/realm/password triplet for Digest
 * authentication. (That is, it returns a stringified MD5 hash of
 * @username, @realm, and @password concatenated together). This is
 * the form that is needed as the return value of
 * #SoupAuthDomainDigest's auth handler.
 *
 * For security reasons, you should store the encoded hash, rather
 * than storing the cleartext password itself and calling this method
 * only when you need to verify it. This way, if your server is
 * compromised, the attackers will not gain access to cleartext
 * passwords which might also be usable at other sites. (Note also
 * that the encoded password returned by this method is identical to
 * the encoded password stored in an Apache .htdigest file.)
 *
 * Return value: the encoded password
 **/
char *
soup_auth_domain_digest_encode_password (const char *username,
					 const char *realm,
					 const char *password)
{
	char hex_urp[33];

	soup_auth_digest_compute_hex_urp (username, realm, password, hex_urp);
	return g_strdup (hex_urp);
}

static char *
evil_auth_callback (SoupAuthDomain *domain, SoupMessage *msg,
		    const char *username, gpointer encoded_password)
{
	return g_strdup (encoded_password);
}

/**
 * soup_auth_domain_digest_evil_check_password:
 * @domain: the auth domain
 * @msg: the possibly-authenticated request
 * @username: the username to check @msg against
 * @password: the password to check @msg against
 *
 * Checks if @msg correctly authenticates @username via @password in
 * @domain.
 *
 * Don't use this method; it's evil. It requires you to have a
 * cleartext password database, which means that if your server is
 * compromised, the attackers will have access to all of your users'
 * passwords, which may also be their passwords on other servers. It
 * is much better to store the passwords encoded in some format (eg,
 * via soup_auth_domain_digest_encode_password() when using Digest
 * authentication), so that if the server is compromised, the
 * attackers won't be able to use the encoded passwords elsewhere.
 *
 * At any rate, even if you do have a cleartext password database, you
 * still don't need to use this method, as you can just call
 * soup_auth_domain_digest_encode_password() on the cleartext password
 * from the #SoupAuthDomainDigestAuthCallback anyway. This method
 * really only exists so as not to break certain libraries written
 * against libsoup 2.2 whose public APIs depend on the existence of a
 * "check this password against this request" method.
 *
 * Return value: %TRUE if @msg matches @username and @password,
 * %FALSE if not.
 **/
gboolean
soup_auth_domain_digest_evil_check_password (SoupAuthDomain *domain,
					     SoupMessage    *msg,
					     const char     *username,
					     const char     *password)
{
	SoupAuthDomainDigestPrivate *priv =
		SOUP_AUTH_DOMAIN_DIGEST_GET_PRIVATE (domain);
	char *encoded_password;
	const char *header;
	SoupAuthDomainDigestAuthCallback old_callback;
	gpointer old_data;
	char *matched_username;

	encoded_password = soup_auth_domain_digest_encode_password (
		username, soup_auth_domain_get_realm (domain), password);

	old_callback = priv->auth_callback;
	old_data = priv->auth_data;

	priv->auth_callback = evil_auth_callback;
	priv->auth_data = encoded_password;

	header = soup_message_headers_get (msg->request_headers, "Authorization");
	matched_username = accepts (domain, msg, header);

	priv->auth_callback = old_callback;
	priv->auth_data = old_data;

	g_free (encoded_password);

	if (matched_username) {
		g_free (matched_username);
		return TRUE;
	} else
		return FALSE;
}
