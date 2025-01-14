/*----------------------------------------------------------------------------
|
| File Name: capldll.cpp
|
|            Example of a capl DLL implementation module and using CAPLLbacks.
|-----------------------------------------------------------------------------
|               A U T H O R   I D E N T I T Y
|-----------------------------------------------------------------------------
|   Author             Initials
|   ------             --------
|   Thomas  Riegraf    Ri              Vector Informatik GmbH
|   Hans    Quecke     Qu              Vector Informatik GmbH
|   Stefan  Albus      As              Vector Informatik GmbH
|-----------------------------------------------------------------------------
|               R E V I S I O N   H I S T O R Y
|-----------------------------------------------------------------------------
| Date         Ver  Author  Description
| ----------   ---  ------  --------------------------------------------------
| 2003-10-07   1.0  As      Created
| 2007-03-26   1.1  Ej      Export of the DLL function table as variable
|                           Use of CAPL_DLL_INFO3
|                           Support of long name CAPL function calls
| 2020-01-23   1.2  As      Support for GCC and Clang compiler on Linux
|                           Support for MINGW-64 compiler on Windows
|-----------------------------------------------------------------------------
|               C O P Y R I G H T
|-----------------------------------------------------------------------------
| Copyright (c) 1994 - 2003 by Vector Informatik GmbH.  All rights reserved.
 ----------------------------------------------------------------------------*/


#define USECDLL_FEATURE
#define _BUILDNODELAYERDLL

#include <iostream>
#include "../Includes/cdll.h"
#include "../Includes/VIA.h"
#include "../Includes/VIA_CDLL.h"

#include "json.h"
#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"json_vc71_libmtd.lib")


#if defined(_WIN64) || defined(__linux__)
  #define X64
#endif


class CaplInstanceData;
typedef std::map<uint32_t, CaplInstanceData*> VCaplMap;
typedef std::map<uint32_t, VIACapl*> VServiceMap;


// ============================================================================
// global variables
// ============================================================================

static uint32_t data = 0;
static char dlldata[100];

VCaplMap    gCaplMap;
VServiceMap gServiceMap;


// ============================================================================
// CaplInstanceData
//
// Data local for a single CAPL Block.
//
// A CAPL-DLL can be used by more than one CAPL-Block, so every piece of
// information thats like a global variable in CAPL, must now be wrapped into
// an instance of an object.
// ============================================================================
class CaplInstanceData
{
public:
  CaplInstanceData(VIACapl* capl);

  void GetCallbackFunctions();
  void ReleaseCallbackFunctions();

  // Definition of the class function.
  // This class function will call the CAPL callback functions
  uint32_t ShowValue(uint32_t x);
  uint32_t ShowDates(int16_t x, uint32_t y, int16_t z);
  void     DllInfo(const char* x);
  void     ArrayValues(uint32_t flags, uint32_t numberOfDatabytes, uint8_t databytes[], uint8_t controlcode);
  void     DllVersion(const char* y);

private:

  // Pointer of the CAPL callback functions
  VIACaplFunction*  mShowValue;
  VIACaplFunction*  mShowDates;
  VIACaplFunction*  mDllInfo;
  VIACaplFunction*  mArrayValues;
  VIACaplFunction*  mDllVersion;

  VIACapl*          mCapl;
};


CaplInstanceData::CaplInstanceData(VIACapl* capl)
  // This function will initialize the CAPL callback function
  // with the NLL Pointer
 : mCapl(capl),
   mShowValue(nullptr),
   mShowDates(nullptr),
   mDllInfo(nullptr),
   mArrayValues(nullptr),
   mDllVersion(nullptr)
{}

static bool sCheckParams(VIACaplFunction* f, char rtype, const char* ptype)
{
  char      type;
  int32_t   pcount;
  VIAResult rc;

  // check return type
  rc = f->ResultType(&type);
  if (rc!=kVIA_OK || type!=rtype)
  {
    return false;
  }

  // check number of parameters
  rc = f->ParamCount(&pcount);
  if (rc!=kVIA_OK || strlen(ptype)!=pcount )
  {
    return false;
  }

  // check type of parameters
  for (int32_t i=0; i<pcount; ++i)
  {
    rc = f->ParamType(&type, i);
    if (rc!=kVIA_OK || type!=ptype[i])
    {
      return false;
    }
  }

  return true;
}

static VIACaplFunction* sGetCaplFunc(VIACapl* capl, const char* fname, char rtype, const char* ptype)
{
  VIACaplFunction* f;

  // get capl function object
  VIAResult rc =  capl->GetCaplFunction(&f, fname);
  if (rc!=kVIA_OK || f==nullptr)
  {
    return nullptr;
  }

  // check signature of function
  if ( sCheckParams(f, rtype, ptype) )
  {
     return f;
  }
  else
  {
    capl->ReleaseCaplFunction(f);
    return nullptr;
  }
}

void CaplInstanceData::GetCallbackFunctions()
{
  // Get a CAPL function handle. The handle stays valid until end of
  // measurement or a call of ReleaseCaplFunction.
  mShowValue   = sGetCaplFunc(mCapl, "CALLBACK_ShowValue", 'D', "D");
  mShowDates   = sGetCaplFunc(mCapl, "CALLBACK_ShowDates", 'D', "IDI");
  mDllInfo     = sGetCaplFunc(mCapl, "CALLBACK_DllInfo", 'V', "C");
  mArrayValues = sGetCaplFunc(mCapl, "CALLBACK_ArrayValues", 'V', "DBB");
  mDllVersion  = sGetCaplFunc(mCapl, "CALLBACK_DllVersion", 'V', "C");
}

void CaplInstanceData::ReleaseCallbackFunctions()
{
  // Release all the requested Callback functions
  mCapl->ReleaseCaplFunction(mShowValue);
  mShowValue = nullptr;
  mCapl->ReleaseCaplFunction(mShowDates);
  mShowDates = nullptr;
  mCapl->ReleaseCaplFunction(mDllInfo);
  mDllInfo = nullptr;
  mCapl->ReleaseCaplFunction(mArrayValues);
  mArrayValues = nullptr;
  mCapl->ReleaseCaplFunction(mDllVersion);
  mDllVersion = nullptr;
}

void CaplInstanceData::DllVersion(const char* y)
{
  // Prepare the parameters for the call stack of CAPL.
  // Arrays uses a 8 byte (64 bit DLL: 12 byte) on the stack, 4 Bytes for the number of element,
  // and 4 bytes (64 bit DLL: 8 byte) for the pointer to the array
  int32_t sizeX = (int32_t)strlen(y)+1;

#if defined(X64)
  uint8_t params[16];              // parameters for call stack, 16 Bytes total (8 bytes per parameter, reverse order of parameters)
  memcpy(params+8, &sizeX, 4);   // array size    of first parameter, 4 Bytes
  memcpy(params+0, &y,     8);   // array pointer of first parameter, 8 Bytes
#else
  uint8_t params[8];               // parameters for call stack, 8 Bytes total
  memcpy(params+0, &sizeX, 4);   // array size    of first parameter, 4 Bytes
  memcpy(params+4, &y,     4);   // array pointer of first parameter, 4 Bytes
#endif

  if(mDllVersion!=nullptr)
  {
    uint32_t result; // dummy variable
    VIAResult rc =  mDllVersion->Call(&result, params);
  }
}


uint32_t CaplInstanceData::ShowValue(uint32_t x)
{
#if defined(X64)
  uint8_t params[8];               // parameters for call stack, 8 Bytes total
  memcpy(params + 0, &x, 8);     // first parameter, 8 Bytes
#else
  void* params = &x;   // parameters for call stack
#endif

  uint32_t result;

  if(mShowValue!=nullptr)
  {
    VIAResult rc =  mShowValue->Call(&result, params);
    if (rc==kVIA_OK)
    {
       return result;
    }
  }
  return -1;
}

uint32_t CaplInstanceData::ShowDates(int16_t x, uint32_t y, int16_t z)
{
  // Prepare the parameters for the call stack of CAPL. The stack grows
  // from top to down, so the first parameter in the parameter list is the last
  // one in memory. CAPL uses also a 32 bit alignment for the parameters.

#if defined(X64)
  uint8_t params[24];          // parameters for call stack, 24 Bytes total (8 bytes per parameter, reverse order of parameters)
  memcpy(params+16, &z, 2);  // third  parameter, offset 16, 2 Bytes
  memcpy(params+ 8, &y, 4);  // second parameter, offset 8,  4 Bytes
  memcpy(params+ 0, &x, 2);  // first  parameter, offset 0,  2 Bytes
#else
  uint8_t params[12];         // parameters for call stack, 12 Bytes total
  memcpy(params+0, &z, 2);  // third  parameter, offset 0, 2 Bytes
  memcpy(params+4, &y, 4);  // second parameter, offset 4, 4 Bytes
  memcpy(params+8, &x, 2);  // first  parameter, offset 8, 2 Bytes
#endif

  uint32_t result;

  if(mShowDates!=nullptr)
  {
    VIAResult rc =  mShowDates->Call(&result, params);
    if (rc==kVIA_OK)
    {
       return rc;   // call successful
    }
  }

  return -1; // call failed
}

void CaplInstanceData::DllInfo(const char* x)
{
  // Prepare the parameters for the call stack of CAPL.
  // Arrays uses a 8 byte (64 bit DLL: 12 byte) on the stack, 4 Bytes for the number of element,
  // and 4 bytes (64 bit DLL: 8 byte) for the pointer to the array
  int32_t sizeX = (int32)strlen(x)+1;

#if defined(X64)
  uint8_t params[16];              // parameters for call stack, 16 Bytes total (8 bytes per parameter, reverse order of parameters)
  memcpy(params+8, &sizeX, 4);   // array size    of first parameter, 4 Bytes
  memcpy(params+0, &x,     8);   // array pointer of first parameter, 8 Bytes
#else
  uint8_t params[8];               // parameters for call stack, 8 Bytes total
  memcpy(params+0, &sizeX, 4);   // array size    of first parameter, 4 Bytes
  memcpy(params+4, &x,     4);   // array pointer of first parameter, 4 Bytes
#endif

  if(mDllInfo!=nullptr)
  {
    uint32_t result; // dummy variable
    VIAResult rc =  mDllInfo->Call(&result, params);
  }
}

void CaplInstanceData::ArrayValues(uint32_t flags, uint32_t numberOfDatabytes, uint8_t databytes[], uint8_t controlcode)
{
  // Prepare the parameters for the call stack of CAPL. The stack grows
  // from top to down, so the first parameter in the parameter list is the last
  // one in memory. CAPL uses also a 32 bit alignment for the parameters.
  // Arrays uses a 8 byte (64 bit DLL: 12 byte) on the stack, 4 Bytes for the number of element,
  // and 4 bytes (64 bit DLL: 8 byte) for the pointer to the array

#if defined(X64)
  uint8_t params[32];                           // parameters for call stack, 32 Bytes total (8 bytes per parameter, reverse order of parameters)
  memcpy(params+24, &controlcode,       1);   // third parameter,                  offset 24, 1 Bytes
  memcpy(params+16, &numberOfDatabytes, 4);   // second parameter (array size),    offset 16, 4 Bytes
  memcpy(params+ 8, &databytes,         8);   // second parameter (array pointer), offset  8, 8 Bytes
  memcpy(params+ 0, &flags,             4);   // first  parameter,                 offset  0, 4 Bytes
#else
  uint8_t params[16];                           // parameters for call stack, 16 Bytes total
  memcpy(params+ 0, &controlcode,       1);   // third parameter,                  offset  0, 1 Bytes
  memcpy(params+ 4, &numberOfDatabytes, 4);   // second parameter (array size),    offset  4, 4 Bytes
  memcpy(params+ 8, &databytes,         4);   // second parameter (array pointer), offset  8, 4 Bytes
  memcpy(params+12, &flags,             4);   // first  parameter,                 offset 12, 4 Bytes
#endif

  if(mArrayValues!=nullptr)
  {
    uint32_t result; // dummy variable
    VIAResult rc =  mArrayValues ->Call(&result, params);
  }

}

CaplInstanceData* GetCaplInstanceData(uint32_t handle)
{
  VCaplMap::iterator lSearchResult(gCaplMap.find(handle));
  if ( gCaplMap.end()==lSearchResult )
  {
    return nullptr;
  } 
  else 
  {
    return lSearchResult->second;
  }
}

// ============================================================================
// CaplInstanceData
//
// Data local for a single CAPL Block.
//
// A CAPL-DLL can be used by more than one CAPL-Block, so every piece of
// information thats like a global variable in CAPL, must now be wrapped into
// an instance of an object.
// ============================================================================

void CAPLEXPORT CAPLPASCAL appInit (uint32_t handle)
{
  CaplInstanceData* instance = GetCaplInstanceData(handle);
  if ( nullptr==instance )
  {
    VServiceMap::iterator lSearchService(gServiceMap.find(handle));
    if ( gServiceMap.end()!=lSearchService )
    {
      VIACapl* service = lSearchService->second;
      try
      {
        instance = new CaplInstanceData(service);
      }
      catch ( std::bad_alloc& )
      {
        return; // proceed without change
      }
      instance->GetCallbackFunctions();
      gCaplMap[handle] = instance;
    }
  }
}

void CAPLEXPORT CAPLPASCAL appEnd (uint32_t handle)
{
  CaplInstanceData* inst = GetCaplInstanceData(handle);
  if (inst==nullptr)
  {
    return;
  }
  inst->ReleaseCallbackFunctions();

  delete inst;
  inst = nullptr;
  gCaplMap.erase(handle);
}

int32_t CAPLEXPORT CAPLPASCAL appSetValue (uint32_t handle, int32_t x)
{
  CaplInstanceData* inst = GetCaplInstanceData(handle);
  if (inst==nullptr)
  {
    return -1;
  }

  return inst->ShowValue(x);
}

int32_t CAPLEXPORT CAPLPASCAL appReadData (uint32_t handle, int32_t a)
{
  CaplInstanceData* inst = GetCaplInstanceData(handle);
  if (inst==nullptr)
  {
    return -1;
  }

  int16_t  x = (a>=0) ? +1 : -1;
  uint32_t y = abs(a);
  int16_t  z = (int16)(a & 0x0f000000) >> 24;

  inst->DllVersion("Version 1.1");

  inst->DllInfo("DLL: processing");

  uint8_t databytes[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

  inst->ArrayValues( 0xaabbccdd, sizeof(databytes), databytes, 0x01);

  return inst->ShowDates( x, y, z);
}


// ============================================================================
// VIARegisterCDLL
// ============================================================================

VIACLIENT(void) VIARegisterCDLL (VIACapl* service)
{
  uint32_t  handle;
  VIAResult result;

  if (service==nullptr)
  {
    return;
  }

  result = service->GetCaplHandle(&handle);
  if(result!=kVIA_OK)
  {
    return;
  }

  // appInit (internal) resp. "DllInit" (CAPL code) has to follow
  gServiceMap[handle] = service;
}

void ClearAll()
{
  // destroy objects created by this DLL
  // may result from forgotten DllEnd calls
  VCaplMap::iterator lIter=gCaplMap.begin();
  const int32_t cNumberOfEntries = (int32_t)gCaplMap.size();
  int32_t i = 0;
  while ( lIter!=gCaplMap.end() && i<cNumberOfEntries )
  {
    appEnd( (*lIter).first );
    lIter = gCaplMap.begin(); // first element should have vanished
    i++; // assure that no more erase trials take place than the original size of the map
  }

  // just for clarity (would be done automatically)
  gCaplMap.clear();
  gServiceMap.clear();
}

/*
void CAPLEXPORT CAPLPASCAL voidFct( void )
{
   // do something
   data = 55;
}

uint32_t CAPLEXPORT CAPLPASCAL appLongFuncName( void )
{
  return 1;
}

void CAPLEXPORT CAPLPASCAL appPut(uint32_t x)
{
  data = x;
}

uint32_t CAPLEXPORT CAPLPASCAL appGet(void)
{
  return data;
}

int32_t CAPLEXPORT CAPLPASCAL appAdd(int32_t x, int32_t y)
{
  int32_t z = x + y;

  return z;
}

int32_t CAPLEXPORT CAPLPASCAL appSubtract(int32_t x, int32_t y)
{
  int32_t z = x - y;

  return z;
}

int32_t CAPLEXPORT CAPLPASCAL appAddValues63(int32_t val01, int32_t val02, int32_t val03, int32_t val04, int32_t val05, int32_t val06, int32_t val07, int32_t val08,
                                             int32_t val09, int32_t val10, int32_t val11, int32_t val12, int32_t val13, int32_t val14, int32_t val15, int32_t val16,
                                             int32_t val17, int32_t val18, int32_t val19, int32_t val20, int32_t val21, int32_t val22, int32_t val23, int32_t val24,
                                             int32_t val25, int32_t val26, int32_t val27, int32_t val28, int32_t val29, int32_t val30, int32_t val31, int32_t val32,
                                             int32_t val33, int32_t val34, int32_t val35, int32_t val36, int32_t val37, int32_t val38, int32_t val39, int32_t val40,
                                             int32_t val41, int32_t val42, int32_t val43, int32_t val44, int32_t val45, int32_t val46, int32_t val47, int32_t val48,
                                             int32_t val49, int32_t val50, int32_t val51, int32_t val52, int32_t val53, int32_t val54, int32_t val55, int32_t val56,
                                             int32_t val57, int32_t val58, int32_t val59, int32_t val60, int32_t val61, int32_t val62, int32_t val63)
{
  int32_t z = val01+val02+val03+val04+val05+val06+val07+val08
            + val09+val10+val11+val12+val13+val14+val15+val16
            + val17+val18+val19+val20+val21+val22+val23+val24
            + val25+val26+val27+val28+val29+val30+val31+val32
            + val33+val34+val35+val36+val37+val38+val39+val40
            + val41+val42+val43+val44+val45+val46+val47+val48
            + val49+val50+val51+val52+val53+val54+val55+val56
            + val57+val58+val59+val60+val61+val62+val63;

  return z;
}

int32_t CAPLEXPORT CAPLPASCAL appAddValues64(int32_t val01, int32_t val02, int32_t val03, int32_t val04, int32_t val05, int32_t val06, int32_t val07, int32_t val08,
                                             int32_t val09, int32_t val10, int32_t val11, int32_t val12, int32_t val13, int32_t val14, int32_t val15, int32_t val16,
                                             int32_t val17, int32_t val18, int32_t val19, int32_t val20, int32_t val21, int32_t val22, int32_t val23, int32_t val24,
                                             int32_t val25, int32_t val26, int32_t val27, int32_t val28, int32_t val29, int32_t val30, int32_t val31, int32_t val32,
                                             int32_t val33, int32_t val34, int32_t val35, int32_t val36, int32_t val37, int32_t val38, int32_t val39, int32_t val40,
                                             int32_t val41, int32_t val42, int32_t val43, int32_t val44, int32_t val45, int32_t val46, int32_t val47, int32_t val48,
                                             int32_t val49, int32_t val50, int32_t val51, int32_t val52, int32_t val53, int32_t val54, int32_t val55, int32_t val56,
                                             int32_t val57, int32_t val58, int32_t val59, int32_t val60, int32_t val61, int32_t val62, int32_t val63, int32_t val64)
{
  int32_t z = val01+val02+val03+val04+val05+val06+val07+val08
            + val09+val10+val11+val12+val13+val14+val15+val16
            + val17+val18+val19+val20+val21+val22+val23+val24
            + val25+val26+val27+val28+val29+val30+val31+val32
            + val33+val34+val35+val36+val37+val38+val39+val40
            + val41+val42+val43+val44+val45+val46+val47+val48
            + val49+val50+val51+val52+val53+val54+val55+val56
            + val57+val58+val59+val60+val61+val62+val63+val64;

  return z;
}
#define EightLongPars 'L','L','L','L','L','L','L','L'
#define SixtyFourLongPars EightLongPars,EightLongPars,EightLongPars,EightLongPars,EightLongPars,EightLongPars,EightLongPars,EightLongPars

void CAPLEXPORT CAPLPASCAL appGetDataTwoPars(uint32_t numberBytes, uint8_t dataBlock[] )
{
  unsigned int i;
  for (i = 0; i < numberBytes; i++) 
  {
    dataBlock[i] = dlldata[i];
  }
}

void CAPLEXPORT CAPLPASCAL appPutDataTwoPars(uint32_t numberBytes, const uint8_t dataBlock[] )
{
  unsigned int i;
  for (i = 0; i < numberBytes; i++) 
  {
    dlldata[i] = dataBlock[i];
  }
}

// get data from DLL into CAPL memory
void CAPLEXPORT CAPLPASCAL appGetDataOnePar(uint8_t dataBlock[] )
{
  //  get first element
  dataBlock[0] = (uint8_t)data;
}

// put data from CAPL array to DLL
void CAPLEXPORT CAPLPASCAL appPutDataOnePar(const uint8_t dataBlock[] )
{
  // put first element
  data = dataBlock[0];

}
*/

//add new function
Json::Value root;
Json::StyledWriter style_writer;
Json::Reader reader;
WSADATA wsaData;

SOCKET SendSocket;
sockaddr_in RecvAddr;
int Port = 20000;
const char* addr = "127.0.0.1";

SOCKET RecvSocket;
char RecvBuf[1024];
int BufLen = 1024;
sockaddr_in SenderAddr;
int SendAddrSize = sizeof(SenderAddr);

//to implement(Set Function)
void CAPLEXPORT CAPLPASCAL appSetLongtitude(int32_t longtitude)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Longtitude"] = longtitude;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetLatiude(int32_t latitude)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Latitude"] = latitude;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetTransmissionState(int32_t transmission_state)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Transmission"] = transmission_state;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetSpeed(int32_t speed)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Speed"] = speed;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetHeading(int32_t heading)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Heading"] = heading;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetLatAcceleration(int32_t latitude_acceleration)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Acc_Lat"] = latitude_acceleration;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetLongAcceleration(int32_t longtitude_acceleration)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Acc_Lng"] = longtitude_acceleration;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetBasicVehicleClass(int32_t vehicle_class)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Veh_Class"] = vehicle_class;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetEvent(char* events)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Events"] = events;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetEmergencyExtensionsResponseType(int32_t response_type)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Response_Type"] = response_type;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetEmergencyExtensionsLightBarInUse(int32_t light_use)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Lights_Use"] = light_use;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
//reservation (Set Function)
void CAPLEXPORT CAPLPASCAL appSetId(char* id)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Id"] = id;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetSecMark(int32_t sec_mark)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["time_stamp"] = sec_mark;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetElevation(int32_t elevation)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Elevation"] = elevation;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetSemiMajor(int32_t accuracy_semi_major)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Smajor_dev"] = accuracy_semi_major;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetSemiMinor(int32_t accuracy_semi_minor)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Sminor_dev"] = accuracy_semi_minor;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetOrientation(int32_t accuracy_orientation)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Smajor_Orien"] = accuracy_orientation;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetConfidencePosition(int32_t confidence_position)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Pos_Confidence_Pos"] = confidence_position;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetConfidenceElevation(int32_t confidence_elevation)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Pos_Confidence_Ele"] = confidence_elevation;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetWheelAngle(int32_t angle)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Wheel_Angle"] = angle;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetVertAcceleration(int32_t vert_acceleration)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Acc_Vert"] = vert_acceleration;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetYawAcceleration(int32_t yaw_acceleration)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Yaw_Rate"] = yaw_acceleration;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetBrakePadel(int32_t brake_padel)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Brake_Padel"] = brake_padel;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetWheelBrakes(char* wheel_brakes)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Wheel_Brakes"] = wheel_brakes;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetTraction(int32_t traction)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Traction"] = traction;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetABS(int32_t abs)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["ABS"] = abs;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetSCS(int32_t scs)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["SCS"] = scs;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetBrakeBoost(int32_t brake_boost)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Brake_Boost"] = brake_boost;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetAuxBrakes(int32_t aux_brakes)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Aux_Brakes"] = aux_brakes;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetVehicleWidth(int32_t vehicle_width)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Veh_Width"] = vehicle_width;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetVehicleLenth(int32_t vehicle_lenth)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Veh_Len"] = vehicle_lenth;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetVehicleHeight(int32_t vehicle_height)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Veh_Height"] = vehicle_height;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetVehicleFuelType(int32_t vehicle_fuel_type)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Veh_Fuel_Type"] = vehicle_fuel_type;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetLights(char* lights)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Lights"] = lights;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
void CAPLEXPORT CAPLPASCAL appSetSirenUse(int32_t siren_use)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = inet_addr(addr);
	root["Siren_Use"] = siren_use;
	std::string SendBuf = style_writer.write(root);
	sendto(SendSocket, SendBuf.c_str(), SendBuf.size(), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	closesocket(SendSocket);
	WSACleanup();
	return;
}
//to implement(Get Function)
int32_t CAPLEXPORT CAPLPASCAL appGetLatiude(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Latiude"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetLongtitude(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Longtitude"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetTransmissionState(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Transmission"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetSpeed(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Speed"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetHeading(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Heading"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetLatAcceleration(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Acc_Lat"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetLongAcceleration(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Acc_Lng"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetBasicVehicleClass(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Veh_Class"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetEvent(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Events"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetEmergencyExtensionsResponseType(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Response_Type"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetEmergencyExtensionsLightBarInUse(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Lights_Use"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
//reservation (Get Function)
int32_t CAPLEXPORT CAPLPASCAL appGetId(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Id"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetSecMark(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["time_stamp"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetElevation(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Elevation"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetSemiMajor(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Smajor_dev"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetSemiMinor(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Sminor_dev"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetOrientation(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Smajor_Orien"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetConfidencePosition(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Pos_Confidence_Pos"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetConfidenceElevation(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Pos_Confidence_Ele"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetWheelAngle(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Wheel_Angle"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetVertAcceleration(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Acc_Vert"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetYawAcceleration(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Yaw_Rate"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetBrakePadel(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Brake_Padel"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetWheelBrakes(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Wheel_Brakes"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetTraction(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Traction"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetABS(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["ABS"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetSCS(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["SCS"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetBrakeBoost(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Brake_Boost"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetAuxBrakes(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Aux_Brakes"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetVehicleWidth(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Veh_Width"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetVehicleLenth(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Veh_Len"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetVehicleHeight(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Veh_Height"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetVehicleFuelType(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Veh_Fuel_Type"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetLights(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Lights"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}
int32_t CAPLEXPORT CAPLPASCAL appGetSirenUse(void)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(Port);
	RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
	recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SendAddrSize);
	if (reader.parse(RecvBuf, root))
	{
		return root["Siren_Use"].asInt();
	}
	closesocket(RecvSocket);
	WSACleanup();
}

// ============================================================================
// CAPL_DLL_INFO_LIST : list of exported functions
//   The first field is predefined and mustn't be changed!
//   The list has to end with a {0,0} entry!
// New struct supporting function names with up to 50 characters
// ============================================================================
/*CAPL_DLL_INFO4 table[] = {
{CDLL_VERSION_NAME, (CAPL_FARCALL)CDLL_VERSION, "", "", CAPL_DLL_CDECL, 0xabcd, CDLL_EXPORT },

  {"dllInit",           (CAPL_FARCALL)appInit,          "CAPL_DLL","This function will initialize all callback functions in the CAPLDLL",'V', 1, "D", "", {"handle"}},
  {"dllEnd",            (CAPL_FARCALL)appEnd,           "CAPL_DLL","This function will release the CAPL function handle in the CAPLDLL",'V', 1, "D", "", {"handle"}},
  {"dllSetValue",       (CAPL_FARCALL)appSetValue,      "CAPL_DLL","This function will call a callback functions",'L', 2, "DL", "", {"handle","x"}},
  {"dllReadData",       (CAPL_FARCALL)appReadData,      "CAPL_DLL","This function will call a callback functions",'L', 2, "DL", "", {"handle","x"}},
  {"dllPut",            (CAPL_FARCALL)appPut,           "CAPL_DLL","This function will save data from CAPL to DLL memory",'V', 1, "D", "", {"x"}},
  {"dllGet",            (CAPL_FARCALL)appGet,           "CAPL_DLL","This function will read data from DLL memory to CAPL",'D', 0, "", "", {""}},
  {"dllVoid",           (CAPL_FARCALL)voidFct,          "CAPL_DLL","This function will overwrite DLL memory from CAPL without parameter",'V', 0, "", "", {""}},
  {"dllPutDataOnePar",  (CAPL_FARCALL)appPutDataOnePar, "CAPL_DLL","This function will put data from CAPL array to DLL",'V', 1, "B", "\001", {"datablock"}},
  {"dllGetDataOnePar",  (CAPL_FARCALL)appGetDataOnePar, "CAPL_DLL","This function will get data from DLL into CAPL memory",'V', 1, "B", "\001", {"datablock"}},
  {"dllPutDataTwoPars", (CAPL_FARCALL)appPutDataTwoPars,"CAPL_DLL","This function will put two datas from CAPL array to DLL",'V', 2, "DB", "\000\001", {"noOfBytes","datablock"}},// number of pars in octal format
  {"dllGetDataTwoPars", (CAPL_FARCALL)appGetDataTwoPars,"CAPL_DLL","This function will get two datas from DLL into CAPL memory",'V', 2, "DB", "\000\001", {"noOfBytes","datablock"}},
  {"dllAdd",            (CAPL_FARCALL)appAdd,           "CAPL_DLL","This function will add two values. The return value is the result",'L', 2, "LL", "", {"x","y"}},
  {"dllSubtract",       (CAPL_FARCALL)appSubtract,      "CAPL_DLL","This function will substract two values. The return value is the result",'L', 2, "LL", "", {"x","y"}},
  {"dllSupportLongFunctionNamesWithUpTo50Characters",   (CAPL_FARCALL)appLongFuncName,      "CAPL_DLL","This function shows the support of long function names",'D', 0, "", "", {""}},
  {"dllAdd63Parameters", (CAPL_FARCALL)appAddValues63,  "CAPL_DLL", "This function will add 63 values. The return value is the result",'L', 63, "LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL",  "", {"val01","val02","val03","val04","val05","val06","val07","val08","val09","val10","val11","val12","val13","val14","val15","val16","val17","val18","val19","val20","val21","val22","val23","val24","val25","val26","val27","val28","val29","val30","val31","val32","val33","val34","val35","val36","val37","val38","val39","val40","val41","val42","val43","val44","val45","val46","val47","val48","val49","val50","val51","val52","val53","val54","val55","val56","val57","val58","val59","val60","val61","val62","val63"}},
  {"dllAdd64Parameters", (CAPL_FARCALL)appAddValues64,  "CAPL_DLL", "This function will add 64 values. The return value is the result",'L', 64, {SixtyFourLongPars},                                                "", {"val01","val02","val03","val04","val05","val06","val07","val08","val09","val10","val11","val12","val13","val14","val15","val16","val17","val18","val19","val20","val21","val22","val23","val24","val25","val26","val27","val28","val29","val30","val31","val32","val33","val34","val35","val36","val37","val38","val39","val40","val41","val42","val43","val44","val45","val46","val47","val48","val49","val50","val51","val52","val53","val54","val55","val56","val57","val58","val59","val60","val61","val62","val63","val64"}},

{0, 0}
};*/
CAPL_DLL_INFO4 table[] = {
{CDLL_VERSION_NAME, (CAPL_FARCALL)CDLL_VERSION, "", "", CAPL_DLL_CDECL, 0xabcd, CDLL_EXPORT },

  {"dllInit",								(CAPL_FARCALL)appInit,									"CAPL_DLL","This function will initialize all callback functions in the CAPLDLL",'V', 1, "D", "", {"handle"}},
  {"dllEnd",								(CAPL_FARCALL)appEnd,									"CAPL_DLL","This function will release the CAPL function handle in the CAPLDLL",'V', 1, "D", "", {"handle"}},
  {"dllSetValue",							(CAPL_FARCALL)appSetValue,								"CAPL_DLL","This function will call a callback functions",'L', 2, "DL", "", {"handle","x"}},
  {"dllReadData",							(CAPL_FARCALL)appReadData,								"CAPL_DLL","This function will call a callback functions",'L', 2, "DL", "", {"handle","x"}},
  {"SetLatiude",							(CAPL_FARCALL)appSetLatiude,							"Set_Func","This function will send Latiude from CAPL to ROS",'V', 1, "L", "", {"latitude"}},
  {"SetLongtitude",							(CAPL_FARCALL)appSetLongtitude,							"Set_Func","This function will send Longtitude from CAPL to ROS",'V', 1, "L", "", {"longtitude"}},
  {"SetTransmissionState",					(CAPL_FARCALL)appSetTransmissionState,					"Set_Func","This function will send Transmission State from CAPL to ROS",'V', 1, "L", "", {"transmission_state"}},
  {"SetSpeed",								(CAPL_FARCALL)appSetSpeed,								"Set_Func","This function will send Heading from CAPL to ROS",'V', 1, "L", "", {"speed"}},
  {"SetHeading",							(CAPL_FARCALL)appSetHeading,							"Set_Func","This function will send Speed from CAPL to ROS",'V', 1, "L", "", {"heading"}},
  {"SetLatAcceleration",					(CAPL_FARCALL)appSetLatAcceleration,					"Set_Func","This function will send Latitude Acceleration from CAPL to ROS",'V', 1, "L", "", {"latitude_acceleration"}},
  {"SetLongAcceleration",					(CAPL_FARCALL)appSetLongAcceleration,					"Set_Func","This function will send Longitude Acceleration from CAPL to ROS",'V', 1, "L", "", {"longtitude_acceleration"}},
  {"SetBasicVehicleClass",					(CAPL_FARCALL)appSetBasicVehicleClass,					"Set_Func","This function will send Basic Vehicle Class from CAPL to ROS",'V', 1, "L", "", {"vehicle_class"}},
  {"SetEvent",								(CAPL_FARCALL)appSetEvent,								"Set_Func","This function will send Event from CAPL to ROS",'V', 1, "C", "\001", {"events"}},
  {"SetEmergencyExtensionsResponseType",    (CAPL_FARCALL)appSetEmergencyExtensionsResponseType,    "Set_Func","This function will send Emergency Extensions Response Type from CAPL to ROS",'V', 1, "L", "", {"response_type"}},
  {"SetEmergencyExtensionsLightBarInUse",   (CAPL_FARCALL)appSetEmergencyExtensionsLightBarInUse,   "Set_Func","This function will send Emergency Extensions Light Bar In Use from CAPL to ROS",'V', 1, "L", "", {"light_use"}},
  {"SetId",									(CAPL_FARCALL)appSetId,									"Set_Func","This function will send Id from CAPL to ROS",'V', 1, "C", "\001", {"id"}},
  {"SetSecMark",							(CAPL_FARCALL)appSetSecMark,							"Set_Func","This function will send Sec Mark from CAPL to ROS",'V', 1, "L", "", {"sec_mark"}},
  {"SetElevation",							(CAPL_FARCALL)appSetElevation,							"Set_Func","This function will send Elevation from CAPL to ROS",'V', 1, "L", "", {"elevation"}},
  {"SetSemiMajor",							(CAPL_FARCALL)appSetSemiMajor,							"Set_Func","This function will send Semi Major from CAPL to ROS",'V', 1, "L", "", {"accuracy_semi_major"}},
  {"SetSemiMinor",							(CAPL_FARCALL)appSetSemiMinor,							"Set_Func","This function will send Semi Minor from CAPL to ROS",'V', 1, "L", "", {"accuracy_semi_minor"}},
  {"SetOrientation",						(CAPL_FARCALL)appSetOrientation,						"Set_Func","This function will send Orientation from CAPL to ROS",'V', 1, "L", "", {"accuracy_orientation"}},
  {"SetConfidencePosition",					(CAPL_FARCALL)appSetConfidencePosition,					"Set_Func","This function will send Confidence Position from CAPL to ROS",'V', 1, "L", "", {"confidence_position"}},
  {"SetConfidenceElevation",				(CAPL_FARCALL)appSetConfidenceElevation,				"Set_Func","This function will send Confidence Elevation from CAPL to ROS",'V', 1, "L", "", {"confidence_elevation"}},
  {"SetWheelAngle",							(CAPL_FARCALL)appSetWheelAngle,							"Set_Func","This function will send Wheel Angle from CAPL to ROS",'V', 1, "L", "", {"angle"}},
  {"SetVertAcceleration",					(CAPL_FARCALL)appSetVertAcceleration,					"Set_Func","This function will send Vert Acceleration from CAPL to ROS",'V', 1, "L", "", {"vert_acceleration"}},
  {"SetYawAcceleration",					(CAPL_FARCALL)appSetYawAcceleration,					"Set_Func","This function will send Yaw Acceleration from CAPL to ROS",'V', 1, "L", "", {"yaw_acceleration"}},
  {"SetBrakePadel",							(CAPL_FARCALL)appSetBrakePadel,							"Set_Func","This function will send Brake Padel from CAPL to ROS",'V', 1, "L", "", {"brake_padel"}},
  {"SetWheelBrakes",						(CAPL_FARCALL)appSetWheelBrakes,						"Set_Func","This function will send Wheel Brakes from CAPL to ROS",'V', 1, "C", "\001", {"wheel_brakes"}},
  {"SetTraction",							(CAPL_FARCALL)appSetTraction,							"Set_Func","This function will send Traction from CAPL to ROS",'V', 1, "L", "", {"traction"}},
  {"SetABS",								(CAPL_FARCALL)appSetABS,								"Set_Func","This function will send ABS from CAPL to ROS",'V', 1, "L", "", {"abs"}},
  {"SetSCS",								(CAPL_FARCALL)appSetSCS,								"Set_Func","This function will send SCS from CAPL to ROS",'V', 1, "L", "", {"scs"}},
  {"SetBrakeBoost",							(CAPL_FARCALL)appSetBrakeBoost,							"Set_Func","This function will send Brake Boost from CAPL to ROS",'V', 1, "L", "", {"brake_boost"}},
  {"SetAuxBrakes",							(CAPL_FARCALL)appSetAuxBrakes,							"Set_Func","This function will send Aux Brakes from CAPL to ROS",'V', 1, "L", "", {"aux_brakes"}},
  {"SetVehicleWidth",						(CAPL_FARCALL)appSetVehicleWidth,						"Set_Func","This function will send Vehicle Width from CAPL to ROS",'V', 1, "L", "", {"vehicle_width"}},
  {"SetVehicleLenth",						(CAPL_FARCALL)appSetVehicleLenth,						"Set_Func","This function will send Vehicle Lenth from CAPL to ROS",'V', 1, "L", "", {"vehicle_lenth"}},
  {"SetVehicleHeight",						(CAPL_FARCALL)appSetVehicleHeight,						"Set_Func","This function will send Vehicle Height from CAPL to ROS",'V', 1, "L", "", {"vehicle_height"}},
  {"SetVehicleFuelType",					(CAPL_FARCALL)appSetVehicleFuelType,					"Set_Func","This function will send VehicleFuel Type from CAPL to ROS",'V', 1, "L", "", {"vehicle_fuel_type"}},
  {"SetLights",								(CAPL_FARCALL)appSetLights,								"Set_Func","This function will send Lights from CAPL to ROS",'V', 1, "C", "\001", {"lights"}},
  {"SetSetSirenUse",						(CAPL_FARCALL)appSetSirenUse,							"Set_Func","This function will send Siren Use from CAPL to ROS",'V', 1, "L", "", {"siren_use"}},
  {"GetLatiude",							(CAPL_FARCALL)appGetLatiude,							"Get_Func","This function will receive Latiude from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetLongtitude",							(CAPL_FARCALL)appGetLongtitude,							"Get_Func","This function will receive Longtitude from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetTransmissionState",					(CAPL_FARCALL)appGetTransmissionState,					"Get_Func","This function will receive Transmission State from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetSpeed",								(CAPL_FARCALL)appGetSpeed,								"Get_Func","This function will receive Heading from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetHeading",							(CAPL_FARCALL)appGetHeading,							"Get_Func","This function will receive Speed from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetLatAcceleration",					(CAPL_FARCALL)appGetLatAcceleration,					"Get_Func","This function will receive Latitude Acceleration from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetLongAcceleration",					(CAPL_FARCALL)appGetLongAcceleration,					"Get_Func","This function will receive Longitude Acceleration from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetBasicVehicleClass",					(CAPL_FARCALL)appGetBasicVehicleClass,					"Get_Func","This function will receive Basic Vehicle Class from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetEvent",								(CAPL_FARCALL)appGetEvent,								"Get_Func","This function will receive Event from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetEmergencyExtensionsResponGetype",    (CAPL_FARCALL)appGetEmergencyExtensionsResponseType,    "Get_Func","This function will receive Emergency Extensions Response Type from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetEmergencyExtensionsLightBarInUse",   (CAPL_FARCALL)appGetEmergencyExtensionsLightBarInUse,   "Get_Func","This function will receive Emergency Extensions Light Bar In Use from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetId",									(CAPL_FARCALL)appGetId,									"Get_Func","This function will receive Id from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetSecMark",							(CAPL_FARCALL)appGetSecMark,							"Get_Func","This function will receive Sec Mark from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetElevation",							(CAPL_FARCALL)appGetElevation,							"Get_Func","This function will receive Elevation from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetSemiMajor",							(CAPL_FARCALL)appGetSemiMajor,							"Get_Func","This function will receive Semi Major from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetSemiMinor",							(CAPL_FARCALL)appGetSemiMinor,							"Get_Func","This function will receive Semi Minor from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetOrientation",						(CAPL_FARCALL)appGetOrientation,						"Get_Func","This function will receive Orientation from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetConfidencePosition",					(CAPL_FARCALL)appGetConfidencePosition,					"Get_Func","This function will receive Confidence Position from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetConfidenceElevation",				(CAPL_FARCALL)appGetConfidenceElevation,				"Get_Func","This function will receive Confidence Elevation from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetWheelAngle",							(CAPL_FARCALL)appGetWheelAngle,							"Get_Func","This function will receive Wheel Angle from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetVertAcceleration",					(CAPL_FARCALL)appGetVertAcceleration,					"Get_Func","This function will receive Vert Acceleration from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetYawAcceleration",					(CAPL_FARCALL)appGetYawAcceleration,					"Get_Func","This function will receive Yaw Acceleration from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetBrakePadel",							(CAPL_FARCALL)appGetBrakePadel,							"Get_Func","This function will receive Brake Padel from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetWheelBrakes",						(CAPL_FARCALL)appGetWheelBrakes,						"Get_Func","This function will receive Wheel Brakes from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetTraction",							(CAPL_FARCALL)appGetTraction,							"Get_Func","This function will receive Traction from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetABS",								(CAPL_FARCALL)appGetABS,								"Get_Func","This function will receive ABS from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetSCS",								(CAPL_FARCALL)appGetSCS,								"Get_Func","This function will receive SCS from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetBrakeBoost",							(CAPL_FARCALL)appGetBrakeBoost,							"Get_Func","This function will receive Brake Boost from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetAuxBrakes",							(CAPL_FARCALL)appGetAuxBrakes,							"Get_Func","This function will receive Aux Brakes from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetVehicleWidth",						(CAPL_FARCALL)appGetVehicleWidth,						"Get_Func","This function will receive Vehicle Width from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetVehicleLenth",						(CAPL_FARCALL)appGetVehicleLenth,						"Get_Func","This function will receive Vehicle Lenth from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetVehicleHeight",						(CAPL_FARCALL)appGetVehicleHeight,						"Get_Func","This function will receive Vehicle Height from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetVehicleFuelType",					(CAPL_FARCALL)appGetVehicleFuelType,					"Get_Func","This function will receive VehicleFuel Type from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetLights",								(CAPL_FARCALL)appGetLights,								"Get_Func","This function will receive Lights from ROS to CAPL",'L', 0, "V", "", {""}},
  {"GetGetSirenUse",						(CAPL_FARCALL)appGetSirenUse,							"Get_Func","This function will receive Siren Use from ROS to CAPL",'L', 0, "V", "", {""}},

{0, 0}
};
CAPLEXPORT CAPL_DLL_INFO4* caplDllTable4 = table;




