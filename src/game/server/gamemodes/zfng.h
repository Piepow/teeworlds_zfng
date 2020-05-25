/* (c) KeksTW. */
#ifndef GAME_SERVER_GAMEMODES_ZFNG_H
#define GAME_SERVER_GAMEMODES_ZFNG_H

#include <game/server/gamecontroller.h>
#include <game/server/broadcaster.h>
#include <game/server/nuke.h>
#include <base/vmath.h>

class CGameControllerZFNG : public IGameController
{
public:
	CGameControllerZFNG(class CGameContext* pGameServer);
	CGameControllerZFNG(class CGameContext* pGameServer, CConfiguration& pConfig);

	virtual bool IsTeamplay() const;
	virtual bool UseFakeTeams();
	virtual bool IsInfection() const;

	// Internal game state
	enum EGameState
	{
		IGS_WAITING_FOR_PLAYERS,
		IGS_WAITING_FOR_INFECTION,
		IGS_WAITING_FLAG,
		IGS_NORMAL,
		IGS_NUKE_DETONATED,
		IGS_FINISHING_OFF_ZOMBIES,
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

	virtual bool IsInfectionStarted();
	virtual int GetAutoTeam(int NotThisID);
	virtual bool CanChangeTeam(CPlayer* pPlayer, int JoinTeam);
	bool CanSpawn();
	virtual bool CanSpawn(int Team, vec2* pOutPos);
private:
	CBroadcaster m_Broadcaster;

	void CountPlayers();
	int m_NumHumans; // Cache for performance
	int m_NumInfected; // Cache for performance

	// Calculate how many tees to infect at the start of a round, which depends
	// on number of players
	int CalcMinimumInfected();

	vec2 m_aFlagStandPositions[2];
	class CFlagStand* m_apFlagStands[2];
	class CFlag* m_pFlag;
	void SpawnFlag();
	void RemoveFlag();
	void DoFlag();
	void ReturnFlag(CCharacter* pCharacter);
	void TakeFlag(CCharacter* pCharacter);
	int DropFlagMaybe(class CCharacter* pVictim, class CPlayer* pKiller);
	bool HasFlagHitDeath();
	void DoDroppedFlag();
	void DoFlagCapture();
	void SpawnFlagStand(int Team);

	void FinishOffZombies();
	void DoInitialInfections();
	void UninfectAll();

	void AnnounceWinners();

	CNuke* m_Nuke;
};
#endif
