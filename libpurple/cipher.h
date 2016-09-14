/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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

#ifndef PURPLE_CIPHER_H
#define PURPLE_CIPHER_H
/**
 * SECTION:cipher
 * @section_id: libpurple-cipher
 * @short_description: <filename>cipher.h</filename>
 * @title: Cipher and Hash API
 */

#include <glib.h>
#include <glib-object.h>
#include <string.h>

#define PURPLE_TYPE_CIPHER				(purple_cipher_get_type())
#define PURPLE_CIPHER(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CIPHER, PurpleCipher))
#define PURPLE_CIPHER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CIPHER, PurpleCipherClass))
#define PURPLE_IS_CIPHER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CIPHER))
#define PURPLE_IS_CIPHER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CIPHER))
#define PURPLE_CIPHER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CIPHER, PurpleCipherClass))

typedef struct _PurpleCipher       PurpleCipher;
typedef struct _PurpleCipherClass  PurpleCipherClass;

#define PURPLE_TYPE_HASH				(purple_hash_get_type())
#define PURPLE_HASH(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_HASH, PurpleHash))
#define PURPLE_HASH_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_HASH, PurpleHashClass))
#define PURPLE_IS_HASH(obj)				(G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_HASH))
#define PURPLE_IS_HASH_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_HASH))
#define PURPLE_HASH_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_HASH, PurpleHashClass))

typedef struct _PurpleHash       PurpleHash;
typedef struct _PurpleHashClass  PurpleHashClass;

/**
 * PurpleCipherBatchMode:
 * @PURPLE_CIPHER_BATCH_MODE_ECB: Electronic Codebook Mode
 * @PURPLE_CIPHER_BATCH_MODE_CBC: Cipher Block Chaining Mode
 *
 * Modes for batch encrypters
 */
typedef enum {
	PURPLE_CIPHER_BATCH_MODE_ECB,
	PURPLE_CIPHER_BATCH_MODE_CBC
} PurpleCipherBatchMode;

/**
 * PurpleCipher:
 *
 * Purple Cipher is an opaque data structure and should not be used directly.
 */
struct _PurpleCipher {
	GObject gparent;
};

struct _PurpleCipherClass {
	GObjectClass parent_class;

	/** The reset function */
	void (*reset)(PurpleCipher *cipher);

	/** The reset state function */
	void (*reset_state)(PurpleCipher *cipher);

	/** The set initialization vector function */
	void (*set_iv)(PurpleCipher *cipher, guchar *iv, size_t len);

	/** The append data function */
	void (*append)(PurpleCipher *cipher, const guchar *data, size_t len);

	/** The digest function */
	gboolean (*digest)(PurpleCipher *cipher, guchar digest[], size_t len);

	/** The get digest size function */
	size_t (*get_digest_size)(PurpleCipher *cipher);

	/** The encrypt function */
	ssize_t (*encrypt)(PurpleCipher *cipher, const guchar input[], size_t in_len, guchar output[], size_t out_size);

	/** The decrypt function */
	ssize_t (*decrypt)(PurpleCipher *cipher, const guchar input[], size_t in_len, guchar output[], size_t out_size);

	/** The set salt function */
	void (*set_salt)(PurpleCipher *cipher, const guchar *salt, size_t len);

	/** The set key function */
	void (*set_key)(PurpleCipher *cipher, const guchar *key, size_t len);

	/** The get key size function */
	size_t (*get_key_size)(PurpleCipher *cipher);

	/** The set batch mode function */
	void (*set_batch_mode)(PurpleCipher *cipher, PurpleCipherBatchMode mode);

	/** The get batch mode function */
	PurpleCipherBatchMode (*get_batch_mode)(PurpleCipher *cipher);

	/** The get block size function */
	size_t (*get_block_size)(PurpleCipher *cipher);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurpleHash:
 *
 * Purple Hash is an opaque data structure and should not be used directly.
 */
struct _PurpleHash {
	GObject gparent;
};

struct _PurpleHashClass {
	GObjectClass parent_class;

	/** The reset function */
	void (*reset)(PurpleHash *hash);

	/** The reset state function */
	void (*reset_state)(PurpleHash *hash);

	/** The append data function */
	void (*append)(PurpleHash *hash, const guchar *data, size_t len);

	/** The digest function */
	gboolean (*digest)(PurpleHash *hash, guchar digest[], size_t len);

	/** The get digest size function */
	size_t (*get_digest_size)(PurpleHash *hash);

	/** The get block size function */
	size_t (*get_block_size)(PurpleHash *hash);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/*****************************************************************************/
/* PurpleCipher API                                                          */
/*****************************************************************************/

/**
 * purple_cipher_get_type:
 *
 * Returns: The #GType for the Cipher object.
 */
GType purple_cipher_get_type(void);

/**
 * purple_cipher_reset:
 * @cipher:  The cipher
 *
 * Resets a cipher to it's default value
 * Note: If you have set an IV you will have to set it after resetting
 */
void purple_cipher_reset(PurpleCipher *cipher);

/**
 * purple_cipher_reset_state:
 * @cipher:  The cipher
 *
 * Resets a cipher state to it's default value, but doesn't touch stateless
 * configuration.
 *
 * That means, IV and digest will be wiped out, but keys, ops or salt
 * will remain untouched.
 */
void purple_cipher_reset_state(PurpleCipher *cipher);

/**
 * purple_cipher_set_iv:
 * @cipher:  The cipher
 * @iv:      The initialization vector to set
 * @len:     The len of the IV
 *
 * Sets the initialization vector for a cipher
 * Note: This should only be called right after a cipher is created or reset
 */
void purple_cipher_set_iv(PurpleCipher *cipher, guchar *iv, size_t len);

/**
 * purple_cipher_append:
 * @cipher:  The cipher
 * @data:    The data to append
 * @len:     The length of the data
 *
 * Appends data to the cipher context
 */
void purple_cipher_append(PurpleCipher *cipher, const guchar *data, size_t len);

/**
 * purple_cipher_digest:
 * @cipher:  The cipher
 * @digest:  The return buffer for the digest
 * @len:     The length of the buffer
 *
 * Digests a cipher context
 */
gboolean purple_cipher_digest(PurpleCipher *cipher, guchar digest[], size_t len);

/**
 * purple_cipher_digest_to_str:
 * @cipher:   The cipher
 * @digest_s: The return buffer for the string digest
 * @len:      The length of the buffer
 *
 * Converts a guchar digest into a hex string
 */
gboolean purple_cipher_digest_to_str(PurpleCipher *cipher, gchar digest_s[], size_t len);

/**
 * purple_cipher_get_digest_size:
 * @cipher: The cipher whose digest size to get
 *
 * Gets the digest size of a cipher
 *
 * Returns: The digest size of the cipher
 */
size_t purple_cipher_get_digest_size(PurpleCipher *cipher);

/**
 * purple_cipher_encrypt:
 * @cipher:   The cipher
 * @input:    The data to encrypt
 * @in_len:   The length of the data
 * @output:   The output buffer
 * @out_size: The size of the output buffer
 *
 * Encrypts data using the cipher
 *
 * Returns: A length of data that was outputed or -1, if failed
 */
ssize_t purple_cipher_encrypt(PurpleCipher *cipher, const guchar input[], size_t in_len, guchar output[], size_t out_size);

/**
 * purple_cipher_decrypt:
 * @cipher:   The cipher
 * @input:    The data to encrypt
 * @in_len:   The length of the returned value
 * @output:   The output buffer
 * @out_size: The size of the output buffer
 *
 * Decrypts data using the cipher
 *
 * Returns: A length of data that was outputed or -1, if failed
 */
ssize_t purple_cipher_decrypt(PurpleCipher *cipher, const guchar input[], size_t in_len, guchar output[], size_t out_size);

/**
 * purple_cipher_set_salt:
 * @cipher:  The cipher whose salt to set
 * @salt:    The salt
 * @len:     The length of the salt
 *
 * Sets the salt on a cipher
 */
void purple_cipher_set_salt(PurpleCipher *cipher, const guchar *salt, size_t len);

/**
 * purple_cipher_set_key:
 * @cipher:  The cipher whose key to set
 * @key:     The key
 * @len:     The size of the key
 *
 * Sets the key on a cipher
 */
void purple_cipher_set_key(PurpleCipher *cipher, const guchar *key, size_t len);

/**
 * purple_cipher_get_key_size:
 * @cipher: The cipher whose key size to get
 *
 * Gets the size of the key if the cipher supports it
 *
 * Returns: The size of the key
 */
size_t purple_cipher_get_key_size(PurpleCipher *cipher);

/**
 * purple_cipher_set_batch_mode:
 * @cipher:  The cipher whose batch mode to set
 * @mode:    The batch mode under which the cipher should operate
 *
 * Sets the batch mode of a cipher
 */
void purple_cipher_set_batch_mode(PurpleCipher *cipher, PurpleCipherBatchMode mode);

/**
 * purple_cipher_get_batch_mode:
 * @cipher: The cipher whose batch mode to get
 *
 * Gets the batch mode of a cipher
 *
 * Returns: The batch mode under which the cipher is operating
 */
PurpleCipherBatchMode purple_cipher_get_batch_mode(PurpleCipher *cipher);

/**
 * purple_cipher_get_block_size:
 * @cipher: The cipher whose block size to get
 *
 * Gets the block size of a cipher
 *
 * Returns: The block size of the cipher
 */
size_t purple_cipher_get_block_size(PurpleCipher *cipher);

/*****************************************************************************/
/* PurpleHash API                                                            */
/*****************************************************************************/

/**
 * purple_hash_get_type:
 *
 * Returns: The #GType for the Hash object.
 */
GType purple_hash_get_type(void);

/**
 * purple_hash_reset:
 * @hash:  The hash
 *
 * Resets a hash to it's default value
 * Note: If you have set an IV you will have to set it after resetting
 */
void purple_hash_reset(PurpleHash *hash);

/**
 * purple_hash_reset_state:
 * @hash:  The hash
 *
 * Resets a hash state to it's default value, but doesn't touch stateless
 * configuration.
 *
 * That means, IV and digest will be wiped out, but keys, ops or salt
 * will remain untouched.
 */
void purple_hash_reset_state(PurpleHash *hash);

/**
 * purple_hash_append:
 * @hash:    The hash
 * @data:    The data to append
 * @len:     The length of the data
 *
 * Appends data to the hash context
 */
void purple_hash_append(PurpleHash *hash, const guchar *data, size_t len);

/**
 * purple_hash_digest:
 * @hash:    The hash
 * @digest:  The return buffer for the digest
 * @len:     The length of the buffer
 *
 * Digests a hash context
 */
gboolean purple_hash_digest(PurpleHash *hash, guchar digest[], size_t len);

/**
 * purple_hash_digest_to_str:
 * @hash:     The hash
 * @digest_s: The return buffer for the string digest
 * @len:      The length of the buffer
 *
 * Converts a guchar digest into a hex string
 */
gboolean purple_hash_digest_to_str(PurpleHash *hash, gchar digest_s[], size_t len);

/**
 * purple_hash_get_digest_size:
 * @hash: The hash whose digest size to get
 *
 * Gets the digest size of a hash
 *
 * Returns: The digest size of the hash
 */
size_t purple_hash_get_digest_size(PurpleHash *hash);

/**
 * purple_hash_get_block_size:
 * @hash: The hash whose block size to get
 *
 * Gets the block size of a hash
 *
 * Returns: The block size of the hash
 */
size_t purple_hash_get_block_size(PurpleHash *hash);

G_END_DECLS

#endif /* PURPLE_CIPHER_H */
