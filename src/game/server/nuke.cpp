#include "nuke.h"

#define TICK_SPEED m_pGameServer->Server()->TickSpeed()
#define TICK m_pGameServer->Server()->Tick()

CNuke::CNuke(class CGameContext* pGameServer, vec2 Center) :
	m_pGameServer(pGameServer),
	m_Center(Center)
{
	m_Radius = 0.0f;
	m_ExplosionSpeed = 4.0f;
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
	// The nuke it blows up the entire map

	float Left = m_Center.x - m_Radius;
	float Right = m_Center.x + m_Radius;
	float Top = m_Center.y - m_Radius;
	float Bottom = m_Center.y + m_Radius;

	if (round_to_int(Left) / 32 < 0 &&
		round_to_int(Right) / 32 > m_pGameServer->Collision()->GetWidth() &&
		round_to_int(Top) / 32 < 0 &&
		round_to_int(Bottom) / 32 > m_pGameServer->Collision()->GetHeight())
	{ return true; }

	if (frandom() < 0.75f) {
		for (int i = 0; i < m_NumExplosions; i++)
		{
			if (frandom() < 0.75f) continue;
			float Angle = (float)(i) * 2.0 * pi / m_NumExplosions;
			vec2 ExplosionPos =
				m_Center +
				vec2(m_Radius * cos(Angle), m_Radius * sin(Angle));

			if (round_to_int(ExplosionPos.x) / 32 < 0 ||
				round_to_int(ExplosionPos.x) / 32 > m_pGameServer->Collision()->GetWidth() ||
				round_to_int(ExplosionPos.y) / 32 < 0 ||
				round_to_int(ExplosionPos.y) / 32 > m_pGameServer->Collision()->GetHeight())
			{ continue; }

			// Only create explosions in empty tiles

			int ExplosionCollision = m_pGameServer->Collision()->GetCollisionAt(
				ExplosionPos.x,
				ExplosionPos.y
			);

			int Collision =
				ExplosionCollision &
				(
					CCollision::COLFLAG_SOLID |
					CCollision::COLFLAG_DEATH |
					CCollision::COLFLAG_SPIKE_NORMAL |
					CCollision::COLFLAG_SPIKE_RED |
					CCollision::COLFLAG_SPIKE_BLUE |
					CCollision::COLFLAG_SPIKE_GOLD |
					CCollision::COLFLAG_SPIKE_GREEN |
					CCollision::COLFLAG_SPIKE_PURPLE
				);

			if (Collision == 0) {
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

	m_ExplosionSpeed += 1.0f;
	m_Radius += m_ExplosionSpeed;
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
