﻿// This code is part of Pcap_DNSProxy(Windows)
// Pcap_DNSProxy, A local DNS server base on WinPcap and LibPcap.
// Copyright (C) 2012-2015 Chengr28
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

//Read Texts input and label types defines
#define READTEXT_PARAMETER             0
#define READTEXT_HOSTS                 1U
#define READTEXT_IPFILTER              2U
#define LABEL_STOP                     1U
#define LABEL_IPFILTER                 2U
#define LABEL_IPFILTER_BLACKLIST       3U
#define LABEL_IPFILTER_LOCAL_ROUTING   4U
#define LABEL_HOSTS                    5U
#define LABEL_HOSTS_LOCAL              6U
#define LABEL_HOSTS_WHITELIST          7U
#define LABEL_HOSTS_BANNED             8U

//Length defines
#define READ_TEXT_MINSIZE                     2U
#define READ_PARAMETER_MINSIZE                8U
#define READ_HOSTS_MINSIZE                    3U
#define READ_IPFILTER_MINSIZE                 5U
#define READ_IPFILTER_BLACKLIST_MINSIZE       3U
#define READ_IPFILTER_LOCAL_ROUTING_MINSIZE   4U

extern ConfigurationTable Parameter;
extern std::vector<std::wstring> ConfigFileList;
extern std::vector<FileData> IPFilterFileList, HostsFileList;
extern DNSCurveConfigurationTable DNSCurveParameter;
extern std::vector<HostsTable> *HostsListUsing, *HostsListModificating;
extern std::vector<AddressRange> *AddressRangeUsing, *AddressRangeModificating;
extern std::vector<ResultBlacklistTable> *ResultBlacklistUsing, *ResultBlacklistModificating;
extern std::vector<AddressPrefixBlock> *LocalRoutingListUsing, *LocalRoutingListModificating;
extern std::deque<DNSCacheData> DNSCacheList;
extern std::mutex HostsListLock, AddressRangeLock, DNSCacheListLock, ResultBlacklistLock, LocalRoutingListLock;

//Read texts
inline bool __fastcall ReadText(const FILE *Input, const size_t InputType, const size_t FileIndex)
{
//Initialization
	std::shared_ptr<char> FileBuffer(new char[FILE_BUFFER_SIZE]()), TextBuffer(new char[FILE_BUFFER_SIZE]()), TextData(new char[FILE_BUFFER_SIZE]());
	size_t ReadLength = 0, Index = 0, TextLength = 0, TextBufferLength = 0, Line = 0, LabelType = 0;
	auto CRLF_Length = false, EraseBOM = true, LabelComments = false;

//Read data.
	while (!feof((FILE *)Input))
	{
	//Read file and Mark last read.
		ReadLength = fread_s(FileBuffer.get(), FILE_BUFFER_SIZE, sizeof(char), FILE_BUFFER_SIZE, (FILE *)Input);

	//Erase BOM of Unicode Transformation Format/UTF at first.
		if (EraseBOM)
		{
			if (ReadLength <= 4U)
			{
				if (InputType == READTEXT_HOSTS) //ReadHosts
					PrintError(LOG_ERROR_HOSTS, L"Data of a line is too short", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
				else if (InputType == READTEXT_IPFILTER) //ReadIPFilter
					PrintError(LOG_ERROR_IPFILTER, L"Data of a line is too short", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
				else //ReadParameter
					PrintError(LOG_ERROR_PARAMETER, L"Data of a line is too short", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);

				return false;
			}
			else {
				EraseBOM = false;
			}

		//8-bit Unicode Transformation Format/UTF-8 with BOM
//			if (FileBuffer.get()[0] == 0xFFFFFFEF && FileBuffer.get()[1U] == 0xFFFFFFBB && FileBuffer.get()[2U] == 0xFFFFFFBF) //0xEF, 0xBB, 0xBF(Unsigned char)
			if ((UCHAR)FileBuffer.get()[0] == 0xEF && (UCHAR)FileBuffer.get()[1U] == 0xBB && (UCHAR)FileBuffer.get()[2U] == 0xBF) //0xEF, 0xBB, 0xBF
			{
				memmove(FileBuffer.get(), FileBuffer.get() + BOM_UTF_8_LENGTH, FILE_BUFFER_SIZE - BOM_UTF_8_LENGTH);
				memset(FileBuffer.get() + FILE_BUFFER_SIZE - BOM_UTF_8_LENGTH, 0, BOM_UTF_8_LENGTH);
				ReadLength -= BOM_UTF_8_LENGTH;
			}
		//32-bit Unicode Transformation Format/UTF-32 Little Endian/LE
//			else if (FileBuffer.get()[0] == 0xFFFFFFFF && FileBuffer.get()[1U] == 0xFFFFFFFE && FileBuffer.get()[2U] == 0 && FileBuffer.get()[3U] == 0) //0xFF, 0xFE, 0x00, 0x00(Unsigned char)
			else if ((UCHAR)FileBuffer.get()[0] == 0xFF && (UCHAR)FileBuffer.get()[1U] == 0xFE && FileBuffer.get()[2U] == 0 && FileBuffer.get()[3U] == 0) //0xFF, 0xFE, 0x00, 0x00
			{
				memmove(FileBuffer.get(), FileBuffer.get() + BOM_UTF_32_LENGTH, FILE_BUFFER_SIZE - BOM_UTF_32_LENGTH);
				memset(FileBuffer.get() + FILE_BUFFER_SIZE - BOM_UTF_32_LENGTH, 0, BOM_UTF_32_LENGTH);
				ReadLength -= BOM_UTF_32_LENGTH;
			}
		//32-bit Unicode Transformation Format/UTF-32 Big Endian/BE
//			else if (FileBuffer.get()[0] == 0 && FileBuffer.get()[1U] == 0 && FileBuffer.get()[2U] == 0xFFFFFFFE && FileBuffer.get()[3U] == 0xFFFFFFFF) //0x00, 0x00, 0xFE, 0xFF(Unsigned char)
			else if (FileBuffer.get()[0] == 0 && FileBuffer.get()[1U] == 0 && (UCHAR)FileBuffer.get()[2U] == 0xFE && (UCHAR)FileBuffer.get()[3U] == 0xFF) //0x00, 0x00, 0xFE, 0xFF
			{
				memmove(FileBuffer.get(), FileBuffer.get() + BOM_UTF_32_LENGTH, FILE_BUFFER_SIZE - BOM_UTF_32_LENGTH);
				memset(FileBuffer.get() + FILE_BUFFER_SIZE - BOM_UTF_32_LENGTH, 0, BOM_UTF_32_LENGTH);
				ReadLength -= BOM_UTF_32_LENGTH;
			}
		//16-bit Unicode Transformation Format/UTF-16 Little Endian/LE
//			else if (FileBuffer.get()[0] == 0xFFFFFFFF && FileBuffer.get()[1U] == 0xFFFFFFFE) //0xFF, 0xFE(Unsigned char)
			else if ((UCHAR)FileBuffer.get()[0] == 0xFF && (UCHAR)FileBuffer.get()[1U] == 0xFE) //0xFF, 0xFE
			{
				memmove(FileBuffer.get(), FileBuffer.get() + BOM_UTF_16_LENGTH, FILE_BUFFER_SIZE - BOM_UTF_16_LENGTH);
				memset(FileBuffer.get() + FILE_BUFFER_SIZE - BOM_UTF_16_LENGTH, 0, BOM_UTF_16_LENGTH);
				ReadLength -= BOM_UTF_16_LENGTH;
			}
		//16-bit Unicode Transformation Format/UTF-16 Big Endian/BE
//			else if (FileBuffer.get()[0] == 0xFFFFFFFE && FileBuffer.get()[1U] == 0xFFFFFFFF) //0xFE, 0xFF(Unsigned char)
			else if ((UCHAR)FileBuffer.get()[0] == 0xFE && (UCHAR)FileBuffer.get()[1U] == 0xFF) //0xFE, 0xFF
			{
				memmove(FileBuffer.get(), FileBuffer.get() + BOM_UTF_16_LENGTH, FILE_BUFFER_SIZE - BOM_UTF_16_LENGTH);
				memset(FileBuffer.get() + FILE_BUFFER_SIZE - BOM_UTF_16_LENGTH, 0, BOM_UTF_16_LENGTH);
				ReadLength -= BOM_UTF_16_LENGTH;
			}
		//8-bit Unicode Transformation Format/UTF-8 without BOM/Microsoft Windows ANSI Codepages
//			else {
//				;
//			}
		}

	//Mark words.
		for (Index = 0;Index < ReadLength;Index++)
		{
			if (FileBuffer.get()[Index] != 0)
			{
				if (!CRLF_Length && (FileBuffer.get()[Index] == ASCII_CR || FileBuffer.get()[Index] == ASCII_LF))
					CRLF_Length = true;

				TextBuffer.get()[TextBufferLength] = FileBuffer.get()[Index];
				TextBufferLength++;
			}
		}

	//Lines length check
		if (!CRLF_Length)
		{
			if (InputType == READTEXT_HOSTS) //ReadHosts
				PrintError(LOG_ERROR_HOSTS, L"Data of a line is too long", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
			else if (InputType == READTEXT_IPFILTER) //ReadIPFilter
				PrintError(LOG_ERROR_IPFILTER, L"Data of a line is too long", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
			else //ReadParameter
				PrintError(LOG_ERROR_PARAMETER, L"Data of a line is too long", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);

			return false;
		}
		else {
			CRLF_Length = false;
		}

		memset(FileBuffer.get(), 0, FILE_BUFFER_SIZE);
		memcpy(FileBuffer.get(), TextBuffer.get(), TextBufferLength);
		ReadLength = TextBufferLength;
		memset(TextBuffer.get(), 0, FILE_BUFFER_SIZE);
		TextBufferLength = 0;

	//Read data.
		for (Index = 0;Index < ReadLength;Index++)
		{
			if (FileBuffer.get()[Index] == ASCII_CR) //Macintosh format.
			{
				Line++;

			//Read texts.
				if (TextLength > READ_TEXT_MINSIZE)
				{
				//ReadHosts
					if (InputType == READTEXT_HOSTS)
					{
						ReadHostsData(TextData.get(), FileIndex, Line, LabelType, LabelComments);
					}
				//ReadIPFilter
					else if (InputType == READTEXT_IPFILTER)
					{
						ReadIPFilterData(TextData.get(), FileIndex, Line, LabelType, LabelComments);
					}
				//ReadParameter
					else {
						if (ReadParameterData(TextData.get(), FileIndex, Line, LabelComments) == EXIT_FAILURE)
							return false;
					}
				}

				memset(TextData.get(), 0, FILE_BUFFER_SIZE);
				TextLength = 0;
			}
			else if (FileBuffer.get()[Index] == ASCII_LF) //Unix format.
			{
				if (Index > 0 && FileBuffer.get()[Index - 1U] != ASCII_CR) //Windows format.
					Line++;

			//Read texts.
				if (TextLength > READ_TEXT_MINSIZE)
				{
				//ReadHosts
					if (InputType == READTEXT_HOSTS)
					{
						ReadHostsData(TextData.get(), FileIndex, Line, LabelType, LabelComments);
					}
				//ReadIPFilter
					else if (InputType == READTEXT_IPFILTER)
					{
						ReadIPFilterData(TextData.get(), FileIndex, Line, LabelType, LabelComments);
					}
				//ReadParameter
					else {
						if (ReadParameterData(TextData.get(), FileIndex, Line, LabelComments) == EXIT_FAILURE)
							return false;
					}
				}

				memset(TextData.get(), 0, FILE_BUFFER_SIZE);
				TextLength = 0;
			}
			else if (Index == ReadLength - 1U && feof((FILE *)Input)) //Last line
			{
				Line++;
				TextData.get()[TextLength] = FileBuffer.get()[Index];

			//Read texts.
				if (TextLength > READ_TEXT_MINSIZE)
				{
				//ReadHosts
					if (InputType == READTEXT_HOSTS)
					{
						if (ReadHostsData(TextData.get(), FileIndex, Line, LabelType, LabelComments) == EXIT_FAILURE)
							return false;
					}
				//ReadIPFilter
					else if (InputType == READTEXT_IPFILTER)
					{
						if (ReadIPFilterData(TextData.get(), FileIndex, Line, LabelType, LabelComments) == EXIT_FAILURE)
							return false;
					}
				//ReadParameter
					else {
						if (ReadParameterData(TextData.get(), FileIndex, Line, LabelComments) == EXIT_FAILURE)
							return false;
					}
				}

				return true;
			}
			else {
				TextData.get()[TextLength] = FileBuffer.get()[Index];
				TextLength++;
			}
		}

		memset(FileBuffer.get(), 0, FILE_BUFFER_SIZE);
	}

	return true;
}

//Check Multi-line comments
inline size_t __fastcall ReadMultiLineComments(const PSTR Buffer, std::string &Data, bool &LabelComments)
{
	if (LabelComments)
	{
		if (Data.find("*/") != std::string::npos)
		{
			Data = Buffer + Data.find("*/") + 2U;
			LabelComments = false;
		}
		else {
			return EXIT_FAILURE;
		}
	}
	while (Data.find("/*") != std::string::npos)
	{
		if (Data.find("*/") == std::string::npos)
		{
			Data.erase(Data.find("/*"), Data.length() - Data.find("/*"));
			LabelComments = true;
			break;
		}
		else {
			Data.erase(Data.find("/*"), Data.find("*/") - Data.find("/*") + 2U);
		}
	}

	return EXIT_SUCCESS;
}

//Read parameter from file
size_t __fastcall ReadParameter(void)
{
//Initialization
	FILE *Input = nullptr;
	size_t Index = 0;

//Open file.
	std::wstring ConfigFileName = Parameter.Path->front();
	ConfigFileName.append(L"Config.ini");
	ConfigFileList.push_back(ConfigFileName);
	ConfigFileName = Parameter.Path->front();
	ConfigFileName.append(L"Config.conf");
	ConfigFileList.push_back(ConfigFileName);
	ConfigFileName = Parameter.Path->front();
	ConfigFileName.append(L"Config");
	ConfigFileList.push_back(ConfigFileName);
	ConfigFileName.clear();
	ConfigFileName.shrink_to_fit();
	for (Index = 0;Index < ConfigFileList.size();Index++)
	{
		if (_wfopen_s(&Input, ConfigFileList[Index].c_str(), L"rb") == 0)
		{
			if (Input != nullptr)
				break;
		}

	//Check all configuration files.
		if (Index == ConfigFileList.size() - 1U)
		{
			PrintError(LOG_ERROR_PARAMETER, L"Cannot open any configuration files", NULL, nullptr, NULL);
			return EXIT_FAILURE;
		}
	}

//Check whole file size.
	HANDLE ConfigFileHandle = CreateFileW(ConfigFileList[Index].c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (ConfigFileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER ConfigFileSize = {0};
		if (GetFileSizeEx(ConfigFileHandle, &ConfigFileSize) == 0)
		{
			CloseHandle(ConfigFileHandle);
		}
		else {
			CloseHandle(ConfigFileHandle);
			if (ConfigFileSize.QuadPart >= DEFAULT_FILE_MAXSIZE)
			{
				PrintError(LOG_ERROR_PARAMETER, L"Configuration file size is too large", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
				return EXIT_FAILURE;
			}
		}
	}

//Read data.
	if (Input != nullptr)
	{
		if (!ReadText(Input, READTEXT_PARAMETER, Index))
			return EXIT_FAILURE;
		fclose(Input);
	}

//Check parameters.
	if (Parameter.Version > PRODUCT_VERSION) //Version check
	{
		PrintError(LOG_ERROR_PARAMETER, L"Configuration file version error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
		return EXIT_FAILURE;
	}
	else if (Parameter.Version < PRODUCT_VERSION)
	{
		PrintError(LOG_ERROR_PARAMETER, L"Configuration file is not the latest version", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
	}

//Clear when Print Running Log is disable.
	if (Parameter.PrintStatus == 0)
	{
		delete Parameter.RunningLogPath;
		Parameter.RunningLogPath = nullptr;
	}

//Log max size check
	if (Parameter.LogMaxSize == 0)
	{
		Parameter.LogMaxSize = DEFAULT_LOG_MAXSIZE;
	}
	else if (Parameter.LogMaxSize < DEFAULT_LOG_MINSIZE || Parameter.LogMaxSize > DEFAULT_FILE_MAXSIZE)
	{
		PrintError(LOG_ERROR_PARAMETER, L"Log file size error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
		return EXIT_FAILURE;
	}

//Multi Request Times check
	if (Parameter.MultiRequestTimes > MULTI_REQUEST_TIMES_MAXNUM)
	{
		PrintError(LOG_ERROR_PARAMETER, L"Multi requesting times error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
		return EXIT_FAILURE;
	}
	else if (Parameter.MultiRequestTimes < 1U)
	{
		Parameter.MultiRequestTimes = 1U;
	}

//DNS and Alternate Targets check
	if (!Parameter.DNSTarget.IPv6_Multi->empty())
	{
		Parameter.AlternateMultiRequest = true;

	//Copy DNS Server Data when Main or Alternate data are empty.
		if (Parameter.DNSTarget.IPv6.AddressData.Storage.ss_family == NULL)
		{
			uint8_t HopLimitTemp = 0;
			if (Parameter.DNSTarget.IPv6.HopLimitData.HopLimit > 0)
				HopLimitTemp = Parameter.DNSTarget.IPv6.HopLimitData.HopLimit;
			Parameter.DNSTarget.IPv6 = Parameter.DNSTarget.IPv6_Multi->front();
			Parameter.DNSTarget.IPv6.HopLimitData.HopLimit = HopLimitTemp;
			Parameter.DNSTarget.IPv6_Multi->erase(Parameter.DNSTarget.IPv6_Multi->begin());
		}

		if (Parameter.DNSTarget.Alternate_IPv6.AddressData.Storage.ss_family == NULL && !Parameter.DNSTarget.IPv6_Multi->empty())
		{
			uint8_t HopLimitTemp = 0;
			if (Parameter.DNSTarget.Alternate_IPv6.HopLimitData.HopLimit > 0)
				HopLimitTemp = Parameter.DNSTarget.Alternate_IPv6.HopLimitData.HopLimit;
			Parameter.DNSTarget.Alternate_IPv6 = Parameter.DNSTarget.IPv6_Multi->front();
			Parameter.DNSTarget.Alternate_IPv6.HopLimitData.HopLimit = HopLimitTemp;
			Parameter.DNSTarget.IPv6_Multi->erase(Parameter.DNSTarget.IPv6_Multi->begin());
		}

		//Multi select mode check
		if (Parameter.DNSTarget.IPv6_Multi->size() + 2U > FD_SETSIZE || //UDP requesting
			Parameter.RequestMode == REQUEST_TCPMODE && (Parameter.DNSTarget.IPv6_Multi->size() + 2U) * Parameter.MultiRequestTimes > FD_SETSIZE) //TCP requesting
		{
			PrintError(LOG_ERROR_PARAMETER, L"Too many multi addresses", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
			return EXIT_FAILURE;
		}

	//Multi DNS Server check
		if (Parameter.DNSTarget.IPv6_Multi->empty())
		{
			delete Parameter.DNSTarget.IPv6_Multi;
			Parameter.DNSTarget.IPv6_Multi = nullptr;
		}
	}
	else {
		delete Parameter.DNSTarget.IPv6_Multi;
		Parameter.DNSTarget.IPv6_Multi = nullptr;
	}
	if (!Parameter.DNSTarget.IPv4_Multi->empty())
	{
		Parameter.AlternateMultiRequest = true;

	//Copy DNS Server Data when Main or Alternate data are empty.
		if (Parameter.DNSTarget.IPv4.AddressData.Storage.ss_family == NULL)
		{
			uint8_t TTLTemp = 0;
			if (Parameter.DNSTarget.IPv4.HopLimitData.TTL > 0)
				TTLTemp = Parameter.DNSTarget.IPv4.HopLimitData.TTL;
			Parameter.DNSTarget.IPv4 = Parameter.DNSTarget.IPv4_Multi->front();
			Parameter.DNSTarget.IPv4.HopLimitData.TTL = TTLTemp;
			Parameter.DNSTarget.IPv4_Multi->erase(Parameter.DNSTarget.IPv4_Multi->begin());
		}

		if (Parameter.DNSTarget.Alternate_IPv4.AddressData.Storage.ss_family == NULL && !Parameter.DNSTarget.IPv4_Multi->empty())
		{
			uint8_t TTLTemp = 0;
			if (Parameter.DNSTarget.Alternate_IPv4.HopLimitData.TTL > 0)
				TTLTemp = Parameter.DNSTarget.Alternate_IPv4.HopLimitData.TTL;
			Parameter.DNSTarget.Alternate_IPv4 = Parameter.DNSTarget.IPv4_Multi->front();
			Parameter.DNSTarget.Alternate_IPv4.HopLimitData.TTL = TTLTemp;
			Parameter.DNSTarget.IPv4_Multi->erase(Parameter.DNSTarget.IPv4_Multi->begin());
		}

	//Multi select mode check
		if (Parameter.DNSTarget.IPv4_Multi->size() + 2U > FD_SETSIZE || //UDP requesting
			Parameter.RequestMode == REQUEST_TCPMODE && (Parameter.DNSTarget.IPv4_Multi->size() + 2U) * Parameter.MultiRequestTimes > FD_SETSIZE) //TCP requesting
		{
			PrintError(LOG_ERROR_PARAMETER, L"Too many multi addresses", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
			return EXIT_FAILURE;
		}

	//Multi DNS Server check
		if (Parameter.DNSTarget.IPv4_Multi->empty())
		{
			delete Parameter.DNSTarget.IPv4_Multi;
			Parameter.DNSTarget.IPv4_Multi = nullptr;
		}
	}
	else {
		delete Parameter.DNSTarget.IPv4_Multi;
		Parameter.DNSTarget.IPv4_Multi = nullptr;
	}
	if (Parameter.DNSTarget.IPv6.AddressData.Storage.ss_family == NULL && Parameter.DNSTarget.Alternate_IPv6.AddressData.Storage.ss_family != NULL)
	{
		Parameter.DNSTarget.IPv6 = Parameter.DNSTarget.Alternate_IPv6;
		memset(&Parameter.DNSTarget.Alternate_IPv6, 0, sizeof(DNSServerData));
	}
	if (Parameter.DNSTarget.IPv4.AddressData.Storage.ss_family == NULL && Parameter.DNSTarget.Alternate_IPv4.AddressData.Storage.ss_family != NULL)
	{
		Parameter.DNSTarget.IPv4 = Parameter.DNSTarget.Alternate_IPv4;
		memset(&Parameter.DNSTarget.Alternate_IPv4, 0, sizeof(DNSServerData));
	}
	if (Parameter.DNSTarget.Local_IPv6.AddressData.Storage.ss_family == NULL && Parameter.DNSTarget.Alternate_Local_IPv6.AddressData.Storage.ss_family != NULL)
	{
		Parameter.DNSTarget.Local_IPv6 = Parameter.DNSTarget.Alternate_Local_IPv6;
		memset(&Parameter.DNSTarget.Alternate_Local_IPv6, 0, sizeof(DNSServerData));
	}
	if (Parameter.DNSTarget.Local_IPv4.AddressData.Storage.ss_family == NULL && Parameter.DNSTarget.Alternate_Local_IPv4.AddressData.Storage.ss_family != NULL)
	{
		Parameter.DNSTarget.Local_IPv4 = Parameter.DNSTarget.Alternate_Local_IPv4;
		memset(&Parameter.DNSTarget.Alternate_Local_IPv4, 0, sizeof(DNSServerData));
	}
	if (Parameter.DNSTarget.IPv4.AddressData.Storage.ss_family == NULL && Parameter.DNSTarget.IPv6.AddressData.Storage.ss_family == NULL ||
	//Check repeating items.
		Parameter.DNSTarget.IPv4.AddressData.Storage.ss_family != NULL && Parameter.DNSTarget.Alternate_IPv4.AddressData.Storage.ss_family != NULL && Parameter.DNSTarget.IPv4.AddressData.IPv4.sin_addr.S_un.S_addr == Parameter.DNSTarget.Alternate_IPv4.AddressData.IPv4.sin_addr.S_un.S_addr ||
		Parameter.DNSTarget.Local_IPv4.AddressData.Storage.ss_family != NULL && Parameter.DNSTarget.Alternate_Local_IPv4.AddressData.Storage.ss_family != NULL && Parameter.DNSTarget.Local_IPv4.AddressData.IPv4.sin_addr.S_un.S_addr == Parameter.DNSTarget.Alternate_Local_IPv4.AddressData.IPv4.sin_addr.S_un.S_addr ||
		Parameter.DNSTarget.IPv6.AddressData.Storage.ss_family != NULL && Parameter.DNSTarget.Alternate_IPv6.AddressData.Storage.ss_family != NULL && memcmp(&Parameter.DNSTarget.IPv6.AddressData.IPv6.sin6_addr, &Parameter.DNSTarget.Alternate_IPv6.AddressData.IPv6.sin6_addr, sizeof(in6_addr)) == 0 ||
		Parameter.DNSTarget.Local_IPv6.AddressData.Storage.ss_family != NULL && Parameter.DNSTarget.Alternate_Local_IPv6.AddressData.Storage.ss_family != NULL && memcmp(&Parameter.DNSTarget.Local_IPv6.AddressData.IPv6.sin6_addr, &Parameter.DNSTarget.Alternate_Local_IPv6.AddressData.IPv6.sin6_addr, sizeof(in6_addr)) == 0)
	{
		PrintError(LOG_ERROR_PARAMETER, L"DNS Targets error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
		return EXIT_FAILURE;
	}

	//Hop Limit or TTL Fluctuations check
	if (Parameter.HopLimitFluctuation > 0)
	{
		//IPv4
		if (Parameter.DNSTarget.IPv4.HopLimitData.TTL > 0 &&
			((size_t)Parameter.DNSTarget.IPv4.HopLimitData.TTL + (size_t)Parameter.HopLimitFluctuation > U8_MAXNUM ||
			(SSIZE_T)Parameter.DNSTarget.IPv4.HopLimitData.TTL < (SSIZE_T)Parameter.HopLimitFluctuation + 1) ||
			Parameter.DNSTarget.Alternate_IPv4.HopLimitData.TTL > 0 &&
			((size_t)Parameter.DNSTarget.Alternate_IPv4.HopLimitData.TTL + (size_t)Parameter.HopLimitFluctuation > U8_MAXNUM ||
			(SSIZE_T)Parameter.DNSTarget.Alternate_IPv4.HopLimitData.TTL < (SSIZE_T)Parameter.HopLimitFluctuation + 1) ||
		//IPv6
			Parameter.DNSTarget.IPv6.HopLimitData.HopLimit > 0 &&
			((size_t)Parameter.DNSTarget.IPv6.HopLimitData.HopLimit + (size_t)Parameter.HopLimitFluctuation > U8_MAXNUM ||
			(SSIZE_T)Parameter.DNSTarget.IPv6.HopLimitData.HopLimit < (SSIZE_T)Parameter.HopLimitFluctuation + 1) ||
			Parameter.DNSTarget.Alternate_IPv6.HopLimitData.HopLimit > 0 &&
			((size_t)Parameter.DNSTarget.Alternate_IPv6.HopLimitData.HopLimit + (size_t)Parameter.HopLimitFluctuation > U8_MAXNUM ||
			(SSIZE_T)Parameter.DNSTarget.Alternate_IPv6.HopLimitData.HopLimit < (SSIZE_T)Parameter.HopLimitFluctuation + 1))
		{
			PrintError(LOG_ERROR_PARAMETER, L"Hop Limit or TTL Fluctuations error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL); //Hop Limit and TTL must between 1 and 255.
			return EXIT_FAILURE;
		}
	}

	//Other error which need to print to log.
	if (!Parameter.PcapCapture && !Parameter.HostsOnly && !Parameter.DNSCurve && Parameter.RequestMode != REQUEST_TCPMODE)
	{
		PrintError(LOG_ERROR_PARAMETER, L"Pcap Capture error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
		return EXIT_FAILURE;
	}
	if (Parameter.LocalMain && Parameter.DNSTarget.Local_IPv4.AddressData.Storage.ss_family == NULL && Parameter.DNSTarget.Local_IPv6.AddressData.Storage.ss_family)
	{
		PrintError(LOG_ERROR_PARAMETER, L"Local Main error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
		return EXIT_FAILURE;
	}
	if (Parameter.CacheType != 0 && Parameter.CacheParameter == 0)
	{
		PrintError(LOG_ERROR_PARAMETER, L"DNS Cache error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
		return EXIT_FAILURE;
	}
	if (Parameter.EDNS0PayloadSize < OLD_DNS_MAXSIZE)
	{
		if (Parameter.EDNS0PayloadSize > 0)
			PrintError(LOG_ERROR_PARAMETER, L"EDNS0 Payload Size must longer than 512 bytes(Old DNS packets minimum supported size)", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
		Parameter.EDNS0PayloadSize = EDNS0_MINSIZE; //Default UDP maximum payload size.
	}
	else if (Parameter.EDNS0PayloadSize >= PACKET_MAXSIZE - sizeof(ipv6_hdr) - sizeof(udp_hdr))
	{
		PrintError(LOG_ERROR_PARAMETER, L"EDNS0 Payload Size may be too long", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
		Parameter.EDNS0PayloadSize = EDNS0_MINSIZE;
	}
	if (Parameter.DNSTarget.Alternate_IPv4.AddressData.Storage.ss_family == NULL && Parameter.DNSTarget.Alternate_IPv6.AddressData.Storage.ss_family == NULL &&
		Parameter.DNSTarget.Alternate_Local_IPv4.AddressData.Storage.ss_family == NULL && Parameter.DNSTarget.Alternate_Local_IPv6.AddressData.Storage.ss_family == NULL &&
		Parameter.DNSCurve && DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.AddressData.Storage.ss_family == NULL && DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.AddressData.Storage.ss_family == NULL)
	{
		PrintError(LOG_ERROR_PARAMETER, L"Alternate Multi requesting error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
		return EXIT_FAILURE;
	}

	//Only check and set.
	if (Parameter.RequestMode != REQUEST_TCPMODE) //TCP Mode options check
		Parameter.TCPDataCheck = false;
	if (Parameter.DNSTarget.IPv4.AddressData.Storage.ss_family == NULL)
		Parameter.IPv4DataCheck = false;
	if (Parameter.DNSCurve) //DNSCurve options check
	{
		//Libsodium initialization
		if (sodium_init() != EXIT_SUCCESS)
		{
			PrintError(LOG_ERROR_DNSCURVE, L"Libsodium initialization error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
			return EXIT_FAILURE;
		}

		//Client keys check
		if (!CheckEmptyBuffer(DNSCurveParameter.Client_PublicKey, crypto_box_PUBLICKEYBYTES) && !CheckEmptyBuffer(DNSCurveParameter.Client_SecretKey, crypto_box_SECRETKEYBYTES) &&
			!VerifyKeypair(DNSCurveParameter.Client_PublicKey, DNSCurveParameter.Client_SecretKey))
		{
			PrintError(LOG_ERROR_DNSCURVE, L"Client keypair(public key and secret key) error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
			return EXIT_FAILURE;
		}
		else if (DNSCurveParameter.IsEncryption)
		{
			memset(DNSCurveParameter.Client_PublicKey, 0, crypto_box_PUBLICKEYBYTES);
			memset(DNSCurveParameter.Client_SecretKey, 0, crypto_box_SECRETKEYBYTES);
			crypto_box_curve25519xsalsa20poly1305_keypair(DNSCurveParameter.Client_PublicKey, DNSCurveParameter.Client_SecretKey);
		}
		else {
			delete[] DNSCurveParameter.Client_PublicKey;
			delete[] DNSCurveParameter.Client_SecretKey;
			DNSCurveParameter.Client_PublicKey = nullptr;
			DNSCurveParameter.Client_SecretKey = nullptr;
		}

	//DNSCurve targets check
		if (DNSCurveParameter.DNSCurveTarget.IPv4.AddressData.Storage.ss_family == NULL && DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.AddressData.Storage.ss_family != NULL)
		{
			DNSCurveParameter.DNSCurveTarget.IPv4 = DNSCurveParameter.DNSCurveTarget.Alternate_IPv4;
			memset(&DNSCurveParameter.DNSCurveTarget.Alternate_IPv4, 0, sizeof(DNSCurveServerData));
		}
		if (DNSCurveParameter.DNSCurveTarget.IPv6.AddressData.Storage.ss_family == NULL && DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.AddressData.Storage.ss_family != NULL)
		{
			DNSCurveParameter.DNSCurveTarget.IPv6 = DNSCurveParameter.DNSCurveTarget.Alternate_IPv6;
			memset(&DNSCurveParameter.DNSCurveTarget.Alternate_IPv6, 0, sizeof(DNSCurveServerData));
		}

		if (DNSCurveParameter.DNSCurveTarget.IPv4.AddressData.Storage.ss_family == NULL && DNSCurveParameter.DNSCurveTarget.IPv6.AddressData.Storage.ss_family == NULL ||
		//Check repeating items.
			DNSCurveParameter.DNSCurveTarget.IPv4.AddressData.Storage.ss_family != NULL && DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.AddressData.Storage.ss_family != NULL && DNSCurveParameter.DNSCurveTarget.IPv4.AddressData.IPv4.sin_addr.S_un.S_addr == DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.AddressData.IPv4.sin_addr.S_un.S_addr ||
			DNSCurveParameter.DNSCurveTarget.IPv6.AddressData.Storage.ss_family != NULL && DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.AddressData.Storage.ss_family != NULL && memcmp(&DNSCurveParameter.DNSCurveTarget.IPv6.AddressData.IPv6.sin6_addr, &DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.AddressData.IPv6.sin6_addr, sizeof(in6_addr) == 0))
		{
			PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Targets error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
			return EXIT_FAILURE;
		}

	//Eencryption options check
		if (DNSCurveParameter.IsEncryptionOnly && !DNSCurveParameter.IsEncryption)
		{
			DNSCurveParameter.IsEncryption = true;
			PrintError(LOG_ERROR_PARAMETER, L"DNSCurve encryption options error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
		}

	//Main(IPv6)
		if (DNSCurveParameter.IsEncryption && DNSCurveParameter.DNSCurveTarget.IPv6.AddressData.Storage.ss_family != NULL)
		{
		//Empty Server Fingerprint
			if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.IPv6.ServerFingerprint, crypto_box_PUBLICKEYBYTES))
			{
			//Encryption Only mode check
				if (DNSCurveParameter.IsEncryptionOnly &&
					CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.IPv6.SendMagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Encryption Only mode error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
					return EXIT_FAILURE;
				}

			//Empty Provider Name
				if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.IPv6.ProviderName, DOMAIN_MAXSIZE))
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNSCurve empty Provider Name error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
					return EXIT_FAILURE;
				}

			//Empty Public Key
				if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.IPv6.ServerPublicKey, crypto_box_PUBLICKEYBYTES))
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNSCurve empty Public Key error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
					return EXIT_FAILURE;
				}
			}
			else {
				crypto_box_curve25519xsalsa20poly1305_beforenm(
					DNSCurveParameter.DNSCurveTarget.IPv6.PrecomputationKey,
					DNSCurveParameter.DNSCurveTarget.IPv6.ServerFingerprint,
					DNSCurveParameter.Client_SecretKey);
			}
		}
		else {
			delete[] DNSCurveParameter.DNSCurveTarget.IPv6.ProviderName;
			delete[] DNSCurveParameter.DNSCurveTarget.IPv6.PrecomputationKey;
			delete[] DNSCurveParameter.DNSCurveTarget.IPv6.ServerPublicKey;
			delete[] DNSCurveParameter.DNSCurveTarget.IPv6.ReceiveMagicNumber;
			delete[] DNSCurveParameter.DNSCurveTarget.IPv6.SendMagicNumber;

			DNSCurveParameter.DNSCurveTarget.IPv6.ProviderName = nullptr;
			DNSCurveParameter.DNSCurveTarget.IPv6.PrecomputationKey = nullptr;
			DNSCurveParameter.DNSCurveTarget.IPv6.ServerPublicKey = nullptr;
			DNSCurveParameter.DNSCurveTarget.IPv6.ReceiveMagicNumber = nullptr;
			DNSCurveParameter.DNSCurveTarget.IPv6.SendMagicNumber = nullptr;
		}

	//Main(IPv4)
		if (DNSCurveParameter.IsEncryption && DNSCurveParameter.DNSCurveTarget.IPv4.AddressData.Storage.ss_family != NULL)
		{
		//Empty Server Fingerprint
			if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.IPv4.ServerFingerprint, crypto_box_PUBLICKEYBYTES))
			{
			//Encryption Only mode check
				if (DNSCurveParameter.IsEncryptionOnly &&
					CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.IPv4.SendMagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Encryption Only mode error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
					return EXIT_FAILURE;
				}

			//Empty Provider Name
				if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.IPv4.ProviderName, DOMAIN_MAXSIZE))
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNSCurve empty Provider Name error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
					return EXIT_FAILURE;
				}

			//Empty Public Key
				if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.IPv4.ServerPublicKey, crypto_box_PUBLICKEYBYTES))
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNSCurve empty Public Key error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
					return EXIT_FAILURE;
				}
			}
			else {
				crypto_box_curve25519xsalsa20poly1305_beforenm(
					DNSCurveParameter.DNSCurveTarget.IPv4.PrecomputationKey,
					DNSCurveParameter.DNSCurveTarget.IPv4.ServerFingerprint,
					DNSCurveParameter.Client_SecretKey);
			}
		}
		else {
			delete[] DNSCurveParameter.DNSCurveTarget.IPv4.ProviderName;
			delete[] DNSCurveParameter.DNSCurveTarget.IPv4.PrecomputationKey;
			delete[] DNSCurveParameter.DNSCurveTarget.IPv4.ServerPublicKey;
			delete[] DNSCurveParameter.DNSCurveTarget.IPv4.ReceiveMagicNumber;
			delete[] DNSCurveParameter.DNSCurveTarget.IPv4.SendMagicNumber;

			DNSCurveParameter.DNSCurveTarget.IPv4.ProviderName = nullptr;
			DNSCurveParameter.DNSCurveTarget.IPv4.PrecomputationKey = nullptr;
			DNSCurveParameter.DNSCurveTarget.IPv4.ServerPublicKey = nullptr;
			DNSCurveParameter.DNSCurveTarget.IPv4.ReceiveMagicNumber = nullptr;
			DNSCurveParameter.DNSCurveTarget.IPv4.SendMagicNumber = nullptr;
		}

	//Alternate(IPv6)
		if (DNSCurveParameter.IsEncryption && DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.AddressData.Storage.ss_family != NULL)
		{
		//Empty Server Fingerprint
			if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ServerFingerprint, crypto_box_PUBLICKEYBYTES))
			{
			//Encryption Only mode check
				if (DNSCurveParameter.IsEncryptionOnly &&
					CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.SendMagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Encryption Only mode error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
					return EXIT_FAILURE;
				}

			//Empty Provider Name
				if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ProviderName, DOMAIN_MAXSIZE))
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNSCurve empty Provider Name error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
					return EXIT_FAILURE;
				}

			//Empty Public Key
				if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ServerPublicKey, crypto_box_PUBLICKEYBYTES))
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNSCurve empty Public Key error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
					return EXIT_FAILURE;
				}
			}
			else {
				crypto_box_curve25519xsalsa20poly1305_beforenm(
					DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.PrecomputationKey,
					DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ServerFingerprint,
					DNSCurveParameter.Client_SecretKey);
			}
		}
		else {
			delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ProviderName;
			delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.PrecomputationKey;
			delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ServerPublicKey;
			delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ReceiveMagicNumber;
			delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.SendMagicNumber;

			DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ProviderName = nullptr;
			DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.PrecomputationKey = nullptr;
			DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ServerPublicKey = nullptr;
			DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ReceiveMagicNumber = nullptr;
			DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.SendMagicNumber = nullptr;
		}

	//Alternate(IPv4)
		if (DNSCurveParameter.IsEncryption && DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.AddressData.Storage.ss_family != NULL)
		{
		//Empty Server Fingerprint
			if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ServerFingerprint, crypto_box_PUBLICKEYBYTES))
			{
			//Encryption Only mode check
				if (DNSCurveParameter.IsEncryptionOnly &&
					CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.SendMagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Encryption Only mode error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
					return EXIT_FAILURE;
				}

			//Empty Provider Name
				if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ProviderName, DOMAIN_MAXSIZE))
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNSCurve empty Provider Name error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
					return EXIT_FAILURE;
				}

			//Empty Public Key
				if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ServerPublicKey, crypto_box_PUBLICKEYBYTES))
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNSCurve empty Public Key error", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
					return EXIT_FAILURE;
				}
			}
			else {
				crypto_box_curve25519xsalsa20poly1305_beforenm(
					DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.PrecomputationKey,
					DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ServerFingerprint,
					DNSCurveParameter.Client_SecretKey);
			}
		}
		else {
			delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ProviderName;
			delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.PrecomputationKey;
			delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ServerPublicKey;
			delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ReceiveMagicNumber;
			delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.SendMagicNumber;

			DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ProviderName = nullptr;
			DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.PrecomputationKey = nullptr;
			DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ServerPublicKey = nullptr;
			DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ReceiveMagicNumber = nullptr;
			DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.SendMagicNumber = nullptr;
		}
	}
	else {
		delete[] DNSCurveParameter.DNSCurveTarget.IPv4.ProviderName;
		delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ProviderName;
		delete[] DNSCurveParameter.DNSCurveTarget.IPv6.ProviderName;
		delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ProviderName;
	//DNSCurve Keys
		delete[] DNSCurveParameter.Client_PublicKey;
		delete[] DNSCurveParameter.Client_SecretKey;
		delete[] DNSCurveParameter.DNSCurveTarget.IPv4.PrecomputationKey;
		delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.PrecomputationKey;
		delete[] DNSCurveParameter.DNSCurveTarget.IPv6.PrecomputationKey;
		delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.PrecomputationKey;
		delete[] DNSCurveParameter.DNSCurveTarget.IPv4.ServerPublicKey;
		delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ServerPublicKey;
		delete[] DNSCurveParameter.DNSCurveTarget.IPv6.ServerPublicKey;
		delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ServerPublicKey;
		delete[] DNSCurveParameter.DNSCurveTarget.IPv4.ServerFingerprint;
		delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ServerFingerprint;
		delete[] DNSCurveParameter.DNSCurveTarget.IPv6.ServerFingerprint;
		delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ServerFingerprint;
	//DNSCurve Magic Numbers
		delete[] DNSCurveParameter.DNSCurveTarget.IPv4.ReceiveMagicNumber;
		delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ReceiveMagicNumber;
		delete[] DNSCurveParameter.DNSCurveTarget.IPv6.ReceiveMagicNumber;
		delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ReceiveMagicNumber;
		delete[] DNSCurveParameter.DNSCurveTarget.IPv4.SendMagicNumber;
		delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.SendMagicNumber;
		delete[] DNSCurveParameter.DNSCurveTarget.IPv6.SendMagicNumber;
		delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.SendMagicNumber;

		DNSCurveParameter.DNSCurveTarget.IPv4.ProviderName = nullptr, DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ProviderName = nullptr, DNSCurveParameter.DNSCurveTarget.IPv6.ProviderName = nullptr, DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ProviderName = nullptr;
		DNSCurveParameter.Client_PublicKey = nullptr, DNSCurveParameter.Client_SecretKey = nullptr;
		DNSCurveParameter.DNSCurveTarget.IPv4.PrecomputationKey = nullptr, DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.PrecomputationKey = nullptr, DNSCurveParameter.DNSCurveTarget.IPv6.PrecomputationKey = nullptr, DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.PrecomputationKey = nullptr;
		DNSCurveParameter.DNSCurveTarget.IPv4.ServerPublicKey = nullptr, DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ServerPublicKey = nullptr, DNSCurveParameter.DNSCurveTarget.IPv6.ServerPublicKey = nullptr, DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ServerPublicKey = nullptr;
		DNSCurveParameter.DNSCurveTarget.IPv4.ServerFingerprint = nullptr, DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ServerFingerprint = nullptr, DNSCurveParameter.DNSCurveTarget.IPv6.ServerFingerprint = nullptr, DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ServerFingerprint = nullptr;
		DNSCurveParameter.DNSCurveTarget.IPv4.SendMagicNumber = nullptr, DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.SendMagicNumber = nullptr, DNSCurveParameter.DNSCurveTarget.IPv6.SendMagicNumber = nullptr, DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.SendMagicNumber = nullptr;
		DNSCurveParameter.DNSCurveTarget.IPv4.ReceiveMagicNumber = nullptr, DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ReceiveMagicNumber = nullptr, DNSCurveParameter.DNSCurveTarget.IPv6.ReceiveMagicNumber = nullptr, DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ReceiveMagicNumber = nullptr;
	}

//Default settings
	//Hosts and IPFilter file list initialization


	//Other default setting
	if (Parameter.ListenPort == 0)
		Parameter.ListenPort = htons(IPPORT_DNS);
	if (!Parameter.EDNS0Label)
	{
		if (Parameter.DNSSECRequest)
		{
			PrintError(LOG_ERROR_PARAMETER, L"EDNS0 Label must turn ON when request DNSSEC", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
			Parameter.EDNS0Label = true;
		}

		if (Parameter.DNSCurve)
		{
			PrintError(LOG_ERROR_PARAMETER, L"EDNS0 Label must turn ON when request DNSCurve", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
			Parameter.EDNS0Label = true;
		}
	}
	if (Parameter.CompressionPointerMutation && Parameter.EDNS0Label)
	{
		PrintError(LOG_ERROR_PARAMETER, L"Compression Pointer Mutation must turn OFF when request EDNS0 Label", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
		Parameter.CompressionPointerMutation = false;
	}

	//Socket timeout check
	if (Parameter.ReliableSocketTimeout < SOCKET_TIMEOUT_MIN)
		Parameter.ReliableSocketTimeout = DEFAULT_RELIABLE_SOCKET_TIMEOUT;
	if (Parameter.UnreliableSocketTimeout < SOCKET_TIMEOUT_MIN)
		Parameter.UnreliableSocketTimeout = DEFAULT_UNRELIABLE_SOCKET_TIMEOUT;

	if (Parameter.ICMPSpeed > 0)
	{
	//Default ICMP ID is current process ID.
		if (ntohs(Parameter.ICMPID) <= 0)
			Parameter.ICMPID = htons((uint16_t)GetCurrentProcessId());

		if (ntohs(Parameter.ICMPSequence) <= 0)
			Parameter.ICMPSequence = htons(DEFAULT_SEQUENCE);
	}

	if (Parameter.DomainTestSpeed <= SHORTEST_DOMAINTEST_INTERVAL_TIME)
		Parameter.DomainTestSpeed = DEFAULT_DOMAINTEST_INTERVAL_TIME * SECOND_TO_MILLISECOND;

	//Default DNS ID is current process ID.
	if (ntohs(Parameter.DomainTestID) <= 0)
		Parameter.DomainTestID = htons((uint16_t)GetCurrentProcessId());

	if (CheckEmptyBuffer(Parameter.DomainTestData, DOMAIN_MAXSIZE))
	{
		delete[] Parameter.DomainTestData;
		Parameter.DomainTestData = nullptr;
	}

	//Load default padding data from Microsoft Windows Ping.
	if (Parameter.ICMPPaddingDataLength <= 0)
	{
		Parameter.ICMPPaddingDataLength = strlen(DEFAULT_PADDINGDATA) + 1U;
		memcpy(Parameter.ICMPPaddingData, DEFAULT_PADDINGDATA, Parameter.ICMPPaddingDataLength - 1U);
	}

	//Default Local DNS server name
	if (Parameter.LocalFQDNLength <= 0)
	{
		Parameter.LocalFQDNLength = CharToDNSQuery(DEFAULT_LOCAL_SERVERNAME, Parameter.LocalFQDN);
		*Parameter.LocalFQDNString = DEFAULT_LOCAL_SERVERNAME;
	}

	Parameter.HostsDefaultTTL = DEFAULT_HOSTS_TTL;

	//Set Local DNS server PTR response.
	if (Parameter.LocalServerResponseLength <= 0)
	{
		auto pdns_ptr_record = (dns_ptr_record *)Parameter.LocalServerResponse;
		pdns_ptr_record->PTR = htons(DNS_QUERY_PTR);
		pdns_ptr_record->Classes = htons(DNS_CLASS_IN);
		pdns_ptr_record->TTL = htonl(Parameter.HostsDefaultTTL);
		pdns_ptr_record->Type = htons(DNS_RECORD_PTR);
		pdns_ptr_record->Length = htons((uint16_t)Parameter.LocalFQDNLength);
		Parameter.LocalServerResponseLength += sizeof(dns_ptr_record);

		memcpy(Parameter.LocalServerResponse + Parameter.LocalServerResponseLength, Parameter.LocalFQDN, Parameter.LocalFQDNLength);
		Parameter.LocalServerResponseLength += Parameter.LocalFQDNLength;

	//EDNS0 Label
		if (Parameter.EDNS0Label)
		{
			auto pdns_opt_record = (dns_opt_record *)(Parameter.LocalServerResponse + Parameter.LocalServerResponseLength);
			pdns_opt_record->Type = htons(DNS_RECORD_OPT);
			pdns_opt_record->UDPPayloadSize = htons((uint16_t)Parameter.EDNS0PayloadSize);
			Parameter.LocalServerResponseLength += sizeof(dns_opt_record);
		}
	}

//DNSCurve default settings
	if (Parameter.DNSCurve && DNSCurveParameter.IsEncryption)
	{
	//DNSCurve PayloadSize check
		if (DNSCurveParameter.DNSCurvePayloadSize < OLD_DNS_MAXSIZE)
		{
			if (DNSCurveParameter.DNSCurvePayloadSize > 0)
				PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Payload Size must longer than 512 bytes(Old DNS packets minimum supported size)", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
			DNSCurveParameter.DNSCurvePayloadSize = OLD_DNS_MAXSIZE; //Default DNSCurve UDP maximum payload size.
		}
		else if (DNSCurveParameter.DNSCurvePayloadSize >= PACKET_MAXSIZE - sizeof(ipv6_hdr) - sizeof(udp_hdr))
		{
			PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Payload Size may be too long", NULL, (PWSTR)ConfigFileList[Index].c_str(), NULL);
			DNSCurveParameter.DNSCurvePayloadSize = EDNS0_MINSIZE;
		}

	//Main(IPv6)
		if (DNSCurveParameter.DNSCurveTarget.IPv6.AddressData.Storage.ss_family != NULL && CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.IPv6.ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
			memcpy(DNSCurveParameter.DNSCurveTarget.IPv6.ReceiveMagicNumber, DNSCRYPT_RECEIVE_MAGIC, DNSCURVE_MAGIC_QUERY_LEN);

	//Main(IPv4)
		if (DNSCurveParameter.DNSCurveTarget.IPv4.AddressData.Storage.ss_family != NULL && CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.IPv4.ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
			memcpy(DNSCurveParameter.DNSCurveTarget.IPv4.ReceiveMagicNumber, DNSCRYPT_RECEIVE_MAGIC, DNSCURVE_MAGIC_QUERY_LEN);

	//Alternate(IPv6)
		if (DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.AddressData.Storage.ss_family != NULL && CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
			memcpy(DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ReceiveMagicNumber, DNSCRYPT_RECEIVE_MAGIC, DNSCURVE_MAGIC_QUERY_LEN);

	//Alternate(IPv4)
		if (DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.AddressData.Storage.ss_family != NULL && CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
			memcpy(DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ReceiveMagicNumber, DNSCRYPT_RECEIVE_MAGIC, DNSCURVE_MAGIC_QUERY_LEN);

	//DNSCurve keys recheck time
		if (DNSCurveParameter.KeyRecheckTime == 0)
			DNSCurveParameter.KeyRecheckTime = DEFAULT_DNSCURVE_RECHECK_TIME * SECOND_TO_MILLISECOND;
	}

//Sort AcceptTypeList.
	std::sort(Parameter.AcceptTypeList->begin(), Parameter.AcceptTypeList->end());

//Print global parameter list.
	if (Parameter.PrintStatus >= LOG_STATUS_LEVEL1)
		PrintParameterList();

	return EXIT_SUCCESS;
}

//Read parameter data from files
size_t __fastcall ReadParameterData(const PSTR Buffer, const size_t FileIndex, const size_t Line, bool &LabelComments)
{
	std::string Data(Buffer);

//Multi-line comments check
	if (ReadMultiLineComments(Buffer, Data, LabelComments) == EXIT_FAILURE)
		return EXIT_SUCCESS;

	SSIZE_T Result = 0;

//Parameter version less than 0.4 compatible support.
	if (Data.find("Hop Limits/TTL Fluctuation = ") == 0 && Data.length() > strlen("Hop Limits/TTL Fluctuation = "))
	{
		if (Data.length() < strlen("Hop Limits/TTL Fluctuation = ") + 4U)
		{
			Result = strtol(Data.c_str() + strlen("Hop Limits/TTL Fluctuation = "), nullptr, NULL);
			if (Result > 0 && Result < U8_MAXNUM)
				Parameter.HopLimitFluctuation = (uint8_t)Result;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}

//Delete delete spaces, horizontal tab/HT, check comments(Number Sign/NS and double slashs) and check minimum length of ipfilter items.
//Delete comments(Number Sign/NS and double slashs) and check minimum length of configuration items.
	if (Data.find(ASCII_HASHTAG) == 0 || Data.find(ASCII_SLASH) == 0)
		return EXIT_SUCCESS;
	while (Data.find(ASCII_HT) != std::string::npos)
		Data.erase(Data.find(ASCII_HT), 1U);
	while (Data.find(ASCII_SPACE) != std::string::npos)
		Data.erase(Data.find(ASCII_SPACE), 1U);
	if (Data.find(ASCII_HASHTAG) != std::string::npos)
		Data.erase(Data.find(ASCII_HASHTAG));
	else if (Data.find(ASCII_SLASH) != std::string::npos)
		Data.erase(Data.find(ASCII_SLASH));
	if (Data.length() < READ_PARAMETER_MINSIZE)
		return FALSE;

//[Base] block
	if (Data.find("Version=") == 0)
	{
		if (Data.length() > strlen("Version=") && Data.length() < strlen("Version=") + 8U)
		{
			Parameter.Version = atof(Data.c_str() + strlen("Version="));
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}

//Parameter version less than 0.4 compatible support.
	if (Parameter.Version < PRODUCT_VERSION)
	{
	//[Base] block
		if (Parameter.FileRefreshTime == 0 && Data.find("Hosts=") == 0 && Data.length() > strlen("Hosts="))
		{
			if (Data.length() < strlen("Hosts=") + 6U)
			{
				Result = strtol(Data.c_str() + strlen("Hosts="), nullptr, NULL);
				if (Result >= DEFAULT_FILEREFRESH_TIME)
					Parameter.FileRefreshTime = Result * SECOND_TO_MILLISECOND;
				else if (Result > 0 && Result < DEFAULT_FILEREFRESH_TIME)
					Parameter.FileRefreshTime = DEFAULT_FILEREFRESH_TIME * SECOND_TO_MILLISECOND;
				else 
					Parameter.FileRefreshTime = 0; //Read file again set Disable.
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else if (Parameter.DNSTarget.IPv4.AddressData.Storage.ss_family == NULL && Data.find("IPv4DNSAddress=") == 0 && Data.length() > strlen("IPv4DNSAddress="))
		{
			if (Data.length() > strlen("IPv4DNSAddress=") + 6U && Data.length() < strlen("IPv4DNSAddress=") + 20U)
			{
			//Convert IPv4 Address and Port.
				std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
				memcpy(Target.get(), Data.c_str() + strlen("IPv4DNSAddress="), Data.length() - strlen("IPv4DNSAddress="));
				if (AddressStringToBinary(Target.get(), &Parameter.DNSTarget.IPv4.AddressData.IPv4.sin_addr, AF_INET, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}

				Parameter.DNSTarget.IPv4.AddressData.IPv4.sin_port = htons(IPPORT_DNS);
				Parameter.DNSTarget.IPv4.AddressData.Storage.ss_family = AF_INET;
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else if (Parameter.DNSTarget.Local_IPv4.AddressData.Storage.ss_family == NULL && Data.find("IPv4LocalDNSAddress=") == 0 && Data.length() > strlen("IPv4LocalDNSAddress="))
		{
			if (Data.length() > strlen("IPv4LocalDNSAddress=") + 6U && Data.length() < strlen("IPv4LocalDNSAddress=") + 20U)
			{
			//Convert IPv4 Address and Port.
				std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
				memcpy(Target.get(), Data.c_str() + strlen("IPv4LocalDNSAddress="), Data.length() - strlen("IPv4LocalDNSAddress="));
				if (AddressStringToBinary(Target.get(), &Parameter.DNSTarget.Local_IPv4.AddressData.IPv4.sin_addr, AF_INET, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}

				Parameter.DNSTarget.Local_IPv4.AddressData.IPv4.sin_port = htons(IPPORT_DNS);
				Parameter.DNSTarget.Local_IPv4.AddressData.Storage.ss_family = AF_INET;
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else if (Parameter.DNSTarget.IPv6.AddressData.Storage.ss_family == NULL && Data.find("IPv6DNSAddress=") == 0 && Data.length() > strlen("IPv6DNSAddress="))
		{
			if (Data.length() > strlen("IPv6DNSAddress=") + 1U && Data.length() < strlen("IPv6DNSAddress=") + 40U)
			{
			//Convert IPv6 Address and Port.
				std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
				memcpy(Target.get(), Data.c_str() + strlen("IPv6DNSAddress="), Data.length() - strlen("IPv6DNSAddress="));
				if (AddressStringToBinary(Target.get(), &Parameter.DNSTarget.IPv6.AddressData.IPv6.sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}

				Parameter.DNSTarget.IPv6.AddressData.IPv6.sin6_port = htons(IPPORT_DNS);
				Parameter.DNSTarget.IPv6.AddressData.Storage.ss_family = AF_INET6;
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else if (Parameter.DNSTarget.Local_IPv6.AddressData.Storage.ss_family == NULL && Data.find("IPv6LocalDNSAddress=") == 0 && Data.length() > strlen("IPv6LocalDNSAddress="))
		{
			if (Data.length() > strlen("IPv6LocalDNSAddress=") + 1U && Data.length() < strlen("IPv6LocalDNSAddress=") + 40U)
			{
			//Convert IPv6 Address and Port.
				std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
				memcpy(Target.get(), Data.c_str() + strlen("IPv6LocalDNSAddress="), Data.length() - strlen("IPv6LocalDNSAddress="));
				if (AddressStringToBinary(Target.get(), &Parameter.DNSTarget.Local_IPv6.AddressData.IPv6.sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}

				Parameter.DNSTarget.Local_IPv6.AddressData.IPv6.sin6_port = htons(IPPORT_DNS);
				Parameter.DNSTarget.Local_IPv6.AddressData.Storage.ss_family = AF_INET6;
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}

	//[Extend Test] block
		else if (Data.find("IPv4OptionsFilter=1") == 0)
		{
			Parameter.IPv4DataCheck = true;
		}
		else if (Data.find("TCPOptionsFilter=1") == 0)
		{
			Parameter.TCPDataCheck = true;
		}
		else if (Data.find("DNSOptionsFilter=1") == 0)
		{
			Parameter.DNSDataCheck = true;
		}

	//[Data] block
		else if (Parameter.DomainTestSpeed == 0 && Data.find("DomainTestSpeed=") == 0 && Data.length() > strlen("DomainTestSpeed="))
		{
			if (Data.length() < strlen("DomainTestSpeed=") + 6U)
			{
				Result = strtol(Data.c_str() + strlen("DomainTestSpeed="), nullptr, NULL);
				if (Result > 0)
					Parameter.DomainTestSpeed = Result * SECOND_TO_MILLISECOND;
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
	}
	else if (Data.find("FileRefreshTime=") == 0 && Data.length() > strlen("FileRefreshTime="))
	{
		if (Data.length() < strlen("FileRefreshTime=") + 6U)
		{
			Result = strtol(Data.c_str() + strlen("FileRefreshTime="), nullptr, NULL);
			if (Result >= DEFAULT_FILEREFRESH_TIME)
				Parameter.FileRefreshTime = Result * SECOND_TO_MILLISECOND;
			else if (Result > 0 && Result < DEFAULT_FILEREFRESH_TIME)
				Parameter.FileRefreshTime = DEFAULT_FILEREFRESH_TIME * SECOND_TO_MILLISECOND;
			else 
				Parameter.FileRefreshTime = 0; //Read file again Disable.
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("FileHash=1") == 0)
	{
		Parameter.FileHash = true;
	}
	else if (Data.find("AdditionalPath=") == 0 && Data.length() > strlen("AdditionalPath="))
	{
		std::string NameStringTemp;
		std::shared_ptr<wchar_t> wNameStringTemp(new wchar_t[Data.length()]());
		for (Result = strlen("AdditionalPath=");Result < (SSIZE_T)Data.length();Result++)
		{
			if (Result == (SSIZE_T)(Data.length() - 1U))
			{
				NameStringTemp.append(Data, Result, 1U);
			//Case Convert.
				for (auto StringIter = NameStringTemp.begin(); StringIter != NameStringTemp.end(); StringIter++)
				{
					if (*StringIter > ASCII_AT && *StringIter < ASCII_BRACKETS_LEAD)
						*StringIter += ASCII_UPPER_TO_LOWER;
				}

			//Add backslash.
				if (NameStringTemp.back() != ASCII_BACKSLASH)
					NameStringTemp.append("\\");

			//Convert to wide string.
				if (MultiByteToWideChar(CP_ACP, NULL, NameStringTemp.c_str(), MBSTOWCS_NULLTERMINATE, wNameStringTemp.get(), (int)(NameStringTemp.length() + 1U)) > 0)
				{
					for (auto PathIter = Parameter.Path->begin();PathIter < Parameter.Path->end();PathIter++)
					{
						if (*PathIter == wNameStringTemp.get())
							break;

						if (PathIter + 1U == Parameter.Path->end())
						{
							Parameter.Path->push_back(wNameStringTemp.get());
							for (size_t Index = 0;Index < Parameter.Path->back().length();Index++)
							{
								if ((Parameter.Path->back())[Index] == L'\\')
								{
									Parameter.Path->back().insert(Index, L"\\");
									Index++;
								}
							}

							break;
						}
					}
				}
				else {
					PrintError(LOG_ERROR_SYSTEM, L"Multi bytes to wide chars error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
			}
			else if (Data[Result] == ASCII_VERTICAL)
			{
			//Case Convert.
				for (auto StringIter = NameStringTemp.begin(); StringIter != NameStringTemp.end(); StringIter++)
				{
					if (*StringIter > ASCII_AT && *StringIter < ASCII_BRACKETS_LEAD)
						*StringIter += ASCII_UPPER_TO_LOWER;
				}

			//Add backslash.
				if (NameStringTemp.back() != ASCII_BACKSLASH)
					NameStringTemp.append("\\");

			//Convert to wide string.
				if (MultiByteToWideChar(CP_ACP, NULL, NameStringTemp.c_str(), MBSTOWCS_NULLTERMINATE, wNameStringTemp.get(), (int)(NameStringTemp.length() + 1U)) > 0)
				{
					for (auto PathIter = Parameter.Path->begin();PathIter < Parameter.Path->end();PathIter++)
					{
						if (*PathIter == wNameStringTemp.get())
							break;

						if (PathIter + 1U == Parameter.Path->end())
						{
							Parameter.Path->push_back(wNameStringTemp.get());
							for (size_t Index = 0;Index < Parameter.Path->back().length();Index++)
							{
								if ((Parameter.Path->back())[Index] == L'\\')
								{
									Parameter.Path->back().insert(Index, L"\\");
									Index++;
								}

								break;
							}
						}
					}

					memset(wNameStringTemp.get(), 0, sizeof(wchar_t) * Data.length());
				}
				else {
					PrintError(LOG_ERROR_SYSTEM, L"Multi bytes to wide chars error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}

				NameStringTemp.clear();
			}
			else {
				NameStringTemp.append(Data, Result, 1U);
			}
		}
	}
	else if (Data.find("HostsFileName=") == 0 && Data.length() > strlen("HostsFileName="))
	{
		std::string NameStringTemp;
		std::shared_ptr<wchar_t> wNameStringTemp(new wchar_t[Data.length()]());
		for (Result = strlen("HostsFileName=");Result < (SSIZE_T)Data.length();Result++)
		{
			if (Result == (SSIZE_T)(Data.length() - 1U))
			{
				NameStringTemp.append(Data, Result, 1U);
			//Case Convert.
				for (auto StringIter = NameStringTemp.begin();StringIter != NameStringTemp.end();StringIter++)
				{
					if (*StringIter > ASCII_AT && *StringIter < ASCII_BRACKETS_LEAD)
						*StringIter += ASCII_UPPER_TO_LOWER;
				}

				if (MultiByteToWideChar(CP_ACP, NULL, NameStringTemp.c_str(), MBSTOWCS_NULLTERMINATE, wNameStringTemp.get(), (int)(NameStringTemp.length() + 1U)) > 0)
				{
					if (Parameter.HostsFileList->empty())
					{
						Parameter.HostsFileList->push_back(wNameStringTemp.get());
					}
					else {
						for (auto HostsFileListIter = Parameter.HostsFileList->begin();HostsFileListIter < Parameter.HostsFileList->end();HostsFileListIter++)
						{
							if (*HostsFileListIter == wNameStringTemp.get())
								break;

							if (HostsFileListIter + 1U == Parameter.HostsFileList->end())
							{
								Parameter.HostsFileList->push_back(wNameStringTemp.get());
								break;
							}
						}
					}
				}
				else {
					PrintError(LOG_ERROR_SYSTEM, L"Multi bytes to wide chars error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
			}
			else if (Data[Result] == ASCII_VERTICAL)
			{
			//Case Convert.
				for (auto StringIter = NameStringTemp.begin();StringIter != NameStringTemp.end();StringIter++)
				{
					if (*StringIter > ASCII_AT && *StringIter < ASCII_BRACKETS_LEAD)
						*StringIter += ASCII_UPPER_TO_LOWER;
				}

				if (MultiByteToWideChar(CP_ACP, NULL, NameStringTemp.c_str(), MBSTOWCS_NULLTERMINATE, wNameStringTemp.get(), (int)(NameStringTemp.length() + 1U)) > 0)
				{
					if (Parameter.HostsFileList->empty())
					{
						Parameter.HostsFileList->push_back(wNameStringTemp.get());
					}
					else {
						for (auto HostsFileListIter = Parameter.HostsFileList->begin();HostsFileListIter < Parameter.HostsFileList->end();HostsFileListIter++)
						{
							if (*HostsFileListIter == wNameStringTemp.get())
								break;

							if (HostsFileListIter + 1U == Parameter.HostsFileList->end())
							{
								Parameter.HostsFileList->push_back(wNameStringTemp.get());
								break;
							}
						}
					}

					memset(wNameStringTemp.get(), 0, sizeof(wchar_t) * Data.length());
				}
				else {
					PrintError(LOG_ERROR_SYSTEM, L"Multi bytes to wide chars error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}

				NameStringTemp.clear();
			}
			else {
				NameStringTemp.append(Data, Result, 1U);
			}
		}
	}
	else if (Data.find("IPFilterFileName=") == 0 && Data.length() > strlen("IPFilterFileName="))
	{
		std::string NameStringTemp;
		std::shared_ptr<wchar_t> wNameStringTemp(new wchar_t[Data.length()]());
		for (Result = strlen("IPFilterFileName=");Result < (SSIZE_T)Data.length();Result++)
		{
			if (Result == (SSIZE_T)(Data.length() - 1U))
			{
				NameStringTemp.append(Data, Result, 1U);
			//Case Convert.
				for (auto StringIter = NameStringTemp.begin();StringIter != NameStringTemp.end();StringIter++)
				{
					if (*StringIter > ASCII_AT && *StringIter < ASCII_BRACKETS_LEAD)
						*StringIter += ASCII_UPPER_TO_LOWER;
				}

				if (MultiByteToWideChar(CP_ACP, NULL, NameStringTemp.c_str(), MBSTOWCS_NULLTERMINATE, wNameStringTemp.get(), (int)(NameStringTemp.length() + 1U)) > 0)
				{
					if (Parameter.IPFilterFileList->empty())
					{
						Parameter.IPFilterFileList->push_back(wNameStringTemp.get());
					}
					else {
						for (auto IPFilterFileListIter = Parameter.IPFilterFileList->begin();IPFilterFileListIter < Parameter.IPFilterFileList->end();IPFilterFileListIter++)
						{
							if (*IPFilterFileListIter == wNameStringTemp.get())
								break;

							if (IPFilterFileListIter + 1U == Parameter.IPFilterFileList->end())
							{
								Parameter.IPFilterFileList->push_back(wNameStringTemp.get());
								break;
							}
						}
					}
				}
				else {
					PrintError(LOG_ERROR_SYSTEM, L"Multi bytes to wide chars error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
			}
			else if (Data[Result] == ASCII_VERTICAL)
			{
			//Case Convert.
				for (auto StringIter = NameStringTemp.begin();StringIter != NameStringTemp.end();StringIter++)
				{
					if (*StringIter > ASCII_AT && *StringIter < ASCII_BRACKETS_LEAD)
						*StringIter += ASCII_UPPER_TO_LOWER;
				}

				if (MultiByteToWideChar(CP_ACP, NULL, NameStringTemp.c_str(), MBSTOWCS_NULLTERMINATE, wNameStringTemp.get(), (int)(NameStringTemp.length() + 1U)) > 0)
				{
					if (Parameter.IPFilterFileList->empty())
					{
						Parameter.IPFilterFileList->push_back(wNameStringTemp.get());
					}
					else {
						for (auto IPFilterFileListIter = Parameter.IPFilterFileList->begin();IPFilterFileListIter < Parameter.IPFilterFileList->end();IPFilterFileListIter++)
						{
							if (*IPFilterFileListIter == wNameStringTemp.get())
								break;

							if (IPFilterFileListIter + 1U == Parameter.IPFilterFileList->end())
							{
								Parameter.IPFilterFileList->push_back(wNameStringTemp.get());
								break;
							}
						}
					}

					memset(wNameStringTemp.get(), 0, sizeof(wchar_t) * Data.length());
				}
				else {
					PrintError(LOG_ERROR_SYSTEM, L"Multi bytes to wide chars error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}

				NameStringTemp.clear();
			}
			else {
				NameStringTemp.append(Data, Result, 1U);
			}
		}
	}

//[Log] block
	else if (Data.find("PrintError=0") == 0)
	{
		Parameter.PrintError = false;
		delete Parameter.ErrorLogPath;
		Parameter.ErrorLogPath = nullptr;
	}
	else if (Data.find("PrintRunningLog=") == 0 && Data.length() > strlen("PrintRunningLog="))
	{
		if (Data.length() < strlen("PrintRunningLog=") + 2U)
		{
			Result = strtol(Data.c_str() + strlen("PrintRunningLog="), nullptr, NULL);
			if (Result > LOG_STATUS_CLOSED)
			{
				Parameter.PrintStatus = Result;

			//Print status.
				switch (Result)
				{
					case LOG_STATUS_LEVEL1:
					{
						PrintStatus(L"Print Running Log Level 1");
					}break;
					case LOG_STATUS_LEVEL2:
					{
						PrintStatus(L"Print Running Log Level 2");
					}break;
					case LOG_STATUS_LEVEL3:
					{
						PrintStatus(L"Print Running Log Level 3");
					}break;
					default:
					{
						PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}break;
				}
			}
		//Print running status Disable.
			else {
				Parameter.PrintStatus = 0; 
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("LogMaximumSize=") == 0 && Data.length() > strlen("LogMaximumSize="))
	{
		if (Data.find("KB") != std::string::npos || Data.find("Kb") != std::string::npos || Data.find("kB") != std::string::npos || Data.find("kb") != std::string::npos)
		{
			Data.erase(Data.length() - 2U, 2U);

		//Mark bytes.
			Result = strtol(Data.c_str() + strlen("LogMaximumSize="), nullptr, NULL);
			if (Result >= 0)
			{
				Parameter.LogMaxSize = Result * KILOBYTE_TIMES;
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else if (Data.find("MB") != std::string::npos || Data.find("Mb") != std::string::npos || Data.find("mB") != std::string::npos || Data.find("mb") != std::string::npos)
		{
			Data.erase(Data.length() - 2U, 2U);

		//Mark bytes.
			Result = strtol(Data.c_str() + strlen("LogMaximumSize="), nullptr, NULL);
			if (Result >= 0)
			{
				Parameter.LogMaxSize = Result * MEGABYTE_TIMES;
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else if (Data.find("GB") != std::string::npos || Data.find("Gb") != std::string::npos || Data.find("gB") != std::string::npos || Data.find("gb") != std::string::npos)
		{
			Data.erase(Data.length() - 2U, 2U);

		//Mark bytes.
			Result = strtol(Data.c_str() + strlen("LogMaximumSize="), nullptr, NULL);
			if (Result >= 0)
			{
				Parameter.LogMaxSize = Result * GIGABYTE_TIMES;
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else {
		//Check number.
			for (auto StringIter = Data.begin() + strlen("LogMaximumSize=");StringIter != Data.end();StringIter++)
			{
				if (*StringIter < ASCII_ZERO || *StringIter > ASCII_NINE)
				{
					PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
			}

		//Mark bytes.
			Result = strtol(Data.c_str() + strlen("LogMaximumSize="), nullptr, NULL);
			if (Result >= 0)
			{
				Parameter.LogMaxSize = Result;
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
	}

//[DNS] block
	else if (Data.find("Protocol=TCP") == 0 || Data.find("Protocol=Tcp") == 0 || Data.find("Protocol=tcp") == 0)
	{
		if (Data.find("Protocol=TCP") == 0 || Data.find("Protocol=Tcp") == 0 || Data.find("Protocol=tcp") == 0)
			Parameter.RequestMode = REQUEST_TCPMODE;
		else 
			Parameter.RequestMode = REQUEST_UDPMODE;
	}
	else if (Data.find("HostsOnly=1") == 0)
	{
		Parameter.HostsOnly = true;
	}
	else if (Data.find("LocalMain=1") == 0)
	{
		Parameter.LocalMain = true;
	}
	else if (Data.find("LocalRouting=1") == 0)
	{
		Parameter.LocalRouting = true;
	}
	else if (Data.find("CacheType=") == 0 && Data.length() > strlen(("CacheType=")))
	{
		if (Data.find("CacheType=Timer") == 0)
			Parameter.CacheType = CACHE_TIMER;
		else if (Data.find("CacheType=Queue") == 0)
			Parameter.CacheType = CACHE_QUEUE;
	}
	else if (Parameter.CacheType != 0 && Data.find("CacheParameter=") == 0 && Data.length() > strlen("CacheParameter="))
	{
		Result = strtol(Data.c_str() + strlen("CacheParameter="), nullptr, NULL);
		if (Result > 0)
		{
			if (Parameter.CacheType == CACHE_TIMER)
				Parameter.CacheParameter = Result * SECOND_TO_MILLISECOND;
			else //CACHE_QUEUE
				Parameter.CacheParameter = Result;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DefaultTTL=") == 0 && Data.length() > strlen("DefaultTTL="))
	{
		if (Data.length() < strlen("DefaultTTL=") + 6U)
		{
			Result = strtol(Data.c_str() + strlen("DefaultTTL="), nullptr, NULL);
			if (Result > 0 && Result <= U16_MAXNUM)
			{
				Parameter.HostsDefaultTTL = (uint32_t)Result;
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"Default TTL error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}

//[Listen] block
	else if (Data.find("PcapCapture=1") == 0)
	{
		Parameter.PcapCapture = true;
	}
	else if (Data.find("OperationMode=") == 0)
	{
		if (Data.find("OperationMode=Private") == 0 || Data.find("OperationMode=private") == 0)
			Parameter.OperationMode = LISTEN_PRIVATEMODE;
		else if (Data.find("OperationMode=Server") == 0 || Data.find("OperationMode=server") == 0)
			Parameter.OperationMode = LISTEN_SERVERMODE;
		else if (Data.find("OperationMode=Custom") == 0 || Data.find("OperationMode=custom") == 0)
			Parameter.OperationMode = LISTEN_CUSTOMMODE;
		else 
			Parameter.OperationMode = LISTEN_PROXYMODE;
	}
	else if (Data.find("ListenProtocol=") == 0)
	{
		if (Data.find("IPv6") != std::string::npos || Data.find("IPV6") != std::string::npos || Data.find("ipv6") != std::string::npos)
		{
			if (Data.find("IPv4") != std::string::npos || Data.find("IPV4") != std::string::npos || Data.find("ipv4") != std::string::npos)
				Parameter.ListenProtocol = LISTEN_IPV4_IPV6;
			else 
				Parameter.ListenProtocol = LISTEN_IPV6;
		}
		else {
			Parameter.ListenProtocol = LISTEN_IPV4;
		}
	}
	else if (Data.find("ListenPort=") == 0 && Data.length() > strlen("ListenPort="))
	{
		if (Data.length() < strlen("ListenPort=") + 6U)
		{
			Result = ServiceNameToHex((PSTR)Data.c_str() + strlen("ListenPort="));
			if (Result == 0)
			{
				Result = strtol(Data.c_str() + strlen("ListenPort="), nullptr, NULL);
				if (Result > 0 && Result <= U16_MAXNUM)
				{
					Parameter.ListenPort = htons((uint16_t)Result);
				}
				else {
					PrintError(LOG_ERROR_PARAMETER, L"Localhost server listening Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPFilterType=Permit") == 0 || Data.find("IPFilterType=permit") == 0)
	{
		Parameter.IPFilterType = true;
	}
	else if (Data.find("IPFilterLevel<") == 0 && Data.length() > strlen("IPFilterLevel<"))
	{
		if (Data.length() < strlen("IPFilterLevel<") + 4U)
		{
			Result = strtol(Data.c_str() + strlen("IPFilterLevel<"), nullptr, NULL);
			if (Result > 0 && Result <= U16_MAXNUM)
			{
				Parameter.IPFilterLevel = (size_t)Result;
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"IPFilter Level error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("AcceptType=") == 0 && Data.length() > strlen("AcceptType="))
	{
		if (Data.find(ASCII_COLON) == std::string::npos)
		{
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
		else {
		//Permit or Deny.
			if (Data.find("Permit:") != std::string::npos || Data.find("permit:") != std::string::npos)
				Parameter.AcceptType = true;
			else 
				Parameter.AcceptType = false;

			std::string TypeString(Data, Data.find(ASCII_COLON) + 1U);
		//Add to global list.
			if (TypeString.empty())
			{
				PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
			else if (TypeString.find(ASCII_COMMA) == std::string::npos && TypeString.find(ASCII_VERTICAL) == std::string::npos)
			{
				Result = DNSTypeNameToHex((PSTR)TypeString.c_str());
				if (Result == 0)
				{
				//Number types
					Result = strtol(TypeString.c_str(), nullptr, NULL);
					if (Result > 0 && Result <= U16_MAXNUM)
					{
						Parameter.AcceptTypeList->push_back(htons((uint16_t)Result));
					}
					else {
						PrintError(LOG_ERROR_PARAMETER, L"DNS Records type error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}
				}
				else {
					Parameter.AcceptTypeList->push_back((uint16_t)Result);
				}
			}
			else {
				std::string TypeStringTemp;
				Result = 0;
				for (size_t Index = 0;Index < TypeString.length();Index++)
				{
				//Last value
					if (Index == TypeString.length() - 1U)
					{
						TypeStringTemp.append(TypeString, Result, (SSIZE_T)Index - Result + 1U);
						Result = DNSTypeNameToHex((PSTR)TypeString.c_str());
						if (Result == 0) 
						{
						//Number types
							Result = strtol(TypeString.c_str(), nullptr, NULL);
							if (Result > 0 && Result <= U16_MAXNUM)
							{
								Parameter.AcceptTypeList->push_back(htons((uint16_t)Result));
							}
							else {
								PrintError(LOG_ERROR_PARAMETER, L"DNS Records type error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
								return EXIT_FAILURE;
							}
						}
						else {
							Parameter.AcceptTypeList->push_back((uint16_t)Result);
						}
					}
					else if (TypeString[Index] == ASCII_COMMA || TypeString[Index] == ASCII_VERTICAL)
					{
						TypeStringTemp.append(TypeString, Result, (SSIZE_T)Index - Result);
						Result = DNSTypeNameToHex((PSTR)TypeString.c_str());
						if (Result == 0)
						{
						//Number types
							Result = strtol(TypeString.c_str(), nullptr, NULL);
							if (Result > 0 && Result <= U16_MAXNUM)
							{
								Parameter.AcceptTypeList->push_back(htons((uint16_t)Result));
							}
							else {
								PrintError(LOG_ERROR_PARAMETER, L"DNS Records type error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
								return EXIT_FAILURE;
							}
						}
						else {
							Parameter.AcceptTypeList->push_back((uint16_t)Result);
						}

						TypeStringTemp.clear();
						Result = Index + 1U;
					}
				}
			}
		}
	}

//[Addresses] block
	else if (Data.find("IPv4DNSAddress=") == 0 && Data.length() > strlen("IPv4DNSAddress="))
	{
		if (Data.length() > strlen("IPv4DNSAddress=") + 8U && (Data.length() < strlen("IPv4DNSAddress=") + 22U || Data.find(ASCII_VERTICAL) != std::string::npos))
		{
		//IPv4 Address and Port check
			if (Data.find(ASCII_COLON) == std::string::npos || Data.find(ASCII_COLON) < strlen("IPv4DNSAddress=") + IPV4_SHORTEST_ADDRSTRING)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
		//Multi requesting.
			if (Data.find(ASCII_VERTICAL) != std::string::npos)
			{
				std::shared_ptr<DNSServerData> DNSServerDataTemp(new DNSServerData());
				Data.erase(0, strlen("IPv4DNSAddress="));

			//Read data.
				while (Data.find(ASCII_COLON) != std::string::npos && Data.find(ASCII_VERTICAL) != std::string::npos && Data.find(ASCII_COLON) < Data.find(ASCII_VERTICAL))
				{
				//Convert IPv4 Address.
					memcpy(Target.get(), Data.c_str(), Data.find(ASCII_COLON));
					if (AddressStringToBinary(Target.get(), &DNSServerDataTemp->AddressData.IPv4.sin_addr, AF_INET, Result) == EXIT_FAILURE)
					{
						PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}

				//Convert IPv4 Port.
					Data.erase(0, Data.find(ASCII_COLON) + 1U);
					memset(Target.get(), 0, ADDR_STRING_MAXSIZE);
					memcpy(Target.get(), Data.c_str(), Data.find(ASCII_VERTICAL));
					Result = ServiceNameToHex(Target.get());
					if (Result == 0)
					{
						Result = strtol(Target.get(), nullptr, NULL);
						if (Result <= 0 || Result > U16_MAXNUM)
						{
							PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
							return EXIT_FAILURE;
						}
						else {
							DNSServerDataTemp->AddressData.IPv4.sin_port = htons((uint16_t)Result);
						}
					}
					memset(Target.get(), 0, ADDR_STRING_MAXSIZE);

				//Add to global list.
					DNSServerDataTemp->AddressData.Storage.ss_family = AF_INET;
					Parameter.DNSTarget.IPv4_Multi->push_back(*DNSServerDataTemp);
					memset(DNSServerDataTemp.get(), 0, sizeof(DNSServerData));
					Data.erase(0, Data.find(ASCII_VERTICAL) + 1U);
				}

			//Last data
				//Convert IPv4 Address.
				if (Data.find(ASCII_COLON) != std::string::npos)
				{
					memcpy(Target.get(), Data.c_str(), Data.find(ASCII_COLON));
				}
				else {
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
				if (AddressStringToBinary(Target.get(), &DNSServerDataTemp->AddressData.IPv4.sin_addr, AF_INET, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}

				//Convert IPv4 Port.
				Data.erase(0, Data.find(ASCII_COLON) + 1U);
				memset(Target.get(), 0, ADDR_STRING_MAXSIZE);
				memcpy(Target.get(), Data.c_str(), Data.length());
				Result = ServiceNameToHex(Target.get());
				if (Result == 0)
				{
					Result = strtol(Target.get(), nullptr, NULL);
					if (Result <= 0 || Result > U16_MAXNUM)
					{
						PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}
					else {
						DNSServerDataTemp->AddressData.IPv4.sin_port = htons((uint16_t)Result);
					}
				}
				Target.reset();

			//Add to global list.
				DNSServerDataTemp->AddressData.Storage.ss_family = AF_INET;
				Parameter.DNSTarget.IPv4_Multi->push_back(*DNSServerDataTemp);
			}
			else {
			//Convert IPv4 Address.
				memcpy(Target.get(), Data.c_str() + strlen("IPv4DNSAddress="), Data.find(ASCII_COLON) - strlen("IPv4DNSAddress="));
				if (AddressStringToBinary(Target.get(), &Parameter.DNSTarget.IPv4.AddressData.IPv4.sin_addr, AF_INET, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
				Target.reset();

			//Convert IPv4 Port.
				Result = ServiceNameToHex((PSTR)Data.c_str() + Data.find(ASCII_COLON) + 1U);
				if (Result == 0)
				{
					Result = strtol(Data.c_str() + Data.find(ASCII_COLON) + 1U, nullptr, NULL);
					if (Result <= 0 || Result > U16_MAXNUM)
					{
						PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}
					else {
						Parameter.DNSTarget.IPv4.AddressData.IPv4.sin_port = htons((uint16_t)Result);
					}
				}

				Parameter.DNSTarget.IPv4.AddressData.Storage.ss_family = AF_INET;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4AlternateDNSAddress=") == 0 && Data.length() > strlen("IPv4AlternateDNSAddress="))
	{
		if (Data.length() > strlen("IPv4AlternateDNSAddress=") + 8U && (Data.length() < strlen("IPv4AlternateDNSAddress=") + 22U || Data.find(ASCII_VERTICAL) != std::string::npos))
		{
		//IPv4 Address and Port check
			if (Data.find(ASCII_COLON) == std::string::npos || Data.find(ASCII_COLON) < strlen("IPv4AlternateDNSAddress=") + IPV4_SHORTEST_ADDRSTRING)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
		//Multi requesting.
			if (Data.find(ASCII_VERTICAL) != std::string::npos)
			{
				std::shared_ptr<DNSServerData> DNSServerDataTemp(new DNSServerData());
				Data.erase(0, strlen("IPv4AlternateDNSAddress="));

			//Read data.
				while (Data.find(ASCII_COLON) != std::string::npos && Data.find(ASCII_VERTICAL) != std::string::npos && Data.find(ASCII_COLON) < Data.find(ASCII_VERTICAL))
				{
				//Convert IPv4 Address.
					memcpy(Target.get(), Data.c_str(), Data.find(ASCII_COLON));
					if (AddressStringToBinary(Target.get(), &DNSServerDataTemp->AddressData.IPv4.sin_addr, AF_INET, Result) == EXIT_FAILURE)
					{
						PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}

				//Convert IPv4 Port.
					Data.erase(0, Data.find(ASCII_COLON) + 1U);
					memset(Target.get(), 0, ADDR_STRING_MAXSIZE);
					memcpy(Target.get(), Data.c_str(), Data.find(ASCII_VERTICAL));
					Result = ServiceNameToHex(Target.get());
					if (Result == 0)
					{
						Result = strtol(Target.get(), nullptr, NULL);
						if (Result <= 0 || Result > U16_MAXNUM)
						{
							PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
							return EXIT_FAILURE;
						}
						else {
							DNSServerDataTemp->AddressData.IPv4.sin_port = htons((uint16_t)Result);
						}
					}
					memset(Target.get(), 0, ADDR_STRING_MAXSIZE);

				//Add to global list.
					DNSServerDataTemp->AddressData.Storage.ss_family = AF_INET;
					Parameter.DNSTarget.IPv4_Multi->push_back(*DNSServerDataTemp);
					memset(DNSServerDataTemp.get(), 0, sizeof(DNSServerData));
					Data.erase(0, Data.find(ASCII_VERTICAL) + 1U);
				}

			//Last data
				//Convert IPv4 Address.
				if (Data.find(ASCII_COLON) != std::string::npos)
				{
					memcpy(Target.get(), Data.c_str(), Data.find(ASCII_COLON));
				}
				else {
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
				if (AddressStringToBinary(Target.get(), &DNSServerDataTemp->AddressData.IPv4.sin_addr, AF_INET, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}

				//Convert IPv4 Port.
				Data.erase(0, Data.find(ASCII_COLON) + 1U);
				memset(Target.get(), 0, ADDR_STRING_MAXSIZE);
				memcpy(Target.get(), Data.c_str(), Data.length());
				Result = ServiceNameToHex(Target.get());
				if (Result == 0)
				{
					Result = strtol(Target.get(), nullptr, NULL);
					if (Result <= 0 || Result > U16_MAXNUM)
					{
						PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}
					else {
						DNSServerDataTemp->AddressData.IPv4.sin_port = htons((uint16_t)Result);
					}
				}
				Target.reset();

			//Add to global list.
				DNSServerDataTemp->AddressData.Storage.ss_family = AF_INET;
				Parameter.DNSTarget.IPv4_Multi->push_back(*DNSServerDataTemp);
			}
			else {
			//Convert IPv4 Address.
				memcpy(Target.get(), Data.c_str() + strlen("IPv4AlternateDNSAddress="), Data.find(ASCII_COLON) - strlen("IPv4AlternateDNSAddress="));
				if (AddressStringToBinary(Target.get(), &Parameter.DNSTarget.Alternate_IPv4.AddressData.IPv4.sin_addr, AF_INET, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
				Target.reset();

			//Convert IPv4 Port.
				Result = ServiceNameToHex((PSTR)Data.c_str() + Data.find(ASCII_COLON) + 1U);
				if (Result == 0)
				{
					Result = strtol(Data.c_str() + Data.find(ASCII_COLON) + 1U, nullptr, NULL);
					if (Result <= 0 || Result > U16_MAXNUM)
					{
						PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}
					else {
						Parameter.DNSTarget.Alternate_IPv4.AddressData.IPv4.sin_port = htons((uint16_t)Result);
					}
				}

				Parameter.DNSTarget.Alternate_IPv4.AddressData.Storage.ss_family = AF_INET;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4LocalDNSAddress=") == 0 && Data.length() > strlen("IPv4LocalDNSAddress="))
	{
		if (Data.length() > strlen("IPv4LocalDNSAddress=") + 8U && Data.length() < strlen("IPv4LocalDNSAddress=") + 22U)
		{
		//IPv4 Address and Port check
			if (Data.find(ASCII_COLON) == std::string::npos || Data.find(ASCII_COLON) < strlen("IPv4LocalDNSAddress=") + IPV4_SHORTEST_ADDRSTRING)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

		//Convert IPv4 Address.
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("IPv4LocalDNSAddress="), Data.find(ASCII_COLON) - strlen("IPv4LocalDNSAddress="));
			if (AddressStringToBinary(Target.get(), &Parameter.DNSTarget.Local_IPv4.AddressData.IPv4.sin_addr, AF_INET, Result) == EXIT_FAILURE)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

		//Convert IPv4 Port.
			Result = ServiceNameToHex((PSTR)Data.c_str() + Data.find(ASCII_COLON) + 1U);
			if (Result == 0)
			{
				Result = strtol(Data.c_str() + Data.find(ASCII_COLON) + 1U, nullptr, NULL);
				if (Result <= 0 || Result > U16_MAXNUM)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
				else {
					Parameter.DNSTarget.Local_IPv4.AddressData.IPv4.sin_port = htons((uint16_t)Result);
				}
			}

			Parameter.DNSTarget.Local_IPv4.AddressData.Storage.ss_family = AF_INET;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4LocalAlternateDNSAddress=") == 0 && Data.length() > strlen("IPv4LocalAlternateDNSAddress="))
	{
		if (Data.length() > strlen("IPv4LocalAlternateDNSAddress=") + 8U && Data.length() < strlen("IPv4LocalAlternateDNSAddress=") + 22U)
		{
		//IPv4 Address and Port check
			if (Data.find(ASCII_COLON) == std::string::npos || Data.find(ASCII_COLON) < strlen("IPv4LocalAlternateDNSAddress=") + IPV4_SHORTEST_ADDRSTRING)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

		//Convert IPv4 Address.
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("IPv4LocalAlternateDNSAddress="), Data.find(ASCII_COLON) - strlen("IPv4LocalAlternateDNSAddress="));
			if (AddressStringToBinary(Target.get(), &Parameter.DNSTarget.Alternate_Local_IPv4.AddressData.IPv4.sin_addr, AF_INET, Result) == EXIT_FAILURE)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

		//Convert IPv4 Port.
			Result = ServiceNameToHex((PSTR)Data.c_str() + Data.find(ASCII_COLON) + 1U);
			if (Result == 0)
			{
				Result = strtol(Data.c_str() + Data.find(ASCII_COLON) + 1U, nullptr, NULL);
				if (Result <= 0 || Result > U16_MAXNUM)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
				else {
					Parameter.DNSTarget.Alternate_Local_IPv4.AddressData.IPv4.sin_port = htons((uint16_t)Result);
				}
			}

			Parameter.DNSTarget.Alternate_Local_IPv4.AddressData.Storage.ss_family = AF_INET;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6DNSAddress=") == 0 && Data.length() > strlen("IPv6DNSAddress="))
	{
		if (Data.length() > strlen("IPv6DNSAddress=") + 6U && (Data.length() < strlen("IPv6DNSAddress=") + 48U || Data.find(ASCII_VERTICAL) != std::string::npos))
		{
		//IPv6 Address and Port check.
			if (Data.find(ASCII_BRACKETS_LEAD) == std::string::npos || Data.find(ASCII_BRACKETS_TRAIL) == std::string::npos || 
				Data.find(ASCII_BRACKETS_TRAIL) < strlen("IPv6DNSAddress=") + IPV6_SHORTEST_ADDRSTRING)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
		//Multi requesting
			if (Data.find(ASCII_VERTICAL) != std::string::npos)
			{
				std::shared_ptr<DNSServerData> DNSServerDataTemp(new DNSServerData());
				Data.erase(0, strlen("IPv6DNSAddress="));

			//Delete all front brackets and port colon.
				while (Data.find(ASCII_BRACKETS_LEAD) != std::string::npos)
					Data.erase(Data.find(ASCII_BRACKETS_LEAD), 1U);
				while (Data.find("]:") != std::string::npos)
					Data.erase(Data.find("]:") + 1U, 1U);

			//Read data.
				while (Data.find(ASCII_BRACKETS_TRAIL) != std::string::npos && Data.find(ASCII_VERTICAL) != std::string::npos && Data.find(ASCII_BRACKETS_TRAIL) < Data.find(ASCII_VERTICAL))
				{
				//Convert IPv6 Address.
					memcpy(Target.get(), Data.c_str(), Data.find(ASCII_BRACKETS_TRAIL));
					if (AddressStringToBinary(Target.get(), &DNSServerDataTemp->AddressData.IPv6.sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
					{
						PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}

				//Convert IPv6 Port.
					Data.erase(0, Data.find(ASCII_BRACKETS_TRAIL) + 1U);
					memset(Target.get(), 0, ADDR_STRING_MAXSIZE);
					memcpy(Target.get(), Data.c_str(), Data.find(ASCII_VERTICAL));
					Result = ServiceNameToHex(Target.get());
					if (Result == 0)
					{
						Result = strtol(Target.get(), nullptr, NULL);
						if (Result <= 0 || Result > U16_MAXNUM)
						{
							PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
							return EXIT_FAILURE;
						}
						else {
							DNSServerDataTemp->AddressData.IPv6.sin6_port = htons((uint16_t)Result);
						}
					}
					memset(Target.get(), 0, ADDR_STRING_MAXSIZE);

				//Add to global list.
					DNSServerDataTemp->AddressData.Storage.ss_family = AF_INET6;
					Parameter.DNSTarget.IPv6_Multi->push_back(*DNSServerDataTemp);
					memset(DNSServerDataTemp.get(), 0, sizeof(DNSServerData));
					Data.erase(0, Data.find(ASCII_VERTICAL) + 1U);
				}

			//Last data
				//Convert IPv6 Address.
				if (Data.find(ASCII_BRACKETS_TRAIL) != std::string::npos)
				{
					memcpy(Target.get(), Data.c_str(), Data.find(ASCII_BRACKETS_TRAIL));
				}
				else {
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
				if (AddressStringToBinary(Target.get(), &DNSServerDataTemp->AddressData.IPv6.sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}

				//Convert IPv6 Port.
				Data.erase(0, Data.find(ASCII_BRACKETS_TRAIL) + 1U);
				memset(Target.get(), 0, ADDR_STRING_MAXSIZE);
				memcpy(Target.get(), Data.c_str(), Data.length());
				Result = ServiceNameToHex(Target.get());
				if (Result == 0)
				{
					Result = strtol(Target.get(), nullptr, NULL);
					if (Result <= 0 || Result > U16_MAXNUM)
					{
						PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}
					else {
						DNSServerDataTemp->AddressData.IPv6.sin6_port = htons((uint16_t)Result);
					}
				}

			//Add to global list.
				DNSServerDataTemp->AddressData.Storage.ss_family = AF_INET6;
				Parameter.DNSTarget.IPv6_Multi->push_back(*DNSServerDataTemp);
			}
			else {
			//Convert IPv6 Address.
				memcpy(Target.get(), Data.c_str() + strlen("IPv6DNSAddress=") + 1U, Data.find(ASCII_BRACKETS_TRAIL) - strlen("IPv6DNSAddress=") - 1U);
				if (AddressStringToBinary(Target.get(), &Parameter.DNSTarget.IPv6.AddressData.IPv6.sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}

			//Convert IPv6 Port.
				Result = ServiceNameToHex((PSTR)Data.c_str() + Data.find(ASCII_BRACKETS_TRAIL) + 2U);
				if (Result == 0)
				{
					Result = strtol(Data.c_str() + Data.find(ASCII_BRACKETS_TRAIL) + 2U, nullptr, NULL);
					if (Result <= 0 || Result > U16_MAXNUM)
					{
						PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}
					else {
						Parameter.DNSTarget.IPv6.AddressData.IPv6.sin6_port = htons((uint16_t)Result);
					}
				}

				Parameter.DNSTarget.IPv6.AddressData.Storage.ss_family = AF_INET6;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6AlternateDNSAddress=") == 0 && Data.length() > strlen("IPv6AlternateDNSAddress="))
	{
		if (Data.length() > strlen("IPv6AlternateDNSAddress=") + 6U && (Data.length() < strlen("IPv6AlternateDNSAddress=") + 48U || Data.find(ASCII_VERTICAL) != std::string::npos))
		{
		//IPv6 Address and Port check.
			if (Data.find(ASCII_BRACKETS_LEAD) == std::string::npos || Data.find(ASCII_BRACKETS_TRAIL) == std::string::npos || 
				Data.find(ASCII_BRACKETS_TRAIL) < strlen("IPv6AlternateDNSAddress=") + IPV6_SHORTEST_ADDRSTRING)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
		//Multi requesting
			if (Data.find(ASCII_VERTICAL) != std::string::npos)
			{
				std::shared_ptr<DNSServerData> DNSServerDataTemp(new DNSServerData());
				Data.erase(0, strlen("IPv6AlternateDNSAddress="));

			//Delete all front brackets and port colon.
				while (Data.find(ASCII_BRACKETS_LEAD) != std::string::npos)
					Data.erase(Data.find(ASCII_BRACKETS_LEAD), 1U);
				while (Data.find("]:") != std::string::npos)
					Data.erase(Data.find("]:") + 1U, 1U);

			//Read data.
				while (Data.find(ASCII_BRACKETS_TRAIL) != std::string::npos && Data.find(ASCII_VERTICAL) != std::string::npos && Data.find(ASCII_BRACKETS_TRAIL) < Data.find(ASCII_VERTICAL))
				{
				//Convert IPv6 Address.
					memcpy(Target.get(), Data.c_str(), Data.find(ASCII_BRACKETS_TRAIL));
					if (AddressStringToBinary(Target.get(), &DNSServerDataTemp->AddressData.IPv6.sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
					{
						PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}

				//Convert IPv6 Port.
					Data.erase(0, Data.find(ASCII_BRACKETS_TRAIL) + 1U);
					memset(Target.get(), 0, ADDR_STRING_MAXSIZE);
					memcpy(Target.get(), Data.c_str(), Data.find(ASCII_VERTICAL));
					Result = ServiceNameToHex(Target.get());
					if (Result == 0)
					{
						Result = strtol(Target.get(), nullptr, NULL);
						if (Result <= 0 || Result > U16_MAXNUM)
						{
							PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
							return EXIT_FAILURE;
						}
						else {
							DNSServerDataTemp->AddressData.IPv6.sin6_port = htons((uint16_t)Result);
						}
					}
					memset(Target.get(), 0, ADDR_STRING_MAXSIZE);

				//Add to global list.
					DNSServerDataTemp->AddressData.Storage.ss_family = AF_INET6;
					Parameter.DNSTarget.IPv6_Multi->push_back(*DNSServerDataTemp);
					memset(DNSServerDataTemp.get(), 0, sizeof(DNSServerData));
					Data.erase(0, Data.find(ASCII_VERTICAL) + 1U);
				}

			//Last data
				//Convert IPv6 Address.
				if (Data.find(ASCII_BRACKETS_TRAIL) != std::string::npos)
				{
					memcpy(Target.get(), Data.c_str(), Data.find(ASCII_BRACKETS_TRAIL));
				}
				else {
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
				if (AddressStringToBinary(Target.get(), &DNSServerDataTemp->AddressData.IPv6.sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}

				//Convert IPv6 Port.
				Data.erase(0, Data.find(ASCII_BRACKETS_TRAIL) + 1U);
				memset(Target.get(), 0, ADDR_STRING_MAXSIZE);
				memcpy(Target.get(), Data.c_str(), Data.length());
				Result = ServiceNameToHex(Target.get());
				if (Result == 0)
				{
					Result = strtol(Target.get(), nullptr, NULL);
					if (Result <= 0 || Result > U16_MAXNUM)
					{
						PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}
					else {
						DNSServerDataTemp->AddressData.IPv6.sin6_port = htons((uint16_t)Result);
					}
				}

			//Add to global list.
				DNSServerDataTemp->AddressData.Storage.ss_family = AF_INET6;
				Parameter.DNSTarget.IPv6_Multi->push_back(*DNSServerDataTemp);
			}
			else {
			//Convert IPv6 Address
				memcpy(Target.get(), Data.c_str() + strlen("IPv6AlternateDNSAddress=") + 1U, Data.find(ASCII_BRACKETS_TRAIL) - strlen("IPv6AlternateDNSAddress=") - 1U);
				if (AddressStringToBinary(Target.get(), &Parameter.DNSTarget.Alternate_IPv6.AddressData.IPv6.sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}

			//Convert IPv6 Port.
				Result = ServiceNameToHex((PSTR)Data.c_str() + Data.find(ASCII_BRACKETS_TRAIL) + 2U);
				if (Result == 0)
				{
					Result = strtol(Data.c_str() + Data.find(ASCII_BRACKETS_TRAIL) + 2U, nullptr, NULL);
					if (Result <= 0 || Result > U16_MAXNUM)
					{
						PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}
					else {
						Parameter.DNSTarget.Alternate_IPv6.AddressData.IPv6.sin6_port = htons((uint16_t)Result);
					}
				}

				Parameter.DNSTarget.Alternate_IPv6.AddressData.Storage.ss_family = AF_INET6;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6LocalDNSAddress=") == 0 && Data.length() > strlen("IPv6LocalDNSAddress="))
	{
		if (Data.length() > strlen("IPv6LocalDNSAddress=") + 6U && Data.length() < strlen("IPv6LocalDNSAddress=") + 48U)
		{
		//IPv6 Address and Port check.
			if (Data.find(ASCII_BRACKETS_LEAD) == std::string::npos || Data.find(ASCII_BRACKETS_TRAIL) == std::string::npos || 
				Data.find(ASCII_BRACKETS_TRAIL) < strlen("IPv6LocalDNSAddress=") + IPV6_SHORTEST_ADDRSTRING)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

		//Convert IPv6 Address.
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("IPv6LocalDNSAddress=") + 1U, Data.find(ASCII_BRACKETS_TRAIL) - strlen("IPv6LocalDNSAddress=") - 1U);
			if (AddressStringToBinary(Target.get(), &Parameter.DNSTarget.Local_IPv6.AddressData.IPv6.sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

		//Convert IPv6 Port.
			Result = ServiceNameToHex((PSTR)Data.c_str() + Data.find(ASCII_BRACKETS_TRAIL) + 2U);
			if (Result == 0)
			{
				Result = strtol(Data.c_str() + Data.find(ASCII_BRACKETS_TRAIL) + 2U, nullptr, NULL);
				if (Result <= 0 || Result > U16_MAXNUM)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
				else {
					Parameter.DNSTarget.Local_IPv6.AddressData.IPv6.sin6_port = htons((uint16_t)Result);
				}
			}

			Parameter.DNSTarget.Local_IPv6.AddressData.Storage.ss_family = AF_INET6;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6LocalAlternateDNSAddress=") == 0 && Data.length() > strlen("IPv6LocalAlternateDNSAddress="))
	{
		if (Data.length() > strlen("IPv6LocalAlternateDNSAddress=") + 6U && Data.length() < strlen("IPv6LocalAlternateDNSAddress=") + 48U)
		{
		//IPv6 Address and Port check
			if (Data.find(ASCII_BRACKETS_LEAD) == std::string::npos || Data.find(ASCII_BRACKETS_TRAIL) == std::string::npos || 
				Data.find(ASCII_BRACKETS_TRAIL) < strlen("IPv6LocalAlternateDNSAddress=") + IPV6_SHORTEST_ADDRSTRING)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

		//Convert IPv6 Address.
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("IPv6LocalAlternateDNSAddress=") + 1U, Data.find(ASCII_BRACKETS_TRAIL) - strlen("IPv6LocalAlternateDNSAddress=") - 1U);
			if (AddressStringToBinary(Target.get(), &Parameter.DNSTarget.Alternate_Local_IPv6.AddressData.IPv6.sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

		//Convert IPv6 Port.
			Result = ServiceNameToHex((PSTR)Data.c_str() + Data.find(ASCII_BRACKETS_TRAIL) + 2U);
			if (Result == 0)
			{
				Result = strtol(Data.c_str() + Data.find(ASCII_BRACKETS_TRAIL) + 2U, nullptr, NULL);
				if (Result <= 0 || Result > U16_MAXNUM)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
				else {
					Parameter.DNSTarget.Alternate_Local_IPv6.AddressData.IPv6.sin6_port = htons((uint16_t)Result);
				}
			}

			Parameter.DNSTarget.Alternate_Local_IPv6.AddressData.Storage.ss_family = AF_INET6;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}

//[Values] block
	else if (Data.find("EDNS0PayloadSize=") == 0 && Data.length() > strlen("EDNS0PayloadSize="))
	{
		if (Data.length() < strlen("EDNS0PayloadSize=") + 6U)
		{
			Result = strtol(Data.c_str() + strlen("EDNS0PayloadSize="), nullptr, NULL);
			if (Result >= 0)
				Parameter.EDNS0PayloadSize = Result;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4TTL=") == 0 && Data.length() > strlen("IPv4TTL="))
	{
		if (Data.length() > strlen("IPv4TTL=") && Data.length() < strlen("IPv4TTL=") + ADDR_STRING_MAXSIZE)
		{
			if (Data.find(ASCII_VERTICAL) == std::string::npos)
			{
				Result = strtol(Data.c_str() + strlen("IPv4TTL="), nullptr, NULL);
				if (Result > 0 && Result < U8_MAXNUM)
					Parameter.DNSTarget.IPv4.HopLimitData.TTL = (uint8_t)Result;
			}
			else {
				std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
				Data.erase(0, strlen("IPv4TTL="));
				size_t Index = 0;

				while (Data.find(ASCII_VERTICAL) != std::string::npos && Data.find(ASCII_VERTICAL) > 0)
				{
					memset(Target.get(), 0, ADDR_STRING_MAXSIZE);
					memcpy(Target.get(), Data.c_str(), Data.find(ASCII_VERTICAL));
					Data.erase(0, Data.find(ASCII_VERTICAL) + 1U);
					Result = strtol(Target.get(), nullptr, NULL);

				//Mark TTL.
					if (Result > 0 && Result < U8_MAXNUM && Parameter.DNSTarget.IPv4_Multi->size() > Index)
						Parameter.DNSTarget.IPv4_Multi->at(Index).HopLimitData.TTL = (uint8_t)Result;
					
					Index++;
				}

			//Last item
				memset(Target.get(), 0, ADDR_STRING_MAXSIZE);
				memcpy(Target.get(), Data.c_str(), Data.length());
				Result = strtol(Target.get(), nullptr, NULL);
				if (Result > 0 && Result < U8_MAXNUM && Parameter.DNSTarget.IPv4_Multi->size() > Index)
					Parameter.DNSTarget.IPv4_Multi->at(Index).HopLimitData.TTL = (uint8_t)Result;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6HopLimits=") == 0 && Data.length() > strlen("IPv6HopLimits="))
	{
		if (Data.length() > strlen("IPv6HopLimits=") && Data.length() < strlen("IPv6HopLimits=") + ADDR_STRING_MAXSIZE)
		{
			if (Data.find(ASCII_VERTICAL) == std::string::npos)
			{
				Result = strtol(Data.c_str() + strlen("IPv6HopLimits="), nullptr, NULL);
				if (Result > 0 && Result < U8_MAXNUM)
					Parameter.DNSTarget.IPv6.HopLimitData.HopLimit = (uint8_t)Result;
			}
			else {
				std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
				Data.erase(0, strlen("IPv6HopLimits="));
				size_t Index = 0;

				while (Data.find(ASCII_VERTICAL) != std::string::npos && Data.find(ASCII_VERTICAL) > 0)
				{
					memset(Target.get(), 0, ADDR_STRING_MAXSIZE);
					memcpy(Target.get(), Data.c_str(), Data.find(ASCII_VERTICAL));
					Data.erase(0, Data.find(ASCII_VERTICAL) + 1U);
					Result = strtol(Target.get(), nullptr, NULL);

				//Mark HopLimit.
					if (Result > 0 && Result < U8_MAXNUM && Parameter.DNSTarget.IPv6_Multi->size() > Index)
						Parameter.DNSTarget.IPv6_Multi->at(Index).HopLimitData.HopLimit = (uint8_t)Result;
					
					Index++;
				}

			//Last item
				memset(Target.get(), 0, ADDR_STRING_MAXSIZE);
				memcpy(Target.get(), Data.c_str(), Data.length());
				Result = strtol(Target.get(), nullptr, NULL);
				if (Result > 0 && Result < U8_MAXNUM && Parameter.DNSTarget.IPv6_Multi->size() > Index)
					Parameter.DNSTarget.IPv6_Multi->at(Index).HopLimitData.HopLimit = (uint8_t)Result;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4AlternateTTL=") == 0 && Data.length() > strlen("IPv4AlternateTTL="))
	{
		if (Data.length() > strlen("IPv4AlternateTTL=") && Data.length() < strlen("IPv4AlternateTTL=") + ADDR_STRING_MAXSIZE)
		{
			if (Data.find(ASCII_VERTICAL) == std::string::npos)
			{
				Result = strtol(Data.c_str() + strlen("IPv4AlternateTTL="), nullptr, NULL);
				if (Result > 0 && Result < U8_MAXNUM)
					Parameter.DNSTarget.Alternate_IPv4.HopLimitData.TTL = (uint8_t)Result;
			}
			else {
				std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
				Data.erase(0, strlen("IPv4AlternateTTL="));
				size_t Index = 0;
				for (size_t InnerIndex = 0;InnerIndex < Parameter.DNSTarget.IPv4_Multi->size();InnerIndex++)
				{
					if (Parameter.DNSTarget.IPv4_Multi->at(InnerIndex).HopLimitData.TTL > 0)
						Index = InnerIndex;
				}

				while (Data.find(ASCII_VERTICAL) != std::string::npos && Data.find(ASCII_VERTICAL) > 0)
				{
					memset(Target.get(), 0, ADDR_STRING_MAXSIZE);
					memcpy(Target.get(), Data.c_str(), Data.find(ASCII_VERTICAL));
					Data.erase(0, Data.find(ASCII_VERTICAL) + 1U);
					Result = strtol(Target.get(), nullptr, NULL);

				//Mark TTL.
					if (Result > 0 && Result < U8_MAXNUM && Parameter.DNSTarget.IPv4_Multi->size() > Index)
						Parameter.DNSTarget.IPv4_Multi->at(Index).HopLimitData.TTL = (uint8_t)Result;
					
					Index++;
				}

			//Last item
				memset(Target.get(), 0, ADDR_STRING_MAXSIZE);
				memcpy(Target.get(), Data.c_str(), Data.length());
				Result = strtol(Target.get(), nullptr, NULL);
				if (Result > 0 && Result < U8_MAXNUM && Parameter.DNSTarget.IPv4_Multi->size() > Index)
					Parameter.DNSTarget.IPv4_Multi->at(Index).HopLimitData.TTL = (uint8_t)Result;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6AlternateHopLimits=") == 0 && Data.length() > strlen("IPv6AlternateHopLimits="))
	{
		if (Data.length() > strlen("IPv6AlternateHopLimits=") && Data.length() < strlen("IPv6AlternateHopLimits=") + ADDR_STRING_MAXSIZE)
		{
			if (Data.find(ASCII_VERTICAL) == std::string::npos)
			{
				Result = strtol(Data.c_str() + strlen("IPv6AlternateHopLimits="), nullptr, NULL);
				if (Result > 0 && Result < U8_MAXNUM)
					Parameter.DNSTarget.Alternate_IPv6.HopLimitData.HopLimit = (uint8_t)Result;
			}
			else {
				std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
				Data.erase(0, strlen("IPv6AlternateHopLimits="));
				size_t Index = 0;
				for (size_t InnerIndex = 0;InnerIndex < Parameter.DNSTarget.IPv6_Multi->size();InnerIndex++)
				{
					if (Parameter.DNSTarget.IPv6_Multi->at(InnerIndex).HopLimitData.HopLimit > 0)
						Index = InnerIndex;
				}

				while (Data.find(ASCII_VERTICAL) != std::string::npos && Data.find(ASCII_VERTICAL) > 0)
				{
					memset(Target.get(), 0, ADDR_STRING_MAXSIZE);
					memcpy(Target.get(), Data.c_str(), Data.find(ASCII_VERTICAL));
					Data.erase(0, Data.find(ASCII_VERTICAL) + 1U);
					Result = strtol(Target.get(), nullptr, NULL);

				//Mark HopLimit.
					if (Result > 0 && Result < U8_MAXNUM && Parameter.DNSTarget.IPv6_Multi->size() > Index)
						Parameter.DNSTarget.IPv6_Multi->at(Index).HopLimitData.HopLimit = (uint8_t)Result;
					
					Index++;
				}

			//Last item
				memset(Target.get(), 0, ADDR_STRING_MAXSIZE);
				memcpy(Target.get(), Data.c_str(), Data.length());
				Result = strtol(Target.get(), nullptr, NULL);
				if (Result > 0 && Result < U8_MAXNUM && Parameter.DNSTarget.IPv6_Multi->size() > Index)
					Parameter.DNSTarget.IPv6_Multi->at(Index).HopLimitData.HopLimit = (uint8_t)Result;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("HopLimitsFluctuation=") == 0 && Data.length() > strlen("HopLimitsFluctuation="))
	{
		if (Data.length() < strlen("HopLimitsFluctuation=") + 4U)
		{
			Result = strtol(Data.c_str() + strlen("HopLimitsFluctuation="), nullptr, NULL);
			if (Result > 0 && Result < U8_MAXNUM)
				Parameter.HopLimitFluctuation = (uint8_t)Result;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("ReliableSocketTimeout=") == 0 && Data.length() > strlen("ReliableSocketTimeout="))
	{
		if (Data.length() < strlen("ReliableSocketTimeout=") + 9U)
		{
			Result = strtol(Data.c_str() + strlen("ReliableSocketTimeout="), nullptr, NULL);
			if (Result > SOCKET_TIMEOUT_MIN)
				Parameter.ReliableSocketTimeout = (int)Result;
			else 
				Parameter.ReliableSocketTimeout = SOCKET_TIMEOUT_MIN;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("UnreliableSocketTimeout=") == 0 && Data.length() > strlen("UnreliableSocketTimeout="))
	{
		if (Data.length() < strlen("UnreliableSocketTimeout=") + 9U)
		{
			Result = strtol(Data.c_str() + strlen("UnreliableSocketTimeout="), nullptr, NULL);
			if (Result > SOCKET_TIMEOUT_MIN)
				Parameter.UnreliableSocketTimeout = (int)Result;
			else 
				Parameter.UnreliableSocketTimeout = SOCKET_TIMEOUT_MIN;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("ICMPTest=") == 0 && Data.length() > strlen("ICMPTest="))
	{
		if (Data.length() < strlen("ICMPTest=") + 6U)
		{
			Result = strtol(Data.c_str() + strlen("ICMPTest="), nullptr, NULL);
			if (Result >= 5)
				Parameter.ICMPSpeed = Result * SECOND_TO_MILLISECOND;
			else if (Result > 0 && Result < DEFAULT_ICMPTEST_TIME)
				Parameter.ICMPSpeed = DEFAULT_ICMPTEST_TIME * SECOND_TO_MILLISECOND;
			else 
				Parameter.ICMPSpeed = 0; //ICMP Test Disable.
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DomainTest=") == 0 && Data.length() > strlen("DomainTest="))
	{
		if (Data.length() < strlen("DomainTest=") + 6U)
		{
			Result = strtol(Data.c_str() + strlen("DomainTest="), nullptr, NULL);
			if (Result > 0)
				Parameter.DomainTestSpeed = Result * SECOND_TO_MILLISECOND;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);

			return EXIT_FAILURE;
		}
	}
	else if (Data.find("AlternateTimes=") == 0 && Data.length() > strlen("AlternateTimes="))
	{
		if (Data.length() < strlen("AlternateTimes=") + 6U)
		{
			Result = strtol(Data.c_str() + strlen("AlternateTimes="), nullptr, NULL);
			if (Result > 0)
				Parameter.AlternateTimes = Result;
			else 
				Parameter.AlternateTimes = DEFAULT_ALTERNATE_TIMES;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("AlternateTimeRange=") == 0 && Data.length() > strlen("AlternateTimeRange="))
	{
		if (Data.length() < strlen("AlternateTimeRange=") + 6U)
		{
			Result = strtol(Data.c_str() + strlen("AlternateTimeRange="), nullptr, NULL);
			if (Result >= DEFAULT_ALTERNATE_RANGE)
				Parameter.AlternateTimeRange = Result * SECOND_TO_MILLISECOND;
			else 
				Parameter.AlternateTimeRange = DEFAULT_ALTERNATE_RANGE * SECOND_TO_MILLISECOND;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("AlternateResetTime=") == 0 && Data.length() > strlen("AlternateResetTime="))
	{
		if (Data.length() < strlen("AlternateResetTime=") + 6U)
		{
			Result = strtol(Data.c_str() + strlen("AlternateResetTime="), nullptr, NULL);
			if (Result >= DEFAULT_ALTERNATERESET_TIME)
				Parameter.AlternateResetTime = Result * SECOND_TO_MILLISECOND;
			else 
				Parameter.AlternateResetTime = DEFAULT_ALTERNATERESET_TIME * SECOND_TO_MILLISECOND;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("MultiRequestTimes=") == 0 && Data.length() > strlen("MultiRequestTimes="))
	{
		if (Data.length() < strlen("MultiRequestTimes=") + 6U)
		{
			Result = strtol(Data.c_str() + strlen("MultiRequestTimes="), nullptr, NULL);
			if (Result > 0)
				Parameter.MultiRequestTimes = Result + 1U;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}

//[Switches] block
	else if (Data.find("DomainCaseConversion=1") == 0)
	{
		Parameter.DomainCaseConversion = true;
	}
	else if (Data.find("CompressionPointerMutation=") == 0 && Data.length() > strlen("CompressionPointerMutation="))
	{
		if (Data.find(ASCII_ONE) != std::string::npos)
			Parameter.CPMPointerToHeader = true;
		if (Data.find(ASCII_TWO) != std::string::npos)
			Parameter.CPMPointerToRR = true;
		if (Data.find(ASCII_THREE) != std::string::npos)
			Parameter.CPMPointerToAdditional = true;
		if (Parameter.CPMPointerToHeader || 
			Parameter.CPMPointerToRR || 
			Parameter.CPMPointerToAdditional)
				Parameter.CompressionPointerMutation = true;
	}
	else if (Data.find("EDNS0Label=1") == 0)
	{
		Parameter.EDNS0Label = true;
	}
	else if (Data.find("DNSSECRequest=1") == 0)
	{
		Parameter.DNSSECRequest = true;
	}
	else if (Data.find("AlternateMultiRequest=1") == 0)
	{
		Parameter.AlternateMultiRequest = true;
	}
	else if (Data.find("IPv4DataFilter=1") == 0)
	{
		Parameter.IPv4DataCheck = true;
	}
	else if (Data.find("TCPDataFilter=1") == 0)
	{
		Parameter.TCPDataCheck = true;
	}
	else if (Data.find("DNSDataFilter=1") == 0)
	{
		Parameter.DNSDataCheck = true;
	}
	else if (Data.find("BlacklistFilter=1") == 0)
	{
		Parameter.Blacklist = true;
	}

//[Data] block
	else if (Data.find("ICMPID=") == 0 && Data.length() > strlen("ICMPID="))
	{
		if (Data.length() < strlen("ICMPID=") + 7U)
		{
			Result = strtol(Data.c_str() + strlen("ICMPID="), nullptr, NULL);
			if (Result > 0)
				Parameter.ICMPID = htons((uint16_t)Result);
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("ICMPSequence=") == 0 && Data.length() > strlen("ICMPSequence="))
	{
		if (Data.length() < strlen("ICMPSequence=") + 7U)
		{
			Result = strtol(Data.c_str() + strlen("ICMPSequence="), nullptr, NULL);
			if (Result > 0)
				Parameter.ICMPSequence = htons((uint16_t)Result);
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("ICMPPaddingData=") == 0 && Data.length() > strlen("ICMPPaddingData="))
	{
		if (Data.length() > strlen("ICMPPaddingData=") + 17U && Data.length() < strlen("ICMPPaddingData=") + ICMP_PADDING_MAXSIZE - 1U)
		{
			Parameter.ICMPPaddingDataLength = Data.length() - strlen("ICMPPaddingData=") - 1U;
			memcpy(Parameter.ICMPPaddingData, Data.c_str() + strlen("ICMPPaddingData="), Data.length() - strlen("ICMPPaddingData="));
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DomainTestID=") == 0 && Data.length() > strlen("DomainTestID="))
	{
		if (Data.length() < strlen("DomainTestID=") + 7U)
		{
			Result = strtol(Data.c_str() + strlen("DomainTestID="), nullptr, NULL);
			if (Result > 0)
				Parameter.DomainTestID = htons((uint16_t)Result);
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DomainTestData=") == 0 && Data.length() > strlen("DomainTestData="))
	{
		if (Data.length() > strlen("DomainTestData=") + DOMAIN_MINSIZE && Data.length() < strlen("DomainTestData=") + DOMAIN_DATA_MAXSIZE)
		{
			memcpy(Parameter.DomainTestData, Data.c_str() + strlen("DomainTestData="), Data.length() - strlen("DomainTestData="));
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("LocalhostServerName=") == 0 && Data.length() > strlen("LocalhostServerName="))
	{
		if (Data.length() > strlen("LocalhostServerName=") + DOMAIN_MINSIZE && Data.length() < strlen("LocalhostServerName=") + DOMAIN_DATA_MAXSIZE)
		{
			std::shared_ptr<char> LocalFQDN(new char[DOMAIN_MAXSIZE]());
			Parameter.LocalFQDNLength = Data.length() - strlen("LocalhostServerName=");
			memcpy(LocalFQDN.get(), Data.c_str() + strlen("LocalhostServerName="), Parameter.LocalFQDNLength);
			*Parameter.LocalFQDNString = LocalFQDN.get();
			Result = CharToDNSQuery(LocalFQDN.get(), Parameter.LocalFQDN);
			if (Result > DOMAIN_MINSIZE)
			{
				Parameter.LocalFQDNLength = Result;
			}
			else {
				Parameter.LocalFQDNLength = 0;
				memset(Parameter.LocalFQDN, 0, DOMAIN_MAXSIZE);
				Parameter.LocalFQDNString->clear();
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}

//[DNSCurve] block
	else if (Data.find("DNSCurve=1") == 0)
	{
		Parameter.DNSCurve = true;
	}
	else if (Data.find("DNSCurveProtocol=TCP") == 0 || Data.find("DNSCurveProtocol=Tcp") == 0 || Data.find("DNSCurveProtocol=tcp") == 0)
	{
		DNSCurveParameter.DNSCurveMode = DNSCURVE_REQUEST_TCPMODE;
	}
	else if (Data.find("DNSCurvePayloadSize=") == 0 && Data.length() > strlen("DNSCurvePayloadSize="))
	{
		if (Data.length() > strlen("DNSCurvePayloadSize=") + 2U)
		{
			Result = strtol(Data.c_str() + strlen("DNSCurvePayloadSize="), nullptr, NULL);
			if (Result > sizeof(eth_hdr) + sizeof(ipv4_hdr) + sizeof(udp_hdr) + sizeof(uint16_t) + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES)
				DNSCurveParameter.DNSCurvePayloadSize = Result;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("Encryption=1") == 0)
	{
		DNSCurveParameter.IsEncryption = true;
	}
	else if (Data.find("IsEncryptionOnly=1") == 0)
	{
		DNSCurveParameter.IsEncryptionOnly = true;
	}
	else if (Data.find("KeyRecheckTime=") == 0 && Data.length() > strlen("KeyRecheckTime="))
	{
		if (Data.length() < strlen("KeyRecheckTime=") + 6U)
		{
			Result = strtol(Data.c_str() + strlen("KeyRecheckTime="), nullptr, NULL);
			if (Result >= SHORTEST_DNSCURVE_RECHECK_TIME && Result < DEFAULT_DNSCURVE_RECHECK_TIME)
				DNSCurveParameter.KeyRecheckTime = Result * SECOND_TO_MILLISECOND;
			else 
				DNSCurveParameter.KeyRecheckTime = DEFAULT_DNSCURVE_RECHECK_TIME * SECOND_TO_MILLISECOND;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}

//[DNSCurve Addresses] block
	else if (Data.find("DNSCurveIPv4DNSAddress=") == 0 && Data.length() > strlen("DNSCurveIPv4DNSAddress="))
	{
		if (Data.length() > strlen("DNSCurveIPv4DNSAddress=") + 8U && Data.length() < strlen("DNSCurveIPv4DNSAddress=") + 22U)
		{
		//IPv4 Address and Port check
			if (Data.find(ASCII_COLON) == std::string::npos || Data.find(ASCII_COLON) < strlen("DNSCurveIPv4DNSAddress=") + IPV4_SHORTEST_ADDRSTRING)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

		//Convert IPv4 Address.
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("DNSCurveIPv4DNSAddress="), Data.find(ASCII_COLON) - strlen("DNSCurveIPv4DNSAddress="));
			if (AddressStringToBinary(Target.get(), &DNSCurveParameter.DNSCurveTarget.IPv4.AddressData.IPv4.sin_addr, AF_INET, Result) == EXIT_FAILURE)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

		//Convert IPv4 Port.
			Result = ServiceNameToHex((PSTR)Data.c_str() + Data.find(ASCII_COLON) + 1U);
			if (Result == 0)
			{
				Result = strtol(Data.c_str() + Data.find(ASCII_COLON) + 1U, nullptr, NULL);
				if (Result <= 0 || Result > U16_MAXNUM)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
				else {
					DNSCurveParameter.DNSCurveTarget.IPv4.AddressData.IPv4.sin_port = htons((uint16_t)Result);
				}
			}

			DNSCurveParameter.DNSCurveTarget.IPv4.AddressData.Storage.ss_family = AF_INET;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DNSCurveIPv4AlternateDNSAddress=") == 0 && Data.length() > strlen("DNSCurveIPv4AlternateDNSAddress="))
	{
		if (Data.length() > strlen("DNSCurveIPv4AlternateDNSAddress=") + 8U && Data.length() < strlen("DNSCurveIPv4AlternateDNSAddress=") + 22U)
		{
		//IPv4 Address and Port check
			if (Data.find(ASCII_COLON) == std::string::npos || Data.find(ASCII_COLON) < strlen("DNSCurveIPv4AlternateDNSAddress=") + IPV4_SHORTEST_ADDRSTRING)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

		//Convert IPv4 Address.
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("DNSCurveIPv4AlternateDNSAddress="), Data.find(ASCII_COLON) - strlen("DNSCurveIPv4AlternateDNSAddress="));
			if (AddressStringToBinary(Target.get(), &DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.AddressData.IPv4.sin_addr, AF_INET, Result) == EXIT_FAILURE)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

		//Convert IPv4 Port.
			Result = ServiceNameToHex((PSTR)Data.c_str() + Data.find(ASCII_COLON) + 1U);
			if (Result == 0)
			{
				Result = strtol(Data.c_str() + Data.find(ASCII_COLON) + 1U, nullptr, NULL);
				if (Result <= 0 || Result > U16_MAXNUM)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv4 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
				else {
					DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.AddressData.IPv4.sin_port = htons((uint16_t)Result);
				}
			}

			DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.AddressData.Storage.ss_family = AF_INET;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DNSCurveIPv6DNSAddress=") == 0 && Data.length() > strlen("DNSCurveIPv6DNSAddress="))
	{
		if (Data.length() > strlen("DNSCurveIPv6DNSAddress=") + 6U && Data.length() < strlen("DNSCurveIPv6DNSAddress=") + 48U)
		{
		//IPv6 Address and Port check
			if (Data.find(ASCII_BRACKETS_LEAD) == std::string::npos || Data.find(ASCII_BRACKETS_TRAIL) == std::string::npos || 
				Data.find(ASCII_BRACKETS_TRAIL) < strlen("DNSCurveIPv6DNSAddress=") + IPV6_SHORTEST_ADDRSTRING)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

		//Convert IPv6 Address.
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("DNSCurveIPv6DNSAddress=") + 1U, Data.find(ASCII_BRACKETS_TRAIL) - strlen("DNSCurveIPv6DNSAddress=") - 1U);
			if (AddressStringToBinary(Target.get(), &DNSCurveParameter.DNSCurveTarget.IPv6.AddressData.IPv6.sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

		//Convert IPv6 Port.
			Result = ServiceNameToHex((PSTR)Data.c_str() + Data.find(ASCII_BRACKETS_TRAIL) + 2U);
			if (Result == 0)
			{
				Result = strtol(Data.c_str() + Data.find(ASCII_BRACKETS_TRAIL) + 2U, nullptr, NULL);
				if (Result <= 0 || Result > U16_MAXNUM)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
				else {
					DNSCurveParameter.DNSCurveTarget.IPv6.AddressData.IPv6.sin6_port = htons((uint16_t)Result);
				}
			}

			DNSCurveParameter.DNSCurveTarget.IPv6.AddressData.Storage.ss_family = AF_INET6;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DNSCurveIPv6AlternateDNSAddress=") == 0 && Data.length() > strlen("DNSCurveIPv6AlternateDNSAddress="))
	{
		if (Data.length() > strlen("DNSCurveIPv6AlternateDNSAddress=") + 6U && Data.length() < strlen("DNSCurveIPv6AlternateDNSAddress=") + 48U)
		{
		//IPv6 Address and Port check
			if (Data.find(ASCII_BRACKETS_LEAD) == std::string::npos || Data.find(ASCII_BRACKETS_TRAIL) == std::string::npos || 
				Data.find(ASCII_BRACKETS_TRAIL) < strlen("DNSCurveIPv6AlternateDNSAddress=") + IPV6_SHORTEST_ADDRSTRING)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

		//Convert IPv6 Address.
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("DNSCurveIPv6AlternateDNSAddress=") + 1U, Data.find(ASCII_BRACKETS_TRAIL) - strlen("DNSCurveIPv6AlternateDNSAddress=") - 1U);
			if (AddressStringToBinary(Target.get(), &DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.AddressData.IPv6.sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
			{
				PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Address format error", Result, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}

		//Convert IPv6 Port.
			Result = ServiceNameToHex((PSTR)Data.c_str() + Data.find(ASCII_BRACKETS_TRAIL) + 2U);
			if (Result == 0)
			{
				Result = strtol(Data.c_str() + Data.find(ASCII_BRACKETS_TRAIL) + 2U, nullptr, NULL);
				if (Result <= 0 || Result > U16_MAXNUM)
				{
					PrintError(LOG_ERROR_PARAMETER, L"DNS server IPv6 Port error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
					return EXIT_FAILURE;
				}
				else {
					DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.AddressData.IPv6.sin6_port = htons((uint16_t)Result);
				}
			}

			DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.AddressData.Storage.ss_family = AF_INET6;
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}

	else if (Data.find("DNSCurveIPv4ProviderName=") == 0 && Data.length() > strlen("DNSCurveIPv4ProviderName="))
	{
		if (Data.length() > strlen("DNSCurveIPv4ProviderName=") + DOMAIN_MINSIZE && Data.length() < strlen("DNSCurveIPv4ProviderName=") + DOMAIN_DATA_MAXSIZE)
		{
			for (Result = strlen("DNSCurveIPv4ProviderName=");Result < (SSIZE_T)(Data.length() - strlen("DNSCurveIPv4ProviderName="));Result++)
			{
				for (size_t Index = 0;Index < strlen(Parameter.DomainTable);Index++)
				{
					if (Index == strlen(Parameter.DomainTable) - 1U && Data[Result] != Parameter.DomainTable[Index])
					{
						PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Provider Names error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}
					if (Data[Result] == Parameter.DomainTable[Index])
						break;
				}
			}

			memcpy(DNSCurveParameter.DNSCurveTarget.IPv4.ProviderName, Data.c_str() + strlen("DNSCurveIPv4ProviderName="), Data.length() - strlen("DNSCurveIPv4ProviderName="));
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DNSCurveIPv4AlternateProviderName=") == 0 && Data.length() > strlen("DNSCurveIPv4AlternateProviderName="))
	{
		if (Data.length() > strlen("DNSCurveIPv4AlternateProviderName=") + DOMAIN_MINSIZE && Data.length() < strlen("DNSCurveIPv4AlternateProviderName=") + DOMAIN_DATA_MAXSIZE)
		{
			for (Result = strlen("DNSCurveIPv4AlternateProviderName=");Result < (SSIZE_T)(Data.length() - strlen("DNSCurveIPv4AlternateProviderName="));Result++)
			{
				for (size_t Index = 0;Index < strlen(Parameter.DomainTable);Index++)
				{
					if (Index == strlen(Parameter.DomainTable) - 1U && Data[Result] != Parameter.DomainTable[Index])
					{
						PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Provider Names error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}
					if (Data[Result] == Parameter.DomainTable[Index])
						break;
				}
			}

			memcpy(DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ProviderName, Data.c_str() + strlen("DNSCurveIPv4AlternateProviderName="), Data.length() - strlen("DNSCurveIPv4AlternateProviderName="));
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DNSCurveIPv6ProviderName=") == 0 && Data.length() > strlen("DNSCurveIPv6ProviderName="))
	{
		if (Data.length() > strlen("DNSCurveIPv6ProviderName=") + DOMAIN_MINSIZE && Data.length() < strlen("DNSCurveIPv6ProviderName=") + DOMAIN_DATA_MAXSIZE)
		{
			for (Result = strlen("DNSCurveIPv6ProviderName=");Result < (SSIZE_T)(Data.length() - strlen("DNSCurveIPv6ProviderName="));Result++)
			{
				for (size_t Index = 0;Index < strlen(Parameter.DomainTable);Index++)
				{
					if (Index == strlen(Parameter.DomainTable) - 1U && Data[Result] != Parameter.DomainTable[Index])
					{
						PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Provider Names error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}
					if (Data[Result] == Parameter.DomainTable[Index])
						break;
				}
			}

			memcpy(DNSCurveParameter.DNSCurveTarget.IPv6.ProviderName, Data.c_str() + strlen("DNSCurveIPv6ProviderName="), Data.length() - strlen("DNSCurveIPv6ProviderName="));
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DNSCurveIPv6AlternateProviderName=") == 0 && Data.length() > strlen("DNSCurveIPv6AlternateProviderName="))
	{
		if (Data.length() > strlen("DNSCurveIPv6AlternateProviderName=") + DOMAIN_MINSIZE && Data.length() < strlen("DNSCurveIPv6AlternateProviderName=") + DOMAIN_DATA_MAXSIZE)
		{
			for (Result = strlen("DNSCurveIPv6AlternateProviderName=");Result < (SSIZE_T)(Data.length() - strlen("DNSCurveIPv6AlternateProviderName="));Result++)
			{
				for (size_t Index = 0;Index < strlen(Parameter.DomainTable);Index++)
				{
					if (Index == strlen(Parameter.DomainTable) - 1U && Data[Result] != Parameter.DomainTable[Index])
					{
						PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Provider Names error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
						return EXIT_FAILURE;
					}
					if (Data[Result] == Parameter.DomainTable[Index])
						break;
				}
			}

			memcpy(DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ProviderName, Data.c_str() + strlen("DNSCurveIPv6AlternateProviderName="), Data.length() - strlen("DNSCurveIPv6AlternateProviderName="));
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}

//[DNSCurve Keys] block
	else if (Data.find("ClientPublicKey=") == 0 && Data.length() > strlen("ClientPublicKey="))
	{
		if (Data.length() > strlen("ClientPublicKey=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("ClientPublicKey=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			size_t ResultLength = 0;
			PSTR ResultPointer = nullptr;
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());

			Result = sodium_hex2bin((PUCHAR)Target.get(), ADDR_STRING_MAXSIZE, Data.c_str() + strlen("ClientPublicKey="), Data.length() - strlen("ClientPublicKey="), ": ", &ResultLength, (const char **)&ResultPointer);
			if (Result == 0 && ResultLength == crypto_box_PUBLICKEYBYTES && ResultPointer != nullptr)
			{
				memcpy(DNSCurveParameter.Client_PublicKey, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Keys error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("ClientSecretKey=") == 0 && Data.length() > strlen("ClientSecretKey="))
	{
		if (Data.length() > strlen("ClientSecretKey=") + crypto_box_SECRETKEYBYTES * 2U && Data.length() < strlen("ClientSecretKey=") + crypto_box_SECRETKEYBYTES * 3U)
		{
			size_t ResultLength = 0;
			PSTR ResultPointer = nullptr;
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());

			Result = sodium_hex2bin((PUCHAR)Target.get(), ADDR_STRING_MAXSIZE, Data.c_str() + strlen("ClientSecretKey="), Data.length() - strlen("ClientSecretKey="), ": ", &ResultLength, (const char **)&ResultPointer);
			if (Result == 0 && ResultLength == crypto_box_SECRETKEYBYTES && ResultPointer != nullptr)
			{
				memcpy(DNSCurveParameter.Client_SecretKey, Target.get(), crypto_box_SECRETKEYBYTES);
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Keys error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4DNSPublicKey=") == 0 && Data.length() > strlen("IPv4DNSPublicKey="))
	{
		if (Data.length() > strlen("IPv4DNSPublicKey=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("IPv4DNSPublicKey=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			size_t ResultLength = 0;
			PSTR ResultPointer = nullptr;
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());

			Result = sodium_hex2bin((PUCHAR)Target.get(), ADDR_STRING_MAXSIZE, Data.c_str() + strlen("IPv4DNSPublicKey="), Data.length() - strlen("IPv4DNSPublicKey="), ": ", &ResultLength, (const char **)&ResultPointer);
			if (Result == 0 && ResultLength == crypto_box_PUBLICKEYBYTES && ResultPointer != nullptr)
			{
				memcpy(DNSCurveParameter.DNSCurveTarget.IPv4.ServerPublicKey, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Keys error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4AlternateDNSPublicKey=") == 0 && Data.length() > strlen("IPv4AlternateDNSPublicKey="))
	{
		if (Data.length() > strlen("IPv4AlternateDNSPublicKey=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("IPv4AlternateDNSPublicKey=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			size_t ResultLength = 0;
			PSTR ResultPointer = nullptr;
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());

			Result = sodium_hex2bin((PUCHAR)Target.get(), ADDR_STRING_MAXSIZE, Data.c_str() + strlen("IPv4AlternateDNSPublicKey="), Data.length() - strlen("IPv4AlternateDNSPublicKey="), ": ", &ResultLength, (const char **)&ResultPointer);
			if (Result == 0 && ResultLength == crypto_box_PUBLICKEYBYTES && ResultPointer != nullptr)
			{
				memcpy(DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ServerPublicKey, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Keys error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6DNSPublicKey=") == 0 && Data.length() > strlen("IPv6DNSPublicKey="))
	{
		if (Data.length() > strlen("IPv6DNSPublicKey=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("IPv6DNSPublicKey=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			size_t ResultLength = 0;
			PSTR ResultPointer = nullptr;
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());

			Result = sodium_hex2bin((PUCHAR)Target.get(), ADDR_STRING_MAXSIZE, Data.c_str() + strlen("IPv6DNSPublicKey="), Data.length() - strlen("IPv6DNSPublicKey="), ": ", &ResultLength, (const char **)&ResultPointer);
			if (Result == 0 && ResultLength == crypto_box_PUBLICKEYBYTES && ResultPointer != nullptr)
			{
				memcpy(DNSCurveParameter.DNSCurveTarget.IPv6.ServerPublicKey, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Keys error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6AlternateDNSPublicKey=") == 0 && Data.length() > strlen("IPv6AlternateDNSPublicKey="))
	{
		if (Data.length() > strlen("IPv6AlternateDNSPublicKey=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("IPv6AlternateDNSPublicKey=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			size_t ResultLength = 0;
			PSTR ResultPointer = nullptr;
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());

			Result = sodium_hex2bin((PUCHAR)Target.get(), ADDR_STRING_MAXSIZE, Data.c_str() + strlen("IPv6AlternateDNSPublicKey="), Data.length() - strlen("IPv6AlternateDNSPublicKey="), ": ", &ResultLength, (const char **)&ResultPointer);
			if (Result == 0 && ResultLength == crypto_box_PUBLICKEYBYTES && ResultPointer != nullptr)
			{
				memcpy(DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ServerPublicKey, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Keys error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4DNSFingerprint=") == 0 && Data.length() > strlen("IPv4DNSFingerprint="))
	{
		if (Data.length() > strlen("IPv4DNSFingerprint=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("IPv4DNSFingerprint=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			size_t ResultLength = 0;
			PSTR ResultPointer = nullptr;
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());

			Result = sodium_hex2bin((PUCHAR)Target.get(), ADDR_STRING_MAXSIZE, Data.c_str() + strlen("IPv4DNSFingerprint="), Data.length() - strlen("IPv4DNSFingerprint="), ": ", &ResultLength, (const char **)&ResultPointer);
			if (Result == 0 && ResultLength == crypto_box_PUBLICKEYBYTES && ResultPointer != nullptr)
			{
				memcpy(DNSCurveParameter.DNSCurveTarget.IPv4.ServerFingerprint, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Keys error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4AlternateDNSFingerprint=") == 0 && Data.length() > strlen("IPv4AlternateDNSFingerprint="))
	{
		if (Data.length() > strlen("IPv4AlternateDNSFingerprint=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("IPv4AlternateDNSFingerprint=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			size_t ResultLength = 0;
			PSTR ResultPointer = nullptr;
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());

			Result = sodium_hex2bin((PUCHAR)Target.get(), ADDR_STRING_MAXSIZE, Data.c_str() + strlen("IPv4AlternateDNSFingerprint="), Data.length() - strlen("IPv4AlternateDNSFingerprint="), ": ", &ResultLength, (const char **)&ResultPointer);
			if (Result == 0 && ResultLength == crypto_box_PUBLICKEYBYTES && ResultPointer != nullptr)
			{
				memcpy(DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ServerFingerprint, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Keys error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6DNSFingerprint=") == 0 && Data.length() > strlen("IPv6DNSFingerprint="))
	{
		if (Data.length() > strlen("IPv6DNSFingerprint=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("IPv6DNSFingerprint=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			size_t ResultLength = 0;
			PSTR ResultPointer = nullptr;
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());

			Result = sodium_hex2bin((PUCHAR)Target.get(), ADDR_STRING_MAXSIZE, Data.c_str() + strlen("IPv6DNSFingerprint="), Data.length() - strlen("IPv6DNSFingerprint="), ": ", &ResultLength, (const char **)&ResultPointer);
			if (Result == 0 && ResultLength == crypto_box_PUBLICKEYBYTES && ResultPointer != nullptr)
			{
				memcpy(DNSCurveParameter.DNSCurveTarget.IPv6.ServerFingerprint, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Keys error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6AlternateDNSFingerprint=") == 0 && Data.length() > strlen("IPv6AlternateDNSFingerprint="))
	{
		if (Data.length() > strlen("IPv6AlternateDNSFingerprint=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("IPv6AlternateDNSFingerprint=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			size_t ResultLength = 0;
			PSTR ResultPointer = nullptr;
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());

			Result = sodium_hex2bin((PUCHAR)Target.get(), ADDR_STRING_MAXSIZE, Data.c_str() + strlen("IPv6AlternateDNSFingerprint="), Data.length() - strlen("IPv6AlternateDNSFingerprint="), ": ", &ResultLength, (const char **)&ResultPointer);
			if (Result == 0 && ResultLength == crypto_box_PUBLICKEYBYTES && ResultPointer != nullptr)
			{
				memcpy(DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ServerFingerprint, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				PrintError(LOG_ERROR_PARAMETER, L"DNSCurve Keys error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
				return EXIT_FAILURE;
			}
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}

//[DNSCurve Magic Number] block
	else if (Data.find("IPv4ReceiveMagicNumber=") == 0 && Data.length() > strlen("IPv4ReceiveMagicNumber="))
	{
		if (Data.length() == strlen("IPv4ReceiveMagicNumber=") + DNSCURVE_MAGIC_QUERY_LEN)
		{
			memcpy(DNSCurveParameter.DNSCurveTarget.IPv4.ReceiveMagicNumber, Data.c_str() + strlen("IPv4ReceiveMagicNumber="), DNSCURVE_MAGIC_QUERY_LEN);
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4AlternateReceiveMagicNumber=") == 0 && Data.length() > strlen("IPv4AlternateReceiveMagicNumber="))
	{
		if (Data.length() == strlen("IPv4AlternateReceiveMagicNumber=") + DNSCURVE_MAGIC_QUERY_LEN)
		{
			memcpy(DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.ReceiveMagicNumber, Data.c_str() + strlen("IPv4AlternateReceiveMagicNumber="), DNSCURVE_MAGIC_QUERY_LEN);
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6ReceiveMagicNumber=") == 0 && Data.length() > strlen("IPv6ReceiveMagicNumber="))
	{
		if (Data.length() == strlen("IPv6ReceiveMagicNumber=") + DNSCURVE_MAGIC_QUERY_LEN)
		{
			memcpy(DNSCurveParameter.DNSCurveTarget.IPv6.ReceiveMagicNumber, Data.c_str() + strlen("IPv6ReceiveMagicNumber="), DNSCURVE_MAGIC_QUERY_LEN);
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6AlternateReceiveMagicNumber=") == 0 && Data.length() > strlen("IPv6AlternateReceiveMagicNumber="))
	{
		if (Data.length() == strlen("IPv6AlternateReceiveMagicNumber=") + DNSCURVE_MAGIC_QUERY_LEN)
		{
			memcpy(DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.ReceiveMagicNumber, Data.c_str() + strlen("IPv6AlternateReceiveMagicNumber="), DNSCURVE_MAGIC_QUERY_LEN);
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4DNSMagicNumber=") == 0 && Data.length() > strlen("IPv4DNSMagicNumber="))
	{
		if (Data.length() == strlen("IPv4DNSMagicNumber=") + DNSCURVE_MAGIC_QUERY_LEN)
		{
			memcpy(DNSCurveParameter.DNSCurveTarget.IPv4.SendMagicNumber, Data.c_str() + strlen("IPv4DNSMagicNumber="), DNSCURVE_MAGIC_QUERY_LEN);
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4AlternateDNSMagicNumber=") == 0 && Data.length() > strlen("IPv4AlternateDNSMagicNumber="))
	{
		if (Data.length() == strlen("IPv4AlternateDNSMagicNumber=") + DNSCURVE_MAGIC_QUERY_LEN)
		{
			memcpy(DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.SendMagicNumber, Data.c_str() + strlen("IPv4AlternateDNSMagicNumber="), DNSCURVE_MAGIC_QUERY_LEN);
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6DNSMagicNumber=") == 0 && Data.length() > strlen("IPv6DNSMagicNumber="))
	{
		if (Data.length() == strlen("IPv6DNSMagicNumber=") + DNSCURVE_MAGIC_QUERY_LEN)
		{
			memcpy(DNSCurveParameter.DNSCurveTarget.IPv6.SendMagicNumber, Data.c_str() + strlen("IPv6DNSMagicNumber="), DNSCURVE_MAGIC_QUERY_LEN);
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6AlternateDNSMagicNumber=") == 0 && Data.length() > strlen("IPv6AlternateDNSMagicNumber="))
	{
		if (Data.length() == strlen("IPv6AlternateDNSMagicNumber=") + DNSCURVE_MAGIC_QUERY_LEN)
		{
			memcpy(DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.SendMagicNumber, Data.c_str() + strlen("IPv6AlternateDNSMagicNumber="), DNSCURVE_MAGIC_QUERY_LEN);
		}
		else {
			PrintError(LOG_ERROR_PARAMETER, L"Item length error", NULL, (PWSTR)ConfigFileList[FileIndex].c_str(), Line);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

//Read ipfilter from file
size_t __fastcall ReadIPFilter(void)
{
/* Old version(2015-01-03)
//Initialization(Available when file hash check is ON.)
	SSIZE_T Index = 0;
	std::shared_ptr<char> Buffer;
	FileData FileDataTemp[7U];
	if (Parameter.FileHash)
	{
		std::shared_ptr<char> FileDataBufferTemp(new char[FILE_BUFFER_SIZE]());
		Buffer.swap(FileDataBufferTemp);
		FileDataBufferTemp.reset();

		for (Index = 0;Index < sizeof(FileDataTemp) / sizeof(FileData);Index++)
		{
			std::shared_ptr<BitSequence> FileDataBufferTemp_SHA3(new BitSequence[SHA3_512_SIZE]());
			FileDataTemp[Index].Result.swap(FileDataBufferTemp_SHA3);
		}
	}

//Open file.
	FILE *Input = nullptr;
	std::vector<FileData> FileList;
	for (Index = 0;Index < sizeof(FileDataTemp) / sizeof(FileData);Index++)
		FileDataTemp[Index].FileName = Parameter.Path->front();
	FileDataTemp[0].FileName.append(L"IPFilter.ini");
	FileDataTemp[1U].FileName.append(L"IPFilter.conf");
	FileDataTemp[2U].FileName.append(L"IPFilter.dat");
	FileDataTemp[3U].FileName.append(L"IPFilter.csv");
	FileDataTemp[4U].FileName.append(L"IPFilter");
	FileDataTemp[5U].FileName.append(L"Guarding.P2P");
	FileDataTemp[6U].FileName.append(L"Guarding");
	for (Index = 0;Index < sizeof(FileDataTemp) / sizeof(FileData);Index++)
		FileList.push_back(FileDataTemp[Index]);
*/
//Initialization
	std::shared_ptr<char> Buffer;
	if (Parameter.FileHash)
	{
		std::shared_ptr<char> FileDataBufferTemp(new char[FILE_BUFFER_SIZE]());
		Buffer.swap(FileDataBufferTemp);
		FileDataBufferTemp.reset();
	}

//Creat file list.
	FILE *Input = nullptr;
	for (auto StringIter:*Parameter.Path)
	{
		for (auto InnerStringIter:*Parameter.IPFilterFileList)
		{
			FileData FileDataTemp;

		//Reset.
			FileDataTemp.HashAvailable = false;
			FileDataTemp.FileName.clear();
			FileDataTemp.HashResult.reset();

		//Add to list.
			FileDataTemp.FileName.append(StringIter);
			FileDataTemp.FileName.append(InnerStringIter);
			IPFilterFileList.push_back(FileDataTemp);
		}
	}

//Files monitor
	size_t FileIndex = 0, ReadLength = 0;
	auto HashChanged = false;
	std::vector<AddressRange>::iterator AddressRangeIter;
	std::vector<ResultBlacklistTable>::iterator ResultBlacklistIter;
	std::vector<AddressPrefixBlock>::iterator LocalRoutingListIter;
	HANDLE IPFilterHandle = nullptr;
	std::shared_ptr<LARGE_INTEGER> IPFilterFileSize(new LARGE_INTEGER());
	Keccak_HashInstance HashInstance = {0};
//	std::shared_ptr<in6_addr> IPv6NextAddress(new in6_addr());
//	std::shared_ptr<in_addr> IPv4NextAddress(new in_addr());

	for (;;)
	{
		HashChanged = false;
		if (Parameter.FileHash)
		{
			if (!AddressRangeUsing->empty())
				*AddressRangeModificating = *AddressRangeUsing;

			if (!ResultBlacklistUsing->empty())
				*ResultBlacklistModificating = *ResultBlacklistUsing;

			if (!LocalRoutingListUsing->empty())
				*LocalRoutingListModificating = *LocalRoutingListUsing;
		}

	//Check FileList.
		for (FileIndex = 0;FileIndex < IPFilterFileList.size();FileIndex++)
		{
			if (_wfopen_s(&Input, IPFilterFileList[FileIndex].FileName.c_str(), L"rb") != 0)
			{
			//Clear hash results.
				if (Parameter.FileHash)
				{
					if (IPFilterFileList[FileIndex].HashAvailable)
					{
						HashChanged = true;
						IPFilterFileList[FileIndex].HashResult.reset();
					}
					IPFilterFileList[FileIndex].HashAvailable = false;

				//Clear old iteams.
					for (AddressRangeIter = AddressRangeModificating->begin();AddressRangeIter != AddressRangeModificating->end();)
					{
						if (AddressRangeIter->FileIndex == FileIndex)
						{
							AddressRangeIter = AddressRangeModificating->erase(AddressRangeIter);
							if (AddressRangeIter == AddressRangeModificating->end())
								break;
						}
						else {
							AddressRangeIter++;
						}
					}

					for (ResultBlacklistIter = ResultBlacklistModificating->begin();ResultBlacklistIter != ResultBlacklistModificating->end();)
					{
						if (ResultBlacklistIter->FileIndex == FileIndex)
						{
							ResultBlacklistIter = ResultBlacklistModificating->erase(ResultBlacklistIter);
							if (ResultBlacklistIter == ResultBlacklistModificating->end())
								break;
						}
						else {
							ResultBlacklistIter++;
						}
					}

					for (LocalRoutingListIter = LocalRoutingListModificating->begin();LocalRoutingListIter != LocalRoutingListModificating->end();)
					{
						if (LocalRoutingListIter->FileIndex == FileIndex)
						{
							LocalRoutingListIter = LocalRoutingListModificating->erase(LocalRoutingListIter);
							if (LocalRoutingListIter == LocalRoutingListModificating->end())
								break;
						}
						else {
							LocalRoutingListIter++;
						}
					}
				}

				continue;
			}
			else {
				if (Input == nullptr)
				{
				//Clear hash results.
					if (Parameter.FileHash)
					{
						if (IPFilterFileList[FileIndex].HashAvailable)
						{
							HashChanged = true;
							IPFilterFileList[FileIndex].HashResult.reset();
						}
						IPFilterFileList[FileIndex].HashAvailable = false;

					//Clear old iteams.
						for (AddressRangeIter = AddressRangeModificating->begin();AddressRangeIter != AddressRangeModificating->end();)
						{
							if (AddressRangeIter->FileIndex == FileIndex)
							{
								AddressRangeIter = AddressRangeModificating->erase(AddressRangeIter);
								if (AddressRangeIter == AddressRangeModificating->end())
									break;
							}
							else {
								AddressRangeIter++;
							}
						}

						for (ResultBlacklistIter = ResultBlacklistModificating->begin();ResultBlacklistIter != ResultBlacklistModificating->end();)
						{
							if (ResultBlacklistIter->FileIndex == FileIndex)
							{
								ResultBlacklistIter = ResultBlacklistModificating->erase(ResultBlacklistIter);
								if (ResultBlacklistIter == ResultBlacklistModificating->end())
									break;
							}
							else {
								ResultBlacklistIter++;
							}
						}

						for (LocalRoutingListIter = LocalRoutingListModificating->begin();LocalRoutingListIter != LocalRoutingListModificating->end();)
						{
							if (LocalRoutingListIter->FileIndex == FileIndex)
							{
								LocalRoutingListIter = LocalRoutingListModificating->erase(LocalRoutingListIter);
								if (LocalRoutingListIter == LocalRoutingListModificating->end())
									break;
							}
							else {
								LocalRoutingListIter++;
							}
						}
					}

					continue;
				}

			//Check whole file size.
				IPFilterHandle = CreateFileW(IPFilterFileList[FileIndex].FileName.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (IPFilterHandle != INVALID_HANDLE_VALUE)
				{
					memset(IPFilterFileSize.get(), 0, sizeof(LARGE_INTEGER));
					if (GetFileSizeEx(IPFilterHandle, IPFilterFileSize.get()) == 0)
					{
						CloseHandle(IPFilterHandle);
					}
					else {
						CloseHandle(IPFilterHandle);
						if (IPFilterFileSize->QuadPart >= DEFAULT_FILE_MAXSIZE)
						{
							PrintError(LOG_ERROR_PARAMETER, L"IPFilter file size is too large", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), NULL);
							fclose(Input);
							Input = nullptr;

						//Clear hash results.
							if (Parameter.FileHash)
							{
								if (IPFilterFileList[FileIndex].HashAvailable)
								{
									HashChanged = true;
									IPFilterFileList[FileIndex].HashResult.reset();
								}
								IPFilterFileList[FileIndex].HashAvailable = false;
								
							//Clear old iteams.
								for (AddressRangeIter = AddressRangeModificating->begin();AddressRangeIter != AddressRangeModificating->end();)
								{
									if (AddressRangeIter->FileIndex == FileIndex)
									{
										AddressRangeIter = AddressRangeModificating->erase(AddressRangeIter);
										if (AddressRangeIter == AddressRangeModificating->end())
											break;
									}
									else {
										AddressRangeIter++;
									}
								}

								for (ResultBlacklistIter = ResultBlacklistModificating->begin();ResultBlacklistIter != ResultBlacklistModificating->end();)
								{
									if (ResultBlacklistIter->FileIndex == FileIndex)
									{
										ResultBlacklistIter = ResultBlacklistModificating->erase(ResultBlacklistIter);
										if (ResultBlacklistIter == ResultBlacklistModificating->end())
											break;
									}
									else {
										ResultBlacklistIter++;
									}
								}

								for (LocalRoutingListIter = LocalRoutingListModificating->begin();LocalRoutingListIter != LocalRoutingListModificating->end();)
								{
									if (LocalRoutingListIter->FileIndex == FileIndex)
									{
										LocalRoutingListIter = LocalRoutingListModificating->erase(LocalRoutingListIter);
										if (LocalRoutingListIter == LocalRoutingListModificating->end())
											break;
									}
									else {
										LocalRoutingListIter++;
									}
								}
							}

							continue;
						}
					}
				}

			//Mark or check files hash.
				if (Parameter.FileHash)
				{
					memset(Buffer.get(), 0, FILE_BUFFER_SIZE);
					memset(&HashInstance, 0, sizeof(Keccak_HashInstance));
					Keccak_HashInitialize_SHA3_512(&HashInstance);
					while (!feof(Input))
					{
						ReadLength = fread_s(Buffer.get(), FILE_BUFFER_SIZE, sizeof(char), FILE_BUFFER_SIZE, Input);
						Keccak_HashUpdate(&HashInstance, (BitSequence *)Buffer.get(), ReadLength * BYTES_TO_BITS);
					}
					memset(Buffer.get(), 0, FILE_BUFFER_SIZE);
					Keccak_HashFinal(&HashInstance, (BitSequence *)Buffer.get());

				//Set file pointers to the beginning of file.
					if (_fseeki64(Input, 0, SEEK_SET) != 0)
					{
						PrintError(LOG_ERROR_IPFILTER, L"Read files error", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), NULL);
						fclose(Input);
						Input = nullptr;

					//Clear hash results.
						if (Parameter.FileHash)
						{
							if (IPFilterFileList[FileIndex].HashAvailable)
							{
								HashChanged = true;
								IPFilterFileList[FileIndex].HashResult.reset();
							}
							IPFilterFileList[FileIndex].HashAvailable = false;

						//Clear old iteams.
							for (AddressRangeIter = AddressRangeModificating->begin();AddressRangeIter != AddressRangeModificating->end();)
							{
								if (AddressRangeIter->FileIndex == FileIndex)
								{
									AddressRangeIter = AddressRangeModificating->erase(AddressRangeIter);
									if (AddressRangeIter == AddressRangeModificating->end())
										break;
								}
								else {
									AddressRangeIter++;
								}
							}

							for (ResultBlacklistIter = ResultBlacklistModificating->begin();ResultBlacklistIter != ResultBlacklistModificating->end();)
							{
								if (ResultBlacklistIter->FileIndex == FileIndex)
								{
									ResultBlacklistIter = ResultBlacklistModificating->erase(ResultBlacklistIter);
									if (ResultBlacklistIter == ResultBlacklistModificating->end())
										break;
								}
								else {
									ResultBlacklistIter++;
								}
							}

							for (LocalRoutingListIter = LocalRoutingListModificating->begin();LocalRoutingListIter != LocalRoutingListModificating->end();)
							{
								if (LocalRoutingListIter->FileIndex == FileIndex)
								{
									LocalRoutingListIter = LocalRoutingListModificating->erase(LocalRoutingListIter);
									if (LocalRoutingListIter == LocalRoutingListModificating->end())
										break;
								}
								else {
									LocalRoutingListIter++;
								}
							}
						}

						continue;
					}
					else {
						if (IPFilterFileList[FileIndex].HashAvailable)
						{
							if (memcmp(IPFilterFileList[FileIndex].HashResult.get(), Buffer.get(), SHA3_512_SIZE) == 0)
							{
								fclose(Input);
								Input = nullptr;
								continue;
							}
							else {
								HashChanged = true;
								memcpy(IPFilterFileList[FileIndex].HashResult.get(), Buffer.get(), SHA3_512_SIZE);

							//Clear old iteams.
								for (AddressRangeIter = AddressRangeModificating->begin();AddressRangeIter != AddressRangeModificating->end();)
								{
									if (AddressRangeIter->FileIndex == FileIndex)
									{
										AddressRangeIter = AddressRangeModificating->erase(AddressRangeIter);
										if (AddressRangeIter == AddressRangeModificating->end())
											break;
									}
									else {
										AddressRangeIter++;
									}
								}

								for (ResultBlacklistIter = ResultBlacklistModificating->begin();ResultBlacklistIter != ResultBlacklistModificating->end();)
								{
									if (ResultBlacklistIter->FileIndex == FileIndex)
									{
										ResultBlacklistIter = ResultBlacklistModificating->erase(ResultBlacklistIter);
										if (ResultBlacklistIter == ResultBlacklistModificating->end())
											break;
									}
									else {
										ResultBlacklistIter++;
									}
								}

								for (LocalRoutingListIter = LocalRoutingListModificating->begin();LocalRoutingListIter != LocalRoutingListModificating->end();)
								{
									if (LocalRoutingListIter->FileIndex == FileIndex)
									{
										LocalRoutingListIter = LocalRoutingListModificating->erase(LocalRoutingListIter);
										if (LocalRoutingListIter == LocalRoutingListModificating->end())
											break;
									}
									else {
										LocalRoutingListIter++;
									}
								}
							}
						}
						else {
							HashChanged = true;
							IPFilterFileList[FileIndex].HashAvailable = true;
							std::shared_ptr<BitSequence> HashBufferTemp(new BitSequence[SHA3_512_SIZE]());
							memcpy(HashBufferTemp.get(), Buffer.get(), SHA3_512_SIZE);
							IPFilterFileList[FileIndex].HashResult.swap(HashBufferTemp);

						//Clear old iteams.
							for (AddressRangeIter = AddressRangeModificating->begin();AddressRangeIter != AddressRangeModificating->end();)
							{
								if (AddressRangeIter->FileIndex == FileIndex)
								{
									AddressRangeIter = AddressRangeModificating->erase(AddressRangeIter);
									if (AddressRangeIter == AddressRangeModificating->end())
										break;
								}
								else {
									AddressRangeIter++;
								}
							}

							for (ResultBlacklistIter = ResultBlacklistModificating->begin();ResultBlacklistIter != ResultBlacklistModificating->end();)
							{
								if (ResultBlacklistIter->FileIndex == FileIndex)
								{
									ResultBlacklistIter = ResultBlacklistModificating->erase(ResultBlacklistIter);
									if (ResultBlacklistIter == ResultBlacklistModificating->end())
										break;
								}
								else {
									ResultBlacklistIter++;
								}
							}

							for (LocalRoutingListIter = LocalRoutingListModificating->begin();LocalRoutingListIter != LocalRoutingListModificating->end();)
							{
								if (LocalRoutingListIter->FileIndex == FileIndex)
								{
									LocalRoutingListIter = LocalRoutingListModificating->erase(LocalRoutingListIter);
									if (LocalRoutingListIter == LocalRoutingListModificating->end())
										break;
								}
								else {
									LocalRoutingListIter++;
								}
							}
						}
					}
				}
			}

		//Read data.
			HashChanged = true;
			ReadText(Input, READTEXT_IPFILTER, FileIndex);
			fclose(Input);
			Input = nullptr;
		}

	//Update Address Range list.
		if (!HashChanged)
		{
		//Auto-refresh
			Sleep((DWORD)Parameter.FileRefreshTime);
			continue;
		}

	//Blacklist part
		if (!ResultBlacklistModificating->empty())
		{
		//Check repeating items.
/* Old version(2014-12-28)
			for (ResultBlacklistIter[0] = ResultBlacklistModificating->begin();ResultBlacklistIter[0] != ResultBlacklistModificating->end();ResultBlacklistIter[0]++)
			{
				for (ResultBlacklistIter[1U] = ResultBlacklistIter[0] + 1U;ResultBlacklistIter[1U] != ResultBlacklistModificating->end();ResultBlacklistIter[1U]++)
				{
					if (ResultBlacklistIter[0]->Addresses.front().End.ss_family == 0 && ResultBlacklistIter[1U]->Addresses.front().End.ss_family == 0 &&
						ResultBlacklistIter[0]->Addresses.front().Begin.ss_family == ResultBlacklistIter[1U]->Addresses.front().Begin.ss_family &&
						(ResultBlacklistIter[0]->PatternString.empty() && ResultBlacklistIter[1U]->PatternString.empty() || ResultBlacklistIter[0]->PatternString == ResultBlacklistIter[1U]->PatternString))
					{
					//IPv6
						if (ResultBlacklistIter[0]->Addresses.front().Begin.ss_family == AF_INET6)
						{
							for (AddressRangeIter[0] = ResultBlacklistIter[1U]->Addresses.begin();AddressRangeIter[0] != ResultBlacklistIter[1U]->Addresses.end();AddressRangeIter[0]++)
							{
								for (AddressRangeIter[1U] = ResultBlacklistIter[0]->Addresses.begin();AddressRangeIter[1U] != ResultBlacklistIter[0]->Addresses.end();AddressRangeIter[1U]++)
								{
									if (memcmp(&((PSOCKADDR_IN6)&AddressRangeIter[0]->Begin)->sin6_addr, &((PSOCKADDR_IN6)&AddressRangeIter[1U]->Begin)->sin6_addr, sizeof(in6_addr)) == 0)
									{
										break;
									}
									else if (AddressRangeIter[1U] == ResultBlacklistIter[0]->Addresses.end() - 1U)
									{
										ResultBlacklistIter[0]->Addresses.push_back(*AddressRangeIter[0]);
										break;
									}
								}
							}
						}
					//IPv4
						else {
							for (AddressRangeIter[0] = ResultBlacklistIter[1U]->Addresses.begin();AddressRangeIter[0] != ResultBlacklistIter[1U]->Addresses.end();AddressRangeIter[0]++)
							{
								for (AddressRangeIter[1U] = ResultBlacklistIter[0]->Addresses.begin();AddressRangeIter[1U] != ResultBlacklistIter[0]->Addresses.end();AddressRangeIter[1U]++)
								{
									if (((PSOCKADDR_IN)&AddressRangeIter[0]->Begin)->sin_addr.S_un.S_addr == ((PSOCKADDR_IN)&AddressRangeIter[1U]->Begin)->sin_addr.S_un.S_addr)
									{
										break;
									}
									else if (AddressRangeIter[1U] == ResultBlacklistIter[0]->Addresses.end() - 1U)
									{
										ResultBlacklistIter[0]->Addresses.push_back(*AddressRangeIter[0]);
										break;
									}
								}
							}
						}

						ResultBlacklistIter[0] = ResultBlacklistModificating->erase(ResultBlacklistIter[1U]);
						break;
					}
				}
			}
*/
			std::unique_lock<std::mutex> HostsListMutex(HostsListLock);
			for (auto HostsListIter:*HostsListUsing)
			{
				if (HostsListIter.Type == HOSTS_NORMAL || HostsListIter.Type == HOSTS_BANNED)
				{
					for (ResultBlacklistIter = ResultBlacklistModificating->begin();ResultBlacklistIter != ResultBlacklistModificating->end();)
					{
						if (ResultBlacklistIter->PatternString == HostsListIter.PatternString)
						{
							ResultBlacklistIter = ResultBlacklistModificating->erase(ResultBlacklistIter);
							if (ResultBlacklistIter == ResultBlacklistModificating->end())
								goto StopLoop;
						}
						else {
							ResultBlacklistIter++;
						}
					}
				}
			}

		//Stop loop.
			StopLoop: 
			HostsListMutex.unlock();

		//Sort list.
//			std::partition(ResultBlacklistModificating->begin(), ResultBlacklistModificating->end(), SortResultBlacklistALL);

		//Swap(or cleanup) using list.
			ResultBlacklistModificating->shrink_to_fit();
			std::unique_lock<std::mutex> ResultBlacklistMutex(ResultBlacklistLock);
			ResultBlacklistUsing->swap(*ResultBlacklistModificating);
			ResultBlacklistMutex.unlock();
			ResultBlacklistModificating->clear();
			ResultBlacklistModificating->shrink_to_fit();

		//Clear DNS cache after ResultBlacklist was updated.
			std::unique_lock<std::mutex> DNSCacheListMutex(DNSCacheListLock);
			DNSCacheList.clear();
			DNSCacheList.shrink_to_fit();
			DNSCacheListMutex.unlock();
		}
	//ResultBlacklist Table is empty.
		else {
			std::unique_lock<std::mutex> ResultBlacklistMutex(ResultBlacklistLock);
			ResultBlacklistUsing->clear();
			ResultBlacklistUsing->shrink_to_fit();
			ResultBlacklistMutex.unlock();
			ResultBlacklistModificating->clear();
			ResultBlacklistModificating->shrink_to_fit();
		}

	//Local Routing part
		if (!LocalRoutingListModificating->empty())
		{
		//Swap(or cleanup) using list.
			LocalRoutingListModificating->shrink_to_fit();
			std::unique_lock<std::mutex> LocalRoutingListMutex(LocalRoutingListLock);
			LocalRoutingListUsing->swap(*LocalRoutingListModificating);
			LocalRoutingListMutex.unlock();
			LocalRoutingListModificating->clear();
			LocalRoutingListModificating->shrink_to_fit();

		//Clear DNS cache after LocalRoutingList was updated.
			std::unique_lock<std::mutex> DNSCacheListMutex(DNSCacheListLock);
			DNSCacheList.clear();
			DNSCacheList.shrink_to_fit();
			DNSCacheListMutex.unlock();
		}
	//LocalRoutingList Table is empty.
		else {
			std::unique_lock<std::mutex> LocalRoutingListMutex(LocalRoutingListLock);
			LocalRoutingListUsing->clear();
			LocalRoutingListUsing->shrink_to_fit();
			LocalRoutingListMutex.unlock();
			LocalRoutingListModificating->clear();
			LocalRoutingListModificating->shrink_to_fit();
		}

	//Address Range part
		if (!AddressRangeModificating->empty())
		{
/* Old version(2014-12-28)
		//Check repeating ranges.
			for (AddressRangeIter[0] = AddressRangeModificating->begin();AddressRangeIter[0] != AddressRangeModificating->end();AddressRangeIter[0]++)
			{
				for (AddressRangeIter[1U] = AddressRangeIter[0] + 1U;AddressRangeIter[1U] != AddressRangeModificating->end();AddressRangeIter[1U]++)
				{
				//IPv6
					if (AddressRangeIter[0]->Begin.ss_family == AF_INET6 && AddressRangeIter[1U]->Begin.ss_family == AF_INET6)
					{
					//A-Range is not same as B-Range.
						if (CompareAddresses(&((PSOCKADDR_IN6)&AddressRangeIter[0]->End)->sin6_addr, &((PSOCKADDR_IN6)&AddressRangeIter[1U]->Begin)->sin6_addr, AF_INET6) == ADDRESS_COMPARE_LESS)
						{
							*IPv6NextAddress = ((PSOCKADDR_IN6)&AddressRangeIter[0]->End)->sin6_addr;

						//Check next address.
							for (Index = sizeof(in6_addr) / sizeof(uint16_t) - 1U;Index >= 0;Index--)
							{
								if (IPv6NextAddress->u.Word[Index] == U16_MAXNUM)
								{
									if (Index == 0)
										break;

									IPv6NextAddress->u.Word[Index] = 0;
								}
								else {
									IPv6NextAddress->u.Word[Index] = htons(ntohs(IPv6NextAddress->u.Word[Index]) + 1U);
									break;
								}
							}
							if (memcmp(IPv6NextAddress.get(), &((PSOCKADDR_IN6)&AddressRangeIter[1U]->Begin)->sin6_addr, sizeof(in6_addr)) == 0)
							{
								AddressRangeIter[0]->End = AddressRangeIter[1U]->End;
								AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
								AddressRangeIter[0]--;
							}

							break;
						}
					//B-Range is not same as A-Range.
						else if (CompareAddresses(&AddressRangeIter[0]->Begin, &((PSOCKADDR_IN6)&AddressRangeIter[1U]->End)->sin6_addr, AF_INET6) == ADDRESS_COMPARE_GREATER)
						{
							*IPv6NextAddress = ((PSOCKADDR_IN6)&AddressRangeIter[1U]->End)->sin6_addr;

						//Check next address.
							for (Index = sizeof(in6_addr) / sizeof(uint16_t) - 1U;Index >= 0;Index--)
							{
								if (IPv6NextAddress->u.Word[Index] == U16_MAXNUM)
								{
									if (Index == 0)
										break;

									IPv6NextAddress->u.Word[Index] = 0;
								}
								else {
									IPv6NextAddress->u.Word[Index] = htons(ntohs(IPv6NextAddress->u.Word[Index]) + 1U);
									break;
								}
							}
							if (memcmp(IPv6NextAddress.get(), &AddressRangeIter[0]->Begin, sizeof(in6_addr)) == 0)
							{
								AddressRangeIter[0]->Begin = AddressRangeIter[1U]->Begin;
								AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
								AddressRangeIter[0]--;
							}

							break;
						}
					//A-Range is same as B-Range.
						else if (CompareAddresses(&AddressRangeIter[0]->Begin, &((PSOCKADDR_IN6)&AddressRangeIter[1U]->Begin)->sin6_addr, AF_INET6) == ADDRESS_COMPARE_EQUAL &&
							CompareAddresses(&((PSOCKADDR_IN6)&AddressRangeIter[0]->End)->sin6_addr, &((PSOCKADDR_IN6)&AddressRangeIter[1U]->End)->sin6_addr, AF_INET6) == ADDRESS_COMPARE_EQUAL)
						{
							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					//A-Range connect B-Range or B-Range connect A-Range.
						else if (CompareAddresses(&((PSOCKADDR_IN6)&AddressRangeIter[0]->End)->sin6_addr, &((PSOCKADDR_IN6)&AddressRangeIter[1U]->Begin)->sin6_addr, AF_INET6) == ADDRESS_COMPARE_EQUAL)
						{
							AddressRangeIter[0]->End = AddressRangeIter[1U]->End;
							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
						else if (CompareAddresses(&((PSOCKADDR_IN6)&AddressRangeIter[1U]->End)->sin6_addr, &AddressRangeIter[0]->Begin, AF_INET6) == ADDRESS_COMPARE_EQUAL)
						{
							AddressRangeIter[0]->Begin = AddressRangeIter[1U]->Begin;
							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					//A-Range include B-Range or B-Range include A-Range.
						else if (CompareAddresses(&AddressRangeIter[0]->Begin, &((PSOCKADDR_IN6)&AddressRangeIter[1U]->Begin)->sin6_addr, AF_INET6) == ADDRESS_COMPARE_LESS &&
							CompareAddresses(&((PSOCKADDR_IN6)&AddressRangeIter[0]->End)->sin6_addr, &((PSOCKADDR_IN6)&AddressRangeIter[1U]->End)->sin6_addr, AF_INET6) == ADDRESS_COMPARE_GREATER)
						{
							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
						else if (CompareAddresses(&AddressRangeIter[0]->Begin, &((PSOCKADDR_IN6)&AddressRangeIter[1U]->Begin)->sin6_addr, AF_INET6) == ADDRESS_COMPARE_GREATER &&
							CompareAddresses(&((PSOCKADDR_IN6)&AddressRangeIter[0]->End)->sin6_addr, &((PSOCKADDR_IN6)&AddressRangeIter[1U]->End)->sin6_addr, AF_INET6) == ADDRESS_COMPARE_LESS)
						{
							*AddressRangeIter[0] = *AddressRangeIter[1U];
							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					//A part of A-Range or B-Range is same as a part of B-Range or A-Range, also begin or end of A-Range or B-Range is same as begin or end of A-Range or B-Range.
						if (CompareAddresses(&AddressRangeIter[0]->Begin, &((PSOCKADDR_IN6)&AddressRangeIter[1U]->Begin)->sin6_addr, AF_INET6) == ADDRESS_COMPARE_EQUAL)
						{
							if (CompareAddresses(&((PSOCKADDR_IN6)&AddressRangeIter[0]->End)->sin6_addr, &((PSOCKADDR_IN6)&AddressRangeIter[1U]->End)->sin6_addr, AF_INET6) == ADDRESS_COMPARE_LESS)
								*AddressRangeIter[0] = *AddressRangeIter[1U];

							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
						else if (CompareAddresses(&((PSOCKADDR_IN6)&AddressRangeIter[0]->End)->sin6_addr, &((PSOCKADDR_IN6)&AddressRangeIter[1U]->End)->sin6_addr, AF_INET6) == ADDRESS_COMPARE_EQUAL)
						{
							if (CompareAddresses(&AddressRangeIter[0]->Begin, &((PSOCKADDR_IN6)&AddressRangeIter[1U]->Begin)->sin6_addr, AF_INET6) != ADDRESS_COMPARE_LESS)
								*AddressRangeIter[0] = *AddressRangeIter[1U];

							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					//A part of A-Range or B-Range is same as a part of B-Range or A-Range.
						else if (CompareAddresses(&((PSOCKADDR_IN6)&AddressRangeIter[0]->End)->sin6_addr, &((PSOCKADDR_IN6)&AddressRangeIter[1U]->Begin)->sin6_addr, AF_INET6) == ADDRESS_COMPARE_GREATER)
						{
							AddressRangeIter[0]->End = AddressRangeIter[1U]->End;
							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
						else if (CompareAddresses(&((PSOCKADDR_IN6)&AddressRangeIter[1U]->End)->sin6_addr, &AddressRangeIter[0]->Begin, AF_INET6) == ADDRESS_COMPARE_GREATER)
						{
							AddressRangeIter[0]->Begin = AddressRangeIter[1U]->Begin;
							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					}
				//IPv4
					else if (AddressRangeIter[0]->Begin.ss_family == AF_INET && AddressRangeIter[1U]->Begin.ss_family == AF_INET)
					{
					//A-Range is not same as B-Range.
						if (CompareAddresses(&((PSOCKADDR_IN)&AddressRangeIter[0]->End)->sin_addr, &((PSOCKADDR_IN)&AddressRangeIter[1U]->Begin)->sin_addr, AF_INET) == ADDRESS_COMPARE_LESS)
						{
							*IPv4NextAddress = ((PSOCKADDR_IN)&AddressRangeIter[0]->End)->sin_addr;

							//Check next address.
							IPv4NextAddress->S_un.S_addr = htonl(ntohl(IPv4NextAddress->S_un.S_addr) + 1U);
							if (IPv4NextAddress->S_un.S_addr == ((PSOCKADDR_IN)&AddressRangeIter[1U]->Begin)->sin_addr.S_un.S_addr)
							{
								AddressRangeIter[0]->End = AddressRangeIter[1U]->End;
								AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
								AddressRangeIter[0]--;
							}

							break;
						}
					//B-Range is not same as A-Range.
						else if (CompareAddresses(&((PSOCKADDR_IN)&AddressRangeIter[0]->Begin)->sin_addr, &((PSOCKADDR_IN)&AddressRangeIter[1U]->End)->sin_addr, AF_INET) == ADDRESS_COMPARE_GREATER)
						{
							*IPv4NextAddress = ((PSOCKADDR_IN)&AddressRangeIter[1U]->End)->sin_addr;

							//Check next address.
							IPv4NextAddress->S_un.S_addr = htonl(ntohl(IPv4NextAddress->S_un.S_addr) + 1U);
							if (IPv4NextAddress->S_un.S_addr == ((PSOCKADDR_IN)&AddressRangeIter[0]->Begin)->sin_addr.S_un.S_addr)
							{
								AddressRangeIter[0]->Begin = AddressRangeIter[1U]->Begin;
								AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
								AddressRangeIter[0]--;
							}

							break;
						}
					//A-Range is same as B-Range.
						else if (CompareAddresses(&((PSOCKADDR_IN)&AddressRangeIter[0]->Begin)->sin_addr, &((PSOCKADDR_IN)&AddressRangeIter[1U]->Begin)->sin_addr, AF_INET) == ADDRESS_COMPARE_EQUAL &&
							CompareAddresses(&((PSOCKADDR_IN)&AddressRangeIter[0]->End)->sin_addr, &((PSOCKADDR_IN)&AddressRangeIter[1U]->End)->sin_addr, AF_INET) == ADDRESS_COMPARE_EQUAL)
						{
							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					//A-Range connect B-Range or B-Range connect A-Range.
						else if (CompareAddresses(&((PSOCKADDR_IN)&AddressRangeIter[0]->End)->sin_addr, &((PSOCKADDR_IN)&AddressRangeIter[1U]->Begin)->sin_addr, AF_INET) == ADDRESS_COMPARE_EQUAL)
						{
							AddressRangeIter[0]->End = AddressRangeIter[1U]->End;
							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
						else if (CompareAddresses(&((PSOCKADDR_IN)&AddressRangeIter[1U]->End)->sin_addr, &((PSOCKADDR_IN)&AddressRangeIter[0]->Begin)->sin_addr, AF_INET) == ADDRESS_COMPARE_EQUAL)
						{
							AddressRangeIter[0]->Begin = AddressRangeIter[1U]->Begin;
							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					//A-Range include B-Range or B-Range include A-Range.
						else if (CompareAddresses(&((PSOCKADDR_IN)&AddressRangeIter[0]->Begin)->sin_addr, &((PSOCKADDR_IN)&AddressRangeIter[1U]->Begin)->sin_addr, AF_INET) == ADDRESS_COMPARE_LESS &&
							CompareAddresses(&((PSOCKADDR_IN)&AddressRangeIter[0]->End)->sin_addr, &((PSOCKADDR_IN)&AddressRangeIter[1U]->End)->sin_addr, AF_INET) == ADDRESS_COMPARE_GREATER)
						{
							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
						else if (CompareAddresses(&((PSOCKADDR_IN)&AddressRangeIter[0]->Begin)->sin_addr, &((PSOCKADDR_IN)&AddressRangeIter[1U]->Begin)->sin_addr, AF_INET) == ADDRESS_COMPARE_GREATER &&
							CompareAddresses(&((PSOCKADDR_IN)&AddressRangeIter[0]->End)->sin_addr, &((PSOCKADDR_IN)&AddressRangeIter[1U]->End)->sin_addr, AF_INET) == ADDRESS_COMPARE_LESS)
						{
							*AddressRangeIter[0] = *AddressRangeIter[1U];
							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					//A part of A-Range or B-Range is same as a part of B-Range or A-Range, also begin or end of A-Range or B-Range is same as begin or end of A-Range or B-Range.
						if (CompareAddresses(&((PSOCKADDR_IN)&AddressRangeIter[0]->Begin)->sin_addr, &((PSOCKADDR_IN)&AddressRangeIter[1U]->Begin)->sin_addr, AF_INET) == ADDRESS_COMPARE_EQUAL)
						{
							if (CompareAddresses(&((PSOCKADDR_IN)&AddressRangeIter[0]->End)->sin_addr, &((PSOCKADDR_IN)&AddressRangeIter[1U]->End)->sin_addr, AF_INET) == ADDRESS_COMPARE_LESS)
								*AddressRangeIter[0] = *AddressRangeIter[1U];

							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
						else if (CompareAddresses(&((PSOCKADDR_IN)&AddressRangeIter[0]->End)->sin_addr, &((PSOCKADDR_IN)&AddressRangeIter[1U]->End)->sin_addr, AF_INET) == ADDRESS_COMPARE_EQUAL)
						{
							if (CompareAddresses(&((PSOCKADDR_IN)&AddressRangeIter[0]->Begin)->sin_addr, &((PSOCKADDR_IN)&AddressRangeIter[1U]->Begin)->sin_addr, AF_INET) != ADDRESS_COMPARE_LESS)
								*AddressRangeIter[0] = *AddressRangeIter[1U];

							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					//A part of A-Range or B-Range is same as a part of B-Range or A-Range.
						else if (CompareAddresses(&((PSOCKADDR_IN)&AddressRangeIter[0]->End)->sin_addr, &((PSOCKADDR_IN)&AddressRangeIter[1U]->Begin)->sin_addr, AF_INET) == ADDRESS_COMPARE_GREATER)
						{
							AddressRangeIter[0]->End = AddressRangeIter[1U]->End;
							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
						else if (CompareAddresses(&((PSOCKADDR_IN)&AddressRangeIter[1U]->End)->sin_addr, &((PSOCKADDR_IN)&AddressRangeIter[0]->Begin)->sin_addr, AF_INET) == ADDRESS_COMPARE_GREATER)
						{
							AddressRangeIter[0]->Begin = AddressRangeIter[1U]->Begin;
							AddressRangeIter[0] = AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					}
				}
			}
*/
		//Swap(or cleanup) using list.
			AddressRangeModificating->shrink_to_fit();
			std::unique_lock<std::mutex> AddressRangeMutex(AddressRangeLock);
			AddressRangeUsing->swap(*AddressRangeModificating);
			AddressRangeMutex.unlock();
			AddressRangeModificating->clear();
			AddressRangeModificating->shrink_to_fit();
		}
	//AddressRange Table is empty.
		else {
			std::unique_lock<std::mutex> AddressRangeMutex(AddressRangeLock);
			AddressRangeUsing->clear();
			AddressRangeUsing->shrink_to_fit();
			AddressRangeMutex.unlock();
			AddressRangeModificating->clear();
			AddressRangeModificating->shrink_to_fit();
		}

	//Flush DNS cache.
		if (Parameter.FileHash)
			FlushDNSResolverCache();

	//Auto-refresh
		Sleep((DWORD)Parameter.FileRefreshTime);
	}

	PrintError(LOG_ERROR_SYSTEM, L"Read IPFilter module Monitor terminated", NULL, nullptr, NULL);
	return EXIT_SUCCESS;
}

//Read ipfilter data from files
size_t __fastcall ReadIPFilterData(const PSTR Buffer, const size_t FileIndex, const size_t Line, size_t &LabelType, bool &LabelComments)
{
	std::string Data(Buffer);

//Multi-line comments check, delete spaces, horizontal tab/HT, check comments(Number Sign/NS and double slashs) and check minimum length of ipfilter items.
	if (ReadMultiLineComments(Buffer, Data, LabelComments) == EXIT_FAILURE || Data.find(ASCII_HASHTAG) == 0 || Data.find(ASCII_SLASH) == 0)
		return EXIT_SUCCESS;

//[Local Routing] block(A part)
	if (LabelType == 0 && (Parameter.DNSTarget.Local_IPv4.AddressData.Storage.ss_family != NULL || Parameter.DNSTarget.Local_IPv6.AddressData.Storage.ss_family != NULL) && 
		(IPFilterFileList[FileIndex].FileName.rfind(L"Routing.txt") != std::wstring::npos && IPFilterFileList[FileIndex].FileName.length() > wcslen(L"Routing.txt") && 
		IPFilterFileList[FileIndex].FileName.rfind(L"Routing.txt") == IPFilterFileList[FileIndex].FileName.length() - wcslen(L"Routing.txt") || 
		IPFilterFileList[FileIndex].FileName.rfind(L"Route.txt") != std::wstring::npos && IPFilterFileList[FileIndex].FileName.length() > wcslen(L"Route.txt") && 
		IPFilterFileList[FileIndex].FileName.rfind(L"Route.txt") == IPFilterFileList[FileIndex].FileName.length() - wcslen(L"Route.txt") || 
		IPFilterFileList[FileIndex].FileName.rfind(L"chnrouting.txt") != std::wstring::npos && IPFilterFileList[FileIndex].FileName.length() > wcslen(L"chnrouting.txt") && 
		IPFilterFileList[FileIndex].FileName.rfind(L"chnrouting.txt") == IPFilterFileList[FileIndex].FileName.length() - wcslen(L"chnrouting.txt") || 
		IPFilterFileList[FileIndex].FileName.rfind(L"chnroute.txt") != std::wstring::npos && IPFilterFileList[FileIndex].FileName.length() > wcslen(L"chnroute.txt") && 
		IPFilterFileList[FileIndex].FileName.rfind(L"chnroute.txt") == IPFilterFileList[FileIndex].FileName.length() - wcslen(L"chnroute.txt")))
			LabelType = LABEL_IPFILTER_LOCAL_ROUTING;

//[IPFilter] block
	if (Data.find("[IPFilter]") == 0 || Data.find("[IPfilter]") == 0 || Data.find("[ipfilter]") == 0)
	{
		LabelType = LABEL_IPFILTER;
		return EXIT_SUCCESS;
	}

//[Blacklist] block(A part)
	else if (Data.find("[BlackList]") == 0 || Data.find("[Blacklist]") == 0 || Data.find("[blacklist]") == 0)
	{
		LabelType = LABEL_IPFILTER_BLACKLIST;
		return EXIT_SUCCESS;
	}

//[Local Routing] block(B part)
	else if (Data.find("[Local Routing]") == 0 || Data.find("[Local routing]") == 0 || Data.find("[local routing]") == 0)
	{
		LabelType = LABEL_IPFILTER_LOCAL_ROUTING;
		return EXIT_SUCCESS;
	}

//[Base] block
	else if (Data.find("[Base]") == 0 || Data.find("[base]") == 0 || Data.find("Version = ") == 0 || Data.find("version = ") == 0)
	{
/* Old version(2015-01-12)
		if (Data.length() < strlen("Version = ") + 8U)
		{
			double ReadVersion = atof(Data.c_str() + strlen("Version = "));
			if (ReadVersion > IPFILTER_VERSION)
			{
				PrintError(LOG_ERROR_IPFILTER, L"IPFilter file version error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
*/
		return EXIT_SUCCESS;
	}

//Temporary stop read.
	else if (Data.find("[Stop]") == 0 || Data.find("[stop]") == 0)
	{
		LabelType = LABEL_STOP;
		return EXIT_SUCCESS;
	}
	if (LabelType == LABEL_STOP)
		return EXIT_SUCCESS;

//[Blacklist] block(B part)
	if (LabelType == LABEL_IPFILTER && Data.find(ASCII_MINUS) == std::string::npos)
	{
		PrintError(LOG_ERROR_IPFILTER, L"Item format error", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
		return EXIT_FAILURE;
	}

//Multi-line comments check, delete comments(Number Sign/NS and double slashs) and check minimum length of hosts items.
	else if (Data.rfind(" //") != std::string::npos)
	{
		Data.erase(Data.rfind(" //"), Data.length() - Data.rfind(" //"));
	}
	else if (Data.rfind("	//") != std::string::npos)
	{
		Data.erase(Data.rfind("	//"), Data.length() - Data.rfind("	//"));
	}
	else if (Data.rfind(" #") != std::string::npos)
	{
		Data.erase(Data.rfind(" #"), Data.length() - Data.rfind(" #"));
	}
	else if (Data.rfind("	#") != std::string::npos)
	{
		Data.erase(Data.rfind("	#"), Data.length() - Data.rfind("	#"));
	}

//Blacklist items
	if (Parameter.Blacklist && LabelType == LABEL_IPFILTER_BLACKLIST)
		return ReadBlacklistData(Data, FileIndex, Line);

//Local Routing items
	else if (Parameter.LocalRouting && LabelType == LABEL_IPFILTER_LOCAL_ROUTING)
		return ReadLocalRoutingData(Data, FileIndex, Line);

//Main IPFilter items
	else if (Parameter.OperationMode == LISTEN_CUSTOMMODE && LabelType == LABEL_IPFILTER)
		return ReadMainIPFilterData(Data, FileIndex, Line);

	return EXIT_SUCCESS;
}

//Read Blacklist items in IPFilter file from data
inline size_t __fastcall ReadBlacklistData(std::string Data, const size_t FileIndex, const size_t Line)
{
//Initialization
	AddressRange AddressRangeTemp;
	size_t Index = 0, Separated = 0;
	SSIZE_T Result = 0;

//Delete spaces and horizontal tab/HT before data.
	while (!Data.empty() && (Data[0] == ASCII_HT || Data[0] == ASCII_SPACE))
		Data.erase(0, 1U);

//Delete spaces and horizontal tab/HT before or after verticals.
	while (Data.find("	|") != std::string::npos || Data.find("|	") != std::string::npos || 
			Data.find(" |") != std::string::npos || Data.find("| ") != std::string::npos)
	{
		if (Data.find("	|") != std::string::npos)
			Data.erase(Data.find("	|"), 1U);
		if (Data.find("|	") != std::string::npos)
			Data.erase(Data.find("|	") + 1U, 1U);
		if (Data.find(" |") != std::string::npos)
			Data.erase(Data.find(" |"), 1U);
		if (Data.find("| ") != std::string::npos)
			Data.erase(Data.find("| ") + 1U, 1U);
	}

//Mark separated location.
	if (Data.find(ASCII_COMMA) != std::string::npos)
	{
	//Delete spaces and horizontal tab/HT before commas.
		while (Data.find("	,") != std::string::npos || Data.find(" ,") != std::string::npos)
		{
			if (Data.find("	,") != std::string::npos)
				Data.erase(Data.find("	,"), 1U);
			if (Data.find(" ,") != std::string::npos)
				Data.erase(Data.find(" ,"), 1U);
		}

	//Delete spaces and horizontal tab/HT after commas.
		while (Data.find(ASCII_HT) != std::string::npos && Data.find(ASCII_HT) > Data.find(ASCII_COMMA))
			Data.erase(Data.find(ASCII_HT), 1U);
		while (Data.find(ASCII_SPACE) != std::string::npos && Data.find(ASCII_SPACE) > Data.find(ASCII_COMMA))
			Data.erase(Data.find(ASCII_SPACE), 1U);

	//Common format
		if (Data.find(ASCII_HT) != std::string::npos && Data.find(ASCII_SPACE) != std::string::npos)
		{
			if (Data.find(ASCII_HT) < Data.find(ASCII_SPACE))
				Separated = Data.find(ASCII_HT);
			else
				Separated = Data.find(ASCII_SPACE);
		}
		else if (Data.find(ASCII_HT) != std::string::npos)
		{
			Separated = Data.find(ASCII_HT);
		}
		else if (Data.find(ASCII_SPACE) != std::string::npos)
		{
			Separated = Data.find(ASCII_SPACE);
		}
	//Comma-Separated Values/CSV, RFC 4180(https://tools.ietf.org/html/rfc4180), Common Format and MIME Type for Comma-Separated Values (CSV) Files).
		else {
			Separated = Data.find(ASCII_COMMA);
			Data.erase(Separated, 1U);
		}
	}
//Common format
	else {
		if (Data.find(ASCII_HT) != std::string::npos && Data.find(ASCII_SPACE) != std::string::npos)
		{
			if (Data.find(ASCII_HT) < Data.find(ASCII_SPACE))
				Separated = Data.find(ASCII_HT);
			else
				Separated = Data.find(ASCII_SPACE);
		}
		else if (Data.find(ASCII_HT) != std::string::npos)
		{
			Separated = Data.find(ASCII_HT);
		}
		else if (Data.find(ASCII_SPACE) != std::string::npos)
		{
			Separated = Data.find(ASCII_SPACE);
		}
		else {
			PrintError(LOG_ERROR_IPFILTER, L"Item format error", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
			return EXIT_FAILURE;
		}
	}

//Delete spaces and horizontal tab/HT.
	while (Data.find(ASCII_HT) != std::string::npos)
		Data.erase(Data.find(ASCII_HT), 1U);
	while (Data.find(ASCII_SPACE) != std::string::npos)
		Data.erase(Data.find(ASCII_SPACE), 1U);

//String length check.
	if (Data.length() < READ_IPFILTER_BLACKLIST_MINSIZE ||
		Data.find(ASCII_MINUS) != std::string::npos && Data.find(ASCII_VERTICAL) != std::string::npos &&
		Data.find(ASCII_MINUS) < Separated && Data.find(ASCII_VERTICAL) < Separated && Data.find(ASCII_MINUS) < Data.find(ASCII_VERTICAL))
	{
		PrintError(LOG_ERROR_IPFILTER, L"Item format error", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
		return EXIT_FAILURE;
	}

	ResultBlacklistTable ResultBlacklistTableTemp;
	std::shared_ptr<char> Addr(new char[ADDR_STRING_MAXSIZE]());
//Single address
	if (Data.find(ASCII_VERTICAL) == std::string::npos)
	{
	//AAAA Records(IPv6)
		if (Data.find(ASCII_COLON) < Separated)
		{
		//IPv6 addresses check
			if (Separated > ADDR_STRING_MAXSIZE)
			{
				PrintError(LOG_ERROR_IPFILTER, L"IPv6 Address format error", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
				return EXIT_FAILURE;
			}
			else if (Data[0] < ASCII_ZERO || Data[0] > ASCII_COLON && Data[0] < ASCII_UPPERCASE_A || Data[0] > ASCII_UPPERCASE_F && Data[0] < ASCII_LOWERCASE_A || Data[0] > ASCII_LOWERCASE_F)
			{
				return EXIT_FAILURE;
			}

		//Address range format
			if (Data.find(ASCII_MINUS) != std::string::npos && Data.find(ASCII_MINUS) < Separated)
			{
			//Convert address(Begin).
				memcpy(Addr.get(), Data.c_str(), Data.find(ASCII_MINUS));
				if (AddressStringToBinary(Addr.get(), &((PSOCKADDR_IN6)&AddressRangeTemp.Begin)->sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_IPFILTER, L"IPv6 Address format error", Result, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
					return EXIT_FAILURE;
				}
				AddressRangeTemp.Begin.ss_family = AF_INET6;

			//Convert address(End).
				memcpy(Addr.get(), Data.c_str() + Data.find(ASCII_MINUS) + 1U, Separated - Data.find(ASCII_MINUS) - 1U);
				if (AddressStringToBinary(Addr.get(), &((PSOCKADDR_IN6)&AddressRangeTemp.End)->sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_IPFILTER, L"IPv6 Address format error", Result, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
					return EXIT_FAILURE;
				}
				AddressRangeTemp.End.ss_family = AF_INET6;

			//Check address range.
				if (CompareAddresses(&((PSOCKADDR_IN6)&AddressRangeTemp.Begin)->sin6_addr, &((PSOCKADDR_IN6)&AddressRangeTemp.End)->sin6_addr, AF_INET6) > ADDRESS_COMPARE_EQUAL)
				{
					PrintError(LOG_ERROR_IPFILTER, L"IPv6 Address range error", WSAGetLastError(), (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
					return EXIT_FAILURE;
				}
			}
		//Normal format
			else {
			//Convert address.
				memcpy(Addr.get(), Data.c_str(), Separated);
				if (AddressStringToBinary(Addr.get(), &((PSOCKADDR_IN6)&AddressRangeTemp.Begin)->sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_IPFILTER, L"IPv6 Address format error", Result, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
					return EXIT_FAILURE;
				}

			//Check repeating items.
				if (CheckSpecialAddress(&((PSOCKADDR_IN6)&AddressRangeTemp.Begin)->sin6_addr, AF_INET6, nullptr))
				{
					PrintError(LOG_ERROR_IPFILTER, L"Repeating items error, this item is not available", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
					return EXIT_FAILURE;
				}

				AddressRangeTemp.Begin.ss_family = AF_INET6;
				AddressRangeTemp.End.ss_family = AF_INET6;
				((PSOCKADDR_IN6)&AddressRangeTemp.End)->sin6_addr = ((PSOCKADDR_IN6)&AddressRangeTemp.Begin)->sin6_addr;
			}

			ResultBlacklistTableTemp.Addresses.push_back(AddressRangeTemp);
		}
	//A Records(IPv4)
		else {
		//IPv4 address check
			if (Separated > ADDR_STRING_MAXSIZE)
			{
				PrintError(LOG_ERROR_IPFILTER, L"IPv4 Address format error", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
				return EXIT_FAILURE;
			}
			else if (Data[0] < ASCII_ZERO || Data[0] > ASCII_NINE)
			{
				return EXIT_FAILURE;
			}

		//Address range format
			if (Data.find(ASCII_MINUS) != std::string::npos && Data.find(ASCII_MINUS) < Separated)
			{
			//Convert address(Begin).
				memcpy(Addr.get(), Data.c_str(), Data.find(ASCII_MINUS));
				if (AddressStringToBinary(Addr.get(), &((PSOCKADDR_IN)&AddressRangeTemp.Begin)->sin_addr, AF_INET, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_IPFILTER, L"IPv4 Address format error", Result, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
					return EXIT_FAILURE;
				}
				AddressRangeTemp.Begin.ss_family = AF_INET;

			//Convert address(End).
				memcpy(Addr.get(), Data.c_str() + Data.find(ASCII_MINUS) + 1U, Separated - Data.find(ASCII_MINUS) - 1U);
				if (AddressStringToBinary(Addr.get(), &((PSOCKADDR_IN)&AddressRangeTemp.End)->sin_addr, AF_INET, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_IPFILTER, L"IPv4 Address format error", Result, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
					return EXIT_FAILURE;
				}
				AddressRangeTemp.End.ss_family = AF_INET;

			//Check address range.
				if (CompareAddresses(&((PSOCKADDR_IN)&AddressRangeTemp.Begin)->sin_addr, &((PSOCKADDR_IN)&AddressRangeTemp.End)->sin_addr, AF_INET) > ADDRESS_COMPARE_EQUAL)
				{
					PrintError(LOG_ERROR_IPFILTER, L"IPv4 Address range error", WSAGetLastError(), (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
					return EXIT_FAILURE;
				}
			}
		//Normal format
			else {
			//Convert address.
				memcpy(Addr.get(), Data.c_str(), Separated);
				if (AddressStringToBinary(Addr.get(), &((PSOCKADDR_IN)&AddressRangeTemp.Begin)->sin_addr, AF_INET, Result) == EXIT_FAILURE)
				{
					PrintError(LOG_ERROR_IPFILTER, L"IPv4 Address format error", Result, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
					return EXIT_FAILURE;
				}

			//Check repeating items.
				if (CheckSpecialAddress(&((PSOCKADDR_IN)&AddressRangeTemp.Begin)->sin_addr, AF_INET, nullptr))
				{
					PrintError(LOG_ERROR_IPFILTER, L"Repeating items error, this item is not available", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
					return EXIT_FAILURE;
				}

				AddressRangeTemp.Begin.ss_family = AF_INET;
				AddressRangeTemp.End.ss_family = AF_INET;
				((PSOCKADDR_IN)&AddressRangeTemp.End)->sin_addr = ((PSOCKADDR_IN)&AddressRangeTemp.Begin)->sin_addr;
			}

			ResultBlacklistTableTemp.Addresses.push_back(AddressRangeTemp);
		}
	}
//Multiple Addresses
	else {
		size_t VerticalIndex = 0;

	//AAAA Records(IPv6)
		if (Data.find(ASCII_COLON) < Separated)
		{
		//IPv6 addresses check
			if (Data[0] < ASCII_ZERO || Data[0] > ASCII_COLON && Data[0] < ASCII_UPPERCASE_A || Data[0] > ASCII_UPPERCASE_F && Data[0] < ASCII_LOWERCASE_A || Data[0] > ASCII_LOWERCASE_F)
				return EXIT_FAILURE;

			for (Index = 0;Index <= Separated;Index++)
			{
			//Read data.
				if (Data[Index] == ASCII_VERTICAL || Index == Separated)
				{
				//Length check
					if (Index - VerticalIndex > ADDR_STRING_MAXSIZE)
					{
						PrintError(LOG_ERROR_IPFILTER, L"IPv6 Address format error", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
						return EXIT_FAILURE;
					}

				//Convert addresses.
					memset(Addr.get(), 0, ADDR_STRING_MAXSIZE);
					memcpy(Addr.get(), Data.c_str() + VerticalIndex, Index - VerticalIndex);
					if (AddressStringToBinary(Addr.get(), &((PSOCKADDR_IN6)&AddressRangeTemp.Begin)->sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
					{
						PrintError(LOG_ERROR_IPFILTER, L"IPv6 Address format error", Result, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
						return EXIT_FAILURE;
					}

				//Check repeating items.
					if (CheckSpecialAddress(&((PSOCKADDR_IN6)&AddressRangeTemp.Begin)->sin6_addr, AF_INET6, nullptr))
					{
						PrintError(LOG_ERROR_IPFILTER, L"Repeating items error, this item is not available", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
						return EXIT_FAILURE;
					}

					AddressRangeTemp.Begin.ss_family = AF_INET6;
					AddressRangeTemp.End.ss_family = AF_INET6;
					((PSOCKADDR_IN6)&AddressRangeTemp.End)->sin6_addr = ((PSOCKADDR_IN6)&AddressRangeTemp.Begin)->sin6_addr;
					ResultBlacklistTableTemp.Addresses.push_back(AddressRangeTemp);
					memset(&AddressRangeTemp, 0, sizeof(AddressRange));
					VerticalIndex = Index + 1U;
				}
			}
		}
	//A Records(IPv4)
		else {
		//IPv4 addresses check
			if (Data[0] < ASCII_ZERO || Data[0] > ASCII_NINE)
				return EXIT_FAILURE;

			for (Index = 0;Index <= Separated;Index++)
			{
			//Read data.
				if (Data[Index] == ASCII_VERTICAL || Index == Separated)
				{
				//Length check
					if (Index - VerticalIndex > ADDR_STRING_MAXSIZE)
					{
						PrintError(LOG_ERROR_IPFILTER, L"IPv4 Address format error", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
						return EXIT_FAILURE;
					}

				//Convert addresses.
					memset(Addr.get(), 0, ADDR_STRING_MAXSIZE);
					memcpy(Addr.get(), Data.c_str() + VerticalIndex, Index - VerticalIndex);
					if (AddressStringToBinary(Addr.get(), &((PSOCKADDR_IN)&AddressRangeTemp.Begin)->sin_addr, AF_INET, Result) == EXIT_FAILURE)
					{
						PrintError(LOG_ERROR_IPFILTER, L"IPv4 Address format error", Result, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
						return EXIT_FAILURE;
					}

				//Check repeating items.
					if (CheckSpecialAddress(&((PSOCKADDR_IN)&AddressRangeTemp.Begin)->sin_addr, AF_INET, nullptr))
					{
						PrintError(LOG_ERROR_IPFILTER, L"Repeating items error, this item is not available", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
						return EXIT_FAILURE;
					}

					AddressRangeTemp.Begin.ss_family = AF_INET;
					AddressRangeTemp.End.ss_family = AF_INET;
					((PSOCKADDR_IN)&AddressRangeTemp.End)->sin_addr = ((PSOCKADDR_IN)&AddressRangeTemp.Begin)->sin_addr;
					ResultBlacklistTableTemp.Addresses.push_back(AddressRangeTemp);
					memset(&AddressRangeTemp, 0, sizeof(AddressRange));
					VerticalIndex = Index + 1U;
				}
			}
		}
	}

/* Old version(2014-12-28)
//Check repeating items.
	if (ResultBlacklistTableTemp.Addresses.front().End.ss_family == 0)
	{
		for (auto SockAddrIter = ResultBlacklistTableTemp.Addresses.begin();SockAddrIter != ResultBlacklistTableTemp.Addresses.end();SockAddrIter++)
		{
			if (SockAddrIter != ResultBlacklistTableTemp.Addresses.end() - 1U && !ResultBlacklistTableTemp.Addresses.empty())
			{
			//IPv6
				if (ResultBlacklistTableTemp.Addresses.front().Begin.ss_family == AF_INET6)
				{
					for (auto InnerSockAddrIter = SockAddrIter + 1U;InnerSockAddrIter != ResultBlacklistTableTemp.Addresses.end();InnerSockAddrIter++)
					{
						if (memcmp(&((PSOCKADDR_IN6)&SockAddrIter)->sin6_addr, &((PSOCKADDR_IN6)&InnerSockAddrIter)->sin6_addr, sizeof(in6_addr)) == 0)
						{
							InnerSockAddrIter = ResultBlacklistTableTemp.Addresses.erase(InnerSockAddrIter);

						//End of checking
							if (InnerSockAddrIter == ResultBlacklistTableTemp.Addresses.end())
								goto StopLoop;
							break;
						}
					}
				}
			//IPv4
				else {
					for (auto InnerSockAddrIter = SockAddrIter + 1U;InnerSockAddrIter != ResultBlacklistTableTemp.Addresses.end();InnerSockAddrIter++)
					{
						if (((PSOCKADDR_IN)&SockAddrIter)->sin_addr.S_un.S_addr == ((PSOCKADDR_IN)&InnerSockAddrIter)->sin_addr.S_un.S_addr)
						{
							InnerSockAddrIter = ResultBlacklistTableTemp.Addresses.erase(InnerSockAddrIter);

						//End of checking
							if (InnerSockAddrIter == ResultBlacklistTableTemp.Addresses.end())
								goto StopLoop;
							break;
						}
					}
				}
			}
		}
	}

//Stop loop.
	StopLoop: 
*/
//Mark patterns.
	ResultBlacklistTableTemp.PatternString.append(Data, Separated, Data.length() - Separated);

//Block those IP addresses from all requesting.
	if (ResultBlacklistTableTemp.PatternString == ("ALL") || ResultBlacklistTableTemp.PatternString == ("All") || ResultBlacklistTableTemp.PatternString == ("all"))
	{
		ResultBlacklistTableTemp.PatternString.clear();
		ResultBlacklistTableTemp.PatternString.shrink_to_fit();
	}
	else { //Other requesting
		try {
			std::regex PatternTemp(ResultBlacklistTableTemp.PatternString /* , std::regex_constants::extended */);
			ResultBlacklistTableTemp.Pattern.swap(PatternTemp);
		}
		catch (std::regex_error)
		{
			PrintError(LOG_ERROR_IPFILTER, L"Regular expression pattern error", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
			return EXIT_FAILURE;
		}
	}

//Add to global ResultBlacklistTable.
	ResultBlacklistTableTemp.FileIndex = FileIndex;
	ResultBlacklistModificating->push_back(ResultBlacklistTableTemp);
	return EXIT_SUCCESS;
}

//Read Local Routing items in IPFilter file from data
inline size_t __fastcall ReadLocalRoutingData(std::string Data, const size_t FileIndex, const size_t Line)
{
//Initialization
	AddressPrefixBlock AddressPrefixBlockTemp;

//Main check
	while (Data.find(ASCII_HT) != std::string::npos)
		Data.erase(Data.find(ASCII_HT), 1U);
	while (Data.find(ASCII_SPACE) != std::string::npos)
		Data.erase(Data.find(ASCII_SPACE), 1U);
	if (Data.length() < READ_IPFILTER_LOCAL_ROUTING_MINSIZE)
		return EXIT_SUCCESS;

//Check format of items.
	if (Data.find("/") == std::string::npos || Data.rfind("/") < 3U || Data.rfind("/") == Data.length() - 1U)
	{
		PrintError(LOG_ERROR_PARAMETER, L"Routing address block format error", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
		return EXIT_FAILURE;
	}
	for (auto StringIter:Data)
	{
		if (StringIter < ASCII_PERIOD || StringIter > ASCII_COLON &&
			StringIter < ASCII_UPPERCASE_A || StringIter > ASCII_UPPERCASE_F && StringIter < ASCII_LOWERCASE_A || StringIter > ASCII_LOWERCASE_F)
		{
			PrintError(LOG_ERROR_PARAMETER, L"Routing address block format error", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
			return EXIT_FAILURE;
		}
	}

	std::shared_ptr<char> Address(new char[ADDR_STRING_MAXSIZE]());
	SSIZE_T Result = 0;

	memcpy(Address.get(), Data.c_str(), Data.find("/"));
//IPv6
	if (Data.find(":") != std::string::npos) 
	{
		Data.erase(0, Data.find("/") + 1U);

	//Convert address.
		if (AddressStringToBinary((PSTR)Address.get(), &AddressPrefixBlockTemp.AddressData.IPv6.sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
		{
			PrintError(LOG_ERROR_IPFILTER, L"IPv6 Address format error", Result, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
			return EXIT_FAILURE;
		}

	//Mark network prefix.
		Result = strtol(Data.c_str(), nullptr, NULL);
		if (Result <= 0 || Result > (SSIZE_T)(sizeof(in6_addr) * BYTES_TO_BITS))
		{
			PrintError(LOG_ERROR_IPFILTER, L"IPv6 Prefix error", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
			return EXIT_FAILURE;
		}
		else {
			AddressPrefixBlockTemp.Prefix = (size_t)Result;
		}

		AddressPrefixBlockTemp.AddressData.Storage.ss_family = AF_INET6;
	}
//IPv4
	else {
		Data.erase(0, Data.find("/") + 1U);

	//Convert address.
		if (AddressStringToBinary((PSTR)Address.get(), &AddressPrefixBlockTemp.AddressData.IPv4.sin_addr, AF_INET, Result) == EXIT_FAILURE)
		{
			PrintError(LOG_ERROR_IPFILTER, L"IPv4 Address format error", Result, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
			return EXIT_FAILURE;
		}

	//Mark network prefix.
		Result = strtol(Data.c_str(), nullptr, NULL);
		if (Result <= 0 || Result > (SSIZE_T)(sizeof(in_addr) * BYTES_TO_BITS))
		{
			PrintError(LOG_ERROR_IPFILTER, L"IPv4 Prefix error", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
			return EXIT_FAILURE;
		}
		else {
			AddressPrefixBlockTemp.Prefix = (size_t)Result;
		}

		AddressPrefixBlockTemp.AddressData.Storage.ss_family = AF_INET;
	}

//Add to global LocalRoutingTable.
	AddressPrefixBlockTemp.FileIndex = FileIndex;
	LocalRoutingListModificating->push_back(AddressPrefixBlockTemp);
	return EXIT_SUCCESS;
}

//Read Main IPFilter items in IPFilter file from data
inline size_t __fastcall ReadMainIPFilterData(std::string Data, const size_t FileIndex, const size_t Line)
{
//Initialization
	AddressRange AddressRangeTemp;
	SSIZE_T Result = 0;
	size_t Index = 0;

//Main check
	while (Data.find(ASCII_HT) != std::string::npos)
		Data.erase(Data.find(ASCII_HT), 1U);
	while (Data.find(ASCII_SPACE) != std::string::npos)
		Data.erase(Data.find(ASCII_SPACE), 1U);
	if (Data.length() < READ_IPFILTER_MINSIZE)
		return EXIT_SUCCESS;

//Delete spaces, horizontal tab/HT before data.
	while (!Data.empty() && (Data[0] == ASCII_HT || Data[0] == ASCII_SPACE))
		Data.erase(0, 1U);

//Check format of items.
	if (Data.find(ASCII_COMMA) != std::string::npos && Data.find(ASCII_COMMA) > Data.find(ASCII_MINUS)) //IPFilter.dat
	{
	//IPv4 spacial delete
		if (Data.find(ASCII_PERIOD) != std::string::npos)
		{
		//Delete all zeros before data.
			for (Index = 0;Index < Data.find(ASCII_MINUS);Index++)
			{
				if (Data[Index] == ASCII_ZERO)
				{
					Data.erase(Index, 1U);
					Index--;
				}
				else {
					break;
				}
			}

		//Delete all zeros before minus or after commas in addresses range.
			while (Data.find(".0") != std::string::npos)
				Data.replace(Data.find(".0"), strlen(".0"), ("."));
			while (Data.find("-0") != std::string::npos)
				Data.replace(Data.find("-0"), strlen("-0"), ("-"));
			while (Data.find("..") != std::string::npos)
				Data.replace(Data.find(".."), strlen(".."), (".0."));
			if (Data.find(".-") != std::string::npos)
				Data.replace(Data.find(".-"), strlen(".-"), (".0-"));
			if (Data.find("-.") != std::string::npos)
				Data.replace(Data.find("-."), strlen("-."), ("-0."));
			if (Data[0] == ASCII_PERIOD)
				Data.replace(0, 1U, ("0."));
		}

	//Delete all zeros before minus or after commas in ipfilter level.
		while (Data.find(",000,") != std::string::npos)
			Data.replace(Data.find(",000,"), strlen(",000,"), (",0,"));
		while (Data.find(",00,") != std::string::npos)
			Data.replace(Data.find(",00,"), strlen(",00,"), (",0,"));
		while (Data.find(",00") != std::string::npos)
			Data.replace(Data.find(",00"), strlen(",00"), (","));
		if (Data.find(",0") != std::string::npos && Data[Data.find(",0") + 2U] != ASCII_COMMA)
			Data.replace(Data.find(",0"), strlen(",0"), (","));

	//Mark ipfilter level.
		std::shared_ptr<char> Level(new char[ADDR_STRING_MAXSIZE]());
		memcpy(Level.get(), Data.c_str() + Data.find(ASCII_COMMA) + 1U, Data.find(ASCII_COMMA, Data.find(ASCII_COMMA) + 1U) - Data.find(ASCII_COMMA) - 1U);
		Result = strtol(Level.get(), nullptr, NULL);
		if (Result >= 0 && Result <= U16_MAXNUM)
		{
			AddressRangeTemp.Level = (size_t)Result;
		}
		else {
			PrintError(LOG_ERROR_IPFILTER, L"Level error", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
			return EXIT_FAILURE;
		}

	//Delete all data except addresses range.
		Data.erase(Data.find(ASCII_COMMA));
		if (Data[Data.length() - 1U] == ASCII_PERIOD)
			Data.append("0");
	}
//PeerGuardian Text Lists(P2P) Format(Guarding.P2P), also a little part of IPFilter.dat without level.
	else {
	//IPv4 IPFilter.dat data without level
		if (Data.find(ASCII_COLON) == std::string::npos)
		{
		//Delete all zeros before data.
			for (Index = 0;Index < Data.find(ASCII_MINUS);Index++)
			{
				if (Data[Index] == ASCII_ZERO)
				{
					Data.erase(Index, 1U);
					Index--;
				}
				else {
					break;
				}
			}

		//Delete all zeros before minus or after commas in addresses range.
			while (Data.find(".0") != std::string::npos)
				Data.replace(Data.find(".0"), strlen(".0"), ("."));
			while (Data.find("-0") != std::string::npos)
				Data.replace(Data.find("-0"), strlen("-0"), ("-"));
			while (Data.find("..") != std::string::npos)
				Data.replace(Data.find(".."), strlen(".."), (".0."));
			if (Data.find(".-") != std::string::npos)
				Data.replace(Data.find(".-"), strlen(".-"), (".0-"));
			if (Data.find("-.") != std::string::npos)
				Data.replace(Data.find("-."), strlen("-."), ("-0."));
			if (Data[0] == ASCII_PERIOD)
				Data.replace(0, 1U, ("0."));
			if (Data[Data.length() - 1U] == ASCII_PERIOD)
				Data.append("0");
		}
		else {
		//PeerGuardian Text Lists(P2P) Format(Guarding.P2P)
			if (Data.find(ASCII_COLON) == Data.rfind(ASCII_COLON))
			{
				Data.erase(0, Data.find(ASCII_COLON) + 1U);

			//Delete all zeros before data.
				for (Index = 0;Index < Data.find(ASCII_MINUS);Index++)
				{
					if (Data[Index] == ASCII_ZERO)
					{
						Data.erase(Index, 1U);
						Index--;
					}
					else {
						break;
					}
				}

			//Delete all zeros before minus or after commas in addresses range.
				while (Data.find(".0") != std::string::npos)
					Data.replace(Data.find(".0"), strlen(".0"), ("."));
				while (Data.find("-0") != std::string::npos)
					Data.replace(Data.find("-0"), strlen("-0"), ("-"));
				while (Data.find("..") != std::string::npos)
					Data.replace(Data.find(".."), strlen(".."), (".0."));
				if (Data.find(".-") != std::string::npos)
					Data.replace(Data.find(".-"), strlen(".-"), (".0-"));
				if (Data.find("-.") != std::string::npos)
					Data.replace(Data.find("-."), strlen("-."), ("-0."));
				if (Data[0] == ASCII_PERIOD)
					Data.replace(0, 1U, ("0."));
				if (Data[Data.length() - 1U] == ASCII_PERIOD)
					Data.append("0");
			}
//			else { 
//				//IPv6 IPFilter.dat data without level
//			}
		}
	}

//Read data
	memset(&AddressRangeTemp, 0, sizeof(AddressRange));
	std::shared_ptr<char> Address(new char[ADDR_STRING_MAXSIZE]());
	if (Data.find(ASCII_COLON) != std::string::npos) //IPv6
	{
	//Begin address
		AddressRangeTemp.Begin.ss_family = AF_INET6;
		memcpy(Address.get(), Data.c_str(), Data.find(ASCII_MINUS));
		if (AddressStringToBinary((PSTR)Address.get(), &((PSOCKADDR_IN6)&AddressRangeTemp.Begin)->sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
		{
			PrintError(LOG_ERROR_IPFILTER, L"IPv6 Address format error", Result, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
			return EXIT_FAILURE;
		}
		memset(Address.get(), 0, ADDR_STRING_MAXSIZE);

	//End address
		AddressRangeTemp.End.ss_family = AF_INET6;
		memcpy(Address.get(), Data.c_str() + Data.find(ASCII_MINUS) + 1U, Data.length() - Data.find(ASCII_MINUS));
		if (AddressStringToBinary((PSTR)Address.get(), &((PSOCKADDR_IN6)&AddressRangeTemp.End)->sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
		{
			PrintError(LOG_ERROR_IPFILTER, L"IPv6 Address format error", Result, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
			return EXIT_FAILURE;
		}
		Address.reset();

	//Check address range.
		if (CompareAddresses(&((PSOCKADDR_IN6)&AddressRangeTemp.Begin)->sin6_addr, &((PSOCKADDR_IN6)&AddressRangeTemp.End)->sin6_addr, AF_INET6) > ADDRESS_COMPARE_EQUAL)
		{
			PrintError(LOG_ERROR_IPFILTER, L"IPv6 Address range error", WSAGetLastError(), (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
			return EXIT_FAILURE;
		}
	}
//IPv4
	else {
	//Begin address
		AddressRangeTemp.Begin.ss_family = AF_INET;
		memcpy(Address.get(), Data.c_str(), Data.find(ASCII_MINUS));
		if (AddressStringToBinary((PSTR)Address.get(), &((PSOCKADDR_IN)&AddressRangeTemp.Begin)->sin_addr, AF_INET, Result) == EXIT_FAILURE)
		{
			PrintError(LOG_ERROR_IPFILTER, L"IPv4 Address format error", Result, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
			return EXIT_FAILURE;
		}
		memset(Address.get(), 0, ADDR_STRING_MAXSIZE);

	//End address
		AddressRangeTemp.End.ss_family = AF_INET;
		memcpy(Address.get(), Data.c_str() + Data.find(ASCII_MINUS) + 1U, Data.length() - Data.find(ASCII_MINUS));
		if (AddressStringToBinary((PSTR)Address.get(), &((PSOCKADDR_IN)&AddressRangeTemp.End)->sin_addr, AF_INET, Result) == EXIT_FAILURE)
		{
			PrintError(LOG_ERROR_IPFILTER, L"IPv4 Address format error", Result, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
			return EXIT_FAILURE;
		}
		Address.reset();

	//Check address range.
		if (CompareAddresses(&((PSOCKADDR_IN)&AddressRangeTemp.Begin)->sin_addr, &((PSOCKADDR_IN)&AddressRangeTemp.End)->sin_addr, AF_INET) > ADDRESS_COMPARE_EQUAL)
		{
			PrintError(LOG_ERROR_IPFILTER, L"IPv4 Address range error", NULL, (PWSTR)IPFilterFileList[FileIndex].FileName.c_str(), Line);
			return EXIT_FAILURE;
		}
	}

//Add to global AddressRangeTable.
	AddressRangeTemp.FileIndex = FileIndex;
	AddressRangeModificating->push_back(AddressRangeTemp);
	return EXIT_SUCCESS;
}

//Read hosts from file
size_t __fastcall ReadHosts(void)
{
/* Old version(2015-01-03)
//Initialization(Available when file hash check is ON.)
	std::shared_ptr<char> Buffer;
	FileData FileDataTemp[5U];
	if (Parameter.FileHash)
	{
		std::shared_ptr<char> FileDataBufferTemp(new char[FILE_BUFFER_SIZE]());
		Buffer.swap(FileDataBufferTemp);
		FileDataBufferTemp.reset();

		for (Index = 0;Index < sizeof(FileDataTemp) / sizeof(FileData);Index++)
		{
			std::shared_ptr<BitSequence> FileDataBufferTemp_SHA3(new BitSequence[SHA3_512_SIZE]());
			FileDataTemp[Index].Result.swap(FileDataBufferTemp_SHA3);
		}
	}

//Open file.
	FILE *Input = nullptr;
	std::vector<FileData> FileList;
	for (Index = 0;Index < sizeof(FileDataTemp) / sizeof(FileData);Index++)
		FileDataTemp[Index].FileName = Parameter.Path->front();
	FileDataTemp[0].FileName.append(L"Hosts.ini");
	FileDataTemp[1U].FileName.append(L"Hosts.conf");
	FileDataTemp[2U].FileName.append(L"Hosts");
	FileDataTemp[3U].FileName.append(L"Hosts.txt");
	FileDataTemp[4U].FileName.append(L"Hosts.csv");
	for (Index = 0;Index < sizeof(FileDataTemp) / sizeof(FileData);Index++)
		FileList.push_back(FileDataTemp[Index]);
*/
//Initialization
	std::shared_ptr<char> Buffer;
	if (Parameter.FileHash)
	{
		std::shared_ptr<char> FileDataBufferTemp(new char[FILE_BUFFER_SIZE]());
		Buffer.swap(FileDataBufferTemp);
		FileDataBufferTemp.reset();
	}

//Creat file list.
	FILE *Input = nullptr;
	for (auto StringIter:*Parameter.Path)
	{
		for (auto InnerStringIter:*Parameter.HostsFileList)
		{
			FileData FileDataTemp;

		//Reset.
			FileDataTemp.HashAvailable = false;
			FileDataTemp.FileName.clear();
			FileDataTemp.HashResult.reset();

		//Add to list.
			FileDataTemp.FileName.append(StringIter);
			FileDataTemp.FileName.append(InnerStringIter);
			HostsFileList.push_back(FileDataTemp);
		}
	}

//Files monitor
	size_t FileIndex = 0, ReadLength = 0, Index = 0;
	auto HashChanged = false;
	HANDLE HostsHandle = nullptr;
	std::shared_ptr<LARGE_INTEGER> HostsFileSize(new LARGE_INTEGER());
	std::vector<HostsTable>::iterator HostsListIter;
	Keccak_HashInstance HashInstance = {0};

	for (;;)
	{
		HashChanged = false;
		if (Parameter.FileHash && !HostsListUsing->empty())
			*HostsListModificating = *HostsListUsing;

	//Check FileList.
		for (FileIndex = 0;FileIndex < HostsFileList.size();FileIndex++)
		{
			if (_wfopen_s(&Input, HostsFileList[FileIndex].FileName.c_str(), L"rb") != 0)
			{
			//Clear hash results.
				if (Parameter.FileHash)
				{
					if (HostsFileList[FileIndex].HashAvailable)
					{
						HashChanged = true;
						HostsFileList[FileIndex].HashResult.reset();
					}
					HostsFileList[FileIndex].HashAvailable = false;

				//Clear old iteams.
					for (HostsListIter = HostsListModificating->begin();HostsListIter != HostsListModificating->end();)
					{
						if (HostsListIter->FileIndex == FileIndex)
						{
							HostsListIter = HostsListModificating->erase(HostsListIter);
							if (HostsListIter == HostsListModificating->end())
								break;
						}
						else {
							HostsListIter++;
						}
					}
				}

				continue;
			}
			else {
				if (Input == nullptr)
				{
				//Clear hash results.
					if (Parameter.FileHash)
					{
						if (HostsFileList[FileIndex].HashAvailable)
						{
							HashChanged = true;
							HostsFileList[FileIndex].HashResult.reset();
						}
						HostsFileList[FileIndex].HashAvailable = false;
						
					//Clear old iteams.
						for (HostsListIter = HostsListModificating->begin();HostsListIter != HostsListModificating->end();)
						{
							if (HostsListIter->FileIndex == FileIndex)
							{
								HostsListIter = HostsListModificating->erase(HostsListIter);
								if (HostsListIter == HostsListModificating->end())
									break;
							}
							else {
								HostsListIter++;
							}
						}
					}

					continue;
				}

			//Check whole file size.
				HostsHandle = CreateFileW(HostsFileList[FileIndex].FileName.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (HostsHandle != INVALID_HANDLE_VALUE)
				{
					memset(HostsFileSize.get(), 0, sizeof(LARGE_INTEGER));
					if (GetFileSizeEx(HostsHandle, HostsFileSize.get()) == 0)
					{
						CloseHandle(HostsHandle);
					}
					else {
						CloseHandle(HostsHandle);
						if (HostsFileSize->QuadPart >= DEFAULT_FILE_MAXSIZE)
						{
							PrintError(LOG_ERROR_PARAMETER, L"Hosts file size is too large", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), NULL);
							fclose(Input);
							Input = nullptr;

						//Clear hash results.
							if (Parameter.FileHash)
							{
								if (HostsFileList[FileIndex].HashAvailable)
								{
									HashChanged = true;
									HostsFileList[FileIndex].HashResult.reset();
								}
								HostsFileList[FileIndex].HashAvailable = false;
							
							//Clear old iteams.
								for (HostsListIter = HostsListModificating->begin();HostsListIter != HostsListModificating->end();)
								{
									if (HostsListIter->FileIndex == FileIndex)
									{
										HostsListIter = HostsListModificating->erase(HostsListIter);
										if (HostsListIter == HostsListModificating->end())
											break;
									}
									else {
										HostsListIter++;
									}
								}
							}

							continue;
						}
					}
				}

			//Mark or check files hash.
				if (Parameter.FileHash)
				{
					memset(Buffer.get(), 0, FILE_BUFFER_SIZE);
					memset(&HashInstance, 0, sizeof(Keccak_HashInstance));
					Keccak_HashInitialize_SHA3_512(&HashInstance);
					while (!feof(Input))
					{
						ReadLength = fread_s(Buffer.get(), FILE_BUFFER_SIZE, sizeof(char), FILE_BUFFER_SIZE, Input);
						Keccak_HashUpdate(&HashInstance, (BitSequence *)Buffer.get(), ReadLength * BYTES_TO_BITS);
					}
					memset(Buffer.get(), 0, FILE_BUFFER_SIZE);
					Keccak_HashFinal(&HashInstance, (BitSequence *)Buffer.get());

				//Set file pointers to the beginning of file.
					if (_fseeki64(Input, 0, SEEK_SET) != 0)
					{
						PrintError(LOG_ERROR_HOSTS, L"Read files error", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), NULL);
						fclose(Input);
						Input = nullptr;

					//Clear hash results.
						if (HostsFileList[FileIndex].HashAvailable)
						{
							HashChanged = true;
							HostsFileList[FileIndex].HashResult.reset();
						}
						HostsFileList[FileIndex].HashAvailable = false;
						
					//Clear old iteams.
						for (HostsListIter = HostsListModificating->begin();HostsListIter != HostsListModificating->end();)
						{
							if (HostsListIter->FileIndex == FileIndex)
							{
								HostsListIter = HostsListModificating->erase(HostsListIter);
								if (HostsListIter == HostsListModificating->end())
									break;
							}
							else {
								HostsListIter++;
							}
						}

						continue;
					}
					else {
						if (HostsFileList[FileIndex].HashAvailable)
						{
							if (memcmp(HostsFileList[FileIndex].HashResult.get(), Buffer.get(), SHA3_512_SIZE) == 0)
							{
								fclose(Input);
								Input = nullptr;
								continue;
							}
							else {
								HashChanged = true;
								memcpy(HostsFileList[FileIndex].HashResult.get(), Buffer.get(), SHA3_512_SIZE);

							//Clear old iteams.
								for (HostsListIter = HostsListModificating->begin();HostsListIter != HostsListModificating->end();)
								{
									if (HostsListIter->FileIndex == FileIndex)
									{
										HostsListIter = HostsListModificating->erase(HostsListIter);
										if (HostsListIter == HostsListModificating->end())
											break;
									}
									else {
										HostsListIter++;
									}
								}
							}
						}
						else {
							HashChanged = true;
							HostsFileList[FileIndex].HashAvailable = true;
							std::shared_ptr<BitSequence> HashBufferTemp(new BitSequence[SHA3_512_SIZE]());
							memcpy(HashBufferTemp.get(), Buffer.get(), SHA3_512_SIZE);
							HostsFileList[FileIndex].HashResult.swap(HashBufferTemp);

						//Clear old iteams.
							for (HostsListIter = HostsListModificating->begin();HostsListIter != HostsListModificating->end();)
							{
								if (HostsListIter->FileIndex == FileIndex)
								{
									HostsListIter = HostsListModificating->erase(HostsListIter);
									if (HostsListIter == HostsListModificating->end())
										break;
								}
								else {
									HostsListIter++;
								}
							}
						}
					}
				}
			}

		//Read data.
			HashChanged = true;
			ReadText(Input, READTEXT_HOSTS, FileIndex);
			fclose(Input);
			Input = nullptr;
		}

	//Update Hosts list.
		if (!HashChanged)
		{
		//Auto-refresh
			Sleep((DWORD)Parameter.FileRefreshTime);
			continue;
		}
		else if (!HostsListModificating->empty())
		{
		//Check repeating items.
			for (HostsListIter = HostsListModificating->begin();HostsListIter != HostsListModificating->end();HostsListIter++)
			{
				if (HostsListIter->Type == HOSTS_NORMAL)
				{
				//AAAA Records(IPv6)
					if (HostsListIter->Protocol == AF_INET6 && HostsListIter->Length > sizeof(dns_aaaa_record))
					{
						for (Index = 0;Index < HostsListIter->Length / sizeof(dns_aaaa_record);Index++)
						{
							for (size_t InnerIndex = Index + 1U;InnerIndex < HostsListIter->Length / sizeof(dns_aaaa_record);InnerIndex++)
							{
								if (memcmp(HostsListIter->Response.get() + sizeof(dns_aaaa_record) * Index, 
									HostsListIter->Response.get() + sizeof(dns_aaaa_record) * InnerIndex, sizeof(dns_aaaa_record)) == 0)
								{
									memmove(HostsListIter->Response.get() + sizeof(dns_aaaa_record) * InnerIndex, HostsListIter->Response.get() + sizeof(dns_aaaa_record) * (InnerIndex + 1U), sizeof(dns_aaaa_record) * (HostsListIter->Length / sizeof(dns_aaaa_record) - InnerIndex));
									HostsListIter->Length -= sizeof(dns_aaaa_record);
									InnerIndex--;
								}
							}
						}
					}
				//A Records(IPv4)
					else {
						for (Index = 0;Index < HostsListIter->Length / sizeof(dns_a_record);Index++)
						{
							for (size_t InnerIndex = Index + 1U;InnerIndex < HostsListIter->Length / sizeof(dns_a_record);InnerIndex++)
							{
								if (memcmp(HostsListIter->Response.get() + sizeof(dns_a_record) * Index, 
									HostsListIter->Response.get() + sizeof(dns_a_record) * InnerIndex, sizeof(dns_a_record)) == 0)
								{
									memmove(HostsListIter->Response.get() + sizeof(dns_a_record) * InnerIndex, HostsListIter->Response.get() + sizeof(dns_a_record) * (InnerIndex + 1U), sizeof(dns_a_record) * (HostsListIter->Length / sizeof(dns_a_record) - InnerIndex));
									HostsListIter->Length -= sizeof(dns_a_record);
									InnerIndex--;
								}
							}
						}
					}
				}
			}

		//EDNS0 Lebal
			if (Parameter.EDNS0Label)
			{
				dns_opt_record *pdns_opt_record = nullptr;
				for (HostsListIter = HostsListModificating->begin();HostsListIter != HostsListModificating->end();HostsListIter++)
				{
					if (HostsListIter->Length > PACKET_MAXSIZE - sizeof(dns_opt_record))
					{
						PrintError(LOG_ERROR_HOSTS, L"Data is too long when EDNS0 is available", NULL, nullptr, NULL);
						continue;
					}
					else if (!HostsListIter->Response)
					{
						continue;
					}
					else {
						pdns_opt_record = (dns_opt_record *)(HostsListIter->Response.get() + HostsListIter->Length);
						pdns_opt_record->Type = htons(DNS_RECORD_OPT);
						pdns_opt_record->UDPPayloadSize = htons((uint16_t)Parameter.EDNS0PayloadSize);
						HostsListIter->Length += sizeof(dns_opt_record);
					}
				}
			}

/* Old version(2015-01-28)
		//Sort list.
			std::partition(HostsListModificating->begin(), HostsListModificating->end(), SortHostsListNORMAL);
			std::partition(HostsListModificating->begin(), HostsListModificating->end(), SortHostsListWHITE);
			std::partition(HostsListModificating->begin(), HostsListModificating->end(), SortHostsListBANNED);
*/
		//Swap(or cleanup) using list.
			HostsListModificating->shrink_to_fit();
			std::unique_lock<std::mutex> HostsListMutex(HostsListLock);
			HostsListUsing->swap(*HostsListModificating);
			HostsListMutex.unlock();
			HostsListModificating->clear();
			HostsListModificating->shrink_to_fit();

		//Clear DNS cache after HostsList was updated.
			std::unique_lock<std::mutex> DNSCacheListMutex(DNSCacheListLock);
			DNSCacheList.clear();
			DNSCacheList.shrink_to_fit();
			DNSCacheListMutex.unlock();
		}
	//Hosts Table is empty.
		else {
			std::unique_lock<std::mutex> HostsListMutex(HostsListLock);
			HostsListUsing->clear();
			HostsListUsing->shrink_to_fit();
			HostsListMutex.unlock();
			HostsListModificating->clear();
			HostsListModificating->shrink_to_fit();
		}

	//Flush DNS cache.
		if (Parameter.FileHash)
			FlushDNSResolverCache();
		
	//Auto-refresh
		Sleep((DWORD)Parameter.FileRefreshTime);
	}

	PrintError(LOG_ERROR_SYSTEM, L"Read Hosts module Monitor terminated", NULL, nullptr, NULL);
	return EXIT_SUCCESS;
}

//Read hosts data from files
size_t __fastcall ReadHostsData(const PSTR Buffer, const size_t FileIndex, const size_t Line, size_t &LabelType, bool &LabelComments)
{
	std::string Data(Buffer);

//Multi-line comments check, delete comments(Number Sign/NS and double slashs) and check minimum length of hosts items.
	if (ReadMultiLineComments(Buffer, Data, LabelComments) == EXIT_FAILURE || Data.find(ASCII_HASHTAG) == 0 || Data.find(ASCII_SLASH) == 0)
		return EXIT_SUCCESS;
	else if (Data.rfind(" //") != std::string::npos)
		Data.erase(Data.rfind(" //"), Data.length() - Data.rfind(" //"));
	else if (Data.rfind("	//") != std::string::npos)
		Data.erase(Data.rfind("	//"), Data.length() - Data.rfind("	//"));
	else if (Data.rfind(" #") != std::string::npos)
		Data.erase(Data.rfind(" #"), Data.length() - Data.rfind(" #"));
	else if (Data.rfind("	#") != std::string::npos)
		Data.erase(Data.rfind("	#"), Data.length() - Data.rfind("	#"));
	if (Data.length() < READ_HOSTS_MINSIZE)
		return FALSE;

//[Local Hosts] block(A part)
	if (LabelType == 0 && (Parameter.DNSTarget.Local_IPv4.AddressData.Storage.ss_family != NULL || Parameter.DNSTarget.Local_IPv6.AddressData.Storage.ss_family != NULL) &&
		(HostsFileList[FileIndex].FileName.rfind(L"WhiteList.txt") != std::wstring::npos && HostsFileList[FileIndex].FileName.length() > wcslen(L"WhiteList.txt") &&
		HostsFileList[FileIndex].FileName.rfind(L"WhiteList.txt") == HostsFileList[FileIndex].FileName.length() - wcslen(L"WhiteList.txt") ||
		HostsFileList[FileIndex].FileName.rfind(L"White_List.txt") != std::wstring::npos && HostsFileList[FileIndex].FileName.length() > wcslen(L"White_List.txt") &&
		HostsFileList[FileIndex].FileName.rfind(L"White_List.txt") == HostsFileList[FileIndex].FileName.length() - wcslen(L"White_List.txt") ||
		HostsFileList[FileIndex].FileName.rfind(L"whitelist.txt") != std::wstring::npos && HostsFileList[FileIndex].FileName.length() > wcslen(L"whitelist.txt") &&
		HostsFileList[FileIndex].FileName.rfind(L"whitelist.txt") == HostsFileList[FileIndex].FileName.length() - wcslen(L"whitelist.txt") ||
		HostsFileList[FileIndex].FileName.rfind(L"white_list.txt") != std::wstring::npos && HostsFileList[FileIndex].FileName.length() > wcslen(L"white_list.txt") &&
		HostsFileList[FileIndex].FileName.rfind(L"white_list.txt") == HostsFileList[FileIndex].FileName.length() - wcslen(L"white_list.txt")))
			LabelType = LABEL_HOSTS_LOCAL;

//[Base] block
	if (Data.find("[Base]") == 0 || Data.find("[base]") == 0)
	{
/* Old version(2015-01-12)
		LabelStop = false;
		Local = false;
*/
		return EXIT_SUCCESS;
	}
	else if (Data.find("Version = ") == 0 || Data.find("version = ") == 0)
	{
/* Old version(2015-01-12)
		if (Data.length() < strlen("Version = ") + 8U)
		{
			double ReadVersion = atof(Data.c_str() + strlen("Version = "));
			if (ReadVersion > HOSTS_VERSION)
			{
				PrintError(LOG_ERROR_HOSTS, L"Hosts file version error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
*/
		return EXIT_SUCCESS;
	}

//[Hosts] block
	if (Data.find("[Hosts]") == 0 || Data.find("[hosts]") == 0)
	{
		LabelType = LABEL_HOSTS;
		return EXIT_SUCCESS;
	}

//[Local Hosts] block(B part)
	else if (Data.find("[Local Hosts]") == 0 || Data.find("[Local hosts]") == 0 || Data.find("[local Hosts]") == 0 || Data.find("[local hosts]") == 0)
	{
		if (Parameter.DNSTarget.Local_IPv4.AddressData.Storage.ss_family != NULL || Parameter.DNSTarget.Local_IPv6.AddressData.Storage.ss_family != NULL)
			LabelType = LABEL_HOSTS_LOCAL;
		
		return EXIT_SUCCESS;
	}

//Temporary stop read.
	else if (Data.find("[Stop]") == 0 || Data.find("[stop]") == 0)
	{
		LabelType = LABEL_STOP;
		return EXIT_SUCCESS;
	}
	if (LabelType == LABEL_STOP)
		return EXIT_SUCCESS;

//Whitelist items
	if (Data.find("NULL ") == 0 || Data.find("NULL	") == 0 || Data.find("NULL,") == 0 ||
		Data.find("Null ") == 0 || Data.find("Null	") == 0 || Data.find("Null,") == 0 ||
		Data.find("null ") == 0 || Data.find("null	") == 0 || Data.find("null,") == 0)
			return ReadWhitelistAndBannedData(Data, FileIndex, Line, LABEL_HOSTS_WHITELIST);

//Banned items
	else if (Data.find("BAN ") == 0 || Data.find("BAN	") == 0 || Data.find("BAN,") == 0 || 
		Data.find("BANNED ") == 0 || Data.find("BANNED	") == 0 || Data.find("BANNED,") == 0 || 
		Data.find("Ban ") == 0 || Data.find("Ban	") == 0 || Data.find("Ban,") == 0 || 
		Data.find("Banned ") == 0 || Data.find("Banned	") == 0 || Data.find("Banned,") == 0 || 
		Data.find("ban ") == 0 || Data.find("ban	") == 0 || Data.find("ban,") == 0 || 
		Data.find("banned ") == 0 || Data.find("banned	") == 0 || Data.find("banned,") == 0)
			return ReadWhitelistAndBannedData(Data, FileIndex, Line, LABEL_HOSTS_BANNED);

//[Local Hosts] block
	else if (LabelType == LABEL_HOSTS_LOCAL)
		return ReadLocalHostsData(Data, FileIndex, Line);

//[Hosts] block
	else 
		return ReadMainHostsData(Data, FileIndex, Line);

	return EXIT_SUCCESS;
}

//Read Whitelist and Banned items in Hosts file from data
inline size_t __fastcall ReadWhitelistAndBannedData(std::string Data, const size_t FileIndex, const size_t Line, const size_t LabelType)
{
//Initialization
	HostsTable HostsTableTemp;
	size_t Separated = 0;

//Mark separated location.
	if (Data.find(ASCII_HT) != std::string::npos && Data.find(ASCII_SPACE) != std::string::npos)
	{
		if (Data.find(ASCII_HT) < Data.find(ASCII_SPACE))
			Separated = Data.find(ASCII_HT);
		else
			Separated = Data.find(ASCII_SPACE);
	}
	else if (Data.find(ASCII_HT) != std::string::npos)
	{
		Separated = Data.find(ASCII_HT);
	}
	else if (Data.find(ASCII_SPACE) != std::string::npos)
	{
		Separated = Data.find(ASCII_SPACE);
	}
	else {
		PrintError(LOG_ERROR_HOSTS, L"Item format error", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
		return EXIT_FAILURE;
	}

//Delete spaces and horizontal tab/HT.
	while (Data.find(ASCII_HT) != std::string::npos)
		Data.erase(Data.find(ASCII_HT), 1U);
	while (Data.find(ASCII_SPACE) != std::string::npos)
		Data.erase(Data.find(ASCII_SPACE), 1U);

//Mark patterns
	HostsTableTemp.PatternString.append(Data, Separated, Data.length() - Separated);
	try {
		std::regex PatternHostsTableTemp(HostsTableTemp.PatternString /* , std::regex_constants::extended */);
		HostsTableTemp.Pattern.swap(PatternHostsTableTemp);
	}
	catch (std::regex_error)
	{
		PrintError(LOG_ERROR_HOSTS, L"Regular expression pattern error", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
		return EXIT_FAILURE;
	}

//Check repeating items.
	for (auto HostsTableIter:*HostsListModificating)
	{
		if (HostsTableIter.PatternString == HostsTableTemp.PatternString)
		{
			if (HostsTableIter.Type == HOSTS_NORMAL || HostsTableIter.Type == HOSTS_WHITE && LabelType != LABEL_HOSTS_WHITELIST || 
				HostsTableIter.Type == HOSTS_LOCAL || HostsTableIter.Type == HOSTS_BANNED && LabelType != LABEL_HOSTS_BANNED)
					PrintError(LOG_ERROR_HOSTS, L"Repeating items error, this item is not available", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);

			return EXIT_FAILURE;
		}
	}

//Mark types.
	if (LabelType == LABEL_HOSTS_BANNED)
		HostsTableTemp.Type = HOSTS_BANNED;
	else 
		HostsTableTemp.Type = HOSTS_WHITE;
	HostsTableTemp.FileIndex = FileIndex;
	HostsListModificating->push_back(HostsTableTemp);
	return EXIT_SUCCESS;
}

//Read Local Hosts items in Hosts file from data
inline size_t __fastcall ReadLocalHostsData(std::string Data, const size_t FileIndex, const size_t Line)
{
//Local Hosts check
	if (Parameter.DNSTarget.Local_IPv4.AddressData.Storage.ss_family == NULL && Parameter.DNSTarget.Local_IPv6.AddressData.Storage.ss_family == NULL && Parameter.LocalMain)
		return EXIT_SUCCESS;
	HostsTable HostsTableTemp;

//Delete spaces and horizontal tab/HT.
	while (Data.find(ASCII_HT) != std::string::npos)
		Data.erase(Data.find(ASCII_HT), 1U);
	while (Data.find(ASCII_SPACE) != std::string::npos)
		Data.erase(Data.find(ASCII_SPACE), 1U);

//Check repeating items.
	HostsTableTemp.PatternString = Data;
	for (auto HostsTableIter:*HostsListModificating)
	{
		if (HostsTableIter.PatternString == HostsTableTemp.PatternString)
		{
			if (HostsTableIter.Type != HOSTS_LOCAL)
				PrintError(LOG_ERROR_HOSTS, L"Repeating items error, this item is not available", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);

			return EXIT_FAILURE;
		}
	}

//Mark patterns.
	try {
		std::regex PatternHostsTableTemp(HostsTableTemp.PatternString /* , std::regex_constants::extended */ );
		HostsTableTemp.Pattern.swap(PatternHostsTableTemp);
	}
	catch (std::regex_error)
	{
		PrintError(LOG_ERROR_HOSTS, L"Regular expression pattern error", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
		return EXIT_FAILURE;
	}

//Add to global HostsTable.
	HostsTableTemp.Type = HOSTS_LOCAL;
	HostsTableTemp.FileIndex = FileIndex;
	HostsListModificating->push_back(HostsTableTemp);
	return EXIT_SUCCESS;
}

//Read Main Hosts items in Hosts file from data
inline size_t __fastcall ReadMainHostsData(std::string Data, const size_t FileIndex, const size_t Line)
{
//Initialization
	std::shared_ptr<char> Addr(new char[ADDR_STRING_MAXSIZE]());
	HostsTable HostsTableTemp;
	size_t Separated = 0;

//Delete spaces and horizontal tab/HT before data.
	while (!Data.empty() && (Data[0] == ASCII_HT || Data[0] == ASCII_SPACE))
		Data.erase(0, 1U);

//Delete spaces and horizontal tab/HT before or after verticals.
	while (Data.find("	|") != std::string::npos || Data.find("|	") != std::string::npos ||
		Data.find(" |") != std::string::npos || Data.find("| ") != std::string::npos)
	{
		if (Data.find("	|") != std::string::npos)
			Data.erase(Data.find("	|"), 1U);
		if (Data.find("|	") != std::string::npos)
			Data.erase(Data.find("|	") + 1U, 1U);
		if (Data.find(" |") != std::string::npos)
			Data.erase(Data.find(" |"), 1U);
		if (Data.find("| ") != std::string::npos)
			Data.erase(Data.find("| ") + 1U, 1U);
	}

//Mark separated location.
	if (Data.find(ASCII_COMMA) != std::string::npos)
	{
	//Delete spaces and horizontal tab/HT before commas.
		while (Data.find("	,") != std::string::npos || Data.find(" ,") != std::string::npos)
		{
			if (Data.find("	,") != std::string::npos)
				Data.erase(Data.find("	,"), 1U);
			if (Data.find(" ,") != std::string::npos)
				Data.erase(Data.find(" ,"), 1U);
		}

	//Delete spaces and horizontal tab/HT after commas.
		while (Data.find(ASCII_HT) != std::string::npos && Data.find(ASCII_HT) > Data.find(ASCII_COMMA))
			Data.erase(Data.find(ASCII_HT), 1U);
		while (Data.find(ASCII_SPACE) != std::string::npos && Data.find(ASCII_SPACE) > Data.find(ASCII_COMMA))
			Data.erase(Data.find(ASCII_SPACE), 1U);

	//Common format
		if (Data.find(ASCII_HT) != std::string::npos && Data.find(ASCII_SPACE) != std::string::npos)
		{
			if (Data.find(ASCII_HT) < Data.find(ASCII_SPACE))
				Separated = Data.find(ASCII_HT);
			else
				Separated = Data.find(ASCII_SPACE);
		}
		else if (Data.find(ASCII_HT) != std::string::npos)
		{
			Separated = Data.find(ASCII_HT);
		}
		else if (Data.find(ASCII_SPACE) != std::string::npos)
		{
			Separated = Data.find(ASCII_SPACE);
		}
	//Comma-Separated Values/CSV, RFC 4180(https://tools.ietf.org/html/rfc4180), Common Format and MIME Type for Comma-Separated Values (CSV) Files).
		else {
			Separated = Data.find(ASCII_COMMA);
			Data.erase(Separated, 1U);
		}
	}
//Common format
	else {
		if (Data.find(ASCII_HT) != std::string::npos && Data.find(ASCII_SPACE) != std::string::npos)
		{
			if (Data.find(ASCII_HT) < Data.find(ASCII_SPACE))
				Separated = Data.find(ASCII_HT);
			else
				Separated = Data.find(ASCII_SPACE);
		}
		else if (Data.find(ASCII_HT) != std::string::npos)
		{
			Separated = Data.find(ASCII_HT);
		}
		else if (Data.find(ASCII_SPACE) != std::string::npos)
		{
			Separated = Data.find(ASCII_SPACE);
		}
		else {
			PrintError(LOG_ERROR_HOSTS, L"Item format error", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
			return EXIT_FAILURE;
		}
	}

//Delete spaces and horizontal tab/HT.
	while (Data.find(ASCII_HT) != std::string::npos)
		Data.erase(Data.find(ASCII_HT), 1U);
	while (Data.find(ASCII_SPACE) != std::string::npos)
		Data.erase(Data.find(ASCII_SPACE), 1U);

//String length check
	if (Separated < 3U)
		return EXIT_FAILURE;

//Response initialization
	std::shared_ptr<char> BufferHostsTableTemp(new char[PACKET_MAXSIZE]());
	HostsTableTemp.Response.swap(BufferHostsTableTemp);
	BufferHostsTableTemp.reset();
	dns_aaaa_record *pdns_aaaa_rsp = nullptr;
	dns_a_record *pdns_a_rsp = nullptr;
	SSIZE_T Result = 0;

//Single address
	if (Data.find(ASCII_VERTICAL) == std::string::npos)
	{
	//AAAA Records(IPv6)
		if (Data.find(ASCII_COLON) < Separated)
		{
		//IPv6 addresses check
			if (Separated > ADDR_STRING_MAXSIZE)
			{
				PrintError(LOG_ERROR_HOSTS, L"IPv6 Address format error", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
				return EXIT_FAILURE;
			}
			else if (Data[0] < ASCII_ZERO || Data[0] > ASCII_COLON && Data[0] < ASCII_UPPERCASE_A || Data[0] > ASCII_UPPERCASE_F && Data[0] < ASCII_LOWERCASE_A || Data[0] > ASCII_LOWERCASE_F)
			{
				return EXIT_FAILURE;
			}

		//Make responses.
			pdns_aaaa_rsp = (dns_aaaa_record *)HostsTableTemp.Response.get();
			pdns_aaaa_rsp->Name = htons(DNS_QUERY_PTR);
			pdns_aaaa_rsp->Classes = htons(DNS_CLASS_IN);
			pdns_aaaa_rsp->TTL = htonl(Parameter.HostsDefaultTTL);
			pdns_aaaa_rsp->Type = htons(DNS_RECORD_AAAA);
			pdns_aaaa_rsp->Length = htons(sizeof(in6_addr));

		//Convert addresses.
			memcpy(Addr.get(), Data.c_str(), Separated);
			if (AddressStringToBinary(Addr.get(), &pdns_aaaa_rsp->Addr, AF_INET6, Result) == EXIT_FAILURE)
			{
				PrintError(LOG_ERROR_HOSTS, L"IPv6 Address format error", Result, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
				return EXIT_FAILURE;
			}

			HostsTableTemp.Protocol = AF_INET6;
			HostsTableTemp.Length = sizeof(dns_aaaa_record);
		}
	//A Records(IPv4)
		else {
		//IPv4 addresses check
			if (Separated > ADDR_STRING_MAXSIZE)
			{
				PrintError(LOG_ERROR_HOSTS, L"IPv4 Address format error", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
				return EXIT_FAILURE;
			}
			else if (Data[0] < ASCII_ZERO || Data[0] > ASCII_NINE)
			{
				return EXIT_FAILURE;
			}

		//Make responses.
			pdns_a_rsp = (dns_a_record *)HostsTableTemp.Response.get();
			pdns_a_rsp->Name = htons(DNS_QUERY_PTR);
			pdns_a_rsp->Classes = htons(DNS_CLASS_IN);
			pdns_a_rsp->TTL = htonl(Parameter.HostsDefaultTTL);
			pdns_a_rsp->Type = htons(DNS_RECORD_A);
			pdns_a_rsp->Length = htons(sizeof(in_addr));

		//Convert addresses.
			memcpy(Addr.get(), Data.c_str(), Separated);
			if (AddressStringToBinary(Addr.get(), &pdns_a_rsp->Addr, AF_INET, Result) == EXIT_FAILURE)
			{
				PrintError(LOG_ERROR_HOSTS, L"IPv4 Address format error", Result, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
				return EXIT_FAILURE;
			}

			HostsTableTemp.Protocol = AF_INET;
			HostsTableTemp.Length = sizeof(dns_a_record);
		}
	}
//Multiple Addresses
	else {
		size_t Index = 0, VerticalIndex = 0;

	//AAAA Records(IPv6)
		if (Data.find(ASCII_COLON) < Separated)
		{
		//IPv6 addresses check
			if (Data[0] < ASCII_ZERO || Data[0] > ASCII_COLON && Data[0] < ASCII_UPPERCASE_A || Data[0] > ASCII_UPPERCASE_F && Data[0] < ASCII_LOWERCASE_A || Data[0] > ASCII_LOWERCASE_F)
				return EXIT_FAILURE;

			HostsTableTemp.Protocol = AF_INET6;
			for (Index = 0;Index <= Separated;Index++)
			{
			//Read data.
				if (Data[Index] == ASCII_VERTICAL || Index == Separated)
				{
				//Length check
					if (HostsTableTemp.Length + sizeof(dns_aaaa_record) > PACKET_MAXSIZE)
					{
						PrintError(LOG_ERROR_HOSTS, L"Too many Hosts IP addresses", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
						return EXIT_FAILURE;
					}
					else if (Index - VerticalIndex > ADDR_STRING_MAXSIZE)
					{
						PrintError(LOG_ERROR_HOSTS, L"IPv6 Address format error", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
						return EXIT_FAILURE;
					}

				//Make responses
					pdns_aaaa_rsp = (dns_aaaa_record *)(HostsTableTemp.Response.get() + HostsTableTemp.Length);
					pdns_aaaa_rsp->Name = htons(DNS_QUERY_PTR);
					pdns_aaaa_rsp->Classes = htons(DNS_CLASS_IN);
					pdns_aaaa_rsp->TTL = htonl(Parameter.HostsDefaultTTL);
					pdns_aaaa_rsp->Type = htons(DNS_RECORD_AAAA);
					pdns_aaaa_rsp->Length = htons(sizeof(in6_addr));

				//Convert addresses.
					memset(Addr.get(), 0, ADDR_STRING_MAXSIZE);
					memcpy(Addr.get(), Data.c_str() + VerticalIndex, Index - VerticalIndex);
					if (AddressStringToBinary(Addr.get(), &pdns_aaaa_rsp->Addr, AF_INET6, Result) == EXIT_FAILURE)
					{
						PrintError(LOG_ERROR_HOSTS, L"IPv6 Address format error", Result, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
						return EXIT_FAILURE;
					}

					HostsTableTemp.Length += sizeof(dns_aaaa_record);
					VerticalIndex = Index + 1U;
				}
			}
		}
	//A Records(IPv4)
		else {
		//IPv4 addresses check
			if (Data[0] < ASCII_ZERO || Data[0] > ASCII_NINE)
				return EXIT_FAILURE;

			HostsTableTemp.Protocol = AF_INET;
			for (Index = 0;Index <= Separated;Index++)
			{
			//Read data.
				if (Data[Index] == ASCII_VERTICAL || Index == Separated)
				{
				//Length check
					if (HostsTableTemp.Length + sizeof(dns_a_record) > PACKET_MAXSIZE)
					{
						PrintError(LOG_ERROR_HOSTS, L"Too many Hosts IP addresses", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
						return EXIT_FAILURE;
					}
					else if (Index - VerticalIndex > ADDR_STRING_MAXSIZE)
					{
						PrintError(LOG_ERROR_HOSTS, L"IPv4 Address format error", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
						return EXIT_FAILURE;
					}

				//Make responses.
					pdns_a_rsp = (dns_a_record *)(HostsTableTemp.Response.get() + HostsTableTemp.Length);
					pdns_a_rsp->Name = htons(DNS_QUERY_PTR);
					pdns_a_rsp->Classes = htons(DNS_CLASS_IN);
					pdns_a_rsp->TTL = htonl(Parameter.HostsDefaultTTL);
					pdns_a_rsp->Type = htons(DNS_RECORD_A);
					pdns_a_rsp->Length = htons(sizeof(in_addr));

				//Convert addresses.
					memset(Addr.get(), 0, ADDR_STRING_MAXSIZE);
					memcpy(Addr.get(), Data.c_str() + VerticalIndex, Index - VerticalIndex);
					if (AddressStringToBinary(Addr.get(), &pdns_a_rsp->Addr, AF_INET, Result) == EXIT_FAILURE)
					{
						PrintError(LOG_ERROR_HOSTS, L"IPv4 Address format error", Result, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
						return EXIT_FAILURE;
					}

					HostsTableTemp.Length += sizeof(dns_a_record);
					VerticalIndex = Index + 1U;
				}
			}
		}
	}

//Mark patterns.
	HostsTableTemp.PatternString.append(Data, Separated, Data.length() - Separated);
	try {
		std::regex PatternHostsTableTemp(HostsTableTemp.PatternString /* , std::regex_constants::extended */);
		HostsTableTemp.Pattern.swap(PatternHostsTableTemp);
	}
	catch (std::regex_error)
	{
		PrintError(LOG_ERROR_HOSTS, L"Regular expression pattern error", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
		return EXIT_FAILURE;
	}

//Check repeating items.
	for (auto HostsListIter = HostsListModificating->begin();HostsListIter != HostsListModificating->end();HostsListIter++)
	{
		if (HostsListIter->PatternString == HostsTableTemp.PatternString)
		{
			if (HostsListIter->Type != HOSTS_NORMAL || HostsListIter->Protocol == 0)
			{
				PrintError(LOG_ERROR_HOSTS, L"Repeating items error, this item is not available", NULL, (PWSTR)HostsFileList[FileIndex].FileName.c_str(), Line);
				return EXIT_FAILURE;
			}
			else {
				if (HostsListIter->Protocol == HostsTableTemp.Protocol)
				{
					if (HostsListIter->Length + HostsTableTemp.Length < PACKET_MAXSIZE)
					{
						memcpy(HostsListIter->Response.get() + HostsListIter->Length, HostsTableTemp.Response.get(), HostsTableTemp.Length);
						HostsListIter->Length += HostsTableTemp.Length;
					}

					return EXIT_SUCCESS;
				}
				else {
					continue;
				}
			}
		}
	}

//Add to global HostsTable.
	if (HostsTableTemp.Length >= sizeof(dns_qry) + sizeof(in_addr)) //Shortest reply is a A Records with Question part.
	{
		HostsTableTemp.Type = HOSTS_NORMAL;
		HostsTableTemp.FileIndex = FileIndex;
		HostsListModificating->push_back(HostsTableTemp);
	}

	return EXIT_SUCCESS;
}
