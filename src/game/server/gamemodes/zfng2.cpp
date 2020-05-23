/* (c) KeksTW    */
#include "zfng2.h"
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

static const int gs_minPlayers = 4;

CGameControllerZFNG2::CGameControllerZFNG2(class CGameContext *pGameServer) :
	IGameController((class CGameContext*)pGameServer),
	m_Broadcaster(pGameServer)
{
	m_pGameType = "zfng2";
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
	switch (GameState)
	{
		case IGS_WAITING_FOR_PLAYERS:
			m_GameState = GameState;
			m_GameStateTimer = TIMER_INFINITE;
			break;
		case IGS_WAITING_FOR_INFECTION:
			m_GameState = GameState;
			m_GameStateTimer = Server()->TickSpeed() * 5;
			IGameController::StartRound();
			break;
		case IGS_WAITING_FOR_INFECTED_FLAG:
			m_GameState = GameState;
			m_GameStateTimer = Server()->TickSpeed() * 2;
			break;
		case IGS_NORMAL:
			SpawnInfectionFlag();
			m_GameState = GameState;
			m_GameStateTimer = TIMER_INFINITE;
			break;
	}
}

void CGameControllerZFNG2::Tick()
{
	if (m_GameOverTick != -1) {
		if (Server()->Tick() > m_GameOverTick + Server()->TickSpeed() * 10) {
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
				SetGameState(IGS_WAITING_FOR_INFECTED_FLAG);
				break;
			case IGS_WAITING_FOR_INFECTED_FLAG:
				SetGameState(IGS_NORMAL);
				break;
		}
	} else {
		// Timer is still running
		switch (m_GameState)
		{
			case IGS_WAITING_FOR_PLAYERS:
				{
					if (NumHumans + NumInfected >= 2) {
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
		}
	}

	m_Broadcaster.Update();
	DoInactivePlayers();
	DoWincheck();
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

int CGameControllerZFNG2::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	// do scoreing
	if(!pKiller || Weapon == WEAPON_GAME)
		return 0;
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
	return 0;
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
	RemoveInfectionFlag();
	SetGameState(IGS_WAITING_FOR_PLAYERS);
}

void CGameControllerZFNG2::EndRound()
{
	IGameController::EndRound();
	GameServer()->SendRoundStats();
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

void CGameControllerZFNG2::SpawnInfectionFlag()
{
	if (m_aFlagStandPositions[TEAM_INFECTED] != NULL) {
		m_pInfectionFlag = new CFlag(&GameServer()->m_World, TEAM_INFECTED);
		vec2 StandPos = m_aFlagStandPositions[TEAM_INFECTED];
		m_pInfectionFlag->m_StandPos = StandPos;
		m_pInfectionFlag->m_Pos = StandPos;
		GameServer()->m_World.InsertEntity(m_pInfectionFlag);
	}
}

void CGameControllerZFNG2::RemoveInfectionFlag()
{
	if (m_pInfectionFlag != NULL)
		GameServer()->m_World.DestroyEntity(m_pInfectionFlag);

	m_pInfectionFlag = NULL;
}

void CGameControllerZFNG2::SpawnFlagStand(int Team)
{
	vec2 StandPos = m_aFlagStandPositions[Team];
	m_apFlagStands[Team] = new CFlagStand(&GameServer()->m_World);
	m_apFlagStands[Team]->m_Pos = StandPos;
	GameServer()->m_World.InsertEntity(m_apFlagStands[Team]);
}
