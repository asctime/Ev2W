/*
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <string.h>

#include "camel-internet-address.h"
#include "camel-mime-utils.h"

#define d(x)

struct _address {
	gchar *name;
	gchar *address;
};

G_DEFINE_TYPE (CamelInternetAddress, camel_internet_address, CAMEL_TYPE_ADDRESS)

static gint
internet_address_decode (CamelAddress *a, const gchar *raw)
{
	struct _camel_header_address *ha, *n;
	gint count = a->addresses->len;

	/* Should probably use its own decoder or something */
	ha = camel_header_address_decode(raw, NULL);
	if (ha) {
		n = ha;
		while (n) {
			if (n->type == CAMEL_HEADER_ADDRESS_NAME) {
				camel_internet_address_add((CamelInternetAddress *)a, n->name, n->v.addr);
			} else if (n->type == CAMEL_HEADER_ADDRESS_GROUP) {
				struct _camel_header_address *g = n->v.members;
				while (g) {
					if (g->type == CAMEL_HEADER_ADDRESS_NAME)
						camel_internet_address_add((CamelInternetAddress *)a, g->name, g->v.addr);
					/* otherwise, it's an error, infact */
					g = g->next;
				}
			}
			n = n->next;
		}
		camel_header_address_list_clear(&ha);
	}

	return a->addresses->len - count;
}

static gchar *
internet_address_encode (CamelAddress *a)
{
	gint i;
	GString *out;
	gchar *ret;
	gint len = 6;		/* "From: ", assume longer of the address headers */

	if (a->addresses->len == 0)
		return NULL;

	out = g_string_new("");

	for (i = 0;i < a->addresses->len; i++) {
		struct _address *addr = g_ptr_array_index(a->addresses, i);
		gchar *enc;

		if (i != 0)
			g_string_append(out, ", ");

		enc = camel_internet_address_encode_address(&len, addr->name, addr->address);
		g_string_append(out, enc);
		g_free(enc);
	}

	ret = out->str;
	g_string_free(out, FALSE);

	return ret;
}

static gint
internet_address_unformat(CamelAddress *a, const gchar *raw)
{
	gchar *buffer, *p, *name, *addr;
	gint c;
	gint count = a->addresses->len;

	if (raw == NULL)
		return 0;

	d(printf("unformatting address: %s\n", raw));

	/* we copy, so we can modify as we go */
	buffer = g_strdup(raw);

	/* this can be simpler than decode, since there are much fewer rules */
	p = buffer;
	name = NULL;
	addr = p;
	do {
		c = (guchar)*p++;
		switch (c) {
			/* removes quotes, they should only be around the total name anyway */
		case '"':
			p[-1] = ' ';
			while (*p)
				if (*p == '"') {
					*p++ = ' ';
					break;
				} else {
					p++;
				}
			break;
		case '<':
			if (name == NULL)
				name = addr;
			addr = p;
			addr[-1] = 0;
			while (*p && *p != '>')
				p++;
			if (*p == 0)
				break;
			p++;
			/* falls through */
		case ',':
			p[-1] = 0;
			/* falls through */
		case 0:
			if (name)
				name = g_strstrip(name);
			addr = g_strstrip(addr);
			if (addr[0]) {
				d(printf("found address: '%s' <%s>\n", name, addr));
				camel_internet_address_add((CamelInternetAddress *)a, name, addr);
			}
			name = NULL;
			addr = p;
			break;
		}
	} while (c);

	g_free(buffer);

	return a->addresses->len - count;
}

static gchar *
internet_address_format (CamelAddress *a)
{
	gint i;
	GString *out;
	gchar *ret;

	if (a->addresses->len == 0)
		return NULL;

	out = g_string_new("");

	for (i = 0;i < a->addresses->len; i++) {
		struct _address *addr = g_ptr_array_index(a->addresses, i);
		gchar *enc;

		if (i != 0)
			g_string_append(out, ", ");

		enc = camel_internet_address_format_address(addr->name, addr->address);
		g_string_append(out, enc);
		g_free(enc);
	}

	ret = out->str;
	g_string_free(out, FALSE);

	return ret;
}

static void
internet_address_remove (CamelAddress *a, gint index)
{
	struct _address *addr;

	if (index < 0 || index >= a->addresses->len)
		return;

	addr = g_ptr_array_index(a->addresses, index);
	g_free(addr->name);
	g_free(addr->address);
	g_free(addr);
	g_ptr_array_remove_index(a->addresses, index);
}

static gint
internet_address_cat (CamelAddress *dest, CamelAddress *source)
{
	gint i;

	g_assert(CAMEL_IS_INTERNET_ADDRESS(source));

	for (i=0;i<source->addresses->len;i++) {
		struct _address *addr = g_ptr_array_index(source->addresses, i);
		camel_internet_address_add((CamelInternetAddress *)dest, addr->name, addr->address);
	}

	return i;
}

static void
camel_internet_address_class_init (CamelInternetAddressClass *class)
{
	CamelAddressClass *address_class;

	address_class = CAMEL_ADDRESS_CLASS (class);
	address_class->decode = internet_address_decode;
	address_class->encode = internet_address_encode;
	address_class->unformat = internet_address_unformat;
	address_class->format = internet_address_format;
	address_class->remove = internet_address_remove;
	address_class->cat = internet_address_cat;
}

static void
camel_internet_address_init (CamelInternetAddress *internet_address)
{
}

/**
 * camel_internet_address_new:
 *
 * Create a new #CamelInternetAddress object.
 *
 * Returns: a new #CamelInternetAddress object
 **/
CamelInternetAddress *
camel_internet_address_new (void)
{
	return g_object_new (CAMEL_TYPE_INTERNET_ADDRESS, NULL);
}

/**
 * camel_internet_address_add:
 * @addr: a #CamelInternetAddress object
 * @name: name associated with the new address
 * @address: routing address associated with the new address
 *
 * Add a new internet address to @addr.
 *
 * Returns: the index of added entry
 **/
gint
camel_internet_address_add (CamelInternetAddress *addr, const gchar *name, const gchar *address)
{
	struct _address *new;
	gint index;

	g_assert(CAMEL_IS_INTERNET_ADDRESS(addr));

	new = g_malloc(sizeof(*new));
	new->name = g_strdup(name);
	new->address = g_strdup(address);
	index = ((CamelAddress *)addr)->addresses->len;
	g_ptr_array_add(((CamelAddress *)addr)->addresses, new);

	return index;
}

/**
 * camel_internet_address_get:
 * @addr: a #CamelInternetAddress object
 * @index: address's array index
 * @namep: holder for the returned name, or %NULL, if not required.
 * @addressp: holder for the returned address, or %NULL, if not required.
 *
 * Get the address at @index.
 *
 * Returns: %TRUE if such an address exists, or %FALSE otherwise
 **/
gboolean
camel_internet_address_get (CamelInternetAddress *addr, gint index, const gchar **namep, const gchar **addressp)
{
	struct _address *a;

	g_assert(CAMEL_IS_INTERNET_ADDRESS(addr));

	if (index < 0 || index >= ((CamelAddress *)addr)->addresses->len)
		return FALSE;

	a = g_ptr_array_index (((CamelAddress *)addr)->addresses, index);
	if (namep)
		*namep = a->name;
	if (addressp)
		*addressp = a->address;
	return TRUE;
}

/**
 * camel_internet_address_find_name:
 * @addr: a #CamelInternetAddress object
 * @name: name to lookup
 * @addressp: holder for address part, or %NULL, if not required.
 *
 * Find address by real name.
 *
 * Returns: the index of the address matching the name, or %-1 if no
 * match was found
 **/
gint
camel_internet_address_find_name(CamelInternetAddress *addr, const gchar *name, const gchar **addressp)
{
	struct _address *a;
	gint i, len;

	g_assert(CAMEL_IS_INTERNET_ADDRESS(addr));

	len = ((CamelAddress *)addr)->addresses->len;
	for (i=0;i<len;i++) {
		a = g_ptr_array_index (((CamelAddress *)addr)->addresses, i);
		if (a->name && !strcmp(a->name, name)) {
			if (addressp)
				*addressp = a->address;
			return i;
		}
	}
	return -1;
}

/**
 * camel_internet_address_find_address:
 * @addr: a #CamelInternetAddress object
 * @address: address to lookup
 * @namep: holder for the matching name, or %NULL, if not required.
 *
 * Find an address by address.
 *
 * Returns: the index of the address, or %-1 if not found
 **/
gint
camel_internet_address_find_address(CamelInternetAddress *addr, const gchar *address, const gchar **namep)
{
	struct _address *a;
	gint i, len;

	g_assert(CAMEL_IS_INTERNET_ADDRESS(addr));

	len = ((CamelAddress *)addr)->addresses->len;
	for (i=0;i<len;i++) {
		a = g_ptr_array_index (((CamelAddress *)addr)->addresses, i);
		if (!strcmp(a->address, address)) {
			if (namep)
				*namep = a->name;
			return i;
		}
	}
	return -1;
}

static void
cia_encode_addrspec(GString *out, const gchar *addr)
{
	const gchar *at, *p;

	at = strchr(addr, '@');
	if (at == NULL)
		goto append;

	p = addr;
	while (p < at) {
		gchar c = *p++;

		/* strictly by rfc, we should split local parts on dots.
		   however i think 2822 changes this, and not many clients grok it, so
		   just quote the whole local part if need be */
		if (!(camel_mime_is_atom(c) || c=='.')) {
			g_string_append_c(out, '"');

			p = addr;
			while (p < at) {
				c = *p++;
				if (c == '"' || c == '\\')
					g_string_append_c(out, '\\');
				g_string_append_c(out, c);
			}
			g_string_append_c(out, '"');
			g_string_append(out, p);

			return;
		}
	}

append:
	g_string_append(out, addr);
}

/**
 * camel_internet_address_encode_address:
 * @len: the length of the line the address is being appended to
 * @name: the unencoded real name associated with the address
 * @addr: the routing address
 *
 * Encode a single address ready for internet usage.  Header folding
 * as per rfc822 is also performed, based on the length *@len.  If @len
 * is %NULL, then no folding will occur.
 *
 * Note: The value at *@in will be updated based on any linewrapping done
 *
 * Returns: the encoded address
 **/
gchar *
camel_internet_address_encode_address(gint *inlen, const gchar *real, const gchar *addr)
{
	gchar *name = camel_header_encode_phrase ((const guchar *) real);
	gchar *ret = NULL;
	gint len = 0;
	GString *out = g_string_new("");

	g_assert(addr);

	if (inlen != NULL)
		len = *inlen;

	if (name && name[0]) {
		if (inlen != NULL && (strlen(name) + len) > CAMEL_FOLD_SIZE) {
			gchar *folded = camel_header_address_fold(name, len);
			gchar *last;
			g_string_append(out, folded);
			g_free(folded);
			last = strrchr(out->str, '\n');
			if (last)
				len = last-(out->str+out->len);
			else
				len = out->len;
		} else {
			g_string_append(out, name);
			len += strlen(name);
		}
	}

	/* NOTE: Strictly speaking, we could and should split the
	 * internal address up if we need to, on atom or specials
	 * boundaries - however, to aid interoperability with mailers
	 * that will probably not handle this case, we will just move
	 * the whole address to its own line. */
	if (inlen != NULL && (strlen(addr) + len) > CAMEL_FOLD_SIZE) {
		g_string_append(out, "\n\t");
		len = 1;
	}

	len -= out->len;

	if (name && name[0])
		g_string_append_printf(out, " <");
	cia_encode_addrspec(out, addr);
	if (name && name[0])
		g_string_append_printf(out, ">");

	len += out->len;

	if (inlen != NULL)
		*inlen = len;

	g_free(name);

	ret = out->str;
	g_string_free(out, FALSE);

	return ret;
}

/**
 * camel_internet_address_format_address:
 * @name: a name, quotes may be stripped from it
 * @addr: an rfc822 routing address
 *
 * Function to format a single address, suitable for display.
 *
 * Returns: a nicely formatted string containing the rfc822 address
 **/
gchar *
camel_internet_address_format_address(const gchar *name, const gchar *addr)
{
	gchar *ret = NULL;

	g_assert(addr);

	if (name && name[0]) {
		const gchar *p = name;
		gchar *o, c;

		while ((c = *p++)) {
			if (c == '\"' || c == ',') {
				o = ret = g_malloc(strlen(name)+3+strlen(addr)+3 + 1);
				p = name;
				*o++ = '\"';
				while ((c = *p++))
					if (c != '\"')
						*o++ = c;
				*o++ = '\"';
				sprintf(o, " <%s>", addr);
				d(printf("encoded '%s' => '%s'\n", name, ret));
				return ret;
			}
		}
		ret = g_strdup_printf("%s <%s>", name, addr);
	} else
		ret = g_strdup(addr);

	return ret;
}
