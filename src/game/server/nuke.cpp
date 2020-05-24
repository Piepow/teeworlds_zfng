#include "nuke.h"

#define TICK_SPEED m_pGameServer->Server()->TickSpeed()
#define TICK m_pGameServer->Server()->Tick()

static const int gs_MaxRadius = 50 * 32; // Units: px

CNuke::CNuke(class CGameContext* pGameServer, vec2 Center) :
	m_pGameServer(pGameServer),
	m_Center(Center)
{
	m_Radius = 0.0f;
	CalcNumExplosions();
	DoEmotes();
}

void CNuke::DoEmotes()
{
	CCharacter *p = (CCharacter*)m_pGameServer->m_World
		.FindFirst(CGameWorld::ENTTYPE_CHARACTER);

	for (; p; p = (CCharacter *)p->TypeNext()) {
		if (p->GetPlayer()->IsInfected()) {
			m_pGameServer->SendEmoticon(
				p->GetPlayer()->GetCID(),
				EMOTICON_GHOST
			);
		} else {
			m_pGameServer->SendEmoticon(
				p->GetPlayer()->GetCID(),
				EMOTICON_EYES
			);
		}
	}
}

bool CNuke::Update()
{
	if (m_Radius > gs_MaxRadius) return true;

	if (frandom() < 0.75f) {
		for (int i = 0; i < m_NumExplosions; i++)
		{
			if (frandom() < 0.75f) continue;
			float Angle = (float)(i) * 2.0 * pi / m_NumExplosions;
			vec2 ExplosionPos =
				m_Center +
				vec2(m_Radius * cos(Angle), m_Radius * sin(Angle));

			if (!m_pGameServer->Collision()->CheckPoint(ExplosionPos)) {
				m_pGameServer->CreateExplosion(
					ExplosionPos, -1, WEAPON_GAME, true
				);

				// Prevent ear-rape
				if (i % 16 == 0) {
					m_pGameServer->CreateSound(
						ExplosionPos,
						SOUND_GRENADE_EXPLODE
					);
				}
			}
		}
	}

	m_Radius += 8;
	CalcNumExplosions();

	DoDeaths();

	return false;
}

void CNuke::CalcNumExplosions()
{
	static const float s_ArcLength = 128;
	float Circumference = 2.0 * pi * m_Radius;
	m_NumExplosions = max(round_to_int(Circumference / s_ArcLength), 1);
}

void CNuke::DoDeaths()
{
	CCharacter *p = (CCharacter*)m_pGameServer->m_World
		.FindFirst(CGameWorld::ENTTYPE_CHARACTER);

	CCharacter* pNext;

	while (p != NULL) {
		// We have to do this before `Die` because `Die` will remove it from
		// the linked list
		pNext = (CCharacter *)p->TypeNext();

		if (p->GetPlayer()->IsInfected()) {
			float Dist = distance(p->m_Pos, m_Center);

			if (Dist <= m_Radius) {
				p->Die(p->GetPlayer()->GetCID(), WEAPON_GAME);
			}
		}

		p = pNext;
	}
}
