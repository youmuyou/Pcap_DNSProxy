// This code is part of Pcap_DNSProxy
// Copyright (C) 2012-2014 Chengr28
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#include "Pcap_DNSProxy.h"

#define Interval          5000        //5000ms or 5s between every sending
#define OnceSend          3

extern Configuration Parameter;
extern PortTable PortList;

//Get Hop Limits/TTL with common DNS request
SSIZE_T __stdcall DomainTest(const size_t Protocol)
{
//Turn OFF Domain Test when running in TCP Mode
	if (Parameter.TCPMode)
		return 0;

//Initialization
	char *Buffer = nullptr, *DNSQuery = nullptr;
	SOCKET_Data *SetProtocol = nullptr;
	try {
		Buffer = new char[UDP_MAXSIZE]();
		DNSQuery = new char[UDP_MAXSIZE/4]();
		SetProtocol = new SOCKET_Data();
	}
	catch (std::bad_alloc)
	{
		delete[] Buffer;
		delete[] DNSQuery;
		delete SetProtocol;
		PrintError("Winsock Error: Memory allocation failed", false, NULL);
		return RETURN_ERROR;
	}
	memset(Buffer, 0, UDP_MAXSIZE);
	memset(DNSQuery, 0, UDP_MAXSIZE/4);
	memset(SetProtocol, 0, sizeof(SOCKET_Data));

//Set request protocol
	if (Protocol == AF_INET6) //IPv6
		SetProtocol->AddrLen = sizeof(sockaddr_in6);
	else //IPv4
		SetProtocol->AddrLen = sizeof(sockaddr_in);

//Make a DNS request with Doamin Test packet
	dns_hdr *TestHdr = (dns_hdr *)Buffer;
	TestHdr->ID = Parameter.DomainTestOptions.DomainTestID;
	TestHdr->Flags = htons(0x0100); //System Standard query
	TestHdr->Questions = htons(0x0001);
	size_t TestLength =  0;

//From Parameter
	if (Parameter.DomainTestOptions.DomainTestCheck)
	{
		TestLength = CharToDNSQuery(Parameter.DomainTestOptions.DomainTest, DNSQuery);
		if (TestLength > 0 && TestLength < UDP_MAXSIZE - sizeof(dns_hdr))
		{
			memcpy(Buffer + sizeof(dns_hdr), DNSQuery, TestLength);
			dns_qry *TestQry = (dns_qry *)(Buffer + sizeof(dns_hdr) + TestLength);
			TestQry->Classes = htons(Class_IN);
			if (Protocol == AF_INET6)
				TestQry->Type = htons(AAAA_Records);
			else 
				TestQry->Type = htons(A_Records);
			delete[] DNSQuery;
		}
		else {
			delete[] Buffer;
			delete[] DNSQuery;
			delete SetProtocol;
			return RETURN_ERROR;
		}
	}

//Send
	size_t Times = 0;
	while (true)
	{
		if (Times == OnceSend)
		{
			Times = 0;
			if (Parameter.DNSTarget.IPv4 && Parameter.HopLimitOptions.IPv4TTL == 0 || //IPv4
				Parameter.DNSTarget.IPv6 && Parameter.HopLimitOptions.IPv6HopLimit == 0) //IPv6
			{
				Sleep(Interval); //5 seconds between every sending.
				continue;
			}
			Sleep((DWORD)Parameter.DomainTestOptions.DomainTestSpeed);
		}
		else {
		//Ramdom domain request
			if (!Parameter.DomainTestOptions.DomainTestCheck)
			{
				RamdomDomain(Parameter.DomainTestOptions.DomainTest, UDP_MAXSIZE/8);
				TestLength = CharToDNSQuery(Parameter.DomainTestOptions.DomainTest, DNSQuery);
				memcpy(Buffer + sizeof(dns_hdr), DNSQuery, TestLength);
				
				dns_qry *TestQry = (dns_qry *)(Buffer + sizeof(dns_hdr) + TestLength);
				TestQry->Classes = htons(Class_IN);
				if (Protocol == AF_INET6)
					TestQry->Type = htons(AAAA_Records);
				else 
					TestQry->Type = htons(A_Records);
			}

			UDPRequest(Buffer, TestLength + sizeof(dns_hdr) + 4, *SetProtocol, -1);
			Sleep(Interval);
			Times++;
		}
	}

	if (!Parameter.DomainTestOptions.DomainTestCheck)
		delete[] DNSQuery;
	delete[] Buffer;
	delete SetProtocol;
	return 0;
}

//Internet Control Message Protocol Echo(Ping) request
SSIZE_T __stdcall ICMPEcho()
{
//Initialization
	char *Buffer = nullptr;
	sockaddr_storage *Addr = nullptr;
	try {
		Buffer = new char[UDP_MAXSIZE]();
		Addr = new sockaddr_storage;
	}
	catch (std::bad_alloc)
	{
		delete[] Buffer;
		delete Addr;
		PrintError("Winsock Error: Memory allocation failed", false, NULL);
		return RETURN_ERROR;
	}
	memset(Buffer, 0, UDP_MAXSIZE);
	memset(Addr, 0, sizeof(sockaddr_storage));
	SYSTEM_SOCKET Request = 0;

//Make a ICMP request echo packet
	icmp_hdr *icmp = (icmp_hdr *)Buffer;
	icmp->Type = 8; //Echo(Ping) request type
	icmp->ID = Parameter.ICMPOptions.ICMPID;
	icmp->Sequence = Parameter.ICMPOptions.ICMPSequence;
	memcpy(Buffer + sizeof(icmp_hdr), Parameter.PaddingDataOptions.PaddingData, Parameter.PaddingDataOptions.PaddingDataLength - 1);
	icmp->Checksum = GetChecksum((USHORT *)Buffer, sizeof(icmp_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1);

	Request = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	((sockaddr_in *)Addr)->sin_family = AF_INET;
	((sockaddr_in *)Addr)->sin_addr = Parameter.DNSTarget.IPv4Target;

//Check socket
	if (Request == INVALID_SOCKET)
	{
		delete[] Buffer;
		delete Addr;
		closesocket(Request);
		PrintError("Winsock Error: ICMP Echo(Ping) requesting error", false, NULL);
		return RETURN_ERROR;
	}

//Send
	size_t Times = 0;
	while (true)
	{
		sendto(Request, Buffer, (int)(sizeof(icmp_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1), NULL, (sockaddr *)Addr, sizeof(sockaddr_in));

		if (Times == OnceSend)
		{
			Times = 0;
			if (Parameter.HopLimitOptions.IPv4TTL == 0)
			{
				Sleep(Interval); //5 seconds between every sending.
				continue;
			}
			Sleep((DWORD)Parameter.ICMPOptions.ICMPSpeed);
		}
		else {
			Sleep(Interval);
			Times++;
		}
	}

	delete[] Buffer;
	delete Addr;
	closesocket(Request);
	return 0;
}

//ICMP Echo(Ping) request
SSIZE_T __stdcall ICMPv6Echo()
{
//Initialization
	char *Buffer = nullptr, *ICMPv6Checksum = nullptr;
	sockaddr_storage *Addr = nullptr, *Validate = nullptr;
	try {
		Buffer = new char[UDP_MAXSIZE]();
		ICMPv6Checksum = new char[UDP_MAXSIZE]();
		Addr = new sockaddr_storage;
		Validate = new sockaddr_storage();
	}
	catch (std::bad_alloc)
	{
		delete[] Buffer;
		delete[] ICMPv6Checksum;
		delete Addr;
		delete Validate;
		PrintError("Winsock Error: Memory allocation failed", false, NULL);
		return RETURN_ERROR;
	}
	memset(Buffer, 0, UDP_MAXSIZE);
	memset(ICMPv6Checksum, 0, UDP_MAXSIZE);
	memset(Addr, 0, sizeof(sockaddr_storage));
	memset(Validate, 0, sizeof(sockaddr_storage));
	SYSTEM_SOCKET Request = 0;

//Make a IPv6 ICMPv6 request echo packet
	bool LocalIPv6 = false;
	icmpv6_hdr *icmpv6 = (icmpv6_hdr *)Buffer;
	icmpv6->Type = ICMPV6_REQUEST;
	icmpv6->Code = 0;
	icmpv6->ID = Parameter.ICMPOptions.ICMPID;
	icmpv6->Sequence = Parameter.ICMPOptions.ICMPSequence;

	ipv6_psd_hdr *psd = (ipv6_psd_hdr *)ICMPv6Checksum;
	psd->Dst = Parameter.DNSTarget.IPv6Target;
	//Validate local IPv6 address
	if (!GetLocalAddress(*Validate, AF_INET6))
	{
		delete[] Buffer;
		delete[] ICMPv6Checksum;
		delete Addr;
		delete Validate;
		PrintError("Winsock Error: Get local IPv6 address error", false, NULL);
		return RETURN_ERROR;
	}
	psd->Src = ((sockaddr_in6 *)Validate)->sin6_addr;
	delete Validate;
	//End
	psd->Length = htonl((ULONG)(sizeof(icmpv6_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1));
	psd->Next_Header = IPPROTO_ICMPV6;

	memcpy(ICMPv6Checksum + sizeof(ipv6_psd_hdr), icmpv6, sizeof(icmpv6_hdr));
	memcpy(ICMPv6Checksum + sizeof(ipv6_psd_hdr) + sizeof(icmpv6_hdr), &Parameter.PaddingDataOptions.PaddingData, Parameter.PaddingDataOptions.PaddingDataLength - 1);
	icmpv6->Checksum = htons(GetChecksum((USHORT *)ICMPv6Checksum, sizeof(ipv6_psd_hdr) + sizeof(icmpv6_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1));
	delete[] ICMPv6Checksum;

	Request = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	((sockaddr_in6 *)Addr)->sin6_family = AF_INET6;
	((sockaddr_in6 *)Addr)->sin6_addr = Parameter.DNSTarget.IPv6Target;

//Check socket
	if (Request == INVALID_SOCKET)
	{
		delete[] Buffer;
		delete Addr;
		closesocket(Request);
		PrintError("Winsock Error: ICMPv6 Echo(Ping) requesting error", false, NULL);
		return RETURN_ERROR;
	}

//Send
	size_t Times = 0;
	while (true)
	{
		sendto(Request, Buffer, (int)(sizeof(icmpv6_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1), NULL, (sockaddr *)Addr, sizeof(sockaddr_in6));

		if (Times == OnceSend)
		{
			Times = 0;
			if (Parameter.HopLimitOptions.IPv6HopLimit == 0)
			{
				Sleep(Interval);
				continue;
			}
			Sleep((DWORD)Parameter.ICMPOptions.ICMPSpeed);
		}
		else {
			Times++;
			Sleep(Interval);
		}
	}

	delete[] Buffer;
	delete Addr;
	closesocket(Request);
	return 0;
}

//Transmission and reception of TCP protocol
SSIZE_T __stdcall TCPRequest(const char *Send, char *Recv, const size_t SendSize, const size_t RecvSize, const SOCKET_Data TargetData)
{
//Initialization
	char *OriginalSend = nullptr, *SendBuffer = nullptr, *RecvBuffer = nullptr;
	sockaddr_storage *Addr = nullptr;
	try {
		OriginalSend = new char[UDP_MAXSIZE]();
		SendBuffer = new char[UDP_MAXSIZE]();
		RecvBuffer = new char[UDP_MAXSIZE]();
		Addr = new sockaddr_storage;
	}
	catch (std::bad_alloc)
	{
		delete[] OriginalSend;
		delete[] SendBuffer;
		delete[] RecvBuffer;
		delete Addr;
		PrintError("Winsock Error: Memory allocation failed", false, NULL);
		return RETURN_ERROR;
	}
	memset(OriginalSend, 0, UDP_MAXSIZE);
	memset(SendBuffer, 0, UDP_MAXSIZE);
	memset(RecvBuffer, 0, UDP_MAXSIZE);
	memset(Addr, 0, sizeof(sockaddr_storage));
	SYSTEM_SOCKET TCPSocket = 0;
	memcpy(OriginalSend, Send, SendSize);

//Add length of request packet(It must be written in header when transpot with TCP protocol)
	USHORT DataLength = htons((USHORT)SendSize);
	memcpy(SendBuffer, &DataLength, sizeof(USHORT));
	memcpy(SendBuffer + sizeof(USHORT), OriginalSend, SendSize);
	delete[] OriginalSend;

//Socket initialization
	if (TargetData.AddrLen == sizeof(sockaddr_in6)) //IPv6
	{
		TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
		((sockaddr_in6 *)Addr)->sin6_addr = Parameter.DNSTarget.IPv6Target;
		((sockaddr_in6 *)Addr)->sin6_family = AF_INET6;
		((sockaddr_in6 *)Addr)->sin6_port = htons(DNS_Port);
	}
	else { //IPv4
		TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		((sockaddr_in *)Addr)->sin_addr = Parameter.DNSTarget.IPv4Target;
		((sockaddr_in *)Addr)->sin_family = AF_INET;
		((sockaddr_in *)Addr)->sin_port = htons(DNS_Port);
	}

	if (TCPSocket == INVALID_SOCKET)
	{
		delete[] SendBuffer;
		delete[] RecvBuffer;
		delete Addr;
		closesocket(TCPSocket);
		PrintError("Winsock Error: TCP request initialization error", false, NULL);
		return RETURN_ERROR;
	}

	if (connect(TCPSocket, (sockaddr *)Addr, TargetData.AddrLen) == SOCKET_ERROR) //The connection is RESET or other errors when connecting.
	{
		delete[] SendBuffer;
		delete[] RecvBuffer;
		delete Addr;
		closesocket(TCPSocket);
		return 0;
	}
	delete Addr;

//Send request
	if (send(TCPSocket, SendBuffer, (int)(SendSize + sizeof(USHORT)), NULL) == SOCKET_ERROR) //The connection is RESET or other errors when sending.
	{
		delete[] SendBuffer;
		delete[] RecvBuffer;
		closesocket(TCPSocket);
		return 0;
	}
	delete[] SendBuffer;

//Receive result
	SSIZE_T RecvLen = recv(TCPSocket, RecvBuffer, (int)RecvSize, NULL) - sizeof(USHORT);
	if (RecvLen < 0) //The connection is RESET or other errors when sending.
	{
		delete[] RecvBuffer;
		closesocket(TCPSocket);
		return RecvLen;
	}
	if (RecvLen == 0)
	{
		delete[] RecvBuffer;
		closesocket(TCPSocket);
		PrintError("Winsock Error: TCP requesting error", false, NULL);
		return RETURN_ERROR;
	}
	memcpy(Recv, RecvBuffer + sizeof(USHORT), RecvLen);

	delete[] RecvBuffer;
	closesocket(TCPSocket);
	return RecvLen;
}

//Transmission of UDP protocol
SSIZE_T __stdcall UDPRequest(const char *Send, const size_t Length, const SOCKET_Data TargetData, const SSIZE_T Index)
{
//Initialization
	char *SendBuffer = nullptr;
	sockaddr_storage *Addr = nullptr;
	try {
		SendBuffer = new char[UDP_MAXSIZE]();
		Addr = new sockaddr_storage;
	}
	catch (std::bad_alloc)
	{
		delete[] SendBuffer;
		delete Addr;
		PrintError("Winsock Error: Memory allocation failed", false, NULL);
		return RETURN_ERROR;
	}
	memset(SendBuffer, 0, UDP_MAXSIZE);
	memset(Addr, 0, sizeof(sockaddr_storage));
	SYSTEM_SOCKET UDPSocket = 0;
	memcpy(SendBuffer, Send, Length);

//Socket initialization
	if (TargetData.AddrLen == sizeof(sockaddr_in6)) //IPv6
		{
			UDPSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
			((sockaddr_in6 *)Addr)->sin6_addr = Parameter.DNSTarget.IPv6Target;
			((sockaddr_in6 *)Addr)->sin6_family = AF_INET6;
			((sockaddr_in6 *)Addr)->sin6_port = htons(DNS_Port);
		}
	else { //IPv4
		UDPSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		((sockaddr_in *)Addr)->sin_addr = Parameter.DNSTarget.IPv4Target;
		((sockaddr_in *)Addr)->sin_family = AF_INET;
		((sockaddr_in *)Addr)->sin_port = htons(DNS_Port);
	}

	if (UDPSocket == INVALID_SOCKET)
	{
		delete[] SendBuffer;
		delete Addr;
		closesocket(UDPSocket);
		PrintError("Winsock Error: UDP request initialization error", false, NULL);
		return RETURN_ERROR;
	}

//Send request
	if (sendto(UDPSocket, SendBuffer, (int)Length, 0, (sockaddr *)Addr, TargetData.AddrLen) == SOCKET_ERROR)
	{
		delete[] SendBuffer;
		delete Addr;
		closesocket(UDPSocket);
		PrintError("Winsock Error: UDP requesting error", false, WSAGetLastError());
		return RETURN_ERROR;
	}

//Sign in port list
	if (Index >= 0)
	{
		getsockname(UDPSocket, (sockaddr *)Addr, (int *)&(TargetData.AddrLen));
		if (TargetData.AddrLen == sizeof(sockaddr_in6)) //IPv6
			PortList.SendPort[Index] = ((sockaddr_in6 *)Addr)->sin6_port;
		else //IPv4
			PortList.SendPort[Index] = ((sockaddr_in *)Addr)->sin_port;
	}

	delete[] SendBuffer;
	delete Addr;
	closesocket(UDPSocket);
	return 0;
}