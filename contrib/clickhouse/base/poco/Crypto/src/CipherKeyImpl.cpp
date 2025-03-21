//
// CipherKeyImpl.cpp
//
// Library: Crypto
// Package: Cipher
// Module:  CipherKeyImpl
//
// Copyright (c) 2008, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "DBPoco/Crypto/CipherKeyImpl.h"
#include "DBPoco/Crypto/CryptoTransform.h"
#include "DBPoco/Crypto/CipherFactory.h"
#include "DBPoco/Exception.h"
#include "DBPoco/RandomStream.h"
#include <openssl/err.h>
#include <openssl/evp.h>


namespace DBPoco {
namespace Crypto {


CipherKeyImpl::CipherKeyImpl(const std::string& name,
	const std::string& passphrase,
	const std::string& salt,
	int iterationCount,
	const std::string& digest):
	_pCipher(0),
	_pDigest(0),
	_name(name),
	_key(),
	_iv()
{
	// dummy access to Cipherfactory so that the EVP lib is initilaized
	CipherFactory::defaultFactory();
	_pCipher = EVP_get_cipherbyname(name.c_str());

	if (!_pCipher)
		throw DBPoco::NotFoundException("Cipher " + name + " was not found");

	_pDigest = EVP_get_digestbyname(digest.c_str());

	if (!_pDigest)
		throw DBPoco::NotFoundException("Digest " + name + " was not found");

	_key = ByteVec(keySize());
	_iv = ByteVec(ivSize());
	generateKey(passphrase, salt, iterationCount);
}


CipherKeyImpl::CipherKeyImpl(const std::string& name,
	const ByteVec& key,
	const ByteVec& iv):
	_pCipher(0),
	_pDigest(0),
	_name(name),
	_key(key),
	_iv(iv)
{
	// dummy access to Cipherfactory so that the EVP lib is initialized
	CipherFactory::defaultFactory();
	_pCipher = EVP_get_cipherbyname(name.c_str());

	if (!_pCipher)
		throw DBPoco::NotFoundException("Cipher " + name + " was not found");
}


CipherKeyImpl::CipherKeyImpl(const std::string& name):
	_pCipher(0),
	_pDigest(0),
	_name(name),
	_key(),
	_iv()
{
	// dummy access to Cipherfactory so that the EVP lib is initilaized
	CipherFactory::defaultFactory();
	_pCipher = EVP_get_cipherbyname(name.c_str());

	if (!_pCipher)
		throw DBPoco::NotFoundException("Cipher " + name + " was not found");
	_key = ByteVec(keySize());
	_iv = ByteVec(ivSize());
	generateKey();
}


CipherKeyImpl::~CipherKeyImpl()
{
}


CipherKeyImpl::Mode CipherKeyImpl::mode() const
{
	switch (EVP_CIPHER_mode(_pCipher))
	{
	case EVP_CIPH_STREAM_CIPHER:
		return MODE_STREAM_CIPHER;

	case EVP_CIPH_ECB_MODE:
		return MODE_ECB;

	case EVP_CIPH_CBC_MODE:
		return MODE_CBC;

	case EVP_CIPH_CFB_MODE:
		return MODE_CFB;

	case EVP_CIPH_OFB_MODE:
		return MODE_OFB;

#if OPENSSL_VERSION_NUMBER >= 0x10001000L
	case EVP_CIPH_CTR_MODE:
		return MODE_CTR;

	case EVP_CIPH_GCM_MODE:
		return MODE_GCM;

#endif
	}
	throw DBPoco::IllegalStateException("Unexpected value of EVP_CIPHER_mode()");
}


void CipherKeyImpl::generateKey()
{
	ByteVec vec;

	getRandomBytes(vec, keySize());
	setKey(vec);

	getRandomBytes(vec, ivSize());
	setIV(vec);
}


void CipherKeyImpl::getRandomBytes(ByteVec& vec, std::size_t count)
{
	DBPoco::RandomInputStream random;

	vec.clear();
	vec.reserve(count);

	for (int i = 0; i < count; ++i)
		vec.push_back(static_cast<unsigned char>(random.get()));
}


void CipherKeyImpl::generateKey(
	const std::string& password,
	const std::string& salt,
	int iterationCount)
{
	unsigned char keyBytes[EVP_MAX_KEY_LENGTH];
	unsigned char ivBytes[EVP_MAX_IV_LENGTH];

	// OpenSSL documentation specifies that the salt must be an 8-byte array.
	unsigned char saltBytes[8];

	if (!salt.empty())
	{
		int len = static_cast<int>(salt.size());
		// Create the salt array from the salt string
		for (int i = 0; i < 8; ++i)
			saltBytes[i] = salt.at(i % len);
		for (int i = 8; i < len; ++i)
			saltBytes[i % 8] ^= salt.at(i);
	}

	// Now create the key and IV, using the MD5 digest algorithm.
	int keySize = EVP_BytesToKey(
		_pCipher,
		_pDigest ? _pDigest : EVP_md5(),
		(salt.empty() ? 0 : saltBytes),
		reinterpret_cast<const unsigned char*>(password.data()),
		static_cast<int>(password.size()),
		iterationCount,
		keyBytes,
		ivBytes);

	// Copy the buffers to our member byte vectors.
	_key.assign(keyBytes, keyBytes + keySize);

	if (ivSize() == 0)
		_iv.clear();
	else
		_iv.assign(ivBytes, ivBytes + ivSize());
}


int CipherKeyImpl::keySize() const
{
	return EVP_CIPHER_key_length(_pCipher);
}


int CipherKeyImpl::blockSize() const
{
	return EVP_CIPHER_block_size(_pCipher);
}


int CipherKeyImpl::ivSize() const
{
	return EVP_CIPHER_iv_length(_pCipher);
}


void CipherKeyImpl::setIV(const ByteVec& iv)
{
	DB_poco_assert(mode() == MODE_GCM || iv.size() == static_cast<ByteVec::size_type>(ivSize()));
	_iv = iv;
}


} } // namespace DBPoco::Crypto
