/* (c) KeksTW    */
#include "zfng2.h"
#include <game/mapitems.h>
#include "../entities/character.h"
#include "../entities/flag.h"
#include "../entities/flag_stand.h"
#include "../player.h"
#include "../fng2define.h"
#include <engine/shared/config.h>
#include <string.h>
#include <stdio.h>

#define TICK_SPEED Server()->TickSpeed()
#define TICK Server()->Tick()

static const int gs_MinPlayers = 2;

CGameControllerZFNG2::CGameControllerZFNG2(class CGameContext *pGameServer) :
	IGameController((class CGameContext*)pGameServer),
	m_Broadcaster(pGameServer)
{
	m_pGameType = "zfng2";
	m_GameFlags = GAMEFLAG_FLAGS;
	SetGameState(IGS_WAITING_FOR_PLAYERS);
}

CGameControllerZFNG2::CGameControllerZFNG2(
	class CGameContext *pGameServer,
	CConfiguration& pConfig
) :
	IGameController((class CGameContext*)pGameServer, pConfig),
	m_Broadcaster(pGameServer)
{
	m_pGameType = "zfng2";
	m_GameFlags = GAMEFLAG_FLAGS;
	SetGameState(IGS_WAITING_FOR_PLAYERS);
}

bool CGameControllerZFNG2::IsTeamplay() const
{
	return true;
}

bool CGameControllerZFNG2::UseFakeTeams()
{
	return true;
}

bool CGameControllerZFNG2::IsInfection() const
{
	return true;
}

void CGameControllerZFNG2::SetGameState(EGameState GameState)
{
	m_GameState = GameState;

	switch (GameState)
	{
		case IGS_WAITING_FOR_PLAYERS:
			m_GameStateTimer = TIMER_INFINITE;
			break;
		case IGS_WAITING_FOR_INFECTION:
			m_GameStateTimer = TICK_SPEED * 2;
			IGameController::StartRound();
			break;
		case IGS_WAITING_FLAG:
			m_GameStateTimer = TICK_SPEED * 2;
			break;
		case IGS_NORMAL:
			SpawnFlag();
			m_GameStateTimer = TIMER_INFINITE;
			break;
		case IGS_NUKE_DETONATED:
			m_Nuke = new CNuke(
				GameServer(),
				m_aFlagStandPositions[TEAM_HUMAN]
			);
			m_GameStateTimer = TIMER_INFINITE;
			// TODO: Prevent players from spawning
			break;
		case IGS_FINISHING_OFF_ZOMBIES:
			FinishOffZombies();
			m_GameStateTimer = TICK_SPEED * 2;
			break;
		case IGS_ROUND_ENDED:
			// We don't do anything here, because we rely on `m_GameOverTick`
			// from `IGameController`
			m_GameStateTimer = TIMER_INFINITE;
			break;
	}
}

void CGameControllerZFNG2::Tick()
{
	if (m_GameOverTick != -1) {
		if (Server()->Tick() > m_GameOverTick + TICK_SPEED * 10) {
			CycleMap();
			StartRound();
			m_RoundCount++;
		} else {
			return;
		}
	}

	if (GameServer()->m_World.m_Paused && m_UnpauseTimer)
	{
		++m_RoundStartTick;
		--m_UnpauseTimer;
		if (!m_UnpauseTimer)
			GameServer()->m_World.m_Paused = false;
		return;
	}

	if (m_GameStateTimer > 0)
		--m_GameStateTimer;

	// `NumMinimumInfected` is the least number of tees that must be infected,
	// which depends on the player count.
	int NumHumans, NumInfected, NumMinimumInfected;
	CountPlayers(NumHumans, NumInfected, NumMinimumInfected);

	if (m_GameStateTimer == 0)
	{
		// Timer just fired
		switch (m_GameState)
		{
			case IGS_WAITING_FOR_INFECTION:
				SetGameState(IGS_WAITING_FLAG);
				break;
			case IGS_WAITING_FLAG:
				SetGameState(IGS_NORMAL);
				break;
			case IGS_FINISHING_OFF_ZOMBIES:
				EndRound();
				break;
		}
	} else {
		// Timer is still running
		switch (m_GameState)
		{
			case IGS_WAITING_FOR_PLAYERS:
				{
					if (NumHumans + NumInfected >= gs_MinPlayers) {
						SetGameState(IGS_WAITING_FOR_INFECTION);
					} else {
						// Do broadcasts
						char aBuf[64];
						str_format(
							aBuf, sizeof aBuf,
							"Waiting for players"
						);
						m_Broadcaster.SetBroadcast(-1, aBuf, -1);
					}
					break;
				}
			case IGS_WAITING_FOR_INFECTION:
				{
					char aBuf[64];
					str_format(
						aBuf, sizeof aBuf,
						"Starting infection in %d seconds",
						5 - ((TICK - m_RoundStartTick) / TICK_SPEED)
					);
					m_Broadcaster.SetBroadcast(-1, aBuf, 10);
					break;
				}
			case IGS_WAITING_FLAG:
				{
					// TODO: Broadcast time for flag to appear
					break;
				}
			case IGS_NORMAL:
				if (m_pFlag != NULL)
					DoFlag();
				break;
			case IGS_NUKE_DETONATED:
				if (m_Nuke->Update()) {
					delete m_Nuke;
					m_Nuke = NULL;
					SetGameState(IGS_FINISHING_OFF_ZOMBIES);
				}
				break;
		}
	}

	m_Broadcaster.Update();
	DoInactivePlayers();

	switch (m_GameState) {
		case IGS_WAITING_FLAG:
		case IGS_NORMAL:
			DoMinInfections(NumHumans, NumInfected, NumMinimumInfected);
			DoWincheck();
			break;
	}
}

bool CGameControllerZFNG2::HasFlagHitDeath()
{
	// Reset if flag hits death or leaves the game layer
	int FlagCollision = GameServer()->Collision()->GetCollisionAt(
		m_pFlag->m_Pos.x,
		m_pFlag->m_Pos.y
	);

	int Collision =
		FlagCollision &
		(
			CCollision::COLFLAG_DEATH |
			CCollision::COLFLAG_SPIKE_NORMAL |
			CCollision::COLFLAG_SPIKE_RED |
			CCollision::COLFLAG_SPIKE_BLUE |
			CCollision::COLFLAG_SPIKE_GOLD |
			CCollision::COLFLAG_SPIKE_GREEN |
			CCollision::COLFLAG_SPIKE_PURPLE
		);

	return Collision != 0 || m_pFlag->GameLayerClipped(m_pFlag->m_Pos);
}

void CGameControllerZFNG2::DoFlag()
{
	if (HasFlagHitDeath())
	{
		GameServer()->Console()->Print(
			IConsole::OUTPUT_LEVEL_DEBUG, "game", "flag_return"
		);
		GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
		m_pFlag->Reset();
		return;
	}

	CCharacter* pCarrier = m_pFlag->m_pCarryingCharacter;
	if (pCarrier == NULL)
	{
		// Nobody is currently carrying the flag

		CCharacter *apCloseCCharacters[MAX_CLIENTS];
		int Num = GameServer()->m_World.FindEntities(
			m_pFlag->m_Pos,
			CFlag::ms_PhysSize,
			(CEntity**)apCloseCCharacters,
			MAX_CLIENTS,
			CGameWorld::ENTTYPE_CHARACTER
		);
		for (int i = 0; i < Num; i++)
		{
			CCharacter* pCharacter = apCloseCCharacters[i];
			if (!pCharacter->IsAlive() ||
				pCharacter->GetPlayer()->GetTeam() == TEAM_SPECTATORS ||
				GameServer()->Collision()->IntersectLine(
					m_pFlag->m_Pos,
					pCharacter->m_Pos,
					NULL, NULL
				)
			) { continue; }

			if (pCharacter->GetPlayer()->IsInfected()) {
				if (!m_pFlag->m_AtStand)
					ReturnFlag(pCharacter);
			}
			else {
				TakeFlag(pCharacter);
				break;
			}
		}

		if (pCarrier == NULL && !m_pFlag->m_AtStand)
			DoDroppedFlag();
	} else {
		// Somebody is currently carrying the flag

		// Update flag position
		m_pFlag->m_Pos = pCarrier->m_Pos;

		float CaptureDistance = distance(
			m_pFlag->m_Pos,
			m_aFlagStandPositions[TEAM_HUMAN]
		);
		float MaxCaptureDistance = CFlag::ms_PhysSize + CCharacter::ms_PhysSize;
		if (CaptureDistance < MaxCaptureDistance) {
			// The flag was captured
			DoFlagCapture();
		}
	}
}

void CGameControllerZFNG2::ReturnFlag(CCharacter* pCharacter)
{
	pCharacter->GetPlayer()->m_Score += 1;
	int cid = pCharacter->GetPlayer()->GetCID();

	char aBuf[256];
	str_format(
		aBuf, sizeof(aBuf),
		"flag_return player='%d:%s'",
		cid, Server()->ClientName(cid)
	);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
	m_pFlag->Reset();
}

void CGameControllerZFNG2::TakeFlag(CCharacter* pCharacter)
{
	if (m_pFlag->m_AtStand)
	{
		m_aTeamscore[TEAM_HUMAN]++;
		m_pFlag->m_GrabTick = Server()->Tick();
	}

	m_pFlag->m_AtStand = 0;
	m_pFlag->m_pCarryingCharacter = pCharacter;
	pCharacter->GetPlayer()->m_Score += 1;
	int cid = pCharacter->GetPlayer()->GetCID();

	char aBuf[256];
	str_format(
		aBuf, sizeof(aBuf),
		"flag_grab player='%d:%s'",
		cid,
		Server()->ClientName(cid)
	);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	for (int c = 0; c < MAX_CLIENTS; c++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[c];
		if (pPlayer == NULL)
			continue;

		if (pPlayer->GetTeam() == TEAM_SPECTATORS &&
			pPlayer->m_SpectatorID != SPEC_FREEVIEW &&
			GameServer()->m_apPlayers[pPlayer->m_SpectatorID] &&
			GameServer()->m_apPlayers[pPlayer->m_SpectatorID]->GetTeam() == TEAM_INFECTED
		)
			GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_EN, c);
		else if (pPlayer->GetTeam() == TEAM_INFECTED)
			GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_EN, c);
		else
			GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_PL, c);
	}
}

void CGameControllerZFNG2::DoDroppedFlag()
{
	CCharacter* pCarrier = m_pFlag->m_pCarryingCharacter;
	if (pCarrier == NULL && !m_pFlag->m_AtStand)
	{
		if (TICK > m_pFlag->m_DropTick + TICK_SPEED * 30)
		{
			GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
			m_pFlag->Reset();
		}
		else
		{
			m_pFlag->m_Vel.y += GameServer()->m_World.m_Core.m_Tuning.m_Gravity;
			GameServer()->Collision()->MoveBox(
				&m_pFlag->m_Pos,
				&m_pFlag->m_Vel,
				vec2(m_pFlag->ms_PhysSize, m_pFlag->ms_PhysSize),
				0.5f
			);
		}
	}
}

void CGameControllerZFNG2::DoFlagCapture()
{
	CCharacter* pCarrier = m_pFlag->m_pCarryingCharacter;
	int carrierCid = pCarrier->GetPlayer()->GetCID();
	pCarrier->GetPlayer()->m_Score += 5;

	char aBuf[512];
	str_format(
		aBuf, sizeof(aBuf),
		"flag_capture player='%d:%s'",
		carrierCid,
		Server()->ClientName(carrierCid)
	);
	GameServer()->Console()->Print(
		IConsole::OUTPUT_LEVEL_DEBUG,
		"game",
		aBuf
	);

	float CaptureTime =
		(TICK - m_pFlag->m_GrabTick) / (float)(TICK_SPEED);

	str_format(
		aBuf, sizeof(aBuf),
		"'%s' captured the flag (%d.%s%d seconds)",
		Server()->ClientName(carrierCid),
		(int)CaptureTime % 60,
		((int)(CaptureTime * 100) % 100) < 10 ? "0" : "",
		(int)(CaptureTime * 100) % 100
	);
	GameServer()->SendChat(-1, -2, aBuf);
	GameServer()->CreateSoundGlobal(SOUND_CTF_CAPTURE);
	RemoveFlag();
	SetGameState(IGS_NUKE_DETONATED);
}

void CGameControllerZFNG2::DoInactivePlayers()
{

	if(m_Config.m_SvInactiveKickTime > 0 && !m_Config.m_SvTournamentMode)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS && !Server()->IsAuthed(i))
			{
				if(Server()->Tick() > GameServer()->m_apPlayers[i]->m_LastActionTick+m_Config.m_SvInactiveKickTime*Server()->TickSpeed()*60)
				{
					switch(m_Config.m_SvInactiveKick)
					{
					case 0:
						{
							// move player to spectator
							((CPlayer*)GameServer()->m_apPlayers[i])->SetTeam(TEAM_SPECTATORS);
						}
						break;
					case 1:
						{
							// move player to spectator if the reserved slots aren't filled yet, kick him otherwise
							int Spectators = 0;
							for(int j = 0; j < MAX_CLIENTS; ++j)
								if(GameServer()->m_apPlayers[j] && GameServer()->m_apPlayers[j]->GetTeam() == TEAM_SPECTATORS)
									++Spectators;
							if(Spectators >= m_Config.m_SvSpectatorSlots)
								Server()->Kick(i, "Kicked for inactivity");
							else
								((CPlayer*)GameServer()->m_apPlayers[i])->SetTeam(TEAM_SPECTATORS);
						}
						break;
					case 2:
						{
							// kick the player
							Server()->Kick(i, "Kicked for inactivity");
						}
					}
				}
			}
		}
	}
}

void CGameControllerZFNG2::DoWincheck()
{
	if(m_GameOverTick == -1 && !m_Warmup && !GameServer()->m_World.m_ResetRequested)
	{
		if(IsTeamplay())
		{
			// check score win condition
			if((m_Config.m_SvScorelimit > 0 && (m_aTeamscore[TEAM_RED] >= m_Config.m_SvScorelimit || m_aTeamscore[TEAM_BLUE] >= m_Config.m_SvScorelimit)) ||
				(m_Config.m_SvTimelimit > 0 && (Server()->Tick()-m_RoundStartTick) >= m_Config.m_SvTimelimit*Server()->TickSpeed()*60))
			{
				if(m_Config.m_SvTournamentMode){
				} else {
					if(m_aTeamscore[TEAM_RED] != m_aTeamscore[TEAM_BLUE])
						EndRound();
					else
						m_SuddenDeath = 1;
				}
			}
		}
		else
		{
			// gather some stats
			int Topscore = 0;
			int TopscoreCount = 0;
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(GameServer()->m_apPlayers[i])
				{
					if(GameServer()->m_apPlayers[i]->m_Score > Topscore)
					{
						Topscore = GameServer()->m_apPlayers[i]->m_Score;
						TopscoreCount = 1;
					}
					else if(GameServer()->m_apPlayers[i]->m_Score == Topscore)
						TopscoreCount++;
				}
			}

			// check score win condition
			if((m_Config.m_SvScorelimit > 0 && Topscore >= m_Config.m_SvScorelimit) ||
				(m_Config.m_SvTimelimit > 0 && (Server()->Tick()-m_RoundStartTick) >= m_Config.m_SvTimelimit*Server()->TickSpeed()*60))
			{
				if(TopscoreCount == 1)
					EndRound();
				else
					m_SuddenDeath = 1;
			}
		}
	}
}

void CGameControllerZFNG2::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);

	CNetObj_GameData *pGameDataObj =
		(CNetObj_GameData *)Server()->SnapNewItem(
			NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData)
		);

	if (pGameDataObj == NULL)
		return;

	if (m_pFlag == NULL) {
		pGameDataObj->m_FlagCarrierBlue = FLAG_MISSING;
	} else {
		CCharacter* pCarrier = m_pFlag->m_pCarryingCharacter;

		if (m_pFlag->m_AtStand) {
			pGameDataObj->m_FlagCarrierBlue = FLAG_ATSTAND;
		} else if (pCarrier && pCarrier->GetPlayer()) {
			pGameDataObj->m_FlagCarrierBlue = pCarrier->GetPlayer()->GetCID();
		} else {
			pGameDataObj->m_FlagCarrierBlue = FLAG_TAKEN;
		}
	}

	pGameDataObj->m_FlagCarrierRed = FLAG_MISSING;
}

bool CGameControllerZFNG2::OnEntity(int Index, vec2 Pos)
{
	if (IGameController::OnEntity(Index, Pos))
		return true;

	switch (Index) {
		case ENTITY_FLAGSTAND_RED:
			m_aFlagStandPositions[TEAM_RED] = Pos;
			SpawnFlagStand(TEAM_RED);
			return true;
		case ENTITY_FLAGSTAND_BLUE:
			m_aFlagStandPositions[TEAM_BLUE] = Pos;
			SpawnFlagStand(TEAM_BLUE);
			return true;
	}

	return false;
}

void CGameControllerZFNG2::OnCharacterSpawn(class CCharacter *pChr)
{
	// default health
	pChr->IncreaseHealth(10);

	// give default weapons
	pChr->GiveWeapon(WEAPON_HAMMER, -1);
	pChr->GiveWeapon(WEAPON_RIFLE, -1);
}

int CGameControllerZFNG2::OnCharacterDeath(
	class CCharacter *pVictim,
	class CPlayer *pKiller,
	int Weapon
) {
	// do scoreing
	if(!pKiller || Weapon == WEAPON_GAME)
		return DropFlagMaybe(pVictim, pKiller);
	if(pKiller == pVictim->GetPlayer())
		pVictim->GetPlayer()->m_Stats.m_Selfkills++; // suicide
	else
	{
		if (Weapon == WEAPON_RIFLE || Weapon == WEAPON_GRENADE){
			if(IsTeamplay() && pVictim->GetPlayer()->GetTeam() == pKiller->GetTeam())
				pKiller->m_Stats.m_Teamkills++; // teamkill
			else {
				pKiller->m_Stats.m_Kills++; // normal kill
				pVictim->GetPlayer()->m_Stats.m_Hits++; //hits by oponent
				m_aTeamscore[pKiller->GetTeam()]++; //make this config.?
			}
		} else if(Weapon == WEAPON_SPIKE_NORMAL){
			if(pKiller->GetCharacter()) GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->m_Pos, pKiller->GetCID(), m_Config.m_SvPlayerScoreSpikeNormal);
			pKiller->m_Stats.m_GrabsNormal++;
			pVictim->GetPlayer()->m_Stats.m_Deaths++;
			pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.5f;
		} else if(Weapon == WEAPON_SPIKE_RED || Weapon == WEAPON_SPIKE_BLUE){
			pKiller->m_Stats.m_GrabsTeam++;
			pVictim->GetPlayer()->m_Stats.m_Deaths++;
			if(pKiller->GetCharacter()) GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->m_Pos, pKiller->GetCID(), m_Config.m_SvPlayerScoreSpikeTeam);
			pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.5f;
		} else if(Weapon == WEAPON_SPIKE_GOLD){
			pKiller->m_Stats.m_GrabsGold++;
			pVictim->GetPlayer()->m_Stats.m_Deaths++;
			pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.5f;
			if(pKiller->GetCharacter()) GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->m_Pos, pKiller->GetCID(), m_Config.m_SvPlayerScoreSpikeGold);
		} else if(Weapon == WEAPON_HAMMER){ //only called if team mate unfroze you
			pKiller->m_Stats.m_Unfreezes++;
		}
	}
	if(Weapon == WEAPON_SELF){
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.75f;
	} else if (Weapon == WEAPON_WORLD)
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.75f;
	if (Weapon != WEAPON_RIFLE &&
		Weapon != WEAPON_GRENADE &&
		Weapon != WEAPON_HAMMER
	) {
		// Only drop the flag if the victim is killed, not frozen
		return DropFlagMaybe(pVictim, pKiller);
	} else {
		return 0;
	}
}

int CGameControllerZFNG2::DropFlagMaybe(
	class CCharacter* pVictim,
	class CPlayer* pKiller
) {
	int HadFlag = 0;
	for (int i = 0; i < 2; i++)
	{
		if (m_pFlag &&
			pKiller &&
			pKiller->GetCharacter() &&
			m_pFlag->m_pCarryingCharacter == pKiller->GetCharacter()
		) { HadFlag |= 2; }

		if (m_pFlag && m_pFlag->m_pCarryingCharacter == pVictim)
		{
			GameServer()->CreateSoundGlobal(SOUND_CTF_DROP);
			m_pFlag->m_DropTick = Server()->Tick();
			m_pFlag->m_pCarryingCharacter = 0;
			m_pFlag->m_Vel = vec2(0,0);

			if (pKiller &&
				pKiller->GetTeam() != pVictim->GetPlayer()->GetTeam()
			) { pKiller->m_Score++; }

			HadFlag |= 1;
		}
	}

	return HadFlag;
}

void CGameControllerZFNG2::PostReset() {
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->Respawn();
			GameServer()->m_apPlayers[i]->m_Score = 0;
			GameServer()->m_apPlayers[i]->ResetStats();
			GameServer()->m_apPlayers[i]->m_ScoreStartTick = Server()->Tick();
			GameServer()->m_apPlayers[i]->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
		}
	}
}

void CGameControllerZFNG2::StartRound()
{
	IGameController::StartRound();
	UninfectAll();
	RemoveFlag();
	SetGameState(IGS_WAITING_FOR_PLAYERS);
}

void CGameControllerZFNG2::EndRound()
{
	IGameController::EndRound();
	SetGameState(IGS_ROUND_ENDED);
}

bool CGameControllerZFNG2::IsInfectionStarted()
{
	switch (m_GameState) {
		case IGS_WAITING_FOR_PLAYERS:
		case IGS_WAITING_FOR_INFECTION:
			return false;
		default:
			return true;
	}
}

int CGameControllerZFNG2::GetAutoTeam(int NotThisID)
{
	if (IsInfectionStarted()) {
		return TEAM_INFECTED;
	} else {
		return TEAM_HUMAN;
	}
}

bool CGameControllerZFNG2::CanChangeTeam(CPlayer *pPlayer, int JoinTeam)
{
	if (IsInfectionStarted()) {
		if (JoinTeam == TEAM_INFECTED || JoinTeam == TEAM_SPECTATORS) {
			return true;
		} else {
			return false;
		}
	} else {
		return true;
	}
}

bool CGameControllerZFNG2::CanSpawn(int Team, vec2* pOutPos)
{
	switch (m_GameState) {
		case IGS_NUKE_DETONATED:
		case IGS_FINISHING_OFF_ZOMBIES:
		case IGS_ROUND_ENDED:
			return false;
		default:
			return IGameController::CanSpawn(Team, pOutPos);
	}
}

void CGameControllerZFNG2::CountPlayers(
	int& NumHumans,
	int& NumInfected,
	int& NumMinimumInfected
) {
	// Set them to zero
	NumHumans = 0;
	NumInfected = 0;

	// Loop through and increment them
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		// Check that the player is able to play (not spectating)
		if (GameServer()->IsClientPlayer(i)) {
			if (GameServer()->m_apPlayers[i]->IsInfected())
				NumInfected++;
			else
				NumHumans++;
		}
	}

	// Figure out how many tees to infect at the start of a round, which
	// depends on number of players
	if (NumHumans + NumInfected <= 1)
		NumMinimumInfected = 0;
	else if (NumHumans + NumInfected <= 3)
		NumMinimumInfected = 1;
	else
		NumMinimumInfected = 2;
}

void CGameControllerZFNG2::SpawnFlag()
{
	if (m_aFlagStandPositions[TEAM_INFECTED] != NULL) {
		m_pFlag = new CFlag(&GameServer()->m_World, TEAM_INFECTED);
		vec2 StandPos = m_aFlagStandPositions[TEAM_INFECTED];
		m_pFlag->m_StandPos = StandPos;
		m_pFlag->m_Pos = StandPos;
		GameServer()->m_World.InsertEntity(m_pFlag);
	}
}

void CGameControllerZFNG2::RemoveFlag()
{
	if (m_pFlag != NULL)
		GameServer()->m_World.DestroyEntity(m_pFlag);

	m_pFlag = NULL;
}

void CGameControllerZFNG2::SpawnFlagStand(int Team)
{
	vec2 StandPos = m_aFlagStandPositions[Team];
	m_apFlagStands[Team] = new CFlagStand(&GameServer()->m_World);
	m_apFlagStands[Team]->m_Pos = StandPos;
	GameServer()->m_World.InsertEntity(m_apFlagStands[Team]);
}

void CGameControllerZFNG2::FinishOffZombies()
{
	CCharacter* p = (CCharacter*)GameServer()->m_World
		.FindFirst(CGameWorld::ENTTYPE_CHARACTER);

	CCharacter* pNext;

	while (p != NULL) {
		// We have to do this before `Die` because `Die` will remove it from
		// the linked list
		pNext = (CCharacter *)p->TypeNext();

		if (p->GetPlayer()->IsInfected()) {
			GameServer()->CreateExplosion(p->m_Pos, -1, WEAPON_GAME, true);
			GameServer()->CreateSound(p->m_Pos, SOUND_GRENADE_EXPLODE);
			p->Die(p->GetPlayer()->GetCID(), WEAPON_GAME);
		}

		p = pNext;
	}
}

void CGameControllerZFNG2::DoMinInfections(
	int NumHumans,
	int NumInfected,
	int NumMinimumInfected
) {
	if (NumInfected < NumMinimumInfected) {
		int NumToInfect = NumMinimumInfected - NumInfected;
		while (NumToInfect > 0)
		{
			// Guilty until proven innocent
			bool AllClientsHaveBeenInfectedBefore = true;

			// Loop through all and infect if not infected before
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				if (GameServer()->IsClientPlayer(i) &&
					!Server()->WasClientInfectedBefore(i)
				) {
					// Technically this may not be accurate but having just any
					// number of previously uninfected clients will cause the
					// suspicion that `AllClientsHaveBeenInfectededBefore` is
					// false because this loop is itself the method to check if
					// `AllClientsHaveBeenInfectedBefore`
					AllClientsHaveBeenInfectedBefore = false;

					GameServer()->m_apPlayers[i]->Infect(true, false);

					// remember infection because it was picked
					// by the server (remember bad luck)
					Server()->RememberInfection(i);

					// update counters
					NumToInfect--;
					// not that we need these below
					NumHumans--;
					NumInfected++;

					// check if quota is met
					if (NumToInfect == 0)
						break;
				}
			}

			if (AllClientsHaveBeenInfectedBefore)
			{
				Server()->ForgetAllInfections();
				// Now the next iteration of the 'while' loop should finish the
				// job
			}
		}
	}
}

void CGameControllerZFNG2::UninfectAll()
{
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		CPlayer* player = GameServer()->m_apPlayers[i];
		if (player != NULL) {
			player->m_HumanTime = 0;
			player->Revive(false, false);
		}
	}
}
