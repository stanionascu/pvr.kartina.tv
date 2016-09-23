/*
 *      Copyright (C) 2013 Stanislav Ionascu
 *      Stanislav Ionascu
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <stdio.h>
#include "kodi/libXBMC_addon.h"
#include "kodi/libXBMC_pvr.h"
#include "kodi/xbmc_pvr_dll.h"
#include "kodi/libKODI_guilib.h"
#include <p8-platform/util/util.h>

#include "kartinatvclient.h"

#ifdef _WIN32
#   define snprintf _snprintf
#endif // _WIN32

using namespace std;
using namespace ADDON;

bool           m_bCreated       = false;
ADDON_STATUS   m_CurStatus      = ADDON_STATUS_UNKNOWN;
int            m_CurChannelId   = 1;

/* User adjustable settings are saved here.
 * Default values are defined inside client.h
 * and exported to the other source files.
 */
std::string g_strUserPath             = "";
std::string g_strClientPath           = "";
std::string g_strCurrentStreamUrl     = "";

std::string g_strUsername             = "";
std::string g_strPassword             = "";
std::string g_strProtectCode        = "";

CHelper_libXBMC_addon *XBMC           = NULL;
CHelper_libXBMC_pvr   *PVR            = NULL;
KartinaTVClient *CLIENT               = NULL;

extern "C" {

void ADDON_ReadSettings(void)
{
    char buffer[1024];
    if (XBMC->GetSetting("username", &buffer))
        g_strUsername = buffer;
    buffer[0] = 0;

    if (XBMC->GetSetting("password", &buffer))
        g_strPassword = buffer;

    if (XBMC->GetSetting("protect_code", &buffer))
        g_strProtectCode = buffer;

    buffer[0] = 0;
}

ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
    if (!hdl || !props)
        return ADDON_STATUS_UNKNOWN;

    PVR_PROPERTIES* pvrprops = (PVR_PROPERTIES*)props;

    XBMC = new CHelper_libXBMC_addon;
    if (!XBMC->RegisterMe(hdl))
    {
        SAFE_DELETE(XBMC);
        return ADDON_STATUS_PERMANENT_FAILURE;
    }

    PVR = new CHelper_libXBMC_pvr;
    if (!PVR->RegisterMe(hdl))
    {
        SAFE_DELETE(PVR);
        SAFE_DELETE(XBMC);
        return ADDON_STATUS_PERMANENT_FAILURE;
    }

    XBMC->Log(LOG_DEBUG, "%s - Creating the Kartina.TV PVR add-on", __FUNCTION__);

    m_CurStatus     = ADDON_STATUS_UNKNOWN;
    g_strUserPath   = pvrprops->strUserPath;
    g_strClientPath = pvrprops->strClientPath;
    XBMC->Log(LOG_DEBUG, "Add-on path: %s", g_strClientPath.data());
    XBMC->Log(LOG_DEBUG, "User profile path: %s", g_strUserPath.data());

    ADDON_ReadSettings();
    if (g_strUsername.empty() || g_strPassword.empty())
        return ADDON_STATUS_NEED_SETTINGS;

    CLIENT = new KartinaTVClient(XBMC, PVR);
    CLIENT->setUserProfilePath(g_strUserPath);
    CLIENT->setCredentials(g_strUsername, g_strPassword);
    if (!g_strProtectCode.empty())
        CLIENT->setProtectCode(g_strProtectCode);

    if (CLIENT->login())
        XBMC->QueueNotification(ADDON::QUEUE_INFO, "Welcome!");
    else
        return ADDON_STATUS_NEED_SETTINGS;

    m_CurStatus = ADDON_STATUS_OK;
    m_bCreated = true;
    return m_CurStatus;
}

ADDON_STATUS ADDON_GetStatus()
{
    return m_CurStatus;
}

void ADDON_Destroy()
{
    delete CLIENT;
    CLIENT = 0;

    m_bCreated = false;
    m_CurStatus = ADDON_STATUS_UNKNOWN;
}

bool ADDON_HasSettings()
{
    return true;
}

unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{
    return 0;
}

ADDON_STATUS ADDON_SetSetting(const char *settingName, const void *settingValue)
{
    if (strcmp(settingName, "username") == 0 ||
            strcmp(settingName, "password") == 0)
        return ADDON_STATUS_NEED_SETTINGS;

    return ADDON_STATUS_OK;
}

void ADDON_Stop()
{
}

void ADDON_FreeSettings()
{
}

void ADDON_Announce(const char *flag, const char *sender, const char *message, const void *data)
{
}

/***********************************************************
 * PVR Client AddOn specific public library functions
 ***********************************************************/

const char* GetPVRAPIVersion(void)
{
    static const char *strApiVersion = XBMC_PVR_API_VERSION;
    return strApiVersion;
}

const char* GetMininumPVRAPIVersion(void)
{
    static const char *strMinApiVersion = XBMC_PVR_MIN_API_VERSION;
    return strMinApiVersion;
}

const char* GetGUIAPIVersion(void)
{
    static const char *strGuiApiVersion = KODI_GUILIB_API_VERSION;
    return strGuiApiVersion;
}

const char* GetMininumGUIAPIVersion(void)
{
    static const char *strGuiMinApiVersion = KODI_GUILIB_MIN_API_VERSION;
    return strGuiMinApiVersion;
}

PVR_ERROR GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities)
{
    XBMC->Log(LOG_DEBUG, "GetAddonCapabilities!");
    pCapabilities->bSupportsEPG             = true;
    pCapabilities->bSupportsTV              = true;
    pCapabilities->bSupportsRadio           = true;
    pCapabilities->bSupportsChannelGroups   = true;
    pCapabilities->bSupportsRecordings      = false;
    pCapabilities->bHandlesInputStream      = false;
    pCapabilities->bHandlesDemuxing         = false;

    return PVR_ERROR_NO_ERROR;
}

const char *GetBackendName(void)
{
    static const char *strBackendName = "kartina.tv live pvr add-on";
    return strBackendName;
}

const char *GetBackendVersion(void)
{
    static const char *strBackendVersion = "0.1";
    return strBackendVersion;
}

const char *GetConnectionString(void)
{
    static const char *strConnectionString = "connected";
    return strConnectionString;
}

PVR_ERROR GetDriveSpace(long long *iTotal, long long *iUsed)
{
    *iTotal = 0;
    *iUsed  = 0;
    return PVR_ERROR_NO_ERROR;
}

PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
    XBMC->Log(LOG_DEBUG, "KartinaTVClient::loadEpgFromCache");

    if (CLIENT->loadEpgFromCache(handle, channel, iStart, iEnd))
        return PVR_ERROR_NO_ERROR;

    return PVR_ERROR_SERVER_ERROR;
}

int GetChannelsAmount(void)
{
    return CLIENT->channelCount();
}

PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio)
{
    if (CLIENT->loadChannelsFromCache(handle, bRadio))
        return PVR_ERROR_NO_ERROR;

    return PVR_ERROR_SERVER_ERROR;
}

const char *GetLiveStreamURL(const PVR_CHANNEL &channel)
{
    m_CurChannelId = channel.iChannelNumber;
    g_strCurrentStreamUrl = CLIENT->requestStreamUrl(channel);

    XBMC->Log(LOG_DEBUG, "GetLiveStreamURL! %s", g_strCurrentStreamUrl.data());
    return g_strCurrentStreamUrl.data();
}

bool OpenLiveStream(const PVR_CHANNEL &channel)
{
    return true;
}

void CloseLiveStream(void)
{
    XBMC->Log(LOG_DEBUG, "CloseLiveStream!");
    g_strCurrentStreamUrl = "";
}

int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize)
{
    return 0;
}

int GetCurrentClientChannel(void)
{
    XBMC->Log(LOG_DEBUG, "GetCurrentClientChannel!");
    return m_CurChannelId;
}

bool SwitchChannel(const PVR_CHANNEL &channel)
{
    XBMC->Log(LOG_DEBUG, "SwitchChannel!");
    CloseLiveStream();
    return OpenLiveStream(channel);
}

int GetChannelGroupsAmount(void)
{
    return CLIENT->channelGroupsCount();
}

PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
    if (CLIENT->loadChannelGroupsFromCache(handle, bRadio))
        return PVR_ERROR_NO_ERROR;
    return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
    if (CLIENT->loadChannelGroupMembersFromCache(handle, group))
        return PVR_ERROR_NO_ERROR;
    return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR SignalStatus(PVR_SIGNAL_STATUS &signalStatus)
{
    snprintf(signalStatus.strAdapterName, sizeof(signalStatus.strAdapterName), "kartina tv live pvr adapter 1");
    snprintf(signalStatus.strAdapterStatus, sizeof(signalStatus.strAdapterStatus), "OK");

    return PVR_ERROR_NO_ERROR;
}

const char *GetBackendHostname(void)
{
    return KartinaTVClient::API_SERVER.c_str();
}

/** UNUSED API FUNCTIONS */
bool IsRealTimeStream() { return true; }
PVR_ERROR SetEPGTimeFrame(int) { return PVR_ERROR_NOT_IMPLEMENTED; }
void OnSystemSleep() { }
void OnSystemWake() { }
void OnPowerSavingActivated() { }
void OnPowerSavingDeactivated() { }
PVR_ERROR GetTimerTypes(PVR_TIMER_TYPE types[], int *typesCount) { return PVR_ERROR_NOT_IMPLEMENTED; }
bool IsTimeshifting() { return false; }
PVR_ERROR GetStreamProperties(PVR_STREAM_PROPERTIES* pProperties) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetRecordingsAmount(bool deleted) { return -1; }
PVR_ERROR GetRecordings(ADDON_HANDLE handle, bool deleted) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR UndeleteRecording(const PVR_RECORDING& recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteAllRecordingsFromTrash() { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelScan(void) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR CallMenuHook(const PVR_MENUHOOK &menuhook, const PVR_MENUHOOK_DATA &item) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR MoveChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelSettings(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelAdd(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
bool OpenRecordedStream(const PVR_RECORDING &recording) { return false; }
void CloseRecordedStream(void) {}
int ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize) { return 0; }
long long SeekRecordedStream(long long iPosition, int iWhence /* = SEEK_SET */) { return 0; }
long long PositionRecordedStream(void) { return -1; }
long long LengthRecordedStream(void) { return 0; }
void DemuxReset(void) {}
void DemuxFlush(void) {}
long long SeekLiveStream(long long iPosition, int iWhence /* = SEEK_SET */) { return -1; }
long long PositionLiveStream(void) { return -1; }
long long LengthLiveStream(void) { return -1; }
PVR_ERROR DeleteRecording(const PVR_RECORDING &recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameRecording(const PVR_RECORDING &recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING &recording, int count) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING &recording, int lastplayedposition) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetRecordingEdl(const PVR_RECORDING&, PVR_EDL_ENTRY[], int*) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording) { return -1; }
int GetTimersAmount(void) { return -1; }
PVR_ERROR GetTimers(ADDON_HANDLE handle) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR AddTimer(const PVR_TIMER &timer) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteTimer(const PVR_TIMER &timer, bool bForceDelete) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR UpdateTimer(const PVR_TIMER &timer) { return PVR_ERROR_NOT_IMPLEMENTED; }
void DemuxAbort(void) {}
DemuxPacket* DemuxRead(void) { return NULL; }
unsigned int GetChannelSwitchDelay(void) { return 0; }
void PauseStream(bool bPaused) {}
bool CanPauseStream(void) { return false; }
bool CanSeekStream(void) { return false; }
bool SeekTime(int,bool,double*) { return false; }
void SetSpeed(int) {}
time_t GetPlayingTime() { return 0; }
time_t GetBufferTimeStart() { return 0; }
time_t GetBufferTimeEnd() { return 0; }
}
