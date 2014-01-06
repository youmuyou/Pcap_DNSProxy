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

extern std::wstring Path;
extern Configuration Parameter;

//Print errors to log(s)
SSIZE_T __stdcall PrintError(const char *Buffer, const bool Pcap, const SSIZE_T ErrCode)
{
//Print Error(s): ON/OFF
	if (!Parameter.PrintError)
		return 0;

//Get date&time
	tm Time = {0};
	time_t tTime = 0;
	time(&tTime);
	localtime_s(&Time, &tTime);

//Output to file
	FILE *Output = nullptr;
	std::wstring ErrorLogPath(Path);
	if (Pcap)
	{
		ErrorLogPath.append(_T("PcapError.log"));
		_wfopen_s(&Output, ErrorLogPath.c_str(), _T("a"));
		if (Output == nullptr)
			return RETURN_ERROR;

		fprintf(Output, "WinPcap Error: Capture packets error in <%s>.", Buffer);
		fclose(Output);
		return 0;
	}
	else {
		ErrorLogPath.append(_T("Error.log"));
		_wfopen_s(&Output, ErrorLogPath.c_str(), _T("a"));
		if (Output == nullptr)
			return RETURN_ERROR;
	}

	if (ErrCode == NULL)
		fprintf(Output, "%d/%d/%d %d:%d:%d -> %s.\n", Time.tm_year + 1900, Time.tm_mon + 1, Time.tm_mday, Time.tm_hour, Time.tm_min, Time.tm_sec, Buffer);
	else 
		fprintf(Output, "%d/%d/%d %d:%d:%d -> %s, error ID is %d\n", Time.tm_year + 1900, Time.tm_mon + 1, Time.tm_mday, Time.tm_hour, Time.tm_min, Time.tm_sec, Buffer, (int)ErrCode);

	fclose(Output);
	return 0;
}