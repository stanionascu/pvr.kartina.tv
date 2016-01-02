# -*- coding: utf-8 -*-
# KartinaTV VOD Kodi Addon
#
#     Copyright (C) 2016 Stanislav Ionascu
#     Stanislav Ionascu
#
# This Program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This Program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with XBMC; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
# http://www.gnu.org/copyleft/gpl.html
#

import sys
import httplib, json, urllib, urlparse
import xbmc, xbmcaddon, xbmcgui, xbmcplugin, datetime, HTMLParser

API_SERVER = 'iptv.kartina.tv'
API_URL = '/api/json/'

addon = xbmcaddon.Addon('pvr.kartina.tv')
addon_name = addon.getAddonInfo('name')
addon_url = sys.argv[0]
addon_handle = int(sys.argv[1])
args = urlparse.parse_qs(sys.argv[2][1:])
addon_userdatadir = xbmc.translatePath(addon.getAddonInfo('profile'))
addon_authcache = addon_userdatadir + 'auth_cache'

xbmcplugin.setContent(addon_handle, 'movies')

with open(addon_authcache) as authcache_file:
	authdata = json.load(authcache_file)

authcookie = authdata['sid_name']+'='+authdata['sid']

def make_request(api, params=dict()):
	h = httplib.HTTPConnection(API_SERVER)

	headers = { 'Cookie': authcookie,
		   'Content-Type' : 'application/x-www-form-urlencoded',
		   'User-Agent': 'Kodi 15.2 (Isengard), pvr.kartina.tv'
	}
	h.request('POST', API_URL + api, urllib.urlencode(params), headers)
	response = h.getresponse()
	return response.read()

def get_genres():
	reply = make_request('vod_genres')
	return json.loads(reply)['genres']

def get_items(query):
	reply = make_request('vod_list', query)
	jply = json.loads(reply)
	return int(jply['total']), jply['rows']

def get_channels():
    reply = make_request('channel_list')
    jply = json.loads(reply)
    channels = list()
    for group in jply['groups']:
        for channel in group['channels']:
            channels.append(channel)
    return channels

def get_epg(cid, date):
	reply = make_request('epg', {'cid': cid, 'day' : date })
	jply = json.loads(reply)
	return jply['epg']

def make_addon_url(params=dict()):
	return "%s?%s" % (addon_url, urllib.urlencode(params, True))

if 'mode' in args:
	mode = args['mode'][0]
	if mode=='type':
		type = args['type'][0]
		page = int(args['page'][0])
		args['page'][0] = (page + 1)

		params = { 'type': type,
				'page': page,
				'nums': 100 }

		if type == 'text':
			if 'query' not in args:
				dialog = xbmcgui.Dialog()
				args['query'] = [dialog.input('Search by movie title')]
			params['query'] = args['query'][0]
		
		if type == 'genre':
			params['genre'] = int(args['genre'][0])

		html_parser = HTMLParser.HTMLParser()
		total, vod_items = get_items(params)
		for item in vod_items:
			url = make_addon_url({'mode': 'view', 'id': item['id']})
			poster_url = 'http://' + API_SERVER + item['poster']
			item_name = html_parser.unescape(item['name'])
			li = xbmcgui.ListItem(item_name)
			li.setInfo('video',
			  {'genre': item['genre_str'],
			  'year': item['year'],
			  'title': item_name,
			  'originaltitle': item['name_orig'],
			  'rating': item['rate_kinopoisk'],
			  'mpaa': item['rate_mpaa'],
			  'plot': item['description'],
			  'plotoutline': item['description']}
			)
			li.setArt({'poster': poster_url})
			xbmcplugin.addDirectoryItem(handle=addon_handle,
				url=url,
				listitem=li,
				isFolder=True)
		if (page * 100) < total:
			xbmcplugin.addDirectoryItem(handle=addon_handle,
					url=make_addon_url(dict(args)),
					listitem=xbmcgui.ListItem('[ Next -> ]'),
					isFolder=True)
	elif mode == 'view':
		vod_id = args['id'][0]
		response = make_request('vod_info', {'id': vod_id})
		jsponse = json.loads(response)['film']
		params = { 'mode': 'play' }
		html_parser = HTMLParser.HTMLParser()
		for video in jsponse['videos']:
			params['id'] = video['id']
			poster_url = 'http://' + API_SERVER + jsponse['poster']
			li = xbmcgui.ListItem(video['title'] + ' (' + video['format'] + ')')
			li.setInfo('video',
			  {'genre': jsponse['genre_str'],
			  'year': jsponse['year'],
			  'title': html_parser.unescape(jsponse['name']),
			  'rating': jsponse['rate_kinopoisk'],
			  'mpaa': jsponse['rate_mpaa'],
			  'plot': jsponse['description'],
			  'plotoutline': jsponse['description'],
			  'cast': jsponse['actors'].split(','),
			  'writer': jsponse['scenario'].split(','),
			  'director': jsponse['director'].split(',')
			  }
			)
			li.setArt({'poster': poster_url})
			li.addStreamInfo('video', {
				'codec': video['codec']
				})
			li.addStreamInfo('audio', {
				'codec': video['track1_codec'],
				'language': video['track1_lang']
				})
			xbmcplugin.addDirectoryItem(handle=addon_handle,
				url=make_addon_url(params),
				listitem=li,
				isFolder=False)
	elif mode == 'play':
		id = int(args['id'][0])
		response = make_request('vod_geturl', {'fileid': id})
		fullurl = json.loads(response)['url']
		url = fullurl.split(' ')
		li = xbmcgui.ListItem(xbmc.getInfoLabel('ListItem.Title') + 
						' - ' + 
						xbmc.getInfoLabel('ListItem.Label'))
		li.setInfo('video',
			  {'genre': xbmc.getInfoLabel('ListItem.Genre'),
			  'year': xbmc.getInfoLabel('ListItem.Year'),
			  'rating': xbmc.getInfoLabel('ListItem.Rating'),
			  'mpaa': xbmc.getInfoLabel('ListItem.Mpaa'),
			  'plot': xbmc.getInfoLabel('ListItem.Plot')
			  }
			)
		li.setArt({'poster': xbmc.getInfoLabel('ListItem.Art(poster)')})
		xbmc.Player().play(url[0], listitem=li)
	elif mode == 'archive_play':
		cid = int(args['cid'][0])
		start_time = int(args['start'][0])
		reply = make_request('get_url', {'cid':cid, 'gmt':start_time})
		fullurl = json.loads(reply)['url']
		url = fullurl.split(' ')
		li = xbmcgui.ListItem(xbmc.getInfoLabel('ListItem.Label'))
		xbmc.Player().play(url[0].replace('http/ts', 'http'), listitem=li)
	elif mode == 'genre':
		genres = get_genres()
		for genre in genres:
			url = addon_url + '?mode=type&page=1&type=genre&genre=' + genre['id']
			xbmcplugin.addDirectoryItem(handle=addon_handle,
				url=url,
				listitem=xbmcgui.ListItem(genre['name']),
				isFolder=True)
	elif mode == 'archive':
		params = { 'mode': mode }
		if 'cid' not in args:
			channels = get_channels();
			for channel in channels:
				if 'have_archive' in channel and channel['have_archive'] == 1:
					params['cid'] = channel['id']
					li = xbmcgui.ListItem(channel['name'])
					li.setArt({'poster': 'http://' + API_SERVER + channel['icon']})
					xbmcplugin.addDirectoryItem(handle=addon_handle,
						url=make_addon_url(params),
						listitem=li,
						isFolder=True)
		elif 'date' not in args:
			cid = args['cid'][0]
			params['cid'] = cid
			start_date = datetime.datetime.now()
			end_date = start_date + datetime.timedelta(-14)
			d = start_date
			while (d >= end_date):
				params['date'] = d.strftime('%d%m%y')
				xbmcplugin.addDirectoryItem(handle=addon_handle,
							url=make_addon_url(params),
							listitem=xbmcgui.ListItem(d.strftime('%Y-%m-%d')),
							isFolder=True)
				d += datetime.timedelta(-1)
		else:
			cid = int(args['cid'][0])
			date = args['date'][0]
			params = {'cid' : cid, 'mode' : 'archive_play'}
			epg = get_epg(cid, date)
			now_epoch = int((datetime.datetime.now() - datetime.datetime(1970,1,1)).total_seconds())
			for e in epg:
				progname = e['progname'].split('\n')
				params['start'] = int(e['ut_start'])
				title = e['t_start'] + ' - ' + progname[0]
				li = xbmcgui.ListItem(title)
				if params['start'] < now_epoch:
					xbmcplugin.addDirectoryItem(handle=addon_handle,
						url=make_addon_url(params),
						listitem=li,
						isFolder=False)
else:
	types = [{'type':'best', 'name':'Best movies'},
		  {'type':'last', 'name':'Newest movies'},
		  {'type':'text', 'name':'Search for movies'}]
	params = { 'mode' : 'type',
		   'page': 1 }
	for type in types:
		params['type'] = type['type']
		xbmcplugin.addDirectoryItem(handle=addon_handle,
			url=make_addon_url(params),
			listitem=xbmcgui.ListItem(type['name']),
			isFolder=True)
	xbmcplugin.addDirectoryItem(handle=addon_handle,
		url=make_addon_url({'mode': 'genre'}),
		listitem=xbmcgui.ListItem('Movies by genre'),
		isFolder=True)
	xbmcplugin.addDirectoryItem(handle=addon_handle,
		url=make_addon_url({'mode': 'archive'}),
		listitem=xbmcgui.ListItem('Archive'),
		isFolder=True)

xbmcplugin.endOfDirectory(addon_handle)
