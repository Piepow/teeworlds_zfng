/* (c) KeksTW. */
#ifndef GAME_SERVER_GAMEMODES_ZFNG2_H
#define GAME_SERVER_GAMEMODES_ZFNG2_H

#include <game/server/gamecontroller.h>
#include <base/vmath.h>

class CGameControllerZFNG2 : public IGameController
{
public:
	CGameControllerZFNG2(class CGameContext* pGameServer);
	CGameControllerZFNG2(class CGameContext* pGameServer, CConfiguration& pConfig);

	virtual bool IsTeamplay() const;
	virtual bool UseFakeTeams();
	virtual bool IsInfection() const;

	virtual void Tick();
	virtual void Snap(int SnappingClient);
	virtual void OnCharacterSpawn(class CCharacter *pChr);
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);

	virtual void DoWincheck();

	virtual void PostReset();
protected:
	void EndRound();
};
#endif
