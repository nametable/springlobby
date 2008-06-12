/*

Copyright (c) 2007, Un Shyam & Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef TORRENT_DISABLE_ENCRYPTION

#include <algorithm>

#include <openssl/dh.h>
#include <openssl/engine.h>

#include "libtorrent/pe_crypto.hpp"
#include "libtorrent/assert.hpp"

namespace libtorrent
{

	// Set the prime P and the generator, generate local public key
	DH_key_exchange::DH_key_exchange()
	{
		m_DH = DH_new();
		if (m_DH == 0) throw std::bad_alloc();

		m_DH->p = BN_bin2bn(m_dh_prime, sizeof(m_dh_prime), NULL);
		m_DH->g = BN_bin2bn(m_dh_generator, sizeof(m_dh_generator), NULL);
		if (m_DH->p == 0 || m_DH->g == 0)
		{
			DH_free(m_DH);
			throw std::bad_alloc();
		}

		m_DH->length = 160l;

		TORRENT_ASSERT(sizeof(m_dh_prime) == DH_size(m_DH));
		
		if (DH_generate_key(m_DH) == 0 || m_DH->pub_key == 0)
		{
			DH_free(m_DH);
			throw std::bad_alloc();
		}

		// DH can generate key sizes that are smaller than the size of
		// P with exponentially decreasing probability, in which case
		// the msb's of m_dh_local_key need to be zeroed
		// appropriately.
		int key_size = get_local_key_size();
		int len_dh = sizeof(m_dh_prime); // must equal DH_size(m_DH)
		if (key_size != len_dh)
		{
			TORRENT_ASSERT(key_size > 0 && key_size < len_dh);

			int pad_zero_size = len_dh - key_size;
			std::fill(m_dh_local_key, m_dh_local_key + pad_zero_size, 0);
			BN_bn2bin(m_DH->pub_key, (unsigned char*)m_dh_local_key + pad_zero_size);
		}
		else
			BN_bn2bin(m_DH->pub_key, (unsigned char*)m_dh_local_key); // TODO Check return value
	}

	DH_key_exchange::~DH_key_exchange()
	{
		TORRENT_ASSERT(m_DH);
		DH_free(m_DH);
	}

	char const* DH_key_exchange::get_local_key() const
	{
		return m_dh_local_key;
	}	


	// compute shared secret given remote public key
	void DH_key_exchange::compute_secret(char const* remote_pubkey)
	{
		TORRENT_ASSERT(remote_pubkey);
		BIGNUM* bn_remote_pubkey = BN_bin2bn ((unsigned char*)remote_pubkey, 96, NULL);
		if (bn_remote_pubkey == 0) throw std::bad_alloc();
		char dh_secret[96];

		int secret_size = DH_compute_key((unsigned char*)dh_secret
			, bn_remote_pubkey, m_DH);
		if (secret_size < 0 || secret_size > 96) throw std::bad_alloc();

		if (secret_size != 96)
		{
			TORRENT_ASSERT(secret_size < 96 && secret_size > 0);
			std::fill(m_dh_secret, m_dh_secret + 96 - secret_size, 0);
		}
		std::copy(dh_secret, dh_secret + secret_size, m_dh_secret + 96 - secret_size);
		BN_free(bn_remote_pubkey);
	}

	char const* DH_key_exchange::get_secret() const
	{
		return m_dh_secret;
	}

	const unsigned char DH_key_exchange::m_dh_prime[96] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2,
		0x21, 0x68, 0xC2, 0x34, 0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
		0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, 0x02, 0x0B, 0xBE, 0xA6,
		0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
		0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D,
		0xF2, 0x5F, 0x14, 0x37, 0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
		0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, 0xF4, 0x4C, 0x42, 0xE9,
		0xA6, 0x3A, 0x36, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x05, 0x63
	};

	const unsigned char DH_key_exchange::m_dh_generator[1] = { 2 };

} // namespace libtorrent

#endif // #ifndef TORRENT_DISABLE_ENCRYPTION

