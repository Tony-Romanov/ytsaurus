//
// Limit.cpp
//
// Library: Data
// Package: DataCore
// Module:  Limit
//
// Copyright (c) 2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "DBPoco/Data/Limit.h"


namespace DBPoco {
namespace Data {


Limit::Limit(SizeT value, bool hardLimit, bool isLowerLimit) :
	_value(value),
	_hardLimit(hardLimit),
	_isLowerLimit(isLowerLimit)
{
}


Limit::~Limit()
{
}


} } // namespace DBPoco::Data
