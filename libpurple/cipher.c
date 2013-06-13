/*
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Original des taken from gpg
 *
 * des.c - DES and Triple-DES encryption/decryption Algorithm
 *	Copyright (C) 1998 Free Software Foundation, Inc.
 *
 *	Please see below for more legal information!
 *
 *	 According to the definition of DES in FIPS PUB 46-2 from December 1993.
 *	 For a description of triple encryption, see:
 *	   Bruce Schneier: Applied Cryptography. Second Edition.
 *	   John Wiley & Sons, 1996. ISBN 0-471-12845-7. Pages 358 ff.
 *
 *	 This file is part of GnuPG.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#include "internal.h"
#include "cipher.h"

/******************************************************************************
 * Globals
 *****************************************************************************/
static GObjectClass *parent_class = NULL;

/******************************************************************************
 * Object Stuff
 *****************************************************************************/
static void
purple_cipher_finalize(GObject *obj) {
	purple_cipher_reset(PURPLE_CIPHER(obj));

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
purple_cipher_class_init(PurpleCipherClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	klass->reset = NULL;
	klass->set_iv = NULL;
	klass->append = NULL;
	klass->digest = NULL;
	klass->encrypt = NULL;
	klass->decrypt = NULL;
	klass->set_salt = NULL;
	klass->get_salt_size = NULL;
	klass->set_key = NULL;
	klass->get_key_size = NULL;
	klass->set_batch_mode = NULL;
	klass->get_batch_mode = NULL;
	klass->get_block_size = NULL;
	klass->set_key_with_len = NULL;
	klass->get_name = NULL;

	parent_class = g_type_class_peek_parent(klass);

	obj_class->finalize = purple_cipher_finalize;
}

/******************************************************************************
 * PurpleCipher API
 *****************************************************************************/
const gchar *
purple_cipher_get_name(PurpleCipher *cipher) {
	PurpleCipherClass *klass = NULL;

	g_return_val_if_fail(cipher, NULL);
	g_return_val_if_fail(PURPLE_IS_CIPHER(cipher), NULL);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);
	g_return_val_if_fail(klass->get_name, NULL);

	return klass->get_name(cipher);
}

GType
purple_cipher_get_type(void) {
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleCipherClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_cipher_class_init,
			NULL,
			NULL,
			sizeof(PurpleCipher),
			0,
			NULL,
			NULL
		};

		type = g_type_register_static(G_TYPE_OBJECT,
									  "PurpleCipher",
									  &info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

GType
purple_cipher_batch_mode_get_type(void) {
	static GType type = 0;

	if(type == 0) {
		static const GEnumValue values[] = {
			{ PURPLE_CIPHER_BATCH_MODE_ECB, "ECB", "ECB" },
			{ PURPLE_CIPHER_BATCH_MODE_CBC, "CBC", "CBC" },
			{ 0, NULL, NULL },
		};

		type = g_enum_register_static("PurpleCipherBatchMode", values);
	}

	return type;
}

/**
 * purple_cipher_reset:
 * @cipher: The cipher to reset
 *
 * Resets a cipher to it's default value
 *
 * @note If you have set an IV you will have to set it after resetting
 */
void
purple_cipher_reset(PurpleCipher *cipher) {
	PurpleCipherClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CIPHER(cipher));

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->reset)
		klass->reset(cipher);
}

/**
 * purple_cipher_set_iv:
 * @cipher: The cipher to set the IV to
 * @iv: The initialization vector to set
 * @len: The len of the IV
 *
 * @note This should only be called right after a cipher is created or reset
 *
 * Sets the initialization vector for a cipher
 */
void
purple_cipher_set_iv(PurpleCipher *cipher, guchar *iv, size_t len)
{
	PurpleCipherClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CIPHER(cipher));
	g_return_if_fail(iv);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->set_iv)
		klass->set_iv(cipher, iv, len);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"set_iv method\n",
						klass->get_name ? klass->get_name(cipher) : "");
}

/**
 * purple_cipher_append:
 * @cipher: The cipher to append data to
 * @data: The data to append
 * @len: The length of the data
 *
 * Appends data to the cipher
 */
void
purple_cipher_append(PurpleCipher *cipher, const guchar *data,
								size_t len)
{
	PurpleCipherClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CIPHER(cipher));

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->append)
		klass->append(cipher, data, len);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"append method\n",
						klass->get_name ? klass->get_name(cipher) : "");
}

/**
 * purple_cipher_digest:
 * @cipher: The cipher to digest
 * @in_len: The length of the buffer
 * @digest: The return buffer for the digest
 * @out_len: The length of the returned value
 *
 * Digests a cipher
 *
 * Return Value: TRUE if the digest was successful, FALSE otherwise.
 */
gboolean
purple_cipher_digest(PurpleCipher *cipher, size_t in_len,
						   guchar digest[], size_t *out_len)
{
	PurpleCipherClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_CIPHER(cipher), FALSE);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->digest)
		return klass->digest(cipher, in_len, digest, out_len);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"digest method\n",
						klass->get_name ? klass->get_name(cipher) : "");

	return FALSE;
}

/**
 * purple_cipher_digest_to_str:
 * @cipher: The cipher to get a digest from
 * @in_len: The length of the buffer
 * @digest_s: The return buffer for the string digest
 * @out_len: The length of the returned value
 *
 * Converts a guchar digest into a hex string
 *
 * Return Value: TRUE if the digest was successful, FALSE otherwise.
 */
gboolean
purple_cipher_digest_to_str(PurpleCipher *cipher, size_t in_len,
								   gchar digest_s[], size_t *out_len)
{
	/* 8k is a bit excessive, will tweak later. */
	guchar digest[BUF_LEN * 4];
	gint n = 0;
	size_t dlen = 0;

	g_return_val_if_fail(cipher, FALSE);
	g_return_val_if_fail(digest_s, FALSE);

	if(!purple_cipher_digest(cipher, sizeof(digest), digest, &dlen))
		return FALSE;

	/* in_len must be greater than dlen * 2 so we have room for the NUL. */
	if(in_len <= dlen * 2)
		return FALSE;

	for(n = 0; n < dlen; n++)
		sprintf(digest_s + (n * 2), "%02x", digest[n]);

	digest_s[n * 2] = '\0';

	if(out_len)
		*out_len = dlen * 2;

	return TRUE;
}

/**
 * purple_cipher_encrypt:
 * @cipher: The cipher
 * @data: The data to encrypt
 * @len: The length of the data
 * @output: The output buffer
 * @outlen: The len of data that was outputed
 *
 * Encrypts data using the cipher
 *
 * Return Value: A cipher specific status code
 */
gint
purple_cipher_encrypt(PurpleCipher *cipher, const guchar data[],
							size_t len, guchar output[], size_t *outlen)
{
	PurpleCipherClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_CIPHER(cipher), -1);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->encrypt)
		return klass->encrypt(cipher, data, len, output, outlen);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"encrypt method\n",
						klass->get_name ? klass->get_name(cipher) : "");

	if(outlen)
		*outlen = -1;

	return -1;
}

/**
 * purple_cipher_decrypt:
 * @cipher: The cipher
 * @data: The data to encrypt
 * @len: The length of the returned value
 * @output: The output buffer
 * @outlen: The len of data that was outputed
 *
 * Decrypts data using the cipher
 *
 * Return Value: A cipher specific status code
 */
gint
purple_cipher_decrypt(PurpleCipher *cipher, const guchar data[],
							size_t len, guchar output[], size_t *outlen)
{
	PurpleCipherClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_CIPHER(cipher), -1);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->decrypt)
		return klass->decrypt(cipher, data, len, output, outlen);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"decrypt method\n",
						klass->get_name ? klass->get_name(cipher) : "");

	if(outlen)
		*outlen = -1;

	return -1;
}

/**
 * purple_cipher_set_salt:
 * @cipher: The cipher whose salt to set
 * @salt: The salt
 *
 * Sets the salt on a cipher
 */
void
purple_cipher_set_salt(PurpleCipher *cipher, guchar *salt) {
	PurpleCipherClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CIPHER(cipher));

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->set_salt)
		klass->set_salt(cipher, salt);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"set_salt method\n",
						klass->get_name ? klass->get_name(cipher) : "");
}

/**
 * purple_cipher_get_salt_size:
 * @cipher: The cipher whose salt size to get
 *
 * Gets the size of the salt if the cipher supports it
 *
 * Return Value: The size of the salt
 */
size_t
purple_cipher_get_salt_size(PurpleCipher *cipher) {
	PurpleCipherClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_CIPHER(cipher), -1);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->get_salt_size)
		return klass->get_salt_size(cipher);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"get_salt_size method\n",
						klass->get_name ? klass->get_name(cipher) : "");

	return -1;
}

/**
 * purple_cipher_set_key:
 * @cipher: The cipher whose key to set
 * @key: The key
 *
 * Sets the key on a cipher
 */
void
purple_cipher_set_key(PurpleCipher *cipher, const guchar *key) {
	PurpleCipherClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CIPHER(cipher));

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->set_key)
		klass->set_key(cipher, key);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"set_key method\n",
						klass->get_name ? klass->get_name(cipher) : "");
}

/**
 * purple_cipher_get_key_size:
 * @cipher: The cipher whose key size to get
 *
 * Gets the key size for a cipher
 *
 * Return Value: The size of the key
 */
size_t
purple_cipher_get_key_size(PurpleCipher *cipher) {
	PurpleCipherClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_CIPHER(cipher), -1);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->get_key_size)
		return klass->get_key_size(cipher);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"get_key_size method\n",
						klass->get_name ? klass->get_name(cipher) : "");

	return -1;
}

/**
 * purple_cipher_set_batch_mode:
 * @cipher: The cipher whose batch mode to set
 * @mode: The batch mode under which the cipher should operate
 *
 * Sets the batch mode of a cipher
 */
void
purple_cipher_set_batch_mode(PurpleCipher *cipher,
                                     PurpleCipherBatchMode mode)
{
	PurpleCipherClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CIPHER(cipher));

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->set_batch_mode)
		klass->set_batch_mode(cipher, mode);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"set_batch_mode method\n",
						klass->get_name ? klass->get_name(cipher) : "");
}

/**
 * purple_cipher_get_batch_mode:
 * @cipher: The cipher whose batch mode to get
 *
 * Gets the batch mode of a cipher
 *
 * Return Value: The batch mode under which the cipher is operating
 */
PurpleCipherBatchMode
purple_cipher_get_batch_mode(PurpleCipher *cipher)
{
	PurpleCipherClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_CIPHER(cipher), -1);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->get_batch_mode)
		return klass->get_batch_mode(cipher);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"get_batch_mode method\n",
						klass->get_name ? klass->get_name(cipher) : "");

	return -1;
}

/**
 * purple_cipher_get_block_size:
 * @cipher: The cipher whose block size to get
 *
 * Gets the block size of a cipher
 *
 * Return Value: The block size of the cipher
 */
size_t
purple_cipher_get_block_size(PurpleCipher *cipher)
{
	PurpleCipherClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_CIPHER(cipher), -1);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->get_block_size)
		return klass->get_block_size(cipher);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"get_block_size method\n",
						klass->get_name ? klass->get_name(cipher) : "");

	return -1;
}

/**
 * purple_cipher_set_key_with_len:
 * @cipher: The cipher whose key to set
 * @key: The key
 * @len: The length of the key
 *
 * Sets the key with a given length on a cipher
 */
void
purple_cipher_set_key_with_len(PurpleCipher *cipher,
                                       const guchar *key, size_t len)
{
	PurpleCipherClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CIPHER(cipher));
	g_return_if_fail(key);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->set_key_with_len)
		klass->set_key_with_len(cipher, key, len);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"set_key_with_len method\n",
						klass->get_name ? klass->get_name(cipher) : "");
}
