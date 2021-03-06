/* @@@LICENSE
*
* Copyright (c) 2013 Simon Busch <morphis@gravedo.de>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* LICENSE@@@ */

#ifndef HYBRISBUFFERSERVER_H_
#define HYBRISBUFFERSERVER_H_

#include <glib.h>
#include <list>

class WebosSurfaceManagerRemoteClient;
class WebosSurfaceManagerRemoteClientFactory;

class WebosSurfaceManager
{
public:
	static WebosSurfaceManager* instance();

	void setRemoteClientFactory(WebosSurfaceManagerRemoteClientFactory *factory);

	void onNewConnection();
	void onClientDisconnected(WebosSurfaceManagerRemoteClient *client);

	WebosSurfaceManagerRemoteClient* findClient(unsigned int windowId);

private:
	WebosSurfaceManager();
	~WebosSurfaceManager();

	static gboolean onNewConnectionCb(GIOChannel *channel, GIOCondition condition, gpointer user_data);
	void setup();

	gchar *m_socketPath;
	int m_socketFd;
	GIOChannel *m_channel;
	gint m_socketWatch;
	WebosSurfaceManagerRemoteClientFactory *m_remoteClientFactory;
	std::list<WebosSurfaceManagerRemoteClient*> m_clients;
};

#endif /* HYBRISBUFFERSERVER_H_ */
