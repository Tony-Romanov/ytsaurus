//
// X509Certificate.cpp
//
// Library: NetSSL_OpenSSL
// Package: SSLCore
// Module:  X509Certificate
//
// Copyright (c) 2006-2009, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "DBPoco/Net/X509Certificate.h"
#include "DBPoco/Net/SSLException.h"
#include "DBPoco/Net/SSLManager.h"
#include "DBPoco/Net/DNS.h"
#include "DBPoco/TemporaryFile.h"
#include "DBPoco/FileStream.h"
#include "DBPoco/StreamCopier.h"
#include "DBPoco/String.h"
#include "DBPoco/RegularExpression.h"
#include "DBPoco/DateTimeParser.h"
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>


namespace DBPoco {
namespace Net {


X509Certificate::X509Certificate(std::istream& istr):
	DBPoco::Crypto::X509Certificate(istr)
{	
}


X509Certificate::X509Certificate(const std::string& path):
	DBPoco::Crypto::X509Certificate(path)
{
}


X509Certificate::X509Certificate(X509* pCert):
	DBPoco::Crypto::X509Certificate(pCert)
{
}


X509Certificate::X509Certificate(X509* pCert, bool shared):
	DBPoco::Crypto::X509Certificate(pCert, shared)
{
}


X509Certificate::X509Certificate(const DBPoco::Crypto::X509Certificate& cert):
	DBPoco::Crypto::X509Certificate(cert)
{
}


X509Certificate& X509Certificate::operator = (const DBPoco::Crypto::X509Certificate& cert)
{
	X509Certificate tmp(cert);
	swap(tmp);
	return *this;
}


X509Certificate::~X509Certificate()
{
}


bool X509Certificate::verify(const std::string& hostName) const
{
	return verify(*this, hostName);
}


bool X509Certificate::verify(const DBPoco::Crypto::X509Certificate& certificate, const std::string& hostName)
{		
#if OPENSSL_VERSION_NUMBER < 0x10002000L
	std::string commonName;
	std::set<std::string> dnsNames;
	certificate.extractNames(commonName, dnsNames);
	if (!commonName.empty()) dnsNames.insert(commonName);
	bool ok = (dnsNames.find(hostName) != dnsNames.end());
	if (!ok)
	{
		for (std::set<std::string>::const_iterator it = dnsNames.begin(); !ok && it != dnsNames.end(); ++it)
		{
			try
			{
				// two cases: name contains wildcards or not
				if (containsWildcards(*it))
				{
					// a compare by IPAddress is not possible with wildcards
					// only allow compare by name
					ok = matchWildcard(*it, hostName);
				}
				else
				{
					// it depends on hostName whether we compare by IP or by alias
					IPAddress ip;
					if (IPAddress::tryParse(hostName, ip))
					{
						// compare by IP
						const HostEntry& heData = DNS::resolve(*it);
						const HostEntry::AddressList& addr = heData.addresses();
						HostEntry::AddressList::const_iterator it = addr.begin();
						HostEntry::AddressList::const_iterator itEnd = addr.end();
						for (; it != itEnd && !ok; ++it)
						{
							ok = (*it == ip);
						}
					}
					else
					{
						ok = DBPoco::icompare(*it, hostName) == 0;
					}
				}
			}
			catch (NoAddressFoundException&)
			{
			}
			catch (HostNotFoundException&)
			{
			}
		}
	}
	return ok;
#else
	if (X509_check_host(const_cast<X509*>(certificate.certificate()), hostName.c_str(), hostName.length(), 0, NULL) == 1)
	{
		return true;
	}
	else
	{
		IPAddress ip;
		if (IPAddress::tryParse(hostName, ip))
		{
		    return (X509_check_ip_asc(const_cast<X509*>(certificate.certificate()), hostName.c_str(), 0) == 1);
		}
	}
	return false;
#endif
}


bool X509Certificate::containsWildcards(const std::string& commonName)
{
	return (commonName.find('*') != std::string::npos || commonName.find('?') != std::string::npos);
}


bool X509Certificate::matchWildcard(const std::string& wildcard, const std::string& hostName)
{
	// fix wildcards
	std::string wildcardExpr("^");
	wildcardExpr += DBPoco::replace(wildcard, ".", "\\.");
	DBPoco::replaceInPlace(wildcardExpr, "*", ".*");
	DBPoco::replaceInPlace(wildcardExpr, "..*", ".*");
	DBPoco::replaceInPlace(wildcardExpr, "?", ".?");
	DBPoco::replaceInPlace(wildcardExpr, "..?", ".?");
	wildcardExpr += "$";

	DBPoco::RegularExpression expr(wildcardExpr, DBPoco::RegularExpression::RE_CASELESS);
	return expr.match(hostName);
}


} } // namespace DBPoco::Net
