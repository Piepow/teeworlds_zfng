#ifndef GAME_SERVER_NUKE_H
#define GAME_SERVER_NUKE_H

#include <game/server/gamecontext.h>

class CNuke
{
public:
	CNuke(class CGameContext* pGameServer, vec2 Center);

	// Returns `true` when the nuke has finished.
	bool Update();
private:
	class CGameContext* m_pGameServer;
	vec2 m_Center;
	float m_Radius;
	int m_ExplosionTimer;
	int m_NumExplosions; // Cache for performance
	void CalcNumExplosions();
	void DoDeaths();
};

#endif
