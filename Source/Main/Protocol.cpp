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

extern Configuration Parameter;

//Get local address(es)
bool __stdcall GetLocalAddress(sockaddr_storage &LocalAddr, const int Protocol)
{
//Initialization
	char *HostName = nullptr;
	try {
		HostName = new char[UDP_MAXSIZE/8]();
	}
	catch (std::bad_alloc)
	{
		PrintError("WinPcap Error: Memory allocation failed", false, NULL);
		return false;
	}
	memset(HostName, 0, UDP_MAXSIZE/8);

	addrinfo Hints = {0}, *Result = nullptr, *PTR = nullptr;
	if (Protocol == AF_INET6) //IPv6
		Hints.ai_family = AF_INET6;
	else //IPv4
		Hints.ai_family = AF_INET;
	Hints.ai_socktype = SOCK_DGRAM;
	Hints.ai_protocol = IPPROTO_UDP;

//Get localhost name
	if (gethostname(HostName, UDP_MAXSIZE/8)!=0)
	{
		delete[] HostName;
		PrintError("Winsock Error: Get localhost name error", false, NULL);
		return false;
	}

//Get localhost data
	SSIZE_T Res = getaddrinfo(HostName, NULL, &Hints, &Result);
	if (Res != 0)
	{
		freeaddrinfo(Result);
		delete[] HostName;
		PrintError("Winsock Error: Get local IP address error", false, Res);
		return false;
	}
	delete[] HostName;

//Report
	for(PTR = Result;PTR != nullptr;PTR = PTR->ai_next)
	{
	//IPv6
		if (PTR->ai_family == AF_INET6 && Protocol == AF_INET6 && 
			!IN6_IS_ADDR_LINKLOCAL((in6_addr *)(PTR->ai_addr)) &&
			!(((sockaddr_in6 *)(PTR->ai_addr))->sin6_scope_id == 0)) //Get port from first(Main) IPv6 device
		{
			((sockaddr_in6 *)&LocalAddr)->sin6_addr = ((sockaddr_in6 *)(PTR->ai_addr))->sin6_addr;
			freeaddrinfo(Result);
			return true;
		}
	//IPv4
		else if (PTR->ai_family == AF_INET && Protocol == AF_INET && 
			((sockaddr_in *)(PTR->ai_addr))->sin_addr.S_un.S_addr != INADDR_LOOPBACK && 
			((sockaddr_in *)(PTR->ai_addr))->sin_addr.S_un.S_addr != INADDR_BROADCAST)
		{
			((sockaddr_in *)&LocalAddr)->sin_addr = ((sockaddr_in *)(PTR->ai_addr))->sin_addr;
			freeaddrinfo(Result);
			return true;
		}
	}

	freeaddrinfo(Result);
	return false;
}

//Get Checksum
USHORT __stdcall GetChecksum(const USHORT *Buffer, size_t Length)
{
	ULONG Checksum = 0;
	while(Length > 1)
	{ 
		Checksum += *Buffer++;
		Length -= sizeof(USHORT);
	}
	
	if (Length)
		Checksum += *(UCHAR *)Buffer;

	Checksum = (Checksum >> 16) + (Checksum & 0xFFFF);
	Checksum += (Checksum >> 16);

	return (USHORT)(~Checksum);
}

//Get ICMPv6 checksum
USHORT __stdcall ICMPv6Checksum(const char *Buffer, const size_t Length)
{
	USHORT Result = 0;
	char *Validation = nullptr;
	try {
		Validation = new char[UDP_MAXSIZE]();
	}
	catch (std::bad_alloc)
	{
		PrintError("WinPcap Error: Memory allocation failed", false, NULL);
		return RETURN_ERROR;
	}
	memset(Validation, 0, UDP_MAXSIZE);

	if (Length - sizeof(ipv6_hdr) > 0)
	{
		ipv6_psd_hdr *psd = (ipv6_psd_hdr *)Validation;
		psd->Dst = ((ipv6_hdr *)Buffer)->Dst;
		psd->Src = ((ipv6_hdr *)Buffer)->Src;
		psd->Length = htonl((ULONG)(Length - sizeof(ipv6_hdr)));
		psd->Next_Header = IPPROTO_ICMPV6;

		memcpy(Validation + sizeof(ipv6_psd_hdr), Buffer + sizeof(ipv6_hdr), Length - sizeof(ipv6_hdr));
		Result = GetChecksum((USHORT *)Validation, sizeof(ipv6_psd_hdr) + Length - sizeof(ipv6_hdr));
	}

	delete[] Validation;
	return Result;
}

//Get UDP checksum
USHORT __stdcall UDPChecksum(const char *Buffer, const size_t Length, const size_t Protocol)
{
//Initialization
	char *Validation = nullptr;
	try {
		Validation = new char[UDP_MAXSIZE]();
	}
	catch (std::bad_alloc)
	{
		PrintError("WinPcap Error: Memory allocation failed", false, NULL);
		return RETURN_ERROR;
	}
	memset(Validation, 0, UDP_MAXSIZE);
	
//Get checksum
	USHORT Result = 0;
	if (Protocol == AF_INET6 && Length - sizeof(ipv6_hdr) > 0) //IPv6
	{
		ipv6_psd_hdr *psd = (ipv6_psd_hdr *)Validation;
		psd->Dst = ((ipv6_hdr *)Buffer)->Dst;
		psd->Src = ((ipv6_hdr *)Buffer)->Src;
		psd->Length = htonl((ULONG)(Length - sizeof(ipv6_hdr)));
		psd->Next_Header = IPPROTO_UDP;

		memcpy(Validation + sizeof(ipv6_psd_hdr), Buffer + sizeof(ipv6_hdr), Length - sizeof(ipv6_hdr));
		Result = GetChecksum((USHORT *)Validation, sizeof(ipv6_psd_hdr) + Length - sizeof(ipv6_hdr));
	}
	else if (Protocol == AF_INET && Length - sizeof(ipv4_hdr) > 0) //IPv4
	{
		ipv4_psd_hdr *psd = (ipv4_psd_hdr *)Validation;
		psd->Dst = ((ipv4_hdr *)Buffer)->Dst;
		psd->Src = ((ipv4_hdr *)Buffer)->Src;
		psd->Length = htons((USHORT)(Length - sizeof(ipv4_hdr)));
		psd->Protocol = IPPROTO_UDP;

		memcpy(Validation+sizeof(ipv4_psd_hdr), Buffer + sizeof(ipv4_hdr), Length - sizeof(ipv4_hdr));
		Result = GetChecksum((USHORT *)Validation, sizeof(ipv4_psd_hdr) + Length - sizeof(ipv4_hdr));
	}

	delete[] Validation;
	return Result;
}

//Convert from char to DNS query
size_t __stdcall CharToDNSQuery(const char *FName, char *TName)
{
	int Index[] = {(int)strlen(FName) - 1, 0, 0};
	Index[2] = Index[0] + 1;
	TName[Index[0] + 2] = 0;

	for (;Index[0] >= 0;Index[0]--,Index[2]--)
	{
		if (FName[Index[0]] == 46)
		{
			TName[Index[2]] = Index[1];
			Index[1] = 0;
		}
		else
		{
			TName[Index[2]] = FName[Index[0]];
			Index[1]++;
		}
	}
	TName[Index[2]] = Index[1];

	return (int)(strlen(TName) + 1);
}

//Convert from DNS query to char
size_t __stdcall DNSQueryToChar(const char *TName, char *FName)
{
	int Index[3] = {0};
	for(Index[0] = 0;Index[0] < UDP_MAXSIZE/8;Index[0]++)
	{
		if (Index[0] == 0)
		{
			Index[1] = TName[Index[0]];
		}
		else if (Index[0] == Index[1] + Index[2] + 1)
		{
			Index[1] = TName[Index[0]];
			if (Index[1] == 0)
				break;
			Index[2] = Index[0];

			FName[Index[0] - 1] = 46;
		}
		else {
			FName[Index[0] - 1] = TName[Index[0]];
		}
	}

	return Index[0];
}

//Convert local address(es) to reply DNS PTR Record(s)
SSIZE_T __stdcall LocalAddressToPTR(std::string &Result, const size_t Protocol)
{
//Initialization
	sockaddr_storage *LocalAddr = nullptr;
	char *Addr = nullptr;
	try {
		LocalAddr = new sockaddr_storage();
		Addr = new char[UDP_MAXSIZE/16]();
	}
	catch(std::bad_alloc)
	{
		PrintError("Winsock Error: Memory allocation failed", false, NULL);
		return RETURN_ERROR;
	}
	memset(LocalAddr, 0, sizeof(sockaddr_storage));
	memset(Addr, 0, UDP_MAXSIZE/16);

	if (!GetLocalAddress(*LocalAddr, (int)Protocol))
	{
		delete LocalAddr;
		delete[] Addr;
		return RETURN_ERROR;
	}

	SSIZE_T Index = 0;
	size_t Location = 0, Colon = 0;
	while (true)
	{
	//IPv6
		if (Protocol == AF_INET6)
		{
			std::string Temp[2];
			std::string::iterator iter;
			Location = 0;
			Colon = 0;

		//Convert from in6_addr to string
			if (inet_ntop(AF_INET6, &((sockaddr_in6 *)LocalAddr)->sin6_addr, Addr, UDP_MAXSIZE/16) <= 0)
			{
				delete LocalAddr;
				delete[] Addr;
				PrintError("Winsock Error: Local IPv6 Address format error", false, NULL);
				return RETURN_ERROR;
			}
			Temp[0] = Addr;

		//Convert to standard IPv6 address format A part(":0:" -> ":0000:")
			while (Temp[0].find(":0:", Index) != std::string::npos)
			{
				Index = Temp[0].find(":0:", Index);
				Temp[0].replace(Index, 3, ":0000:");
			}

		//Count colon
			for (Index = 0;(size_t)Index < Temp[0].length();Index++)
			{
				if (Temp[0].at(Index) == 58)
					Colon++;
			}

		//Convert to standard IPv6 address format B part("::" -> ":0000:...")
			Location = Temp[0].find("::");
			Colon = 8 - Colon;
			Temp[1].append(Temp[0], 0, Location);
			while (Colon != 0)
			{
				Temp[1].append(":0000");
				Colon--;
			}
			Temp[1].append(Temp[0], Location + 1, Temp[0].length() - Location + 1);

			for (iter = Temp[1].begin();iter != Temp[1].end();iter++)
			{
				if (*iter == 58)
					Temp[1].erase(iter);
			}

		//Convert to DNS PTR Record and copy to Result
			for (Index = Temp[1].length() - 1;Index != -1;Index--)
			{
				char Word[] = {0, 0};
				Word[0] = Temp[1].at(Index);
				Result.append(Word);
				Result.append(".");
			}

			Result.append("ip6.arpa");
		}
	//IPv4
		else {
			char CharAddr[4][4] = {0};
			size_t Localtion[] = {0, 0};

		//Convert from in_addr to string
			if (inet_ntop(AF_INET, &((sockaddr_in *)LocalAddr)->sin_addr, Addr, UDP_MAXSIZE/16)<=0)
			{
				delete LocalAddr;
				delete[] Addr;
				PrintError("Winsock Error: Local IPv4 Address format error", false, NULL);
				return RETURN_ERROR;
			}

		//Detach Address data
			for (Index = 0;(size_t)Index < strlen(Addr);Index++)
			{
				if (Addr[Index] == 46)
				{
					Localtion[1] = 0;
					Localtion[0]++;
				}
				else {
					CharAddr[Localtion[0]][Localtion[1]] = Addr[Index];
					Localtion[1]++;
				}
			}

		//Convert to DNS PTR Record and copy to Result
			for (Index = 4;Index > 0;Index--)
			{
				Result.append(CharAddr[Index-1]);
				Result.append(".");
			}

			Result.append("in-addr.arpa");
		}

	//Auto-refresh
		if (Parameter.Hosts == 0)
		{
			delete LocalAddr;
			delete[] Addr;
			return 0;
		}
		else {
			Sleep((DWORD)Parameter.Hosts);
		}
	}

	delete LocalAddr;
	delete[] Addr;
	return 0;
}

//Make ramdom domains
void __stdcall RamdomDomain(char *Domain, const size_t Length)
{
	static const char DomainTable[] = (".-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"); //Preferred name syntax(Section 2.3.1 in RFC 1035)
	memset(Parameter.DomainTestOptions.DomainTest, 0, Length);
	size_t RamdomLength = 0, Sign = 0;

//Make ramdom numbers
	srand((UINT)time((time_t *)NULL));
	RamdomLength = rand() % 61 + 3; //Domain length is between 3 and 63(Labels must be 63 characters or less, Section 2.3.1 in RFC 1035)
	for (Sign = 0;Sign < RamdomLength;Sign++)
		Domain[Sign] = DomainTable[rand() % (int)(sizeof(DomainTable) - 2)];

	return;
}