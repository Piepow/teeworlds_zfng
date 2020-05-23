/* (c) KeksTW. */
#ifndef GAME_SERVER_GAMEMODES_ZFNG2_H
#define GAME_SERVER_GAMEMODES_ZFNG2_H

#include <game/server/gamecontroller.h>
#include <game/server/broadcaster.h>
#include <base/vmath.h>

class CGameControllerZFNG2 : public IGameController
{
public:
	CGameControllerZFNG2(class CGameContext* pGameServer);
	CGameControllerZFNG2(class CGameContext* pGameServer, CConfiguration& pConfig);

	virtual bool IsTeamplay() const;
	virtual bool UseFakeTeams();
	virtual bool IsInfection() const;

	// Internal game state
	enum EGameState
	{
		IGS_WAITING_FOR_PLAYERS,
		IGS_WAITING_FOR_INFECTION,
		IGS_WAITING_FOR_INFECTED_FLAG,
		IGS_NORMAL,
		IGS_ROUND_ENDED
	};
	EGameState m_GameState;
	int m_GameStateTimer;

	void SetGameState(EGameState GameState);
	enum { TIMER_INFINITE = -1 };

	virtual void Tick();
	virtual void Snap(int SnappingClient);
	virtual bool OnEntity(int Index, vec2 Pos);
	virtual void OnCharacterSpawn(class CCharacter *pChr);
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);

	void DoInactivePlayers();
	virtual void DoWincheck();

	virtual void PostReset();

protected:
	virtual void StartRound();
	virtual void EndRound();
private:
	CBroadcaster m_Broadcaster;

	void CountPlayers(
		int& NumHumans,
		int& NumInfected,
		int& NumMinimumInfected
	);

	vec2 m_aFlagStandPositions[2];
	class CFlag* m_pInfectionFlag;
	class CFlagStand* m_apFlagStands[2];
	void SpawnInfectionFlag();
	void RemoveInfectionFlag();
	void SpawnFlagStand(int Team);
};
#endif
