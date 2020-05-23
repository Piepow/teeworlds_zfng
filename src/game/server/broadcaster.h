#ifndef GAME_SERVER_BROADCASTER_H
#define GAME_SERVER_BROADCASTER_H

#include <cstdint>
#include <game/server/gamecontext.h>

#define MAX_BROADCAST_STRLEN 256

class CBroadcaster
{
private:
	// Broadcast messages for each client
	char m_aBroadcast[MAX_CLIENTS][MAX_BROADCAST_STRLEN];

	// Tick to do the next broadcast for each client. This is needed to allow
	// the broadcast to persist.
	int m_aNextBroadcast[MAX_CLIENTS];

	// Tick to stop the broadcast for each client
	int m_aBroadcastStop[MAX_CLIENTS];

	// Whether the broadcast message was changed for each client. This is a bit
	// mask.
	int64_t m_Changed;

	class CGameContext *m_pGameServer;
public:
	CBroadcaster(class CGameContext *pGameServer);
	virtual ~CBroadcaster();

	void Reset();

	// Set the broadcast message `pText` for `Cid` with `Lifespan`.
	// If `Cid` < 0, then do it for all clients.
	// If `Lifetime` < 0, then it is infinite.
	void SetBroadcast(int Cid, const char *pText, int Lifespan);

	void Update();
};

#endif
