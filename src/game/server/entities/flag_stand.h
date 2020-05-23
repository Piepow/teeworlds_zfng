#ifndef GAME_SERVER_ENTITIES_FLAG_STAND_H
#define GAME_SERVER_ENTITIES_FLAG_STAND_H

#include <game/server/entity.h>
#include "flag.h"

class CFlagStand : public CEntity
{
public:
	enum { NUM_LASER_DOTS = 20 };
public:
	CFlagStand(CGameWorld *pGameWorld);
	virtual ~CFlagStand();

	virtual void Snap(int SnappingClient);
private:
	int m_aLaserDotIDs[NUM_LASER_DOTS];
};

#endif
