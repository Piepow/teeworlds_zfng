/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include "flag_stand.h"

CFlagStand::CFlagStand(CGameWorld *pGameWorld) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_FLAG_STAND)
{
	for (int i = 0; i < NUM_LASER_DOTS; i++)
		m_aLaserDotIDs[i] = Server()->SnapNewID();
}

CFlagStand::~CFlagStand()
{
	for (int i = 0; i < NUM_LASER_DOTS; ++i)
		Server()->SnapFreeID(m_aLaserDotIDs[i]);
}

void CFlagStand::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	// Make a pretty ring of laser dots
	for (int i = 0; i < NUM_LASER_DOTS; i++)
	{
		float Angle = static_cast<float>(i) * 2.0 * pi / NUM_LASER_DOTS;
		float Radius = 100.0f;
		vec2 Offset = vec2(0.0f, -28.0f);
		vec2 LaserPos =
			m_Pos +
			vec2(Radius * cos(Angle), Radius * sin(Angle)) +
			Offset;

		CNetObj_Projectile *pObj =
			static_cast<CNetObj_Projectile *>(
				Server()->SnapNewItem(
					NETOBJTYPE_PROJECTILE,
					m_aLaserDotIDs[i],
					sizeof(CNetObj_Projectile)
				)
			);

		if (pObj)
		{
			pObj->m_X = (int)LaserPos.x;
			pObj->m_Y = (int)LaserPos.y;
			pObj->m_VelX = 0;
			pObj->m_VelY = 0;
			pObj->m_Type = WEAPON_HAMMER;
		}
	}
}
