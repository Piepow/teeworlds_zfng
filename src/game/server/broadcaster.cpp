#include <base/system.h>
#include <game/server/gamecontext.h>
#include "broadcaster.h"

#undef TICK_SPEED
#undef TICK
#undef GAME_SERVER

#define TICK_SPEED m_pGameServer->Server()->TickSpeed()
#define TICK m_pGameServer->Server()->Tick()
#define GAME_SERVER m_pGameServer

CBroadcaster::CBroadcaster(class CGameContext *pGameServer)
: m_pGameServer(pGameServer)
{
	Reset();
}

CBroadcaster::~CBroadcaster()
{
	Reset();
}

void CBroadcaster::Reset()
{
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_aBroadcast[i][0] = '\0';
		m_aNextBroadcast[i] = -1;
		m_aBroadcastStop[i] = -1;
	}

	m_Changed = ~0; // Set all to 1
}

void CBroadcaster::SetBroadcast(int Cid, const char *pText, int Lifespan)
{
	if (Cid < 0)
	{
		// Set broadcast for all clients
		for (int i = 0; i < MAX_CLIENTS; ++i)
			SetBroadcast(i, pText, Lifespan);

		return;
	}

	if (Lifespan < 0) {
		m_aBroadcastStop[Cid] = -1;
	} else {
		m_aBroadcastStop[Cid] = TICK + Lifespan;
	}

	bool Changed = str_comp(m_aBroadcast[Cid], pText) != 0;
	if (Changed)
	{
		str_copy(m_aBroadcast[Cid], pText, sizeof m_aBroadcast[Cid]);
		m_Changed |= (1 << Cid); // m_Changed[Cid] = 1
	}
}

void CBroadcaster::Update()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (!GAME_SERVER->IsClientReady(i))
			continue;

		if (m_aBroadcastStop[i] >= 0 && m_aBroadcastStop[i] < TICK)
		{
			// Stop broadcasting
			GAME_SERVER->SendBroadcast(" ", i); // Clear the broadcast message
			m_aBroadcast[i][0] = '\0';

			// Do this because we already sent the broadcast here. This
			// prevents future broadcasts from being sent.
			m_Changed &= ~(1 << i); // m_Changed[i] = 0;

			m_aBroadcastStop[i] = -1;
		}

		if (((m_Changed & (1 << i)) || m_aNextBroadcast[i] < TICK) &&
			m_aBroadcast[i][0] != '\0'
		) {
			GAME_SERVER->SendBroadcast(m_aBroadcast[i], i);
			m_aNextBroadcast[i] = TICK + TICK_SPEED * 3;
		}
	}

	m_Changed = 0;
}
