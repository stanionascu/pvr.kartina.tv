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

#include "kartinatvclient.h"

#include <sstream>
#include <ostream>
#include <fstream>
#ifdef _MSC_VER
#include <WinSock2.h>
#else
#include <unistd.h>
#endif
#include <ctime>

#include <p8-platform/sockets/tcp.h>
#include <json/json.h>
#include "kodi/libXBMC_addon.h"
#include "kodi/libXBMC_pvr.h"
#include "p8-platform/util/StdString.h"

const std::string KartinaTVClient::API_SERVER = "iptv.kartina.tv";

namespace {
#define API_URL "/api/json/"
const uint16_t API_PORT = 80;
const uint32_t REQ_TIME_LIMIT = 1010000;

std::string makeApiUrl(const char *functionName)
{
    static const std::string result(API_URL);
    return result + functionName;
}

enum class ErrorCode
{
    OK = 0,
    Authentication = 12,
    QueryLimitExceeded = 31
};

struct KTVError {
    KTVError() : code(ErrorCode::OK) { }

    bool isOk() const
    {
        return code == ErrorCode::OK;
    }

    ErrorCode code;
    std::string message;
};

KTVError checkForError(const Json::Value &root)
{
    KTVError error;
    if (!root.isNull()) {
        const auto &jsonError = root["error"];
        if (!jsonError.isNull()) {
            error.code = (ErrorCode)jsonError["code"].asInt();
            error.message = jsonError["message"].asString();
        }
    }
    return error;
}

struct UrlData {
    std::string hostname;
    std::string port;
    std::string query;
};

UrlData parseUrl(std::string url)
{
    UrlData data;
    data.query = url.substr(url.find('?') + 1);
    size_t hostnameStart = url.find("://") + 3;
    size_t hostnameEnd = url.find(':', hostnameStart) - 1;
    data.hostname = url.substr(hostnameStart, hostnameEnd - 6);
    data.port = "any";

    return data;
}

}

KartinaTVClient::KartinaTVClient(ADDON::CHelper_libXBMC_addon *XBMC, CHelper_libXBMC_pvr *PVR) :
    XBMC(XBMC), PVR(PVR), lastEpgQuery(0, 0), requestNumber(0)
{
}

KartinaTVClient::~KartinaTVClient()
{
    for (auto &c: channelEpgCache) {
        for (auto &t: c.second)
            delete t;
    }
}

bool KartinaTVClient::loadChannelGroupsFromCache(ADDON_HANDLE handle,
                                                 bool bRadio)
{
    for (const auto &group: channelGroupsCache) {
        if (!bRadio) {
            PVR_CHANNEL_GROUP channelGroup = createPvrChannelGroup(group);
            PVR->TransferChannelGroup(handle, &channelGroup);
        }
    }

    return true;
}

bool KartinaTVClient::loadChannelGroupMembersFromCache(
        ADDON_HANDLE handle,
        const PVR_CHANNEL_GROUP &group)
{
    for (const auto &groupMem: channelGroupMembersCache) {
        if (groupMem.name == group.strGroupName) {
            PVR_CHANNEL_GROUP_MEMBER member =
                    createPvrChannelGroupMember(groupMem);
            PVR->TransferChannelGroupMember(handle, &member);
        }
    }

    return true;
}

bool KartinaTVClient::loadChannelsFromCache(ADDON_HANDLE handle, bool bRadio)
{
    if (channelsCache.empty())
        updateChannelList();

    for (const auto &c: channelsCache) {
        if (c.isRadio == bRadio) {
            PVR_CHANNEL channel = createPvrChannel(c);
            PVR->TransferChannelEntry(handle, &channel);
        }
    }

    return true;
}

bool KartinaTVClient::loadEpgFromCache(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t start, time_t end)
{
    XBMC->Log(ADDON::LOG_DEBUG, "KartinaTVClient::loadEpgFromCache %d %d %d",
              channel.iUniqueId,
              start,
              end);

    if (!channel.bIsRadio)
        updateChannelEpg(channel.iUniqueId, start, end);
    auto epg = channelEpgCache.find(channel.iUniqueId);
    if (epg != channelEpgCache.cend()) {
        for (const auto &entry: epg->second)
            PVR->TransferEpgEntry(handle, entry);
    }
    return true;
}

std::string KartinaTVClient::requestStreamUrl(const PVR_CHANNEL &channel)
{
    XBMC->Log(ADDON::LOG_DEBUG, "void KartinaTVClient::requestStreamUrl()");

    bool isProtected = false;
    for (const auto &c: channelsCache) {
        if (c.id == channel.iUniqueId && c.isProtected)
            isProtected = true;
    }
    PostFields parameters;
    parameters.insert(std::make_pair("cid", std::to_string(channel.iUniqueId)));
    if (isProtected)
        parameters.insert(std::make_pair("protect_code", protectCode));

    const auto &root = makeRequest("get_url", parameters);
    KTVError ktvError = checkForError(root);
    if (!root.isNull() && ktvError.isOk()) {
        const char *urlData = root["url"].asCString();

        std::vector<std::string> urlParams;
        std::stringstream stream(urlData);
        std::string param;
        while (stream >> param) {
            XBMC->Log(ADDON::LOG_DEBUG, "Extracted: %d %s", urlParams.size(), param.data());
            urlParams.push_back(param);
        }

        std::string url = urlParams.front();
        // http/ts
        url = url.replace(0, 7, "http");

        return url;
    } else {
        XBMC->Log(ADDON::LOG_ERROR, "Error occured: code %d %s", ktvError.code, ktvError.message.c_str());
        XBMC->Log(ADDON::LOG_ERROR, "Request to API server failed.");
    }

    return "";
}

void KartinaTVClient::setUserProfilePath(const std::string &path)
{
    userPath = path;
}

void KartinaTVClient::setCredentials(const std::string & userName, const std::string & password)
{
    credentials = {userName, password};
}

bool KartinaTVClient::login(bool force)
{
    static const char cacheFileName[] = "auth_cache";
    XBMC->Log(ADDON::LOG_DEBUG, "void KartinaTVClient::login()");

    if (force)
        eraseFileCache(cacheFileName);
    std::string authCache = readFromFileCache(cacheFileName);

    KTVError error;
    Json::Reader json;
    Json::Value root;

    if (authCache.empty()) {
        PostFields parameters;
        parameters.insert(std::make_pair("login", credentials.userName));
        parameters.insert(std::make_pair("pass", credentials.password));
        parameters.insert(std::make_pair("settings", "all"));
        parameters.insert(std::make_pair("softid", "pvr.kartina.tv"));

        authCache = sendRequest("login", parameters);
        if (json.parse(authCache, root)) {
            error = checkForError(root);
        } else {
            XBMC->Log(ADDON::LOG_DEBUG, "Invalid json");
        }

        if (!authCache.empty() && error.code == ErrorCode::OK)
            writeToFileCache(cacheFileName, authCache);
    } else {
        json.parse(authCache, root);
    }

    if (!root.isNull()) {
        sessionId.first = root["sid_name"].asString();
        sessionId.second = root["sid"].asString();
    } else {
        XBMC->Log(ADDON::LOG_ERROR, "Error occured: code %d %s", error.code, error.message.c_str());
        XBMC->Log(ADDON::LOG_ERROR, "Request to API server failed.");
        return false;
    }

    return true;
}

void KartinaTVClient::logout()
{
    XBMC->Log(ADDON::LOG_DEBUG, "void KartinaTVClient::logout()");

    PostFields parameters;
    makeRequest("logout", parameters);
}

void KartinaTVClient::setProtectCode(const std::string &code)
{
    protectCode = code;
}

void KartinaTVClient::updateChannelList()
{
    XBMC->Log(ADDON::LOG_DEBUG, "void KartinaTVClient::updateChannelsList()");

    PostFields parameters;
    const auto &root = makeRequest("channel_list", parameters);
    channelsCache.clear();

    KTVError ktvError = checkForError(root);
    if (!root.isNull() && ktvError.isOk()) {
        const auto &groups = root["groups"];
        for (const auto &group : groups) {
            ChannelGroup channelGroup = channelGroupFromJson(group);
            channelGroupsCache.push_back(channelGroup);
            const Json::Value &channels = group["channels"];
            if (!channels.isNull()) {
                for (const auto &jsonChannel : channels) {
                    Channel channel = channelFromJson(jsonChannel);
                    channel.number = channelsCache.size() + 1;
                    channelsCache.emplace_back(channel);
                    channelGroupMembersCache.emplace_back(
                        createChannelGroupMember(channel, channelGroup));
                }
            }
        }
    } else {
        XBMC->Log(ADDON::LOG_ERROR, "Error occured: code %d %s",
                  ktvError.code, ktvError.message.c_str());
        XBMC->Log(ADDON::LOG_ERROR, "Request to API server failed.");
    }
}

void KartinaTVClient::updateChannelEpg(time_t start, int hours)
{
    XBMC->Log(ADDON::LOG_DEBUG, "KartinaTVClient::updateChannelEpg start=%d hours=%d", start, hours);
    PostFields parameters;
    parameters.insert(std::make_pair("dtime", std::to_string(start)));
    parameters.insert(std::make_pair("period", std::to_string(hours)));
    const auto &root = makeRequest("epg3", parameters);
    channelEpgCache.clear();

    KTVError ktvError = checkForError(root);
    if (!root.isNull() && ktvError.isOk()) {
        const auto &epg3 = root["epg3"];
        for (const auto &channelEpg : epg3) {
            int channelId = -1;
            if (channelEpg["id"].isIntegral())
                channelId = channelEpg["id"].asInt();
            else
                channelId = std::stoi(channelEpg["id"].asString());

            const auto &epg = channelEpg["epg"];
            EPG_TAG *lastEntry = NULL;
            if (!epg.isNull()) {
                for (const auto &program : epg) {
                    auto &channelEpgCacheRef = channelEpgCache[channelId];
                    EPG_TAG *tag = new EPG_TAG;
                    memset(tag, 0, sizeof(EPG_TAG));
                    tag->iChannelNumber = channelId;
                    tag->iUniqueBroadcastId = channelId * 10000 + channelEpgCacheRef.size();
                    tag->firstAired = 0;
                    if (lastEntry == NULL)
                        tag->startTime = start + 1;
                    else
                        tag->startTime = program["ut_start"].asInt();
                    tag->endTime = tag->startTime + 300;
                    if (lastEntry != NULL)
                        lastEntry->endTime = tag->startTime;
                    std::string longTitle = program["progname"].asString();
                    std::istringstream titleStream(longTitle);
                    std::vector<std::string> titleLines;
                    while (!titleStream.eof()) {
                        std::string line;
                        std::getline(titleStream, line);
                        titleLines.push_back(line);
                    }

                    std::string title, plot;
                    if (titleLines.size() > 0)
                        title = titleLines.at(0);
                    else
                        title = longTitle;

                    if (titleLines.size() > 1) {
                        std::ostringstream plotStream(plot);
                        for (unsigned int i = 1; i < titleLines.size(); ++i) {
                            if (i > 1)
                                plotStream << ' ';
                            plotStream << titleLines.at(i);
                        }
                    }

                    tag->strTitle = _strdup(title.c_str());
                    tag->strPlot = _strdup(plot.c_str());

                    channelEpgCacheRef.push_back(tag);
                    lastEntry = tag;
                }
            }
            if (lastEntry)
                lastEntry->endTime = lastEntry->startTime + 600;
        }
    } else {
        XBMC->Log(ADDON::LOG_ERROR, "Error occured: code %d %s", ktvError.code, ktvError.message.c_str());
        XBMC->Log(ADDON::LOG_ERROR, "Request to API server failed.");
    }
}

void KartinaTVClient::updateChannelEpg(int channelId, time_t start, time_t end)
{
    clearChannelEpg(channelId);

    char buf[32] = "";
    EPG_TAG *lastTag = NULL;
    while (start <= end) {
        memset(&buf, 0, sizeof(buf));
        tm *t = std::localtime(&start);
        t->tm_hour = 0;
        t->tm_min = 0;
        t->tm_sec = 0;
        std::strftime(buf, 32, "%d%m%y", t);
        std::string day = buf;
        PostFields req = {
            { "cid", std::to_string(channelId) },
            { "day", day }
        };
        const auto &root = makeRequest("epg", req);
        KTVError ktvError = checkForError(root);
        if (!root.isNull() && ktvError.isOk()) {
            const auto &epg = root["epg"];
            if (epg.empty())
                return;

            int showId = 1;
            for (Json::Value::UInt j = 0; j < epg.size(); ++ j) {
                const auto &show = epg[j];
                EPG_TAG *tag = new EPG_TAG;
                memset(tag, 0, sizeof(EPG_TAG));
                tag->iChannelNumber = channelId;
                tag->firstAired = 0;
                const auto &ut_start = show["ut_start"];
                if (ut_start.isInt())
                    tag->startTime = ut_start.asInt();
                else
                    tag->startTime = std::stoi(ut_start.asString());
                tag->iUniqueBroadcastId = tag->startTime;
                tag->endTime = tag->startTime + 300;
                if (lastTag != NULL)
                    lastTag->endTime = tag->startTime;
                std::string longTitle = show["progname"].asString();
                std::istringstream titleStream(longTitle);
                std::vector<std::string> titleLines;
                while (!titleStream.eof()) {
                    std::string line;
                    std::getline(titleStream, line);
                    titleLines.push_back(line);
                }

                std::string title, plot;
                if (titleLines.size() > 0)
                    title = titleLines.at(0);
                else
                    title = longTitle;

                if (titleLines.size() > 1) {
                    for (unsigned int i = 1; i < titleLines.size(); ++i) {
                        plot += " " + titleLines.at(i);
                    }
                }

                tag->strTitle = _strdup(title.c_str());
                tag->strPlot = _strdup(plot.c_str());

                channelEpgCache[channelId].push_back(tag);
                lastTag = tag;
                ++ showId;
            }
        }
        t->tm_mday += 1;
        start = mktime(t);
    }
}

void KartinaTVClient::clearChannelEpg(int channelId)
{
    for (auto c: channelEpgCache[channelId])
        delete c;
    channelEpgCache.erase(channelId);
}

KartinaTVClient::Channel KartinaTVClient::channelFromJson(const Json::Value &value)
{
    Channel channel;
    channel.id = value["id"].asInt();
    channel.name = value["name"].asString();
    channel.number = channel.id;
    channel.isRadio = value["is_video"].asInt() != 1;
    channel.iconUrl = std::string("http://") + API_SERVER +
            value["icon"].asString();
    channel.streamUrl = std::string("pvr://stream/tv/") +
            std::to_string(channel.id)
            + ".ts";
    channel.isProtected = value["protected"].asInt() == 1;

    return channel;
}

PVR_CHANNEL KartinaTVClient::createPvrChannel(
        const KartinaTVClient::Channel &channel)
{
    PVR_CHANNEL pvr;
    memset(&pvr, 0, sizeof(PVR_CHANNEL));

    pvr.bIsHidden = false;
    pvr.bIsRadio = channel.isRadio;
    pvr.iChannelNumber = channel.number;
    pvr.iUniqueId = channel.id;
    strcpy(pvr.strChannelName, channel.name.c_str());
    strcpy(pvr.strIconPath, channel.iconUrl.c_str());
    strcpy(pvr.strStreamURL, channel.streamUrl.c_str());

    return pvr;
}

KartinaTVClient::ChannelGroup KartinaTVClient::channelGroupFromJson(
        const Json::Value &value)
{
    return ChannelGroup{ value["id"].asInt(), value["name"].asString() };
}

PVR_CHANNEL_GROUP KartinaTVClient::createPvrChannelGroup(
        const KartinaTVClient::ChannelGroup &channelGroup)
{
    PVR_CHANNEL_GROUP group;
    memset(&group, 0, sizeof(PVR_CHANNEL_GROUP));

    group.bIsRadio = false;
    strcpy(group.strGroupName, channelGroup.name.c_str());

    return group;
}

KartinaTVClient::ChannelGroupMember KartinaTVClient::createChannelGroupMember(
        const KartinaTVClient::Channel &channel,
        const KartinaTVClient::ChannelGroup &group)
{
    ChannelGroupMember member;
    member.id = channel.id;
    member.number = channel.number;
    member.name = group.name;

    return member;
}

PVR_CHANNEL_GROUP_MEMBER KartinaTVClient::createPvrChannelGroupMember(
        const KartinaTVClient::ChannelGroupMember &member)
{
    PVR_CHANNEL_GROUP_MEMBER pvrMember;
    memset(&pvrMember, 0, sizeof(PVR_CHANNEL_GROUP_MEMBER));

    pvrMember.iChannelNumber = member.number;
    pvrMember.iChannelUniqueId = member.id;
    strcpy(pvrMember.strGroupName, member.name.c_str());

    return pvrMember;
}

Json::Value KartinaTVClient::makeRequest(const char *apiFunction, PostFields &parameters)
{
    KTVError error;
    Json::Reader reader;
    Json::Value root;
    do {
        reader.parse(sendRequest(apiFunction, parameters), root, false);
        error = checkForError(root);
        switch (error.code) {
        case ErrorCode::OK:
            break;
        case ErrorCode::Authentication:
            if (!login(true)) {
                XBMC->QueueNotification(ADDON::QUEUE_ERROR, "Cannot login");
                return std::string();
            }
            break;
        case ErrorCode::QueryLimitExceeded:
            usleep(REQ_TIME_LIMIT * 2);
            break;
        default:
            return {};
        }
        if (root.isNull())
            usleep(REQ_TIME_LIMIT);
    } while (static_cast<ErrorCode>(error.code) != ErrorCode::OK);

    return root;
}

std::string KartinaTVClient::sendRequest(const char *apiFunction, PostFields &parameters)
{
    static auto lastResetTime = P8PLATFORM::GetTimeMs();
    static uint8_t requestCount = 0;
    static P8PLATFORM::CMutex mutex;
    mutex.Lock();

    auto timeSinceLastReset = P8PLATFORM::GetTimeMs() - lastResetTime;

    XBMC->Log(ADDON::LOG_DEBUG, (KTV_FUNC_INFO ": connecting to " + API_SERVER + "...").c_str());

    if (requestCount >= 4 && timeSinceLastReset < 1000) {
        usleep(REQ_TIME_LIMIT - (timeSinceLastReset * 1000));
        requestCount = 0;
    }
    else if (timeSinceLastReset > 1000) {
        requestCount = 0;
    }

    if (requestCount == 0)
        lastResetTime = P8PLATFORM::GetTimeMs();

    P8PLATFORM::CTcpConnection sock(API_SERVER, API_PORT);
    if (!sock.Open(30000)) {
        XBMC->Log(ADDON::LOG_ERROR, (KTV_FUNC_INFO ": connection to " + API_SERVER + " failed!").c_str());
        mutex.Unlock();
        return std::string();
    }

    XBMC->Log(ADDON::LOG_DEBUG, KTV_FUNC_INFO ": connected...");
    ++ requestCount;

    const std::string &apiCallUrl = makeApiUrl(apiFunction);
    const std::string &postFields = stringifyPostFields(parameters);
    CStdString request = "POST " + apiCallUrl + " HTTP/1.0" + "\r\n" +
        "User-Agent: Kodi 17.0 (Krypton), pvr.kartina.tv" + "\r\n" +
        "Connection: close" + "\r\n" +
        "Host: " + API_SERVER + "\r\n" +
        "Content-Type: application/x-www-form-urlencoded" + "\r\n" +
        "Content-Length: " + std::to_string(postFields.size()) + "\r\n";
    if (!sessionId.second.empty()) {
        request += "Cookie: " + sessionId.first + "=" + sessionId.second + ";\r\n";
    }
    request += "\r\n";
    request += postFields + "\r\n";

    XBMC->Log(ADDON::LOG_DEBUG, postFields.data());

    XBMC->Log(ADDON::LOG_DEBUG, KTV_FUNC_INFO ": sending request %s",
        request.c_str());
    sock.Write(request.GetBuf(), request.size());

    std::string reply;
    while (sock.IsOpen()) {
        char buff[10240];
        ssize_t bytesRead = sock.Read(buff, sizeof(buff), 5000);
        if (bytesRead > 0) {
            reply += std::string(buff, bytesRead);
        }
        else {
            break;
        }
    }

    std::size_t httpDataStart = reply.find("\r\n\r\n");
    std::string headers = reply.substr(0, httpDataStart);
    std::string body;
    if (reply.size() > httpDataStart + 4)
        body = reply.substr(httpDataStart + 4, std::string::npos);

    XBMC->Log(ADDON::LOG_DEBUG, KTV_FUNC_INFO ": received headers %s",
        headers.c_str());

    sock.Close();

    XBMC->Log(ADDON::LOG_DEBUG, KTV_FUNC_INFO ": connection closed.");
    mutex.Unlock();

    return body;
}

std::string KartinaTVClient::stringifyPostFields(const PostFields &fields)
{
    std::string postFields = "";
    for (const auto &f: fields) {
        if (postFields.length() > 0)
            postFields += "&";
        postFields += f.first + "=" + f.second;
    }

    return postFields;
}

std::string KartinaTVClient::readFromFileCache(const std::string &filename)
{
    std::string content;
    const std::string fullPath = userPath + "/" + filename;
    if (XBMC->FileExists(fullPath.c_str(), false)) {
        auto cacheFile = XBMC->OpenFile(fullPath.c_str(), 0);
        if (cacheFile) {
            char buffer[4096] = { 0 };
            ssize_t readBytes = 0;
            while ((readBytes = XBMC->ReadFile(cacheFile, (void*)buffer, sizeof(buffer))) > 0)
                content += buffer;
            XBMC->CloseFile(cacheFile);
        }
        else {
            XBMC->Log(ADDON::LOG_ERROR, (KTV_FUNC_INFO ": failed to open: " + filename).c_str());
        }
    }
    return content;
}

void KartinaTVClient::writeToFileCache(const std::string &filename, const std::string &content)
{
    const std::string fullPath = userPath + "/" + filename;
    auto cacheFile = XBMC->OpenFileForWrite(fullPath.c_str(), true);
    if (cacheFile) {
        XBMC->WriteFile(cacheFile, (void*)content.c_str(), content.size());
        XBMC->CloseFile(cacheFile);
    }
    else {
        XBMC->Log(ADDON::LOG_ERROR, (KTV_FUNC_INFO ": failed to open: " + filename).c_str());
    }
}

void KartinaTVClient::eraseFileCache(const std::string &filename)
{
    const std::string fullPath = userPath + "/" + filename;
    if (XBMC->FileExists(fullPath.c_str(), false)) {
        XBMC->DeleteFile(fullPath.c_str());
    }
}