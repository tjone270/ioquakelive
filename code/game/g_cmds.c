/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
#include "g_local.h"

#include "../../ui/menudef.h"  // for the voice chats

// DeathmatchScoreboardMessage moved to g_gametype_ffa.c

/*
==================
Cmd_Score_f

Request current scoreboard information
==================
*/
void Cmd_Score_f(gentity_t* ent) {
    DeathmatchScoreboardMessage(ent);
}

/*
==================
CheatsOk
==================
*/
qboolean CheatsOk(gentity_t* ent) {
    if (!g_cheats.integer) {
        trap_SendServerCommand(ent - g_entities, "print \"Cheats are not enabled on this server.\n\"");
        return qfalse;
    }
    if (ent->health <= 0) {
        trap_SendServerCommand(ent - g_entities, "print \"You must be alive to use this command.\n\"");
        return qfalse;
    }
    return qtrue;
}

/*
==================
ConcatArgs
==================
*/
char* ConcatArgs(int start) {
    int i, c, tlen;
    static char line[MAX_STRING_CHARS];
    int len;
    char arg[MAX_STRING_CHARS];

    len = 0;
    c = trap_Argc();
    for (i = start; i < c; i++) {
        trap_Argv(i, arg, sizeof(arg));
        tlen = strlen(arg);
        if (len + tlen >= MAX_STRING_CHARS - 1) {
            break;
        }
        memcpy(line + len, arg, tlen);
        len += tlen;
        if (i != c - 1) {
            line[len] = ' ';
            len++;
        }
    }

    line[len] = 0;

    return line;
}

/*
==================
StringIsInteger
==================
*/
qboolean StringIsInteger(const char* s) {
    int i;
    int len;
    qboolean foundDigit;

    len = strlen(s);
    foundDigit = qfalse;

    for (i = 0; i < len; i++) {
        if (!isdigit(s[i])) {
            return qfalse;
        }

        foundDigit = qtrue;
    }

    return foundDigit;
}

/*
==================
ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
int ClientNumberFromString(gentity_t* to, char* s, qboolean checkNums, qboolean checkNames) {
    gclient_t* cl;
    int idnum;
    char cleanName[MAX_STRING_CHARS];

    if (checkNums) {
        // numeric values could be slot numbers
        if (StringIsInteger(s)) {
            idnum = atoi(s);
            if (idnum >= 0 && idnum < level.maxclients) {
                cl = &level.clients[idnum];
                if (cl->pers.connected == CON_CONNECTED) {
                    return idnum;
                }
            }
        }
    }

    if (checkNames) {
        // check for a name match
        for (idnum = 0, cl = level.clients; idnum < level.maxclients; idnum++, cl++) {
            if (cl->pers.connected != CON_CONNECTED) {
                continue;
            }
            Q_strncpyz(cleanName, cl->pers.netname, sizeof(cleanName));
            Q_CleanStr(cleanName);
            if (!Q_stricmp(cleanName, s)) {
                return idnum;
            }
        }
    }

    trap_SendServerCommand(to - g_entities, va("print \"User %s is not on the server\n\"", s));
    return -1;
}

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void Cmd_Give_f(gentity_t* ent) {
    char* name;
    gitem_t* it;
    int i;
    qboolean give_all;
    gentity_t* it_ent;
    trace_t trace;

    if (!CheatsOk(ent)) {
        return;
    }

    name = ConcatArgs(1);

    if (Q_stricmp(name, "all") == 0)
        give_all = qtrue;
    else
        give_all = qfalse;

    if (give_all || Q_stricmp(name, "health") == 0) {
        ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
        if (!give_all)
            return;
    }

    if (give_all || Q_stricmp(name, "weapons") == 0) {
        ent->client->ps.stats[STAT_WEAPONS] = (1 << WP_NUM_WEAPONS) - 1 -
                                              (1 << WP_GRAPPLING_HOOK) - (1 << WP_NONE);
        if (!give_all)
            return;
    }

    if (give_all || Q_stricmp(name, "ammo") == 0) {
        for (i = 0; i < MAX_WEAPONS; i++) {
            ent->client->ps.ammo[i] = 999;
        }
        if (!give_all)
            return;
    }

    if (give_all || Q_stricmp(name, "armor") == 0) {
        ent->client->ps.stats[STAT_ARMOR] = 200;

        if (!give_all)
            return;
    }

    if (Q_stricmp(name, "excellent") == 0) {
        ent->client->ps.persistant[PERS_EXCELLENT_COUNT]++;
        return;
    }
    if (Q_stricmp(name, "impressive") == 0) {
        ent->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
        return;
    }
    if (Q_stricmp(name, "gauntletaward") == 0) {
        ent->client->ps.persistant[PERS_GAUNTLET_FRAG_COUNT]++;
        return;
    }
    if (Q_stricmp(name, "defend") == 0) {
        ent->client->ps.persistant[PERS_DEFEND_COUNT]++;
        return;
    }
    if (Q_stricmp(name, "assist") == 0) {
        ent->client->ps.persistant[PERS_ASSIST_COUNT]++;
        return;
    }

    // spawn a specific item right on the player
    if (!give_all) {
        it = BG_FindItem(name);
        if (!it) {
            return;
        }

        it_ent = G_Spawn();
        VectorCopy(ent->r.currentOrigin, it_ent->s.origin);
        it_ent->classname = it->classname;
        G_SpawnItem(it_ent, it);
        FinishSpawningItem(it_ent);
        memset(&trace, 0, sizeof(trace));
        Touch_Item(it_ent, ent, &trace);
        if (it_ent->inuse) {
            G_FreeEntity(it_ent);
        }
    }
}

/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f(gentity_t* ent) {
    char* msg;

    if (!CheatsOk(ent)) {
        return;
    }

    ent->flags ^= FL_GODMODE;
    if (!(ent->flags & FL_GODMODE))
        msg = "godmode " S_COLOR_RED "OFF" S_COLOR_WHITE "\n";
    else
        msg = "godmode " S_COLOR_GREEN "ON" S_COLOR_WHITE "\n";

    trap_SendServerCommand(ent - g_entities, va("print \"%s\"", msg));
}

/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f(gentity_t* ent) {
    char* msg;

    if (!CheatsOk(ent)) {
        return;
    }

    ent->flags ^= FL_NOTARGET;
    if (!(ent->flags & FL_NOTARGET))
        msg = "notarget " S_COLOR_RED "OFF" S_COLOR_WHITE "\n";
    else
        msg = "notarget " S_COLOR_GREEN "ON" S_COLOR_WHITE "\n";

    trap_SendServerCommand(ent - g_entities, va("print \"%s\"", msg));
}

/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f(gentity_t* ent) {
    char* msg;

    if (!CheatsOk(ent)) {
        return;
    }

    if (ent->client->noclip) {
        msg = "noclip " S_COLOR_RED "OFF" S_COLOR_WHITE "\n";
    } else {
        msg = "noclip " S_COLOR_GREEN "ON" S_COLOR_WHITE "\n";
    }
    ent->client->noclip = !ent->client->noclip;

    trap_SendServerCommand(ent - g_entities, va("print \"%s\"", msg));
}

/*
==================
Cmd_LevelShot_f

This is just to help generate the level pictures
for the menus.  It goes to the intermission immediately
and sends over a command to the client to resize the view,
hide the scoreboard, and take a special screenshot
==================
*/
void Cmd_LevelShot_f(gentity_t* ent) {
    if (!ent->client->pers.localClient) {
        trap_SendServerCommand(ent - g_entities,
                               "print \"The levelshot command must be executed by a local client\n\"");
        return;
    }

    if (!CheatsOk(ent))
        return;

    BeginIntermission();
    trap_SendServerCommand(ent - g_entities, "clientLevelShot");
}

/*
==================
Cmd_TeamTask_f
==================
*/
void Cmd_TeamTask_f(gentity_t* ent) {
    char userinfo[MAX_INFO_STRING];
    char arg[MAX_TOKEN_CHARS];
    int task;
    int client = ent->client - level.clients;

    if (trap_Argc() != 2) {
        return;
    }
    trap_Argv(1, arg, sizeof(arg));
    task = atoi(arg);

    trap_GetUserinfo(client, userinfo, sizeof(userinfo));
    Info_SetValueForKey(userinfo, "teamtask", va("%d", task));
    trap_SetUserinfo(client, userinfo);
    ClientUserinfoChanged(client);
}

/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f(gentity_t* ent) {
    if (ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
        return;
    }
    if (ent->health <= 0) {
        return;
    }
    ent->flags &= ~FL_GODMODE;
    ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
    player_die(ent, ent, ent, 100000, MOD_SUICIDE);
}

/*
=================
BroadcastTeamChange

Let everyone know about a team change
=================
*/
void BroadcastTeamChange(gclient_t* client, int oldTeam) {
    if (client->sess.sessionTeam == TEAM_RED) {
        trap_SendServerCommand(-1, va("cp \"%s" S_COLOR_WHITE " joined the red team.\n\"",
                                      client->pers.netname));
    } else if (client->sess.sessionTeam == TEAM_BLUE) {
        trap_SendServerCommand(-1, va("cp \"%s" S_COLOR_WHITE " joined the blue team.\n\"",
                                      client->pers.netname));
    } else if (client->sess.sessionTeam == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR) {
        trap_SendServerCommand(-1, va("cp \"%s" S_COLOR_WHITE " joined the spectators.\n\"",
                                      client->pers.netname));
    } else if (client->sess.sessionTeam == TEAM_FREE) {
        trap_SendServerCommand(-1, va("cp \"%s" S_COLOR_WHITE " joined the battle.\n\"",
                                      client->pers.netname));
    }
}

/*
=================
SetTeam
=================
*/
void SetTeam(gentity_t* ent, const char* s) {
    int team, oldTeam;
    gclient_t* client;
    int clientNum;
    spectatorState_t specState;
    int specClient;
    int teamLeader;

    //
    // see what change is requested
    //
    client = ent->client;

    clientNum = client - level.clients;
    specClient = 0;
    specState = SPECTATOR_NOT;
    if (!Q_stricmp(s, "scoreboard") || !Q_stricmp(s, "score")) {
        team = TEAM_SPECTATOR;
        specState = SPECTATOR_SCOREBOARD;
    } else if (!Q_stricmp(s, "follow1")) {
        team = TEAM_SPECTATOR;
        specState = SPECTATOR_FOLLOW;
        specClient = -1;
    } else if (!Q_stricmp(s, "follow2")) {
        team = TEAM_SPECTATOR;
        specState = SPECTATOR_FOLLOW;
        specClient = -2;
    } else if (!Q_stricmp(s, "spectator") || !Q_stricmp(s, "s")) {
        team = TEAM_SPECTATOR;
        if (level.numPlayingClients > 0) {
            specState = SPECTATOR_FOLLOW;
            specClient = -1;
        } else {
            specState = SPECTATOR_FREE;
        }
    } else if (g_gametype.integer >= GT_TEAM) {
        // if running a team game, assign player to one of the teams
        specState = SPECTATOR_NOT;
        if (!Q_stricmp(s, "red") || !Q_stricmp(s, "r")) {
            team = TEAM_RED;
        } else if (!Q_stricmp(s, "blue") || !Q_stricmp(s, "b")) {
            team = TEAM_BLUE;
        } else {
            // pick the team with the least number of players
            team = PickTeam(clientNum);
        }

        if (g_teamForceBalance.integer && !client->pers.localClient && !(ent->r.svFlags & SVF_BOT)) {
            int counts[TEAM_NUM_TEAMS];

            counts[TEAM_BLUE] = TeamCount(clientNum, TEAM_BLUE);
            counts[TEAM_RED] = TeamCount(clientNum, TEAM_RED);

            // [QL] Spread of one (not two like Q3)
            if (team == TEAM_RED && counts[TEAM_RED] - counts[TEAM_BLUE] > 0) {
                trap_SendServerCommand(clientNum,
                                       "cp \"Red team has too many players.\n\"");
                return;  // ignore the request
            }
            if (team == TEAM_BLUE && counts[TEAM_BLUE] - counts[TEAM_RED] > 0) {
                trap_SendServerCommand(clientNum,
                                       "cp \"Blue team has too many players.\n\"");
                return;  // ignore the request
            }

            // It's ok, the team we are switching to has fewer players
        }

    } else {
        // force them to spectators if there aren't any spots free
        team = TEAM_FREE;
    }

    // [QL] Team lock check
    if (team != TEAM_SPECTATOR && G_IsTeamLocked(team)) {
        trap_SendServerCommand(clientNum, "cp \"That team is locked.\n\"");
        return;
    }

    // [QL] Team size limit
    if (g_teamsize.integer > 0 && team != TEAM_SPECTATOR) {
        if (g_gametype.integer >= GT_TEAM) {
            int tc = TeamCount(ent - g_entities, team);
            if (tc >= g_teamsize.integer) {
                trap_SendServerCommand(ent - g_entities,
                    va("cp \"Team is full. Team size is currently set to %d.\n\"", g_teamsize.integer));
                return;
            }
        } else if (g_gametype.integer == GT_FFA) {
            if (level.numNonSpectatorClients >= g_teamsize.integer) {
                trap_SendServerCommand(ent - g_entities,
                    va("cp \"Arena is full. Team size is currently set to %d.\n\"", g_teamsize.integer));
                return;
            }
        }
    }

    // override decision if limiting the players
    if ((g_gametype.integer == GT_DUEL) && level.numNonSpectatorClients >= 2) {
        team = TEAM_SPECTATOR;
    } else if (g_maxGameClients.integer > 0 &&
               level.numNonSpectatorClients >= g_maxGameClients.integer) {
        team = TEAM_SPECTATOR;
    }

    //
    // decide if we will allow the change
    //
    oldTeam = client->sess.sessionTeam;
    if (team == oldTeam && team != TEAM_SPECTATOR) {
        return;
    }

    //
    // execute the team change
    //

    // if the player was dead leave the body, but only if they're actually in game
    if (client->ps.stats[STAT_HEALTH] <= 0 && client->pers.connected == CON_CONNECTED) {
        CopyToBodyQue(ent);
    }

    // he starts at 'base'
    client->pers.teamState.state = TEAM_BEGIN;
    if (oldTeam != TEAM_SPECTATOR) {
        // Kill him (makes sure he loses flags, etc)
        ent->flags &= ~FL_GODMODE;
        ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
        player_die(ent, ent, ent, 100000, MOD_SUICIDE);
    }

    // they go to the end of the line for tournements
    if (team == TEAM_SPECTATOR && oldTeam != team)
        AddTournamentQueue(client);

    client->sess.sessionTeam = team;
    client->sess.spectatorState = specState;
    client->sess.spectatorClient = specClient;

    client->sess.teamLeader = qfalse;
    if (team == TEAM_RED || team == TEAM_BLUE) {
        teamLeader = TeamLeader(team);
        // if there is no team leader or the team leader is a bot and this client is not a bot
        if (teamLeader == -1 || (!(g_entities[clientNum].r.svFlags & SVF_BOT) && (g_entities[teamLeader].r.svFlags & SVF_BOT))) {
            SetLeader(team, clientNum);
        }
    }
    // make sure there is a team leader on the team the player came from
    if (oldTeam == TEAM_RED || oldTeam == TEAM_BLUE) {
        CheckTeamLeader(oldTeam);
    }

    // get and distribute relevant parameters
    ClientUserinfoChanged(clientNum);

    // client hasn't spawned yet, they sent an early team command, teampref userinfo, or g_teamAutoJoin is enabled
    if (client->pers.connected != CON_CONNECTED) {
        return;
    }

    BroadcastTeamChange(client, oldTeam);

    ClientBegin(clientNum);
}

/*
=================
StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
void StopFollowing(gentity_t* ent) {
    ent->client->ps.persistant[PERS_TEAM] = TEAM_SPECTATOR;
    ent->client->sess.sessionTeam = TEAM_SPECTATOR;
    ent->client->sess.spectatorState = SPECTATOR_FREE;
    ent->client->ps.pm_flags &= ~PMF_FOLLOW;
    ent->r.svFlags &= ~SVF_BOT;
    ent->client->ps.clientNum = ent - g_entities;

    SetClientViewAngle(ent, ent->client->ps.viewangles);

    // don't use dead view angles
    if (ent->client->ps.stats[STAT_HEALTH] <= 0) {
        ent->client->ps.stats[STAT_HEALTH] = 1;
    }
}

/*
=================
Cmd_Team_f
=================
*/
void Cmd_Team_f(gentity_t* ent) {
    int oldTeam;
    char s[MAX_TOKEN_CHARS];

    if (trap_Argc() != 2) {
        oldTeam = ent->client->sess.sessionTeam;
        switch (oldTeam) {
            case TEAM_BLUE:
                trap_SendServerCommand(ent - g_entities, "print \"Blue team\n\"");
                break;
            case TEAM_RED:
                trap_SendServerCommand(ent - g_entities, "print \"Red team\n\"");
                break;
            case TEAM_FREE:
                trap_SendServerCommand(ent - g_entities, "print \"Free team\n\"");
                break;
            case TEAM_SPECTATOR:
                trap_SendServerCommand(ent - g_entities, "print \"Spectator team\n\"");
                break;
        }
        return;
    }

    if (ent->client->switchTeamTime > level.time) {
        trap_SendServerCommand(ent - g_entities, "print \"May not switch teams more than once per 5 seconds.\n\"");
        return;
    }

    // if they are playing a tournement game, count as a loss
    if ((g_gametype.integer == GT_DUEL) && ent->client->sess.sessionTeam == TEAM_FREE) {
        ent->client->sess.losses++;
    }

    trap_Argv(1, s, sizeof(s));

    SetTeam(ent, s);

    ent->client->switchTeamTime = level.time + 5000;
}

/*
=================
Cmd_Follow_f
=================
*/
void Cmd_Follow_f(gentity_t* ent) {
    int i;
    char arg[MAX_TOKEN_CHARS];

    if (trap_Argc() != 2) {
        if (ent->client->sess.spectatorState == SPECTATOR_FOLLOW) {
            StopFollowing(ent);
        }
        return;
    }

    trap_Argv(1, arg, sizeof(arg));
    i = ClientNumberFromString(ent, arg, qtrue, qtrue);
    if (i == -1) {
        return;
    }

    // can't follow self
    if (&level.clients[i] == ent->client) {
        return;
    }

    // can't follow another spectator
    if (level.clients[i].sess.sessionTeam == TEAM_SPECTATOR) {
        return;
    }

    // if they are playing a tournement game, count as a loss
    if ((g_gametype.integer == GT_DUEL) && ent->client->sess.sessionTeam == TEAM_FREE) {
        ent->client->sess.losses++;
    }

    // first set them to spectator
    if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
        SetTeam(ent, "spectator");
    }

    ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
    ent->client->sess.spectatorClient = i;
}

/*
=================
Cmd_FollowCycle_f
=================
*/
void Cmd_FollowCycle_f(gentity_t* ent, int dir) {
    int clientnum;
    int original;

    // if they are playing a tournement game, count as a loss
    if ((g_gametype.integer == GT_DUEL) && ent->client->sess.sessionTeam == TEAM_FREE) {
        ent->client->sess.losses++;
    }
    // first set them to spectator
    if (ent->client->sess.spectatorState == SPECTATOR_NOT) {
        SetTeam(ent, "spectator");
    }

    if (dir != 1 && dir != -1) {
        G_Error("Cmd_FollowCycle_f: bad dir %i", dir);
    }

    // if dedicated follow client, just switch between the two auto clients
    if (ent->client->sess.spectatorClient < 0) {
        if (ent->client->sess.spectatorClient == -1) {
            ent->client->sess.spectatorClient = -2;
        } else if (ent->client->sess.spectatorClient == -2) {
            ent->client->sess.spectatorClient = -1;
        }
        return;
    }

    clientnum = ent->client->sess.spectatorClient;
    original = clientnum;
    do {
        clientnum += dir;
        if (clientnum >= level.maxclients) {
            clientnum = 0;
        }
        if (clientnum < 0) {
            clientnum = level.maxclients - 1;
        }

        // can only follow connected clients
        if (level.clients[clientnum].pers.connected != CON_CONNECTED) {
            continue;
        }

        // can't follow another spectator
        if (level.clients[clientnum].sess.sessionTeam == TEAM_SPECTATOR) {
            continue;
        }

        // this is good, we can use it
        ent->client->sess.spectatorClient = clientnum;
        ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
        return;
    } while (clientnum != original);

    // leave it where it was
}

/*
==================
G_Say
==================
*/

static void G_SayTo(gentity_t* ent, gentity_t* other, int mode, int color, const char* name, const char* message) {
    if (!other) {
        return;
    }
    if (!other->inuse) {
        return;
    }
    if (!other->client) {
        return;
    }
    if (other->client->pers.connected != CON_CONNECTED) {
        return;
    }
    if (mode == SAY_TEAM && !OnSameTeam(ent, other)) {
        return;
    }
    // no chatting to players in tournements
    if ((g_gametype.integer == GT_DUEL) && other->client->sess.sessionTeam == TEAM_FREE && ent->client->sess.sessionTeam != TEAM_FREE) {
        return;
    }

    trap_SendServerCommand(other - g_entities, va("%s \"%s%c%c%s\"",
                                                  mode == SAY_TEAM ? "tchat" : "chat",
                                                  name, Q_COLOR_ESCAPE, color, message));
}

#define EC "\x19"

void G_Say(gentity_t* ent, gentity_t* target, int mode, const char* chatText) {
    int j;
    gentity_t* other;
    int color;
    char name[64];
    // don't let text be too long for malicious reasons
    char text[MAX_SAY_TEXT];
    char location[64];

    if (g_gametype.integer < GT_TEAM && mode == SAY_TEAM) {
        mode = SAY_ALL;
    }

    switch (mode) {
        default:
        case SAY_ALL:
            G_LogPrintf("say: %s: %s\n", ent->client->pers.netname, chatText);
            Com_sprintf(name, sizeof(name), "%s%c%c" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
            color = COLOR_GREEN;
            break;
        case SAY_TEAM:
            G_LogPrintf("sayteam: %s: %s\n", ent->client->pers.netname, chatText);
            if (Team_GetLocationMsg(ent, location, sizeof(location)))
                Com_sprintf(name, sizeof(name), EC "(%s%c%c" EC ") (%s)" EC ": ",
                            ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, location);
            else
                Com_sprintf(name, sizeof(name), EC "(%s%c%c" EC ")" EC ": ",
                            ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
            color = COLOR_CYAN;
            break;
        case SAY_TELL:
            if (target && target->inuse && target->client && g_gametype.integer >= GT_TEAM &&
                target->client->sess.sessionTeam == ent->client->sess.sessionTeam &&
                Team_GetLocationMsg(ent, location, sizeof(location)))
                Com_sprintf(name, sizeof(name), EC "[%s%c%c" EC "] (%s)" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, location);
            else
                Com_sprintf(name, sizeof(name), EC "[%s%c%c" EC "]" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
            color = COLOR_MAGENTA;
            break;
    }

    Q_strncpyz(text, chatText, sizeof(text));

    if (target) {
        G_SayTo(ent, target, mode, color, name, text);
        return;
    }

    // echo the text to the console
    if (g_dedicated.integer) {
        G_Printf("%s%s\n", name, text);
    }

    // send it to all the appropriate clients
    for (j = 0; j < level.maxclients; j++) {
        other = &g_entities[j];
        G_SayTo(ent, other, mode, color, name, text);
    }
}

static void SanitizeChatText(char* text) {
    int i;

    for (i = 0; text[i]; i++) {
        if (text[i] == '\n' || text[i] == '\r') {
            text[i] = ' ';
        }
    }
}

/*
==================
Cmd_Say_f
==================
*/
static void Cmd_Say_f(gentity_t* ent, int mode, qboolean arg0) {
    char* p;

    if (trap_Argc() < 2 && !arg0) {
        return;
    }

    if (arg0) {
        p = ConcatArgs(0);
    } else {
        p = ConcatArgs(1);
    }

    SanitizeChatText(p);

    G_Say(ent, NULL, mode, p);
}

/*
==================
Cmd_Tell_f
==================
*/
static void Cmd_Tell_f(gentity_t* ent) {
    int targetNum;
    gentity_t* target;
    char* p;
    char arg[MAX_TOKEN_CHARS];

    if (trap_Argc() < 3) {
        trap_SendServerCommand(ent - g_entities, "print \"Usage: tell <player id> <message>\n\"");
        return;
    }

    trap_Argv(1, arg, sizeof(arg));
    targetNum = ClientNumberFromString(ent, arg, qtrue, qtrue);
    if (targetNum == -1) {
        return;
    }

    target = &g_entities[targetNum];
    if (!target->inuse || !target->client) {
        return;
    }

    p = ConcatArgs(2);

    SanitizeChatText(p);

    G_LogPrintf("tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, p);
    G_Say(ent, target, SAY_TELL, p);
    // don't tell to the player self if it was already directed to this player
    // also don't send the chat back to a bot
    if (ent != target && !(ent->r.svFlags & SVF_BOT)) {
        G_Say(ent, ent, SAY_TELL, p);
    }
}

static void G_VoiceTo(gentity_t* ent, gentity_t* other, int mode, const char* id, qboolean voiceonly) {
    int color;
    char* cmd;

    if (!other) {
        return;
    }
    if (!other->inuse) {
        return;
    }
    if (!other->client) {
        return;
    }
    if (mode == SAY_TEAM && !OnSameTeam(ent, other)) {
        return;
    }
    // no chatting to players in tournements
    if (g_gametype.integer == GT_DUEL) {
        return;
    }

    if (mode == SAY_TEAM) {
        color = COLOR_CYAN;
        cmd = "vtchat";
    } else if (mode == SAY_TELL) {
        color = COLOR_MAGENTA;
        cmd = "vtell";
    } else {
        color = COLOR_GREEN;
        cmd = "vchat";
    }

    trap_SendServerCommand(other - g_entities, va("%s %d %d %d %s", cmd, voiceonly, ent->s.number, color, id));
}

void G_Voice(gentity_t* ent, gentity_t* target, int mode, const char* id, qboolean voiceonly) {
    int j;
    gentity_t* other;

    if (g_gametype.integer < GT_TEAM && mode == SAY_TEAM) {
        mode = SAY_ALL;
    }

    if (target) {
        G_VoiceTo(ent, target, mode, id, voiceonly);
        return;
    }

    // echo the text to the console
    if (g_dedicated.integer) {
        G_Printf("voice: %s %s\n", ent->client->pers.netname, id);
    }

    // send it to all the appropriate clients
    for (j = 0; j < level.maxclients; j++) {
        other = &g_entities[j];
        G_VoiceTo(ent, other, mode, id, voiceonly);
    }
}

/*
==================
Cmd_Voice_f
==================
*/
static void Cmd_Voice_f(gentity_t* ent, int mode, qboolean arg0, qboolean voiceonly) {
    char* p;

    if (trap_Argc() < 2 && !arg0) {
        return;
    }

    if (arg0) {
        p = ConcatArgs(0);
    } else {
        p = ConcatArgs(1);
    }

    SanitizeChatText(p);

    G_Voice(ent, NULL, mode, p, voiceonly);
}

/*
==================
Cmd_VoiceTell_f
==================
*/
static void Cmd_VoiceTell_f(gentity_t* ent, qboolean voiceonly) {
    int targetNum;
    gentity_t* target;
    char* id;
    char arg[MAX_TOKEN_CHARS];

    if (trap_Argc() < 3) {
        trap_SendServerCommand(ent - g_entities, va("print \"Usage: %s <player id> <voice id>\n\"", voiceonly ? "votell" : "vtell"));
        return;
    }

    trap_Argv(1, arg, sizeof(arg));
    targetNum = ClientNumberFromString(ent, arg, qtrue, qtrue);
    if (targetNum == -1) {
        return;
    }

    target = &g_entities[targetNum];
    if (!target->inuse || !target->client) {
        return;
    }

    id = ConcatArgs(2);

    SanitizeChatText(id);

    G_LogPrintf("vtell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, id);
    G_Voice(ent, target, SAY_TELL, id, voiceonly);
    // don't tell to the player self if it was already directed to this player
    // also don't send the chat back to a bot
    if (ent != target && !(ent->r.svFlags & SVF_BOT)) {
        G_Voice(ent, ent, SAY_TELL, id, voiceonly);
    }
}

/*
==================
Cmd_VoiceTaunt_f
==================
*/
static void Cmd_VoiceTaunt_f(gentity_t* ent) {
    gentity_t* who;
    int i;

    if (!ent->client) {
        return;
    }

    // insult someone who just killed you
    if (ent->enemy && ent->enemy->client && ent->enemy->client->lastkilled_client == ent->s.number) {
        // i am a dead corpse
        if (!(ent->enemy->r.svFlags & SVF_BOT)) {
            G_Voice(ent, ent->enemy, SAY_TELL, VOICECHAT_DEATHINSULT, qfalse);
        }
        if (!(ent->r.svFlags & SVF_BOT)) {
            G_Voice(ent, ent, SAY_TELL, VOICECHAT_DEATHINSULT, qfalse);
        }
        ent->enemy = NULL;
        return;
    }
    // insult someone you just killed
    if (ent->client->lastkilled_client >= 0 && ent->client->lastkilled_client != ent->s.number) {
        who = g_entities + ent->client->lastkilled_client;
        if (who->client) {
            // who is the person I just killed
            if (who->client->lasthurt_mod[0] == MOD_GAUNTLET) {
                if (!(who->r.svFlags & SVF_BOT)) {
                    G_Voice(ent, who, SAY_TELL, VOICECHAT_KILLGAUNTLET, qfalse);  // and I killed them with a gauntlet
                }
                if (!(ent->r.svFlags & SVF_BOT)) {
                    G_Voice(ent, ent, SAY_TELL, VOICECHAT_KILLGAUNTLET, qfalse);
                }
            } else {
                if (!(who->r.svFlags & SVF_BOT)) {
                    G_Voice(ent, who, SAY_TELL, VOICECHAT_KILLINSULT, qfalse);  // and I killed them with something else
                }
                if (!(ent->r.svFlags & SVF_BOT)) {
                    G_Voice(ent, ent, SAY_TELL, VOICECHAT_KILLINSULT, qfalse);
                }
            }
            ent->client->lastkilled_client = -1;
            return;
        }
    }

    if (g_gametype.integer >= GT_TEAM) {
        // praise a team mate who just got a reward
        for (i = 0; i < MAX_CLIENTS; i++) {
            who = g_entities + i;
            if (who->client && who != ent && who->client->sess.sessionTeam == ent->client->sess.sessionTeam) {
                if (who->client->rewardTime > level.time) {
                    if (!(who->r.svFlags & SVF_BOT)) {
                        G_Voice(ent, who, SAY_TELL, VOICECHAT_PRAISE, qfalse);
                    }
                    if (!(ent->r.svFlags & SVF_BOT)) {
                        G_Voice(ent, ent, SAY_TELL, VOICECHAT_PRAISE, qfalse);
                    }
                    return;
                }
            }
        }
    }

    // just say something
    G_Voice(ent, NULL, SAY_ALL, VOICECHAT_TAUNT, qfalse);
}

static char* gc_orders[] = {
    "hold your position",
    "hold this position",
    "come here",
    "cover me",
    "guard location",
    "search and destroy",
    "report"};

static const int numgc_orders = ARRAY_LEN(gc_orders);

void Cmd_GameCommand_f(gentity_t* ent) {
    int targetNum;
    gentity_t* target;
    int order;
    char arg[MAX_TOKEN_CHARS];

    if (trap_Argc() != 3) {
        trap_SendServerCommand(ent - g_entities, va("print \"Usage: gc <player id> <order 0-%d>\n\"", numgc_orders - 1));
        return;
    }

    trap_Argv(2, arg, sizeof(arg));
    order = atoi(arg);

    if (order < 0 || order >= numgc_orders) {
        trap_SendServerCommand(ent - g_entities, va("print \"Bad order: %i\n\"", order));
        return;
    }

    trap_Argv(1, arg, sizeof(arg));
    targetNum = ClientNumberFromString(ent, arg, qtrue, qtrue);
    if (targetNum == -1) {
        return;
    }

    target = &g_entities[targetNum];
    if (!target->inuse || !target->client) {
        return;
    }

    G_LogPrintf("tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, gc_orders[order]);
    G_Say(ent, target, SAY_TELL, gc_orders[order]);
    // don't tell to the player self if it was already directed to this player
    // also don't send the chat back to a bot
    if (ent != target && !(ent->r.svFlags & SVF_BOT)) {
        G_Say(ent, ent, SAY_TELL, gc_orders[order]);
    }
}

/*
==================
Cmd_Where_f
==================
*/
void Cmd_Where_f(gentity_t* ent) {
    trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", vtos(ent->r.currentOrigin)));
}

// [QL] use shared gametypeDisplayNames[] from bg_misc.c

/*
==================
IsValidCallvoteArg
[QL] Check for command separators/injection in vote arguments
==================
*/
static qboolean IsValidCallvoteArg(const char *s) {
    const char *c;
    for (c = s; *c; c++) {
        if (*c == '\n' || *c == '\r' || *c == ';') {
            return qfalse;
        }
    }
    return qtrue;
}

/*
==================
StartVote
[QL] Begin a vote - set configstrings, announce to all clients
==================
*/
static void StartVote(void) {
    int i;

    level.voteTime = level.time;
    level.voteYes = 0;
    level.voteNo = 0;

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected == CON_DISCONNECTED) {
            continue;
        }
        if (i == level.pendingVoteCaller) {
            level.clients[i].pers.voteState = VOTE_YES;
        } else {
            level.clients[i].pers.voteState = VOTE_PENDING;
        }
        level.clients[i].ps.eFlags &= ~EF_VOTED;
    }
    // caller has voted
    g_entities[level.pendingVoteCaller].client->ps.eFlags |= EF_VOTED;

    trap_SetConfigstring(CS_VOTE_TIME, va("%i", level.voteTime));
    trap_SetConfigstring(CS_VOTE_STRING, level.voteDisplayString);
    trap_SetConfigstring(CS_VOTE_YES, "1");
    trap_SetConfigstring(CS_VOTE_NO, "0");

    trap_SendServerCommand(-1, va("print \"%s called a vote.\n\"",
        g_entities[level.pendingVoteCaller].client->pers.netname));
}

/*
==================
ClearVote
[QL] End a vote - clear configstrings and client vote states
==================
*/
void ClearVote(void) {
    int i;

    level.voteTime = 0;
    level.voteYes = 0;
    level.voteNo = 0;

    for (i = 0; i < level.maxclients; i++) {
        level.clients[i].pers.voteState = VOTE_NONE;
    }

    trap_SetConfigstring(CS_VOTE_TIME, "");
}

/*
==================
Cmd_CallVote_f
[QL] Rewritten to match binary (0x10042580) - 15 vote types,
g_voteFlags, g_voteDelay, g_voteLimit, g_allowSpecVote, g_allowVoteMidGame.
Color-coded help text shows enabled/disabled vote types.
==================
*/
void Cmd_CallVote_f(gentity_t *ent) {
    int clientNum = ent - g_entities;
    char arg1[MAX_STRING_TOKENS];
    char arg2[MAX_STRING_TOKENS];
    int vf = g_voteFlags.integer;

    // --- precondition checks ---

    if (level.time < g_voteDelay.integer) {
        trap_SendServerCommand(clientNum,
            va("print \"Voting will be allowed in %.1f seconds.\n\"",
               (float)(g_voteDelay.integer - level.time) / 1000.0f));
        return;
    }

    if (!g_allowVote.integer) {
        trap_SendServerCommand(clientNum,
            "print \"Public voting is not allowed here.\n\"");
        return;
    }

    if (level.voteTime) {
        trap_SendServerCommand(clientNum,
            "print \"A vote is already in progress.\n\"");
        return;
    }

    if (level.voteExecuteTime && level.time < level.voteExecuteTime) {
        trap_SendServerCommand(clientNum,
            "print \"A vote is being executed.\n\"");
        return;
    }

    if (g_voteLimit.integer &&
        ent->client->pers.voteCount >= g_voteLimit.integer) {
        trap_SendServerCommand(clientNum,
            "print \"You have called the maximum number of votes.\n\"");
        return;
    }

    if (!g_allowSpecVote.integer &&
        ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
        trap_SendServerCommand(clientNum,
            "print \"Not allowed to call a vote as spectator.\n\"");
        return;
    }

    if (!g_allowVoteMidGame.integer && level.warmupTime == 0) {
        trap_SendServerCommand(clientNum,
            "print \"Voting is only allowed during the warm up period.\n\"");
        return;
    }

    // --- parse arguments ---

    trap_Argv(1, arg1, sizeof(arg1));
    trap_Argv(2, arg2, sizeof(arg2));

    if (!IsValidCallvoteArg(arg1) || !IsValidCallvoteArg(arg2)) {
        trap_SendServerCommand(clientNum, "print \"Invalid vote string.\n\"");
        return;
    }

    // --- validate vote command + g_voteFlags checks ---

    if (!Q_stricmp(arg1, "map_restart")) {
        if (vf & VF_MAP_RESTART) {
            trap_SendServerCommand(clientNum,
                "print \"Voting on a map restart is disabled.\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "nextmap")) {
        char nextmap[MAX_STRING_CHARS];
        if (vf & VF_NEXTMAP) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to move to the next map is disabled.\n\"");
            return;
        }
        trap_Cvar_VariableStringBuffer("nextmap", nextmap, sizeof(nextmap));
        if (!nextmap[0] || !Q_stricmpn(nextmap, "map_restart 0", 13)) {
            trap_SendServerCommand(clientNum,
                "print \"No nextmap is currently set.\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "map")) {
        if (vf & VF_MAP) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to change the map being played is disabled.\n\"");
            return;
        }
        if (!arg2[0]) {
            trap_SendServerCommand(clientNum, "print \"Missing map name.\n\"");
            return;
        }
        {
            fileHandle_t f;
            char bspPath[MAX_QPATH];
            Com_sprintf(bspPath, sizeof(bspPath), "maps/%s.bsp", arg2);
            if (trap_FS_FOpenFile(bspPath, &f, FS_READ) < 0) {
                trap_SendServerCommand(clientNum,
                    "print \"Map does not exist.\n\"");
                return;
            }
            trap_FS_FCloseFile(f);
        }
    }
    else if (!Q_stricmp(arg1, "kick") || !Q_stricmp(arg1, "clientkick")) {
        int i;
        if (vf & VF_KICK) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to kick a player is disabled.\n\"");
            return;
        }
        if (!arg2[0]) {
            trap_SendServerCommand(clientNum, "print \"Missing player name/id.\n\"");
            return;
        }
        i = ClientNumberFromString(ent, arg2,
            !Q_stricmp(arg1, "clientkick"), !Q_stricmp(arg1, "kick"));
        if (i == -1) {
            return;
        }
        if (level.clients[i].pers.localClient) {
            trap_SendServerCommand(clientNum,
                "print \"Cannot kick host player.\n\"");
            return;
        }
        Com_sprintf(level.voteString, sizeof(level.voteString),
            "clientkick %d", i);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString),
            "kick %s", level.clients[i].pers.netname);
        goto start_vote;
    }
    else if (!Q_stricmp(arg1, "timelimit")) {
        if (vf & VF_TIMELIMIT) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to change the games time limit is disabled.\n\"");
            return;
        }
        if (!arg2[0]) {
            trap_SendServerCommand(clientNum,
                "print \"Missing desired timelimit.\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "fraglimit")) {
        if (vf & VF_FRAGLIMIT) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to change the games frag limit is disabled.\n\"");
            return;
        }
        if (!arg2[0]) {
            trap_SendServerCommand(clientNum,
                "print \"Missing desired fraglimit.\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "weaprespawn")) {
        if (vf & VF_WEAPRESPAWN) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to change the weapon respawn time is disabled.\n\"");
            return;
        }
        if (!arg2[0]) {
            trap_SendServerCommand(clientNum,
                "print \"Missing desired weapon respawn time.\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "shuffle")) {
        if (vf & VF_SHUFFLE) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to shuffle the teams is disabled.\n\"");
            return;
        }
        if (level.warmupTime == 0) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to shuffle the teams is only permitted during warmup.\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "teamsize")) {
        int ts;
        if (g_gametype.integer == GT_DUEL) {
            trap_SendServerCommand(clientNum,
                "print \"Teamsize is not available in Duel.\n\"");
            return;
        }
        if (vf & VF_TEAMSIZE) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to change team size is disabled.\n\"");
            return;
        }
        if (!arg2[0]) {
            trap_SendServerCommand(clientNum,
                "print \"Missing desired teamsize.\n\"");
            return;
        }
        ts = atoi(arg2);
        if (ts < 0 || ts > 64) {
            trap_SendServerCommand(clientNum,
                va("print \"Invalid team size. (Valid Range: %d - %d)\n\"",
                   g_teamSizeMin.integer, g_maxclients.integer));
            return;
        }
    }
    else if (!Q_stricmp(arg1, "cointoss")) {
        if (vf & VF_COINTOSS) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to coin toss is disabled.\n\"");
            return;
        }
        if (arg2[0] && Q_stricmp(arg2, "heads") && Q_stricmp(arg2, "tails")
            && Q_stricmp(arg2, "h") && Q_stricmp(arg2, "t")) {
            trap_SendServerCommand(clientNum,
                "print \"Valid cointoss parameters are: ^5heads ^5tails\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "random")) {
        int val;
        if (vf & VF_COINTOSS) {
            trap_SendServerCommand(clientNum,
                "print \"Random number generation is disabled.\n\"");
            return;
        }
        if (!arg2[0]) {
            trap_SendServerCommand(clientNum,
                "print \"Missing upper limit.\n\"");
            return;
        }
        val = atoi(arg2);
        if (val < 2 || val > 100) {
            trap_SendServerCommand(clientNum,
                "print \"Invalid upper limit. (Valid Range: 2 - 100)\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "loadouts")) {
        if (vf & VF_LOADOUTS) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to alter loadouts is disabled.\n\"");
            return;
        }
        if (level.warmupTime == 0) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to alter loadouts is only allowed during the warm up period.\n\"");
            return;
        }
        if (Q_stricmp(arg2, "ON") && Q_stricmp(arg2, "OFF")) {
            trap_SendServerCommand(clientNum,
                "print \"^3Valid loadout options are: ^5ON ^5OFF\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "ammo")) {
        if (vf & VF_AMMO) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to alter the ammo system is disabled.\n\"");
            return;
        }
        if (level.warmupTime == 0) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to alter the ammo system is only allowed during the warm up period.\n\"");
            return;
        }
        if (Q_stricmp(arg2, "GLOBAL") && Q_stricmp(arg2, "WEAP")) {
            trap_SendServerCommand(clientNum,
                "print \"^3Valid ammo options are: ^5GLOBAL ^5WEAP\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "timers")) {
        if (vf & VF_TIMERS) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to alter the item timers is disabled.\n\"");
            return;
        }
        if (level.warmupTime == 0) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to alter the item timers is only allowed during the warm up period.\n\"");
            return;
        }
        if (Q_stricmp(arg2, "ON") && Q_stricmp(arg2, "OFF")) {
            trap_SendServerCommand(clientNum,
                "print \"^3Valid item timer options are: ^5ON ^5OFF\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "g_gametype")) {
        int gt;
        if (vf & VF_GAMETYPE) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to change the gametype being played is disabled.\n\"");
            return;
        }
        gt = atoi(arg2);
        if (gt < GT_FFA || gt >= GT_MAX_GAME_TYPE) {
            trap_SendServerCommand(clientNum,
                "print \"Invalid gametype.\n\"");
            return;
        }
        Com_sprintf(level.voteString, sizeof(level.voteString),
            "%s %d", arg1, gt);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString),
            "%s %s", arg1, gametypeDisplayNames[gt]);
        goto start_vote;
    }
    else {
        // [QL] Show color-coded help text: ^5 = enabled, ^1 = disabled
        trap_SendServerCommand(clientNum, "print \"Invalid vote string.\n\"");
        trap_SendServerCommand(clientNum,
            va("print \"%smap           %snextmap        %smap_restart\n\"",
               (vf & VF_MAP) ? "^1" : "^5",
               (vf & VF_NEXTMAP) ? "^1" : "^5",
               (vf & VF_MAP_RESTART) ? "^1" : "^5"));
        trap_SendServerCommand(clientNum,
            va("print \"%skick          %sclientkick\n\"",
               (vf & VF_KICK) ? "^1" : "^5",
               (vf & VF_KICK) ? "^1" : "^5"));
        trap_SendServerCommand(clientNum,
            va("print \"%sshuffle       %steamsize       %scointoss\n\"",
               (vf & VF_SHUFFLE) ? "^1" : "^5",
               (vf & VF_TEAMSIZE) ? "^1" : "^5",
               (vf & VF_COINTOSS) ? "^1" : "^5"));
        trap_SendServerCommand(clientNum,
            va("print \"%stimelimit     %sfraglimit      %sweaprespawn\n\"",
               (vf & VF_TIMELIMIT) ? "^1" : "^5",
               (vf & VF_FRAGLIMIT) ? "^1" : "^5",
               (vf & VF_WEAPRESPAWN) ? "^1" : "^5"));
        trap_SendServerCommand(clientNum,
            va("print \"%sloadouts      %sammo            %stimers\n\"",
               (vf & VF_LOADOUTS) ? "^1" : "^5",
               (vf & VF_AMMO) ? "^1" : "^5",
               (vf & VF_TIMERS) ? "^1" : "^5"));
        trap_SendServerCommand(clientNum,
            "print \"Usage: ^3\\callvote <command> <params>^7\n\"");
        return;
    }

    // --- execute any pending vote first ---

    if (level.voteExecuteTime) {
        level.voteExecuteTime = 0;
        trap_SendConsoleCommand(EXEC_APPEND, va("%s\n", level.voteString));
    }

    // --- build vote string (for commands not handled above) ---

    if (!Q_stricmp(arg1, "map")) {
        Com_sprintf(level.voteString, sizeof(level.voteString),
            "%s %s", arg1, arg2);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString),
            "%s", level.voteString);
    } else if (!Q_stricmp(arg1, "nextmap")) {
        char nextmap[MAX_STRING_CHARS];
        trap_Cvar_VariableStringBuffer("nextmap", nextmap, sizeof(nextmap));
        Com_sprintf(level.voteString, sizeof(level.voteString),
            "%s", nextmap);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString),
            "%s", level.voteString);
    } else if (!Q_stricmp(arg1, "cointoss") || !Q_stricmp(arg1, "random")) {
        Com_sprintf(level.voteString, sizeof(level.voteString),
            "%s %s", arg1, arg2);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString),
            "%s %s", arg1, arg2);
    } else {
        Com_sprintf(level.voteString, sizeof(level.voteString),
            "%s \"%s\"", arg1, arg2);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString),
            "%s", level.voteString);
    }

start_vote:
    level.pendingVoteCaller = clientNum;
    ent->client->pers.voteCount++;
    StartVote();
}

/*
==================
Cmd_IntermissionVote_f
[QL] End-game map voting - players vote 1/2/3 during intermission.
Binary: FUN_100441d0
==================
*/
static void Cmd_IntermissionVote_f(gentity_t *ent) {
    int clientNum = ent - g_entities;
    char msg[64];
    int choice;
    char nextmaps[MAX_STRING_CHARS];

    if (g_voteFlags.integer & VF_ENDMAP_VOTING) {
        return;
    }

    trap_Cvar_VariableStringBuffer("nextmaps", nextmaps, sizeof(nextmaps));
    if (!nextmaps[0]) {
        return;
    }

    trap_Argv(1, msg, sizeof(msg));
    choice = atoi(msg);

    // 20-second voting window
    if (level.time - level.voteTime >= 20000) {
        trap_SendServerCommand(clientNum, "print \"Voting time has expired.\n\"");
        return;
    }

    // 2-second cooldown between votes
    if (level.time - ent->client->pers.lastMapVoteTime < 2000) {
        trap_SendServerCommand(clientNum, "print \"You may only vote once every 2 seconds.\n\"");
        return;
    }

    // Already voted for same map
    if (choice == ent->client->pers.lastMapVoteIndex) {
        trap_SendServerCommand(clientNum, "print \"You already voted for this arena.\n\"");
        return;
    }

    // Validate choice
    if (choice < 1 || choice > 3 || !level.intermissionMapNames[choice - 1][0]) {
        return;
    }

    // Remove previous vote if any
    if (ent->client->pers.lastMapVoteIndex > 0 &&
        ent->client->pers.lastMapVoteIndex <= 3) {
        level.intermissionMapVotes[ent->client->pers.lastMapVoteIndex - 1]--;
    }

    // Record new vote
    ent->client->pers.lastMapVoteTime = level.time;
    ent->client->pers.lastMapVoteIndex = choice;
    level.intermissionMapVotes[choice - 1]++;

    // Broadcast vote
    trap_SendServerCommand(-1,
        va("print \"%s" S_COLOR_WHITE " voted for %s.\n\"",
           ent->client->pers.netname,
           level.intermissionMapTitles[choice - 1][0]
               ? level.intermissionMapTitles[choice - 1]
               : level.intermissionMapNames[choice - 1]));

    // Update map vote counts configstring
    trap_SetConfigstring(CS_ROTATIONVOTES,
        va("%d %d %d",
           level.intermissionMapVotes[0],
           level.intermissionMapVotes[1],
           level.intermissionMapVotes[2]));
}

/*
==================
Cmd_Vote_f
[QL] Updated to use per-client voteState. Redirects to
Cmd_IntermissionVote_f during intermission (binary: 0x10044450).
==================
*/
void Cmd_Vote_f(gentity_t *ent) {
    char msg[64];

    // [QL] During intermission, redirect to map voting
    if (level.intermissionTime) {
        Cmd_IntermissionVote_f(ent);
        return;
    }

    if (!level.voteTime) {
        trap_SendServerCommand(ent - g_entities, "print \"No vote in progress.\n\"");
        return;
    }
    if (ent->client->pers.voteState != VOTE_PENDING) {
        trap_SendServerCommand(ent - g_entities, "print \"Vote already cast.\n\"");
        return;
    }
    if (!g_allowSpecVote.integer &&
        ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
        trap_SendServerCommand(ent - g_entities, "print \"Not allowed to vote as spectator.\n\"");
        return;
    }

    trap_SendServerCommand(ent - g_entities, "print \"Vote cast.\n\"");

    ent->client->ps.eFlags |= EF_VOTED;

    trap_Argv(1, msg, sizeof(msg));

    if (tolower(msg[0]) == 'y' || msg[0] == '1') {
        ent->client->pers.voteState = VOTE_YES;
    } else {
        ent->client->pers.voteState = VOTE_NO;
    }

    // vote counts are recalculated each frame in CheckVote
}

/*
=================
Cmd_SetViewpos_f
=================
*/
void Cmd_SetViewpos_f(gentity_t* ent) {
    vec3_t origin, angles;
    char buffer[MAX_TOKEN_CHARS];
    int i;

    if (!g_cheats.integer) {
        trap_SendServerCommand(ent - g_entities, "print \"Cheats are not enabled on this server.\n\"");
        return;
    }
    if (trap_Argc() != 5) {
        trap_SendServerCommand(ent - g_entities, "print \"usage: setviewpos x y z yaw\n\"");
        return;
    }

    VectorClear(angles);
    for (i = 0; i < 3; i++) {
        trap_Argv(i + 1, buffer, sizeof(buffer));
        origin[i] = atof(buffer);
    }

    trap_Argv(4, buffer, sizeof(buffer));
    angles[YAW] = atof(buffer);

    TeleportPlayer(ent, origin, angles);
}

/*
=================
Cmd_Stats_f
=================
*/
void Cmd_Stats_f(gentity_t* ent) {
    /*
        int max, n, i;

        max = trap_AAS_PointReachabilityAreaIndex( NULL );

        n = 0;
        for ( i = 0; i < max; i++ ) {
            if ( ent->client->areabits[i >> 3] & (1 << (i & 7)) )
                n++;
        }

        //trap_SendServerCommand( ent-g_entities, va("print \"visited %d of %d areas\n\"", n, max));
        trap_SendServerCommand( ent-g_entities, va("print \"%d%% level coverage\n\"", n * 100 / max));
    */
}

/*
=================
[QL] Accuracy command - sends per-weapon accuracy stats to client
Binary: qagamex86.dll 0x1003f6b0
=================
*/
static void Cmd_Acc_f(gentity_t* ent) {
    gclient_t* cl;
    char result[1024];
    char buf[MAX_TOKEN_CHARS];
    int i;

    cl = ent->client;
    if (cl->sess.sessionTeam == TEAM_SPECTATOR &&
        cl->sess.spectatorState == SPECTATOR_FOLLOW) {
        cl = &g_clients[cl->sess.spectatorClient];
    }
    if (!cl) return;

    result[0] = '\0';
    for (i = 0; i < WP_NUM_WEAPONS; i++) {
        if (cl->accuracy_shots == 0 || cl->accuracy_hits == 0) {
            Com_sprintf(buf, sizeof(buf), " ");
        } else {
            Com_sprintf(buf, sizeof(buf), "%d",
                       (cl->accuracy_hits * 100) / cl->accuracy_shots);
        }
        if (strlen(result) + strlen(buf) < sizeof(result)) {
            Q_strcat(result, sizeof(result), buf);
        }
    }
    trap_SendServerCommand(ent - g_entities, va("acc %s", result));
}

/*
=================
[QL] PStats command - sends per-weapon stats to client
Binary: qagamex86.dll 0x1003f840 (identical structure to Cmd_Acc_f)
=================
*/
static void Cmd_PStats_f(gentity_t* ent) {
    gclient_t* cl;
    char result[1024];
    char buf[MAX_TOKEN_CHARS];
    int i;

    cl = ent->client;
    if (cl->sess.sessionTeam == TEAM_SPECTATOR &&
        cl->sess.spectatorState == SPECTATOR_FOLLOW) {
        cl = &g_clients[cl->sess.spectatorClient];
    }
    if (!cl) return;

    result[0] = '\0';
    for (i = 0; i < WP_NUM_WEAPONS; i++) {
        if (cl->accuracy_shots == 0 || cl->accuracy_hits == 0) {
            Com_sprintf(buf, sizeof(buf), " ");
        } else {
            Com_sprintf(buf, sizeof(buf), "%d",
                       (cl->accuracy_hits * 100) / cl->accuracy_shots);
        }
        if (strlen(result) + strlen(buf) < sizeof(result)) {
            Q_strcat(result, sizeof(result), buf);
        }
    }
    trap_SendServerCommand(ent - g_entities, va("pstats %s", result));
}

/*
=================
[QL] ReadyUp command - toggles player ready state during warmup
Binary: qagamex86.dll 0x10045db0
=================
*/
static void Cmd_ReadyUp_f(gentity_t* ent) {
    gclient_t* cl;

    // Only during warmup (warmupTime < 0 means waiting for players)
    if (level.warmupTime >= 0 && level.time >= level.warmupTime) {
        return;
    }

    cl = ent->client;
    if (!cl) return;
    if (cl->sess.sessionTeam == TEAM_SPECTATOR) return;
    if (cl->pers.connected != CON_CONNECTED) return;

    // Toggle ready state
    cl->pers.ready = !cl->pers.ready;
    ClientUserinfoChanged(ent - g_entities);

    // Announce
    if (level.warmupTime < 0) {
        trap_SendServerCommand(-1, va("cp \"%s ^7is ^7%s\n\"",
            cl->pers.netname,
            cl->pers.ready ? "Ready" : "Not Ready"));
    }
}

/*
=================
[QL] Forfeit command - forfeit the current match
Binary: qagamex86.dll 0x10045890
=================
*/
static void Cmd_Forfeit_f(gentity_t* ent) {
    if (!ent->client) return;
    if (ent->client->sess.sessionTeam == TEAM_SPECTATOR) return;

    if (level.timePauseBegin != 0) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Forfeit is not available during a pause or timeout.\n\"");
        return;
    }

    // Only available in round-based gametypes during active play
    switch (g_gametype.integer) {
    case GT_CA:
    case GT_FREEZE:
    case GT_AD:
    case GT_RR:
        if (level.warmupTime > 0 || level.roundState.eNext == 1) {
            trap_SendServerCommand(ent - g_entities,
                "print \"Forfeit is not available during a round countdown.\n\"");
            return;
        }
        break;
    }

    level.matchForfeited = qtrue;
    trap_SendServerCommand(-1, va("cp \"%s ^7has forfeited the match.\n\"",
        ent->client->pers.netname));
}

/*
=================
[QL] Permission-based command handlers
These commands are checked via ClientCommandPermissionCheck
before being dispatched. Privilege levels: 0=anyone, 1=referee, 2=admin
=================
*/

// "?" - List available permission commands with descriptions
static void Cmd_PermHelp_f(gentity_t* ent);  // forward declare

// "players" - List connected players
static void Cmd_Players_f(gentity_t* ent) {
    int i;
    gclient_t* cl;
    char msg[1024];
    char line[128];
    char* teamStr;

    msg[0] = '\0';
    Q_strcat(msg, sizeof(msg), "print \"\n--- Connected Players ---\n\"");
    trap_SendServerCommand(ent - g_entities, msg);

    for (i = 0; i < level.maxclients; i++) {
        cl = &g_clients[i];
        if (cl->pers.connected != CON_CONNECTED) continue;

        switch (cl->sess.sessionTeam) {
        case TEAM_RED: teamStr = "RED"; break;
        case TEAM_BLUE: teamStr = "BLUE"; break;
        case TEAM_SPECTATOR: teamStr = "SPEC"; break;
        default: teamStr = "FREE"; break;
        }

        Com_sprintf(line, sizeof(line), "print \"%2d %-4s %s\n\"",
                   i, teamStr, cl->pers.netname);
        trap_SendServerCommand(ent - g_entities, line);
    }
}

// "timeout" - Call a timeout (uses player's timeout allowance)
static void Cmd_Timeout_f(gentity_t* ent) {
    if (level.timePauseBegin != 0) {
        trap_SendServerCommand(ent - g_entities,
            "print \"A timeout is already in progress.\n\"");
        return;
    }
    if (level.warmupTime != 0) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Cannot call timeout during warmup.\n\"");
        return;
    }
    if (g_timeoutCount.integer > 0 && ent->client->pers.timeouts >= g_timeoutCount.integer) {
        trap_SendServerCommand(ent - g_entities,
            "print \"You have no timeouts remaining.\n\"");
        return;
    }

    ent->client->pers.timeouts++;
    level.timePauseBegin = level.time;
    trap_SendServerCommand(-1, va("cp \"%s ^7called a timeout.\n\"",
        ent->client->pers.netname));
    trap_SetConfigstring(CS_LEVEL_START_TIME,
        va("%i", level.startTime));
}

// "timein" - Cancel a timeout (resume play)
static void Cmd_Timein_f(gentity_t* ent) {
    int pauseDuration;

    if (level.timePauseBegin == 0) {
        trap_SendServerCommand(ent - g_entities,
            "print \"No timeout is in progress.\n\"");
        return;
    }

    pauseDuration = level.time - level.timePauseBegin;
    level.startTime += pauseDuration;
    level.timePauseBegin = 0;

    trap_SetConfigstring(CS_LEVEL_START_TIME,
        va("%i", level.startTime));
    trap_SendServerCommand(-1, "cp \"Timeout ended. Play resumed.\n\"");
}

// "allready" - Force all players to ready state (referee+)
static void Cmd_AllReady_f(gentity_t* ent) {
    int i;

    if (level.warmupTime >= 0) {
        trap_SendServerCommand(ent - g_entities,
            "print \"allready is only available during warmup.\n\"");
        return;
    }

    for (i = 0; i < level.maxclients; i++) {
        if (g_clients[i].pers.connected != CON_CONNECTED) continue;
        if (g_clients[i].sess.sessionTeam == TEAM_SPECTATOR) continue;
        g_clients[i].pers.ready = qtrue;
    }
    trap_SendServerCommand(-1, "cp \"All players have been readied.\n\"");
}

// "pause" - Pause the match (referee+)
static void Cmd_Pause_f(gentity_t* ent) {
    if (level.timePauseBegin != 0) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Match is already paused.\n\"");
        return;
    }
    if (level.warmupTime != 0) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Cannot pause during warmup.\n\"");
        return;
    }

    level.timePauseBegin = level.time;
    trap_SendServerCommand(-1, va("cp \"%s ^7paused the match.\n\"",
        ent->client->pers.netname));
}

// "unpause" - Unpause the match (referee+), same handler as timein
// (uses Cmd_Timein_f)

// "lock" - Lock a team (referee+)
static void Cmd_Lock_f(gentity_t* ent) {
    char arg[MAX_TOKEN_CHARS];
    int team;

    if (trap_Argc() < 2) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Usage: lock <R|B|both>\n\"");
        return;
    }

    trap_Argv(1, arg, sizeof(arg));
    if (Q_stricmp(arg, "r") == 0 || Q_stricmp(arg, "red") == 0) {
        team = TEAM_RED;
    } else if (Q_stricmp(arg, "b") == 0 || Q_stricmp(arg, "blue") == 0) {
        team = TEAM_BLUE;
    } else {
        // Lock both
        trap_Cvar_Set("g_teamRedLocked", "1");
        trap_Cvar_Set("g_teamBlueLocked", "1");
        trap_SendServerCommand(-1, "cp \"Both teams have been locked.\n\"");
        return;
    }

    if (team == TEAM_RED) {
        trap_Cvar_Set("g_teamRedLocked", "1");
        trap_SendServerCommand(-1, "cp \"Red team has been locked.\n\"");
    } else {
        trap_Cvar_Set("g_teamBlueLocked", "1");
        trap_SendServerCommand(-1, "cp \"Blue team has been locked.\n\"");
    }
}

// "unlock" - Unlock a team (referee+)
static void Cmd_Unlock_f(gentity_t* ent) {
    char arg[MAX_TOKEN_CHARS];
    int team;

    if (trap_Argc() < 2) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Usage: unlock <R|B|both>\n\"");
        return;
    }

    trap_Argv(1, arg, sizeof(arg));
    if (Q_stricmp(arg, "r") == 0 || Q_stricmp(arg, "red") == 0) {
        team = TEAM_RED;
    } else if (Q_stricmp(arg, "b") == 0 || Q_stricmp(arg, "blue") == 0) {
        team = TEAM_BLUE;
    } else {
        trap_Cvar_Set("g_teamRedLocked", "0");
        trap_Cvar_Set("g_teamBlueLocked", "0");
        trap_SendServerCommand(-1, "cp \"Both teams have been unlocked.\n\"");
        return;
    }

    if (team == TEAM_RED) {
        trap_Cvar_Set("g_teamRedLocked", "0");
        trap_SendServerCommand(-1, "cp \"Red team has been unlocked.\n\"");
    } else {
        trap_Cvar_Set("g_teamBlueLocked", "0");
        trap_SendServerCommand(-1, "cp \"Blue team has been unlocked.\n\"");
    }
}

// "put" - Move a player to a team (referee+)
static void Cmd_Put_f(gentity_t* ent) {
    char pidStr[MAX_TOKEN_CHARS];
    char teamStr[MAX_TOKEN_CHARS];
    int pid;
    gentity_t* target;

    if (trap_Argc() < 3) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Usage: put <playerid> <R|B|S>\n\"");
        return;
    }

    trap_Argv(1, pidStr, sizeof(pidStr));
    trap_Argv(2, teamStr, sizeof(teamStr));

    pid = atoi(pidStr);
    if (pid < 0 || pid >= level.maxclients) {
        trap_SendServerCommand(ent - g_entities, "print \"Invalid player ID.\n\"");
        return;
    }

    target = &g_entities[pid];
    if (!target->client || target->client->pers.connected != CON_CONNECTED) {
        trap_SendServerCommand(ent - g_entities, "print \"Player not found.\n\"");
        return;
    }

    if (Q_stricmp(teamStr, "r") == 0 || Q_stricmp(teamStr, "red") == 0) {
        SetTeam(target, "red");
    } else if (Q_stricmp(teamStr, "b") == 0 || Q_stricmp(teamStr, "blue") == 0) {
        SetTeam(target, "blue");
    } else if (Q_stricmp(teamStr, "s") == 0 || Q_stricmp(teamStr, "spec") == 0) {
        SetTeam(target, "spectator");
    } else if (Q_stricmp(teamStr, "f") == 0 || Q_stricmp(teamStr, "free") == 0) {
        SetTeam(target, "free");
    } else {
        trap_SendServerCommand(ent - g_entities,
            "print \"Invalid team. Use R, B, S, or F.\n\"");
        return;
    }

    trap_SendServerCommand(-1, va("cp \"%s ^7moved to %s ^7by %s.\n\"",
        target->client->pers.netname, teamStr, ent->client->pers.netname));
}

// "mute" - Mute a player's chat (referee+)
static void Cmd_Mute_f(gentity_t* ent) {
    char pidStr[MAX_TOKEN_CHARS];
    int pid;

    if (trap_Argc() < 2) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Usage: mute <playerid>\n\"");
        return;
    }

    trap_Argv(1, pidStr, sizeof(pidStr));
    pid = atoi(pidStr);
    if (pid < 0 || pid >= level.maxclients ||
        g_clients[pid].pers.connected != CON_CONNECTED) {
        trap_SendServerCommand(ent - g_entities, "print \"Player not found.\n\"");
        return;
    }

    g_clients[pid].sess.muted = 1;
    trap_SendServerCommand(-1, va("print \"%s ^7has been muted.\n\"",
        g_clients[pid].pers.netname));
}

// "unmute" - Unmute a player's chat (referee+)
static void Cmd_Unmute_f(gentity_t* ent) {
    char pidStr[MAX_TOKEN_CHARS];
    int pid;

    if (trap_Argc() < 2) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Usage: unmute <playerid>\n\"");
        return;
    }

    trap_Argv(1, pidStr, sizeof(pidStr));
    pid = atoi(pidStr);
    if (pid < 0 || pid >= level.maxclients ||
        g_clients[pid].pers.connected != CON_CONNECTED) {
        trap_SendServerCommand(ent - g_entities, "print \"Player not found.\n\"");
        return;
    }

    g_clients[pid].sess.muted = 0;
    trap_SendServerCommand(-1, va("print \"%s ^7has been unmuted.\n\"",
        g_clients[pid].pers.netname));
}

// "tempban" / "ban" - Kick and ban a player (referee+)
static void Cmd_Ban_f(gentity_t* ent) {
    char pidStr[MAX_TOKEN_CHARS];
    int pid;
    char cmd[MAX_TOKEN_CHARS];

    if (trap_Argc() < 2) {
        trap_Argv(0, cmd, sizeof(cmd));
        trap_SendServerCommand(ent - g_entities,
            va("print \"Usage: %s <playerid>\n\"", cmd));
        return;
    }

    trap_Argv(1, pidStr, sizeof(pidStr));
    pid = atoi(pidStr);
    if (pid < 0 || pid >= level.maxclients ||
        g_clients[pid].pers.connected != CON_CONNECTED) {
        trap_SendServerCommand(ent - g_entities, "print \"Player not found.\n\"");
        return;
    }

    // Cannot ban admins
    if (g_clients[pid].sess.privileges >= ent->client->sess.privileges) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Cannot ban a player with equal or higher privileges.\n\"");
        return;
    }

    trap_SendServerCommand(-1, va("print \"%s ^7has been banned by %s.\n\"",
        g_clients[pid].pers.netname, ent->client->pers.netname));
    trap_DropClient(pid, "Banned from server");
}

// "unban" - Remove a ban (referee+)
static void Cmd_Unban_f(gentity_t* ent) {
    trap_SendServerCommand(ent - g_entities,
        "print \"Unban not implemented - use removeip from rcon.\n\"");
}

// "listaccess" - List access control entries (referee+)
static void Cmd_ListAccess_f(gentity_t* ent) {
    int i;
    char msg[128];

    trap_SendServerCommand(ent - g_entities,
        "print \"\n--- Access List ---\n\"");
    for (i = 0; i < level.maxclients; i++) {
        if (g_clients[i].pers.connected != CON_CONNECTED) continue;
        if (g_clients[i].sess.privileges == 0) continue;
        Com_sprintf(msg, sizeof(msg), "print \"%2d %-5s %s\n\"",
                   i,
                   g_clients[i].sess.privileges == 2 ? "ADMIN" : "REF",
                   g_clients[i].pers.netname);
        trap_SendServerCommand(ent - g_entities, msg);
    }
}

// "opsay" - Admin chat message (referee+)
static void Cmd_OpSay_f(gentity_t* ent) {
    char msg[MAX_SAY_TEXT];

    if (trap_Argc() < 2) return;

    trap_Argv(1, msg, sizeof(msg));
    // ConcatArgs equivalent for remaining args
    {
        char* p = ConcatArgs(1);
        Q_strncpyz(msg, p, sizeof(msg));
    }

    trap_SendServerCommand(-1, va("chat \"^3[ADMIN] %s: ^7%s\n\"",
        ent->client->pers.netname, msg));
}

// "addadmin" - Grant admin privileges (admin only)
static void Cmd_AddAdmin_f(gentity_t* ent) {
    char pidStr[MAX_TOKEN_CHARS];
    int pid;

    if (trap_Argc() < 2) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Usage: addadmin <playerid>\n\"");
        return;
    }

    trap_Argv(1, pidStr, sizeof(pidStr));
    pid = atoi(pidStr);
    if (pid < 0 || pid >= level.maxclients ||
        g_clients[pid].pers.connected != CON_CONNECTED) {
        trap_SendServerCommand(ent - g_entities, "print \"Player not found.\n\"");
        return;
    }

    g_clients[pid].sess.privileges = 2;
    trap_SendServerCommand(-1, va("print \"%s ^7has been given admin privileges.\n\"",
        g_clients[pid].pers.netname));
}

// "addmod" - Grant moderator/referee privileges (admin only)
static void Cmd_AddMod_f(gentity_t* ent) {
    char pidStr[MAX_TOKEN_CHARS];
    int pid;

    if (trap_Argc() < 2) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Usage: addmod <playerid>\n\"");
        return;
    }

    trap_Argv(1, pidStr, sizeof(pidStr));
    pid = atoi(pidStr);
    if (pid < 0 || pid >= level.maxclients ||
        g_clients[pid].pers.connected != CON_CONNECTED) {
        trap_SendServerCommand(ent - g_entities, "print \"Player not found.\n\"");
        return;
    }

    g_clients[pid].sess.privileges = 1;
    trap_SendServerCommand(-1, va("print \"%s ^7has been given referee privileges.\n\"",
        g_clients[pid].pers.netname));
}

// "demote" - Remove privileges (admin only)
static void Cmd_Demote_f(gentity_t* ent) {
    char pidStr[MAX_TOKEN_CHARS];
    int pid;

    if (trap_Argc() < 2) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Usage: demote <playerid>\n\"");
        return;
    }

    trap_Argv(1, pidStr, sizeof(pidStr));
    pid = atoi(pidStr);
    if (pid < 0 || pid >= level.maxclients ||
        g_clients[pid].pers.connected != CON_CONNECTED) {
        trap_SendServerCommand(ent - g_entities, "print \"Player not found.\n\"");
        return;
    }

    g_clients[pid].sess.privileges = 0;
    trap_SendServerCommand(-1, va("print \"%s ^7has been demoted.\n\"",
        g_clients[pid].pers.netname));
}

// "abort" - Abort match, return to warmup (referee+)
static void Cmd_Abort_f(gentity_t* ent) {
    if (level.warmupTime != 0) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Cannot abort - not in a match.\n\"");
        return;
    }

    trap_SendServerCommand(-1, va("cp \"%s ^7has aborted the match.\n\"",
        ent->client->pers.netname));
    trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
}

// "addscore" - Add to a player's score (referee+)
static void Cmd_AddScore_f(gentity_t* ent) {
    char pidStr[MAX_TOKEN_CHARS];
    char scoreStr[MAX_TOKEN_CHARS];
    int pid, score;

    if (trap_Argc() < 3) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Usage: addscore <playerid> <score>\n\"");
        return;
    }

    trap_Argv(1, pidStr, sizeof(pidStr));
    trap_Argv(2, scoreStr, sizeof(scoreStr));
    pid = atoi(pidStr);
    score = atoi(scoreStr);

    if (pid < 0 || pid >= level.maxclients ||
        g_clients[pid].pers.connected != CON_CONNECTED) {
        trap_SendServerCommand(ent - g_entities, "print \"Player not found.\n\"");
        return;
    }

    g_clients[pid].ps.persistant[PERS_SCORE] += score;
    CalculateRanks();
}

// "addteamscore" - Add to a team's score (referee+)
static void Cmd_AddTeamScore_f(gentity_t* ent) {
    char teamStr[MAX_TOKEN_CHARS];
    char scoreStr[MAX_TOKEN_CHARS];
    int score;

    if (trap_Argc() < 3) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Usage: addteamscore <R|B> <score>\n\"");
        return;
    }

    trap_Argv(1, teamStr, sizeof(teamStr));
    trap_Argv(2, scoreStr, sizeof(scoreStr));
    score = atoi(scoreStr);

    if (Q_stricmp(teamStr, "r") == 0 || Q_stricmp(teamStr, "red") == 0) {
        level.teamScores[TEAM_RED] += score;
    } else if (Q_stricmp(teamStr, "b") == 0 || Q_stricmp(teamStr, "blue") == 0) {
        level.teamScores[TEAM_BLUE] += score;
    } else {
        trap_SendServerCommand(ent - g_entities,
            "print \"Invalid team. Use R or B.\n\"");
        return;
    }
    CalculateRanks();
}

// "setmatchtime" - Set match time in seconds (referee+)
static void Cmd_SetMatchTime_f(gentity_t* ent) {
    char timeStr[MAX_TOKEN_CHARS];
    int seconds;

    if (trap_Argc() < 2) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Usage: setmatchtime <seconds>\n\"");
        return;
    }

    trap_Argv(1, timeStr, sizeof(timeStr));
    seconds = atoi(timeStr);

    level.startTime = level.time - (seconds * 1000);
    trap_SetConfigstring(CS_LEVEL_START_TIME, va("%i", level.startTime));
    trap_SendServerCommand(-1, va("cp \"Match time set to %d seconds.\n\"", seconds));
}

/*
=================
[QL] Permission command table - checked by ClientCommandPermissionCheck
before the main ClientCommand dispatch.
Level: 0=anyone, 1=referee, 2=admin, 3=console-only
=================
*/
typedef struct {
    const char* cmd;
    int requiredLevel;
    void (*handler)(gentity_t* ent);
    const char* description;
} permissionCmd_t;

static permissionCmd_t permissionCmds[] = {
    { "?",            0, NULL,              "show available commands" },
    { "players",      0, Cmd_Players_f,     "show connected players" },
    { "timeout",      0, Cmd_Timeout_f,     "call a time out" },
    { "timein",       0, Cmd_Timein_f,      "call a time in" },
    { "allready",     1, Cmd_AllReady_f,    "force all players to ready" },
    { "pause",        1, Cmd_Pause_f,       "pause the current match" },
    { "unpause",      1, Cmd_Timein_f,      "unpause the current match" },
    { "lock",         1, Cmd_Lock_f,        "stop players from joining a team" },
    { "unlock",       1, Cmd_Unlock_f,      "allows players to join a team" },
    { "put",          1, Cmd_Put_f,         "move a player to a team" },
    { "mute",         1, Cmd_Mute_f,        "mute a player" },
    { "unmute",       1, Cmd_Unmute_f,      "restore a muted player's chat" },
    { "tempban",      1, Cmd_Ban_f,         "temporarily kick a player" },
    { "ban",          1, Cmd_Ban_f,          "permanently ban a player" },
    { "listaccess",   1, Cmd_ListAccess_f,  "list access control entries" },
    { "unban",        1, Cmd_Unban_f,       "remove a ban" },
    { "opsay",        1, Cmd_OpSay_f,       "send an admin message" },
    { "addadmin",     2, Cmd_AddAdmin_f,    "give a player admin privileges" },
    { "addmod",       2, Cmd_AddMod_f,      "give a player moderator privileges" },
    { "demote",       2, Cmd_Demote_f,      "remove a player's privileges" },
    { "abort",        1, Cmd_Abort_f,       "abandon the game and go to warmup" },
    { "addscore",     1, Cmd_AddScore_f,    "add points to a player's score" },
    { "addteamscore", 1, Cmd_AddTeamScore_f, "add points to a team's score" },
    { "setmatchtime", 1, Cmd_SetMatchTime_f, "set match time in seconds" },
    { "rcon",         2, NULL,              "run command on server" },
    { NULL,           0, NULL,              NULL }  // terminator
};

// "?" handler - print the permission command list
static void Cmd_PermHelp_f(gentity_t* ent) {
    int i;
    int privLevel;
    char msg[128];

    privLevel = ent->client->sess.privileges;
    if (ent->client->pers.localClient) privLevel = 2;

    trap_SendServerCommand(ent - g_entities,
        "print \"\n^3--- Available Commands ---\n\"");

    for (i = 0; permissionCmds[i].cmd; i++) {
        if (permissionCmds[i].requiredLevel > privLevel) continue;
        Com_sprintf(msg, sizeof(msg), "print \"  ^5%-14s ^7%s\n\"",
                   permissionCmds[i].cmd,
                   permissionCmds[i].description ? permissionCmds[i].description : "");
        trap_SendServerCommand(ent - g_entities, msg);
    }
}

/*
=================
ClientCommandPermissionCheck

Check if the command matches a permission-table entry.
Returns qtrue if the command was handled (either executed or denied).
Returns qfalse if the command is not in the permission table.
=================
*/
static qboolean ClientCommandPermissionCheck(gentity_t* ent, const char* cmd) {
    int i;
    int privLevel;

    for (i = 0; permissionCmds[i].cmd; i++) {
        if (Q_stricmp(cmd, permissionCmds[i].cmd) == 0) {
            break;
        }
    }

    if (!permissionCmds[i].cmd) {
        return qfalse;  // not a permission command
    }

    if (permissionCmds[i].requiredLevel == 3) {
        return qfalse;  // console-only, not handled here
    }

    // Check privileges
    privLevel = ent->client->sess.privileges;
    if (ent->client->pers.localClient) privLevel = 2;  // localhost = admin

    if (privLevel < permissionCmds[i].requiredLevel) {
        trap_SendServerCommand(ent - g_entities,
            "print \"You do not have the privileges required to use this command\n\"");
        return qtrue;
    }

    // Handle "?" specially
    if (Q_stricmp(cmd, "?") == 0) {
        Cmd_PermHelp_f(ent);
        return qtrue;
    }

    if (permissionCmds[i].handler) {
        permissionCmds[i].handler(ent);
    }
    return qtrue;
}

/*
=================
Cmd_DropFlag_f

[QL] Drop the flag (CTF/1Flag/Harvester). Controlled by g_dropFlag bit 0.
Binary: qagamex86.dll 0x10044af0
=================
*/
static void Cmd_DropFlag_f(gentity_t* ent) {
    gitem_t* item;
    gentity_t* drop;
    vec3_t velocity;
    vec3_t angles;
    int pw;

    if (!(g_dropFlag.integer & 1)) {
        trap_SendServerCommand(ent - g_entities, "print \"Flag dropping is disabled.\n\"");
        return;
    }

    if (g_gametype.integer != GT_CTF && g_gametype.integer != GT_1FCTF &&
        g_gametype.integer != GT_HARVESTER) {
        trap_SendServerCommand(ent - g_entities, "print \"Can only drop flags in CTF/1Flag/Harvester.\n\"");
        return;
    }

    if (ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
        return;
    }

    // Find which flag the player is carrying
    if (ent->client->ps.powerups[PW_REDFLAG]) {
        pw = PW_REDFLAG;
    } else if (ent->client->ps.powerups[PW_BLUEFLAG]) {
        pw = PW_BLUEFLAG;
    } else if (ent->client->ps.powerups[PW_NEUTRALFLAG]) {
        pw = PW_NEUTRALFLAG;
    } else {
        trap_SendServerCommand(ent - g_entities, "print \"You are not carrying a flag.\n\"");
        return;
    }

    item = BG_FindItemForPowerup(pw);
    if (!item) {
        return;
    }

    // Calculate drop velocity from view angles
    VectorCopy(ent->client->ps.viewangles, angles);
    AngleVectors(angles, velocity, NULL, NULL);
    VectorScale(velocity, 400, velocity);
    velocity[2] += 200;

    drop = LaunchItem(item, ent->s.pos.trBase, velocity);
    if (drop) {
        drop->think = Team_DroppedFlagThink;
        drop->nextthink = level.time + 30000;  // 30 second timeout
    }

    ent->client->ps.powerups[pw] = 0;
}

/*
=================
Cmd_DropPowerup_f

[QL] Drop a powerup (team gametypes except CA/FT/Harvester/AD).
Controlled by g_dropFlag bit 1. Dropped powerup has 10s pickup window.
Binary: qagamex86.dll 0x10044cc0
=================
*/
static void Cmd_DropPowerup_f(gentity_t* ent) {
    gitem_t* item;
    gentity_t* drop;
    vec3_t velocity;
    vec3_t angles;
    int pw;
    int remaining;

    if (!(g_dropFlag.integer & 2)) {
        trap_SendServerCommand(ent - g_entities, "print \"Powerup dropping is disabled.\n\"");
        return;
    }

    // Team gametypes only, excluding CA/FT/Harvester/AD
    if (g_gametype.integer < GT_TEAM ||
        g_gametype.integer == GT_CA ||
        g_gametype.integer == GT_FREEZE ||
        g_gametype.integer == GT_HARVESTER ||
        g_gametype.integer == GT_AD) {
        trap_SendServerCommand(ent - g_entities, "print \"Can only drop powerups in team gametypes.\n\"");
        return;
    }

    if (ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
        return;
    }

    // Find a powerup the player has (skip flags at PW_REDFLAG/BLUEFLAG/NEUTRALFLAG = 1-3)
    pw = -1;
    for (int i = PW_QUAD; i < PW_NUM_POWERUPS; i++) {
        if (i == PW_FREEZE) continue;
        if (ent->client->ps.powerups[i] > level.time) {
            pw = i;
            break;
        }
    }

    if (pw < 0) {
        trap_SendServerCommand(ent - g_entities, "print \"You have no powerup to drop.\n\"");
        return;
    }

    item = BG_FindItemForPowerup(pw);
    if (!item) {
        return;
    }

    // Calculate drop velocity
    VectorCopy(ent->client->ps.viewangles, angles);
    AngleVectors(angles, velocity, NULL, NULL);
    VectorScale(velocity, 400, velocity);
    velocity[2] += 200;

    drop = LaunchItem(item, ent->s.pos.trBase, velocity);
    if (drop) {
        // Transfer remaining powerup time
        remaining = ent->client->ps.powerups[pw] - level.time;
        if (remaining < 1000) remaining = 1000;
        drop->count = remaining;           // store remaining time
        drop->nextthink = level.time + 10000;  // 10 second pickup window
        drop->think = G_FreeEntity;
    }

    ent->client->ps.powerups[pw] = 0;
}

/*
=================
Cmd_DropRune_f

[QL] Drop a rune (persistant powerup). Controlled by g_runes cvar.
Not available in Duel. Targets item type IT_HOLDABLE (rune category).
Binary: qagamex86.dll 0x100450f0
=================
*/
static void Cmd_DropRune_f(gentity_t* ent) {
    if (!g_runes.integer) {
        trap_SendServerCommand(ent - g_entities, "print \"Runes are not enabled.\n\"");
        return;
    }

    if (g_gametype.integer == GT_DUEL) {
        trap_SendServerCommand(ent - g_entities, "print \"Cannot drop runes in Duel.\n\"");
        return;
    }

    if (ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
        return;
    }

    if (!ent->client->persistantPowerup) {
        trap_SendServerCommand(ent - g_entities, "print \"You have no rune to drop.\n\"");
        return;
    }

    // Drop the persistant powerup
    Drop_Item(ent, ent->client->persistantPowerup->item, 0);
    ent->client->persistantPowerup = NULL;

    // Clear rune-related powerup stats
    ent->client->ps.stats[STAT_PERSISTANT_POWERUP] = 0;
    ent->client->ps.powerups[PW_SCOUT] = 0;
    ent->client->ps.powerups[PW_GUARD] = 0;
    ent->client->ps.powerups[PW_DOUBLER] = 0;
    ent->client->ps.powerups[PW_AMMOREGEN] = 0;
}

/*
=================
Cmd_DropWeapon_f

[QL] Drop current weapon. Controlled by g_dropFlag bit 2.
Team gametypes only. Player keeps gauntlet as fallback.
Fires EV_DROP_WEAPON event. Binary: qagamex86.dll 0x10045430
=================
*/
static void Cmd_DropWeapon_f(gentity_t* ent) {
    gitem_t* item;
    gentity_t* drop;
    vec3_t velocity;
    vec3_t angles;
    int weapon;
    int ammo;

    if (!(g_dropFlag.integer & 4)) {
        trap_SendServerCommand(ent - g_entities, "print \"Weapon dropping is disabled.\n\"");
        return;
    }

    // Team gametypes only
    if (g_gametype.integer < GT_TEAM) {
        trap_SendServerCommand(ent - g_entities, "print \"Can only drop weapons in team gametypes.\n\"");
        return;
    }

    if (ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
        return;
    }

    weapon = ent->client->ps.weapon;

    // Can't drop gauntlet, grapple, or no weapon
    if (weapon <= WP_GAUNTLET || weapon == WP_GRAPPLING_HOOK || weapon >= WP_NUM_WEAPONS) {
        trap_SendServerCommand(ent - g_entities, "print \"Cannot drop this weapon.\n\"");
        return;
    }

    item = BG_FindItemForWeapon(weapon);
    if (!item) {
        return;
    }

    // Calculate drop velocity
    VectorCopy(ent->client->ps.viewangles, angles);
    AngleVectors(angles, velocity, NULL, NULL);
    VectorScale(velocity, 400, velocity);
    velocity[2] += 200;

    drop = LaunchItem(item, ent->s.pos.trBase, velocity);
    if (drop) {
        // Transfer ammo to the dropped weapon
        ammo = ent->client->ps.ammo[weapon];
        drop->count = ammo;
    }

    // Remove weapon and ammo from player
    ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << weapon);
    ent->client->ps.ammo[weapon] = 0;

    // Fire the drop weapon event
    G_AddEvent(ent, EV_DROP_WEAPON, weapon);

    // Switch to best available weapon
    ent->client->ps.weapon = WP_GAUNTLET;
    for (int i = WP_NUM_WEAPONS - 1; i > WP_GAUNTLET; i--) {
        if (ent->client->ps.stats[STAT_WEAPONS] & (1 << i)) {
            ent->client->ps.weapon = i;
            break;
        }
    }
}

/*
=================
ClientCommand
=================
*/
void ClientCommand(int clientNum) {
    gentity_t* ent;
    char cmd[MAX_TOKEN_CHARS];

    ent = g_entities + clientNum;
    if (!ent->client || ent->client->pers.connected != CON_CONNECTED) {
        if (ent->client && ent->client->pers.localClient) {
            // Handle early team command sent by UI when starting a local
            // team play game.
            trap_Argv(0, cmd, sizeof(cmd));
            if (Q_stricmp(cmd, "team") == 0) {
                Cmd_Team_f(ent);
            }
        }
        return;  // not fully in game yet
    }

    trap_Argv(0, cmd, sizeof(cmd));

    // [QL] Check permission-table commands first
    if (ClientCommandPermissionCheck(ent, cmd)) {
        return;
    }

    if (Q_stricmp(cmd, "say") == 0) {
        Cmd_Say_f(ent, SAY_ALL, qfalse);
        return;
    }
    if (Q_stricmp(cmd, "say_team") == 0) {
        Cmd_Say_f(ent, SAY_TEAM, qfalse);
        return;
    }
    if (Q_stricmp(cmd, "tell") == 0) {
        Cmd_Tell_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "vsay") == 0) {
        Cmd_Voice_f(ent, SAY_ALL, qfalse, qfalse);
        return;
    }
    if (Q_stricmp(cmd, "vsay_team") == 0) {
        Cmd_Voice_f(ent, SAY_TEAM, qfalse, qfalse);
        return;
    }
    if (Q_stricmp(cmd, "vtell") == 0) {
        Cmd_VoiceTell_f(ent, qfalse);
        return;
    }
    if (Q_stricmp(cmd, "vosay") == 0) {
        Cmd_Voice_f(ent, SAY_ALL, qfalse, qtrue);
        return;
    }
    if (Q_stricmp(cmd, "vosay_team") == 0) {
        Cmd_Voice_f(ent, SAY_TEAM, qfalse, qtrue);
        return;
    }
    if (Q_stricmp(cmd, "votell") == 0) {
        Cmd_VoiceTell_f(ent, qtrue);
        return;
    }
    if (Q_stricmp(cmd, "vtaunt") == 0) {
        Cmd_VoiceTaunt_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "score") == 0) {
        Cmd_Score_f(ent);
        return;
    }
    // [QL] acc = score + accuracy (works during intermission)
    if (Q_stricmp(cmd, "acc") == 0) {
        Cmd_Score_f(ent);
        Cmd_Acc_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "pstats") == 0) {
        Cmd_PStats_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "readyup") == 0) {
        Cmd_ReadyUp_f(ent);
        return;
    }

    // ignore all other commands when at intermission
    if (level.intermissionTime) {
        Cmd_Say_f(ent, qfalse, qtrue);
        return;
    }

    if (Q_stricmp(cmd, "give") == 0)
        Cmd_Give_f(ent);
    else if (Q_stricmp(cmd, "god") == 0)
        Cmd_God_f(ent);
    else if (Q_stricmp(cmd, "notarget") == 0)
        Cmd_Notarget_f(ent);
    else if (Q_stricmp(cmd, "noclip") == 0)
        Cmd_Noclip_f(ent);
    else if (Q_stricmp(cmd, "kill") == 0)
        Cmd_Kill_f(ent);
    else if (Q_stricmp(cmd, "teamtask") == 0)
        Cmd_TeamTask_f(ent);
    else if (Q_stricmp(cmd, "levelshot") == 0)
        Cmd_LevelShot_f(ent);
    else if (Q_stricmp(cmd, "follow") == 0)
        Cmd_Follow_f(ent);
    else if (Q_stricmp(cmd, "follownext") == 0)
        Cmd_FollowCycle_f(ent, 1);
    else if (Q_stricmp(cmd, "followprev") == 0)
        Cmd_FollowCycle_f(ent, -1);
    else if (Q_stricmp(cmd, "team") == 0)
        Cmd_Team_f(ent);
    else if (Q_stricmp(cmd, "where") == 0)
        Cmd_Where_f(ent);
    else if (Q_stricmp(cmd, "callvote") == 0 || Q_stricmp(cmd, "cv") == 0)
        Cmd_CallVote_f(ent);
    else if (Q_stricmp(cmd, "vote") == 0)
        Cmd_Vote_f(ent);
    else if (Q_stricmp(cmd, "gc") == 0)
        Cmd_GameCommand_f(ent);
    else if (Q_stricmp(cmd, "setviewpos") == 0)
        Cmd_SetViewpos_f(ent);
    else if (Q_stricmp(cmd, "stats") == 0)
        Cmd_Stats_f(ent);
    // [QL] drop commands
    else if (Q_stricmp(cmd, "dropflag") == 0)
        Cmd_DropFlag_f(ent);
    else if (Q_stricmp(cmd, "droppowerup") == 0)
        Cmd_DropPowerup_f(ent);
    else if (Q_stricmp(cmd, "droprune") == 0)
        Cmd_DropRune_f(ent);
    else if (Q_stricmp(cmd, "dropweapon") == 0)
        Cmd_DropWeapon_f(ent);
    // [QL] additional commands
    else if (Q_stricmp(cmd, "forfeit") == 0)
        Cmd_Forfeit_f(ent);
    else if (Q_stricmp(cmd, "ragequit") == 0)
        trap_DropClient(clientNum, "ragequit");
    else if (Q_stricmp(cmd, "spec") == 0)
        Cmd_Follow_f(ent);  // [QL] spec = follow with filter args
    else if (Q_stricmp(cmd, "specresp") == 0)
        ;  // [QL] spectator respawn - stub (complex bot/spawn interaction)
    else if (Q_stricmp(cmd, "raceinit") == 0)
        ;  // [QL] race mode init - stub (race gametype only)
    else if (Q_stricmp(cmd, "racepoint") == 0)
        ;  // [QL] race mode checkpoint - stub (race gametype only)
    else
        trap_SendServerCommand(clientNum, va("print \"unknown cmd %s\n\"", cmd));
}
