//
// ICMPv4PacketImpl.cpp
//
// Library: Net
// Package: ICMP
// Module:  ICMPv4PacketImpl
//
// Copyright (c) 2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "DBPoco/Net/ICMPv4PacketImpl.h"
#include "DBPoco/Net/NetException.h"
#include "DBPoco/Timestamp.h"
#include "DBPoco/Timespan.h"
#include "DBPoco/NumberFormatter.h"
#include "DBPoco/Process.h"
#include <sstream>


using DBPoco::InvalidArgumentException;
using DBPoco::Timestamp;
using DBPoco::Timespan;
using DBPoco::NumberFormatter;
using DBPoco::UInt8;
using DBPoco::UInt16;
using DBPoco::Int32;


namespace DBPoco {
namespace Net {


const UInt8 ICMPv4PacketImpl::DESTINATION_UNREACHABLE_TYPE       = 3;
const DBPoco::UInt8 ICMPv4PacketImpl::SOURCE_QUENCH_TYPE     = 4;
const DBPoco::UInt8 ICMPv4PacketImpl::REDIRECT_MESSAGE_TYPE  = 5;
const UInt8 ICMPv4PacketImpl::TIME_EXCEEDED_TYPE                 = 11;
const DBPoco::UInt8 ICMPv4PacketImpl::PARAMETER_PROBLEM_TYPE = 12;


const std::string ICMPv4PacketImpl::MESSAGE_TYPE[] = 
{
	"Echo Reply",
	"ICMP 1",
	"ICMP 2",
	"Dest Unreachable",
	"Source Quench",
	"Redirect",
	"ICMP 6",
	"ICMP 7",
	"Echo",
	"ICMP 9",
	"ICMP 10",
	"Time Exceeded",
	"Parameter Problem",
	"Timestamp",
	"Timestamp Reply",
	"Info Request",
	"Info Reply",
	"Unknown type"
};


const std::string ICMPv4PacketImpl::DESTINATION_UNREACHABLE_CODE[] = 
{
	"Net unreachable",
	"Host unreachable",
	"Protocol unreachable",
	"Port unreachable",
	"Fragmentation needed and DF set",
	"Source route failed",
	"Unknown code"
};


const std::string ICMPv4PacketImpl::REDIRECT_MESSAGE_CODE[] = 
{
	"Redirect datagrams for the network",
	"Redirect datagrams for the host",
	"Redirect datagrams for the type of service and network",
	"Redirect datagrams for the type of service and host",
	"Unknown code"
};


const std::string ICMPv4PacketImpl::TIME_EXCEEDED_CODE[] = 
{
	"Time to live exceeded in transit",
	"Fragment reassembly time exceeded",
	"Unknown code"
};


const std::string ICMPv4PacketImpl::PARAMETER_PROBLEM_CODE[] = 
{
	"Pointer indicates the error",
	"Unknown code"
};


ICMPv4PacketImpl::ICMPv4PacketImpl(int dataSize)
	: ICMPPacketImpl(dataSize),
	_seq(0)
{
	initPacket();
}


ICMPv4PacketImpl::~ICMPv4PacketImpl()
{
}


int ICMPv4PacketImpl::packetSize() const
{
	return getDataSize() + sizeof(Header);
}


void ICMPv4PacketImpl::initPacket()
{
	if (_seq >= MAX_SEQ_VALUE) resetSequence();

	Header* icp = (Header*) packet(false);
	icp->type     = ECHO_REQUEST;
	icp->code     = 0;
	icp->checksum = 0;
	icp->seq      = ++_seq;
	icp->id       = static_cast<UInt16>(DBPoco::Process::id());

	struct timeval* ptp = (struct timeval *) (icp + 1);
	*ptp = time();

	icp->checksum = checksum((UInt16*) icp, getDataSize() + sizeof(Header));
}


struct timeval ICMPv4PacketImpl::time(DBPoco::UInt8* buffer, int length) const
{
	struct timeval tv;

	if (0 == buffer || 0 == length)
	{
		Timespan value(Timestamp().epochMicroseconds());
		tv.tv_sec  = (long) value.totalSeconds();
		tv.tv_usec = (long) value.useconds();
	}
	else
	{
		struct timeval* ptv = (struct timeval*) data(buffer, length);
		if (ptv) tv = *ptv;
		else throw InvalidArgumentException("Invalid packet.");
	}
	return tv;
}


ICMPv4PacketImpl::Header* ICMPv4PacketImpl::header(DBPoco::UInt8* buffer, int length) const
{
	DB_poco_check_ptr (buffer);

	int offset = (buffer[0] & 0x0F) * 4;
	if ((offset + sizeof(Header)) > length) return 0;

	buffer += offset;
	return (Header *) buffer;
}


DBPoco::UInt8* ICMPv4PacketImpl::data(DBPoco::UInt8* buffer, int length) const
{
	return ((DBPoco::UInt8*) header(buffer, length)) + sizeof(Header);
}


bool ICMPv4PacketImpl::validReplyID(DBPoco::UInt8* buffer, int length) const
{
	Header *icp = header(buffer, length);
	return icp && (static_cast<DBPoco::UInt16>(Process::id()) == icp->id);
}


std::string ICMPv4PacketImpl::errorDescription(unsigned char* buffer, int length)
{
	Header *icp = header(buffer, length);

	if (!icp) return "Invalid header.";
	if (ECHO_REPLY == icp->type) return std::string(); // not an error

	UInt8 pointer = 0;
	if (PARAMETER_PROBLEM == icp->type)
	{
		UInt8 mask = 0x00FF;
		pointer = icp->id & mask;
	}

	MessageType type = static_cast<MessageType>(icp->type);
	int code = icp->code;
	std::ostringstream err;

	switch (type)
	{
	case DESTINATION_UNREACHABLE_TYPE:
		if (code >= NET_UNREACHABLE && code < DESTINATION_UNREACHABLE_UNKNOWN)
			err << DESTINATION_UNREACHABLE_CODE[code];
		else
			err << DESTINATION_UNREACHABLE_CODE[DESTINATION_UNREACHABLE_UNKNOWN];
		break;
	
	case SOURCE_QUENCH_TYPE:		
		err << "Source quench";
		break;
	
	case REDIRECT_MESSAGE_TYPE:
		if (code >= REDIRECT_NETWORK && code < REDIRECT_MESSAGE_UNKNOWN) 
			err << REDIRECT_MESSAGE_CODE[code];
		else
			err << REDIRECT_MESSAGE_CODE[REDIRECT_MESSAGE_UNKNOWN];
		break;

	case TIME_EXCEEDED_TYPE:
		if (code >= TIME_TO_LIVE || code < TIME_EXCEEDED_UNKNOWN)
			err << TIME_EXCEEDED_CODE[code];
		else
			err << TIME_EXCEEDED_CODE[TIME_EXCEEDED_UNKNOWN];
		break;
	
	case PARAMETER_PROBLEM_TYPE:
		if (POINTER_INDICATES_THE_ERROR != code)
			code = PARAMETER_PROBLEM_UNKNOWN;
		err << PARAMETER_PROBLEM_CODE[code] << ": error in octet #" << pointer;
		break;
	
	default:
		err << "Unknown type.";
		break;
	}

	return err.str();
}

std::string ICMPv4PacketImpl::typeDescription(int typeId)
{
	DB_poco_assert (typeId >= ECHO_REPLY && typeId < MESSAGE_TYPE_LENGTH);

	return MESSAGE_TYPE[typeId];
}


} } // namespace DBPoco::Net
