/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include "flag_stand.h"

#define TICK_SPEED Server()->TickSpeed()
#define TICK Server()->Tick()

CFlagStand::CFlagStand(CGameWorld *pGameWorld, int Type) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_FLAG_STAND)
{
	m_Type = Type;

	for (int i = 0; i < NUM_OUTER_DOTS; i++)
		m_aOuterDots[i] = Server()->SnapNewID();

	if (m_Type == NUKE_DETONATOR) {
		for (int i = 0; i < NUM_INNER_DOTS; i++)
			m_aInnerDots[i] = Server()->SnapNewID();
	}
}

CFlagStand::~CFlagStand()
{
	for (int i = 0; i < NUM_OUTER_DOTS; ++i)
		Server()->SnapFreeID(m_aOuterDots[i]);

	if (m_Type == NUKE_DETONATOR) {
		for (int i = 0; i < NUM_INNER_DOTS; ++i)
			Server()->SnapFreeID(m_aInnerDots[i]);
	}
}

void CFlagStand::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	SnapOuterDots();

	if (m_Type == NUKE_DETONATOR)
		SnapInnerDots();
}

void CFlagStand::SnapOuterDots() {
	float AngularOffset = (float)(TICK % 500) / 500.0f * 2.0f * pi;

	for (int i = 0; i < NUM_OUTER_DOTS; i++)
	{
		float Angle =
			static_cast<float>(i) * 2.0f * pi / NUM_OUTER_DOTS +
			AngularOffset;
		float Radius = 100.0f;
		vec2 Offset = vec2(0.0f, -28.0f);
		vec2 Pos =
			m_Pos +
			vec2(Radius * cos(Angle), Radius * sin(Angle)) +
			Offset;

		CNetObj_Projectile *pObj =
			static_cast<CNetObj_Projectile *>(
				Server()->SnapNewItem(
					NETOBJTYPE_PROJECTILE,
					m_aOuterDots[i],
					sizeof(CNetObj_Projectile)
				)
			);

		if (pObj)
		{
			pObj->m_X = (int)Pos.x;
			pObj->m_Y = (int)Pos.y;
			pObj->m_VelX = 0;
			pObj->m_VelY = 0;
			pObj->m_Type = WEAPON_HAMMER;
		}
	}
}

void CFlagStand::SnapInnerDots() {
	float AngularOffset = (float)(TICK % 250) / 250.0f * 2.0f * pi;

	for (int i = 0; i < NUM_INNER_DOTS; i++)
	{
		float Angle =
			static_cast<float>(i) * 2.0f * pi / NUM_INNER_DOTS +
			AngularOffset;
		float Radius = 50.0f;
		vec2 Offset = vec2(0.0f, -28.0f);
		vec2 Pos =
			m_Pos +
			vec2(Radius * cos(Angle), Radius * sin(Angle)) +
			Offset;

		CNetObj_Laser *pObj =
			static_cast<CNetObj_Laser *>(
				Server()->SnapNewItem(
					NETOBJTYPE_LASER,
					m_aInnerDots[i],
					sizeof(CNetObj_Laser)
				)
			);

		if (pObj)
		{
			pObj->m_X = (int)Pos.x;
			pObj->m_Y = (int)Pos.y;
			pObj->m_FromX = (int)Pos.x;
			pObj->m_FromY = (int)Pos.y;
			pObj->m_StartTick = TICK;
		}
	}
}
