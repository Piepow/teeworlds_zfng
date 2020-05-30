#ifndef GAME_SERVER_ENTITIES_FLAG_STAND_H
#define GAME_SERVER_ENTITIES_FLAG_STAND_H

#include <game/server/entity.h>

class CFlagStand : public CEntity
{
public:
	enum {
		NUM_INNER_DOTS = 10,
		NUM_OUTER_DOTS = 12
	};
	enum {
		NUKE_STAND,
		NUKE_DETONATOR
	};
public:
	CFlagStand(CGameWorld *pGameWorld, int Type);
	virtual ~CFlagStand();

	virtual void Snap(int SnappingClient);
private:
	int m_Type;
	int m_aOuterDots[NUM_OUTER_DOTS];
	int m_aInnerDots[NUM_INNER_DOTS];

	void SnapOuterDots();
	void SnapInnerDots();
};

#endif
