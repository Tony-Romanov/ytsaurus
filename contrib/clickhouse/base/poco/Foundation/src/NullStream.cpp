//
// NullStream.cpp
//
// Library: Foundation
// Package: Streams
// Module:  NullStream
//
// Copyright (c) 2004-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "DBPoco/NullStream.h"


namespace DBPoco {


NullStreamBuf::NullStreamBuf()
{
}


NullStreamBuf::~NullStreamBuf()
{
}

	
int NullStreamBuf::readFromDevice()
{
	return -1;
}


int NullStreamBuf::writeToDevice(char c)
{
	return charToInt(c);
}


NullIOS::NullIOS()
{
	DB_poco_ios_init(&_buf);
}


NullIOS::~NullIOS()
{
}


NullInputStream::NullInputStream(): std::istream(&_buf)
{
}


NullInputStream::~NullInputStream()
{
}


NullOutputStream::NullOutputStream(): std::ostream(&_buf)
{
}


NullOutputStream::~NullOutputStream()
{
}


} // namespace DBPoco
