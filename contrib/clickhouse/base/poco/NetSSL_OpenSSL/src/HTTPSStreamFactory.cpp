//
// HTTPSStreamFactory.cpp
//
// Library: NetSSL_OpenSSL
// Package: HTTPSClient
// Module:  HTTPSStreamFactory
//
// Copyright (c) 2006-2012, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "DBPoco/Net/HTTPSStreamFactory.h"
#include "DBPoco/Net/HTTPSClientSession.h"
#include "DBPoco/Net/HTTPIOStream.h"
#include "DBPoco/Net/HTTPRequest.h"
#include "DBPoco/Net/HTTPResponse.h"
#include "DBPoco/Net/HTTPCredentials.h"
#include "DBPoco/Net/NetException.h"
#include "DBPoco/URI.h"
#include "DBPoco/URIStreamOpener.h"
#include "DBPoco/UnbufferedStreamBuf.h"
#include "DBPoco/NullStream.h"
#include "DBPoco/StreamCopier.h"
#include "DBPoco/Format.h"
#include "DBPoco/Version.h"


using DBPoco::URIStreamFactory;
using DBPoco::URI;
using DBPoco::URIStreamOpener;
using DBPoco::UnbufferedStreamBuf;


namespace DBPoco {
namespace Net {


HTTPSStreamFactory::HTTPSStreamFactory():
	_proxyPort(HTTPSession::HTTP_PORT)
{
}


HTTPSStreamFactory::HTTPSStreamFactory(const std::string& proxyHost, DBPoco::UInt16 proxyPort):
	_proxyHost(proxyHost),
	_proxyPort(proxyPort)
{
}


HTTPSStreamFactory::HTTPSStreamFactory(const std::string& proxyHost, DBPoco::UInt16 proxyPort, const std::string& proxyUsername, const std::string& proxyPassword):
	_proxyHost(proxyHost),
	_proxyPort(proxyPort),
	_proxyUsername(proxyUsername),
	_proxyPassword(proxyPassword)
{
}


HTTPSStreamFactory::~HTTPSStreamFactory()
{
}


std::istream* HTTPSStreamFactory::open(const URI& uri)
{
	DB_poco_assert (uri.getScheme() == "https" || uri.getScheme() == "http");

	URI resolvedURI(uri);
	URI proxyUri;
	HTTPClientSession* pSession = 0;
	HTTPResponse res;
	try
	{
		bool retry = false;
		bool authorize = false;
		int redirects = 0;
		std::string username;
		std::string password;
		
		do
		{
			if (!pSession)
			{
				if (resolvedURI.getScheme() != "http")
					pSession = new HTTPSClientSession(resolvedURI.getHost(), resolvedURI.getPort());
				else
					pSession = new HTTPClientSession(resolvedURI.getHost(), resolvedURI.getPort());

				if (proxyUri.empty())
				{
					if (!_proxyHost.empty())
					{
						pSession->setProxy(_proxyHost, _proxyPort);
						pSession->setProxyCredentials(_proxyUsername, _proxyPassword);
					}
				}
				else
				{
					pSession->setProxy(proxyUri.getHost(), proxyUri.getPort());
					if (!_proxyUsername.empty())
					{
						pSession->setProxyCredentials(_proxyUsername, _proxyPassword);
					}
				}
			}
			std::string path = resolvedURI.getPathAndQuery();
			if (path.empty()) path = "/";
			HTTPRequest req(HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1);
			
			if (authorize)
			{
				HTTPCredentials::extractCredentials(uri, username, password);
				HTTPCredentials cred(username, password);
				cred.authenticate(req, res);
			}

			req.set("User-Agent", DBPoco::format("poco/%d.%d.%d", 
				(DB_POCO_VERSION >> 24) & 0xFF,
				(DB_POCO_VERSION >> 16) & 0xFF,
				(DB_POCO_VERSION >> 8) & 0xFF));
			req.set("Accept", "*/*");

			pSession->sendRequest(req);
			std::istream& rs = pSession->receiveResponse(res);
			bool moved = (res.getStatus() == HTTPResponse::HTTP_MOVED_PERMANENTLY || 
			              res.getStatus() == HTTPResponse::HTTP_FOUND || 
			              res.getStatus() == HTTPResponse::HTTP_SEE_OTHER ||
						  res.getStatus() == HTTPResponse::HTTP_TEMPORARY_REDIRECT);
			if (moved)
			{
				resolvedURI.resolve(res.get("Location"));
				if (!username.empty())
				{
					resolvedURI.setUserInfo(username + ":" + password);
					authorize = false;
				}
				delete pSession; 
				pSession = 0;
				++redirects;
				retry = true;
			}
			else if (res.getStatus() == HTTPResponse::HTTP_OK)
			{
				return new HTTPResponseStream(rs, pSession);
			}
			else if (res.getStatus() == HTTPResponse::HTTP_USEPROXY && !retry)
			{
				// The requested resource MUST be accessed through the proxy 
				// given by the Location field. The Location field gives the 
				// URI of the proxy. The recipient is expected to repeat this 
				// single request via the proxy. 305 responses MUST only be generated by origin servers.
				// only use for one single request!
				proxyUri.resolve(res.get("Location"));
				delete pSession; 
				pSession = 0;
				retry = true; // only allow useproxy once
			}
			else if (res.getStatus() == HTTPResponse::HTTP_UNAUTHORIZED && !authorize)
			{
				authorize = true;
				retry = true;
				DBPoco::NullOutputStream null;
				DBPoco::StreamCopier::copyStream(rs, null);
			}
			else throw HTTPException(res.getReason(), uri.toString());
		}
		while (retry && redirects < MAX_REDIRECTS);
		throw HTTPException("Too many redirects", uri.toString());
	}
	catch (...)
	{
		delete pSession;
		throw;
	}
}


void HTTPSStreamFactory::registerFactory()
{
	URIStreamOpener::defaultOpener().registerStreamFactory("https", new HTTPSStreamFactory);
}


void HTTPSStreamFactory::unregisterFactory()
{
	URIStreamOpener::defaultOpener().unregisterStreamFactory("https");
}


} } // namespace DBPoco::Net
