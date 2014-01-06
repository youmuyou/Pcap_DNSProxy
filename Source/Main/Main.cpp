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

Configuration Parameter;
std::string LocalhostPTR[THREAD_PARTNUM] = {("?"), ("?"), (""), ("")};

extern std::wstring Path;

//The Main function of program
int main(int argc, char *argv[])
{
//Get Path
	if (GetServiceInfo() == RETURN_ERROR)
		return RETURN_ERROR;

//Delete old error log
	std::wstring ErrorPath(Path);
	std::wstring PcapLogPath(Path);
	ErrorPath.append(_T("Error.log"));
	PcapLogPath.append(_T("PcapError.log"));
	DeleteFile(ErrorPath.c_str());
	DeleteFile(PcapLogPath.c_str());
//Read configuration file and WinPcap initialization
	Parameter.PrintError = true;
	if (Parameter.ReadParameter() == RETURN_ERROR || CaptureInitialization() == RETURN_ERROR)
		return RETURN_ERROR;

//Get Localhost DNS PTR Records
	std::thread IPv6LocalAddressThread(LocalAddressToPTR, LocalhostPTR[0], AF_INET6);
	std::thread IPv4LocalAddressThread(LocalAddressToPTR, LocalhostPTR[1], AF_INET);
	IPv6LocalAddressThread.detach();
	IPv4LocalAddressThread.detach();

//Read Hosts
	std::thread HostsThread(&Configuration::ReadHosts, std::ref(Parameter));
	HostsThread.detach();

//Windows Firewall Test
	if (FirewallTest() == RETURN_ERROR)
	{
		PrintError("Winsock Error: Windows Firewall Test failed", false, NULL);
		return RETURN_ERROR;
	}

//Service initialization
	SERVICE_TABLE_ENTRY ServiceTable[] = 
	{
		{LOCALSERVERNAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
		{NULL, NULL}
	};

//Start service
	if(!StartServiceCtrlDispatcher(ServiceTable))
	{
		PrintError("System Error: Service start error", false, NULL);
		return RETURN_ERROR;
	}

	return 0;
}

//Winsock initialization and Windows Firewall Test
inline SSIZE_T __stdcall FirewallTest()
{
//Winsock and socket initialization
	WSADATA WSAData = {0};
	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0 || LOBYTE(WSAData.wVersion) != 2 || HIBYTE(WSAData.wVersion) != 2)
    {
		WSACleanup();
		PrintError("Winsock Error: Winsock initialization error", false, WSAGetLastError());
		return RETURN_ERROR;
	}

//Socket initialization
	SYSTEM_SOCKET LocalFirewall = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	sockaddr_in FirewallAddr = {0};
	FirewallAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	FirewallAddr.sin_family = AF_INET;
	srand((UINT)time((time_t *)NULL));
	FirewallAddr.sin_port = htons((USHORT)(rand() % 65535 + 1));

	if (LocalFirewall == INVALID_SOCKET || bind(LocalFirewall, (sockaddr *)&FirewallAddr, sizeof(sockaddr_in)) == SOCKET_ERROR)
	{
		closesocket(LocalFirewall);
		return RETURN_ERROR;
	}

	closesocket(LocalFirewall);
	return 0;
}