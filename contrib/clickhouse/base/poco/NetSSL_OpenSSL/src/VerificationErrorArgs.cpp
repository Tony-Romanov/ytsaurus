//
// VerificationErrorArgs.cpp
//
// Library: NetSSL_OpenSSL
// Package: SSLCore
// Module:  VerificationErrorArgs
//
// Copyright (c) 2006-2009, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "DBPoco/Net/VerificationErrorArgs.h"


namespace DBPoco {
namespace Net {


VerificationErrorArgs::VerificationErrorArgs(const X509Certificate& cert, int errDepth, int errNum, const std::string& errMsg):
	_cert(cert),
	_errorDepth(errDepth),
	_errorNumber(errNum),
	_errorMessage(errMsg),
	_ignoreError(false)
{
}


VerificationErrorArgs::~VerificationErrorArgs()
{
}


} } // namespace DBPoco::Net
