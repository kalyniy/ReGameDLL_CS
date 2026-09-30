#include "precompiled.h"

void InstallCommands()
{
	static bool installedCommands = false;
	if (installedCommands)
		return;

	if (AreRunningCZero())
	{
		ADD_SERVER_COMMAND("career_continue", SV_Continue_f);
		ADD_SERVER_COMMAND("career_matchlimit", SV_CareerMatchLimit_f);
		ADD_SERVER_COMMAND("career_add_task", SV_CareerAddTask_f);
		ADD_SERVER_COMMAND("career_endround", SV_Career_EndRound_f);
		ADD_SERVER_COMMAND("career_restart", SV_Career_Restart_f);
		ADD_SERVER_COMMAND("tutor_toggle", SV_Tutor_Toggle_f);
	}

	ADD_SERVER_COMMAND("perf_test", SV_LoopPerformance_f);
	ADD_SERVER_COMMAND("print_ent", SV_PrintEntities_f);
	// Cs16Ai P2 engine probes (sv_cheats 1 only)
	ADD_SERVER_COMMAND("probe_setpos", SV_ProbeSetPos_f);
	ADD_SERVER_COMMAND("probe_getpos", SV_ProbeGetPos_f);

	installedCommands = true;
}

// Cs16Ai P2 engine probes (the movement parity and penetration probes; Cs16Ai out/p2/h/round3.md). Refused unless
// sv_cheats is 1. `first` is the index of the first argument after the target:
//   probe_setpos x y z [pitch yaw roll [vx vy vz [duck 0|1]]]     place the player exactly (a teleport: FL_ONGROUND cleared,
//                                                                   the view set with fixangle, velocity and base velocity set)
void ProbeSetPos(CBasePlayer *pPlayer, int first)
{
	if (CVAR_GET_FLOAT("sv_cheats") == 0.0f)
	{
		SERVER_PRINT("probe_setpos: needs sv_cheats 1\n");
		return;
	}
	if (!pPlayer || !pPlayer->IsAlive() || CMD_ARGC() < first + 3)
	{
		SERVER_PRINT("usage: probe_setpos [entindex] x y z [pitch yaw roll [vx vy vz [duck 0|1]]] (a living player)\n");
		return;
	}
	entvars_t *pev = pPlayer->pev;
	const Vector origin(Q_atof(CMD_ARGV(first)), Q_atof(CMD_ARGV(first + 1)), Q_atof(CMD_ARGV(first + 2)));
	if (CMD_ARGC() >= first + 10)
	{
		// the stance first: a ducked player has the ducked hull and view offset (pm_shared PM_Duck's end state)
		if (Q_atoi(CMD_ARGV(first + 9)) != 0)
		{
			pev->flags |= FL_DUCKING;
			pev->view_ofs = VEC_DUCK_VIEW;
			UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
		}
		else
		{
			pev->flags &= ~FL_DUCKING;
			pev->view_ofs = VEC_VIEW;
			UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
		}
		pev->bInDuck = FALSE;
		pev->flDuckTime = 0;
	}
	pev->flags &= ~FL_ONGROUND;
	UTIL_SetOrigin(pev, origin);
	if (CMD_ARGC() >= first + 6)
	{
		const Vector angles(Q_atof(CMD_ARGV(first + 3)), Q_atof(CMD_ARGV(first + 4)), Q_atof(CMD_ARGV(first + 5)));
		pev->angles = angles;
		pev->v_angle = angles;
		pev->fixangle = 1;
	}
	pev->velocity = g_vecZero;
	if (CMD_ARGC() >= first + 9)
		pev->velocity = Vector(Q_atof(CMD_ARGV(first + 6)), Q_atof(CMD_ARGV(first + 7)), Q_atof(CMD_ARGV(first + 8)));
	pev->basevelocity = g_vecZero;
	UTIL_LogPrintf("probe_setpos: #%d \"%s\" origin %.4f %.4f %.4f angles %.4f %.4f %.4f velocity %.4f %.4f %.4f flags %d time %.4f\n",
	               pPlayer->entindex(), STRING(pev->netname), pev->origin.x, pev->origin.y, pev->origin.z, pev->v_angle.x, pev->v_angle.y,
	               pev->v_angle.z, pev->velocity.x, pev->velocity.y, pev->velocity.z, pev->flags, gpGlobals->time);
}

// probe_getpos: where the player is, on the server, now (to the log and the console)
void ProbeGetPos(CBasePlayer *pPlayer)
{
	if (!pPlayer)
		return;
	entvars_t *pev = pPlayer->pev;
	char line[512];
	Q_snprintf(line, sizeof(line), "probe_getpos: #%d \"%s\" origin %.4f %.4f %.4f angles %.4f %.4f %.4f velocity %.4f %.4f %.4f flags %d onground %d ducking %d time %.4f\n",
	           pPlayer->entindex(), STRING(pev->netname), pev->origin.x, pev->origin.y, pev->origin.z, pev->v_angle.x, pev->v_angle.y,
	           pev->v_angle.z, pev->velocity.x, pev->velocity.y, pev->velocity.z, pev->flags, (pev->flags & FL_ONGROUND) ? 1 : 0,
	           (pev->flags & FL_DUCKING) ? 1 : 0, gpGlobals->time);
	UTIL_LogPrintf("%s", line);
	SERVER_PRINT(line);
}

// server console: probe_setpos <entindex> x y z [...]; probe_getpos <entindex>
void SV_ProbeSetPos_f()
{
	if (CMD_ARGC() < 2)
	{
		SERVER_PRINT("usage: probe_setpos <entindex> x y z [pitch yaw roll [vx vy vz [duck 0|1]]]\n");
		return;
	}
	CBasePlayer *pPlayer = UTIL_PlayerByIndex(Q_atoi(CMD_ARGV(1)));
	ProbeSetPos(UTIL_IsValidPlayer(pPlayer) ? pPlayer : nullptr, 2);
}

void SV_ProbeGetPos_f()
{
	if (CVAR_GET_FLOAT("sv_cheats") == 0.0f)
	{
		SERVER_PRINT("probe_getpos: needs sv_cheats 1\n");
		return;
	}
	CBasePlayer *pPlayer = CMD_ARGC() >= 2 ? UTIL_PlayerByIndex(Q_atoi(CMD_ARGV(1))) : nullptr;
	if (!UTIL_IsValidPlayer(pPlayer))
	{
		SERVER_PRINT("usage: probe_getpos <entindex>\n");
		return;
	}
	ProbeGetPos(pPlayer);
}

void SV_Continue_f()
{
	if (CSGameRules()->IsCareer() && CSGameRules()->m_flRestartRoundTime > 100000.0)
	{
		CSGameRules()->m_flRestartRoundTime = gpGlobals->time;

		// go continue
		MESSAGE_BEGIN(MSG_ALL, gmsgCZCareer);
			WRITE_STRING("GOGOGO");
		MESSAGE_END();

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);

			if (!UTIL_IsValidPlayer(pPlayer))
				continue;

			if (!pPlayer->IsBot())
			{
				// at the end of the round is showed window with the proposal surrender or continue
				// now of this time HUD is completely hidden
				// we must to restore HUD after entered continued
				pPlayer->m_iHideHUD &= ~HIDEHUD_ALL;
			}
		}
	}
}

void SV_CareerMatchLimit_f()
{
	if (CMD_ARGC() != 3)
	{
		return;
	}

	if (CSGameRules()->IsCareer())
	{
		CSGameRules()->SetCareerMatchLimit(Q_atoi(CMD_ARGV(1)), Q_atoi(CMD_ARGV(2)));
	}
}

void SV_CareerAddTask_f()
{
	if (CMD_ARGC() != 7)
		return;

	const char *taskName = CMD_ARGV(1);
	const char *weaponName = CMD_ARGV(2);

	int reps = Q_atoi(CMD_ARGV(3));
	bool mustLive = Q_atoi(CMD_ARGV(4)) != 0;
	bool crossRounds = Q_atoi(CMD_ARGV(5)) != 0;
	bool isComplete = Q_atoi(CMD_ARGV(6)) != 0;

	if (TheCareerTasks)
	{
		TheCareerTasks->AddTask(taskName, weaponName, reps, mustLive, crossRounds, isComplete);
	}
}

void SV_Career_EndRound_f()
{
	if (!CSGameRules()->IsCareer() || !CSGameRules()->IsInCareerRound())
	{
		return;
	}

	CBasePlayer *pLocalPlayer = UTIL_GetLocalPlayer();
	if (pLocalPlayer)
	{
		SERVER_COMMAND("kill\n");

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);

			if (!UTIL_IsValidPlayer(pPlayer))
				continue;

			if (pPlayer->IsBot() && pPlayer->m_iTeam == pLocalPlayer->m_iTeam)
			{
				SERVER_COMMAND(UTIL_VarArgs("bot_kill \"%s\"\n", STRING(pPlayer->pev->netname)));
			}
		}
	}
}

void SV_Career_Restart_f()
{
	if (CSGameRules()->IsCareer())
	{
		CSGameRules()->CareerRestart();
	}
}

void SV_Tutor_Toggle_f()
{
	CVAR_SET_FLOAT("tutor_enable", (CVAR_GET_FLOAT("tutor_enable") <= 0.0));
}

void SV_LoopPerformance_f()
{
	CCounter loopCounter;
	loopCounter.Init();

	double start, end;
	int i;

	start = loopCounter.GetCurTime();

	for (i = 0; i < 100; i++)
	{
		CBaseEntity *pSpot;
		for (pSpot = UTIL_FindEntityByString_Old(nullptr, "classname", "info_player_start"); pSpot; pSpot = UTIL_FindEntityByString_Old(pSpot, "classname", "info_player_start"))
			;

		for (pSpot = UTIL_FindEntityByString_Old(nullptr, "classname", "info_player_deathmatch"); pSpot; pSpot = UTIL_FindEntityByString_Old(pSpot, "classname", "info_player_deathmatch"))
			;

		for (pSpot = UTIL_FindEntityByString_Old(nullptr, "classname", "player"); pSpot; pSpot = UTIL_FindEntityByString_Old(pSpot, "classname", "player"))
			;

		for (pSpot = UTIL_FindEntityByString_Old(nullptr, "classname", "bodyque"); pSpot; pSpot = UTIL_FindEntityByString_Old(pSpot, "classname", "bodyque"))
			;
	}

	end = loopCounter.GetCurTime();
	CONSOLE_ECHO(" Time in old search loop %.4f\n", (end - start) * 1000.0);

	// check time new search loop
	start = loopCounter.GetCurTime();

	for (i = 0; i < 100; i++)
	{
		CBaseEntity *pSpot;
		for (pSpot = UTIL_FindEntityByClassname(nullptr, "info_player_start"); pSpot; pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_start"))
			;

		for (pSpot = UTIL_FindEntityByClassname(nullptr, "info_player_deathmatch"); pSpot; pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch"))
			;

		for (pSpot = UTIL_FindEntityByClassname(nullptr, "player"); pSpot; pSpot = UTIL_FindEntityByClassname(pSpot, "player"))
			;

		for (pSpot = UTIL_FindEntityByClassname(nullptr, "bodyque"); pSpot; pSpot = UTIL_FindEntityByClassname(pSpot, "bodyque"))
			;
	}

	end = loopCounter.GetCurTime();
	CONSOLE_ECHO(" Time in new search loop %.4f\n", (end - start) * 1000.0);
}

void SV_PrintEntities_f()
{
	for (int i = 0; i < stringsHashTable.Count(); i++)
	{
		hash_item_t *item = &stringsHashTable[i];

		if (item->pev)
		{
			UTIL_LogPrintf("Print: %s %i %p\n", STRING(stringsHashTable[i].pev->classname), ENTINDEX(ENT(item->pev)), item->pev);
		}

		for (item = stringsHashTable[i].next; item; item = item->next)
		{
			UTIL_LogPrintf("Print: %s %i %p\n", STRING(item->pev->classname), ENTINDEX(ENT(item->pev)), item->pev);
		}
	}
}
