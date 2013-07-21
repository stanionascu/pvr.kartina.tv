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

#ifndef KARTINATVCLIENT_H
#define KARTINATVCLIENT_H

#include <cstddef>
#include <map>
#include <string>
#include <vector>
#include <xbmc_addon_types.h>
#include <time.h>

#ifdef _MSC_VER
#   define KTV_FUNC_INFO __FUNCTION__
#else
#   define KTV_FUNC_INFO __PRETTY_FUNCTION__
#endif

namespace ADDON {
class CHelper_libXBMC_addon;
}
class CHelper_libXBMC_pvr;
struct PVR_CHANNEL;
struct PVR_CHANNEL_GROUP;
struct PVR_CHANNEL_GROUP_MEMBER;
struct EPG_TAG;

namespace Json {
class Value;
}

class KartinaTVClient
{
public:
    struct ChannelGroup
    {
        int id;
        std::string name;
    };

    struct ChannelGroupMember
    {
        int id;
        int number;
        std::string name;
    };

    struct Channel
    {
        int id;
        unsigned int number;
        std::string name;
        std::string streamUrl;
        std::string iconUrl;
        bool isRadio;
        bool isProtected;
    };

    static const std::string API_SERVER;

public:
    typedef std::map<std::string, std::string> PostFields;

    KartinaTVClient(ADDON::CHelper_libXBMC_addon *XBMC, CHelper_libXBMC_pvr *PVR);
    virtual ~KartinaTVClient();

    void setUserProfilePath(const std::string &path);

    bool login(const std::string &user, const std::string &pass);
    void logout();
    void setProtectCode(const std::string &code);

    bool loadChannelGroupsFromCache(ADDON_HANDLE handle, bool bRadio);
    bool loadChannelGroupMembersFromCache(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group);
    bool loadChannelsFromCache(ADDON_HANDLE handle, bool bRadio);
    bool loadEpgFromCache(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t start, time_t end);

    std::string requestStreamUrl(const PVR_CHANNEL &channel);

    inline int channelGroupsCount() const
    {
        return channelGroupsCache.size();
    }

    inline int channelCount() const
    {
        return channelsCache.size();
    }

protected:
    void updateChannelList();
    void updateChannelEpg(time_t start, int hours);
    void updateChannelEpg(int channelId, time_t start, time_t end);
    void clearChannelEpg(int channelId);

    Channel channelFromJson(const Json::Value &value);
    PVR_CHANNEL createPvrChannel(const Channel &channel);

    ChannelGroup channelGroupFromJson(const Json::Value &value);
    PVR_CHANNEL_GROUP createPvrChannelGroup(const ChannelGroup &channelGroup);

    ChannelGroupMember createChannelGroupMember(const Channel &channel,
                                                const ChannelGroup &group);
    PVR_CHANNEL_GROUP_MEMBER createPvrChannelGroupMember(
            const ChannelGroupMember &member);

    std::string makeRequest(const char *apiFunction, PostFields &parameters);
    static std::string stringifyPostFields(const PostFields &fields);

private:
    ADDON::CHelper_libXBMC_addon *XBMC;
    CHelper_libXBMC_pvr *PVR;

    std::pair<std::string, std::string> sessionId;
    std::string protectCode;

    std::vector<Channel> channelsCache;
    std::vector<ChannelGroup> channelGroupsCache;
    std::vector<ChannelGroupMember> channelGroupMembersCache;
    std::map<int, std::vector<EPG_TAG*>> channelEpgCache;

    std::pair<time_t, time_t> lastEpgQuery;

    std::string userPath;

    int requestNumber;
};

#endif // KARTINATVCLIENT_H
