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

// g_client.c -- client functions that don't happen every frame

static vec3_t playerMins = {-15, -15, -24};
static vec3_t playerMaxs = {15, 15, 32};

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32) initial
potential spawning position for deathmatch games.
The first time a player enters the game, they will be at an 'initial' spot.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_deathmatch(gentity_t* ent) {
    int i;

    G_SpawnInt("nobots", "0", &i);
    if (i) {
        ent->flags |= FL_NO_BOTS;
    }
    G_SpawnInt("nohumans", "0", &i);
    if (i) {
        ent->flags |= FL_NO_HUMANS;
    }
}

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
equivalent to info_player_deathmatch
*/
void SP_info_player_start(gentity_t* ent) {
    ent->classname = "info_player_deathmatch";
    SP_info_player_deathmatch(ent);
}

/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32)
The intermission will be viewed from this point.  Target an info_notnull for the view direction.
*/
void SP_info_player_intermission(gentity_t* ent) {
}

/*
=======================================================================

  SelectSpawnPoint

=======================================================================
*/

/*
================
SpotWouldTelefrag

================
*/
qboolean SpotWouldTelefrag(gentity_t* spot) {
    int i, num;
    int touch[MAX_GENTITIES];
    gentity_t* hit;
    vec3_t mins, maxs;

    VectorAdd(spot->s.origin, playerMins, mins);
    VectorAdd(spot->s.origin, playerMaxs, maxs);
    num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

    for (i = 0; i < num; i++) {
        hit = &g_entities[touch[i]];
        // if ( hit->client && hit->client->ps.stats[STAT_HEALTH] > 0 ) {
        if (hit->client) {
            return qtrue;
        }
    }

    return qfalse;
}

/*
================
SelectNearestDeathmatchSpawnPoint

Find the spot that we DON'T want to use
================
*/
#define MAX_SPAWN_POINTS 128
gentity_t* SelectNearestDeathmatchSpawnPoint(vec3_t from) {
    gentity_t* spot;
    vec3_t delta;
    float dist, nearestDist;
    gentity_t* nearestSpot;

    nearestDist = 999999;
    nearestSpot = NULL;
    spot = NULL;

    while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
        VectorSubtract(spot->s.origin, from, delta);
        dist = VectorLength(delta);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearestSpot = spot;
        }
    }

    return nearestSpot;
}

/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point that doesn't telefrag
================
*/
#define MAX_SPAWN_POINTS 128
gentity_t* SelectRandomDeathmatchSpawnPoint(qboolean isbot) {
    gentity_t* spot;
    int count;
    int selection;
    gentity_t* spots[MAX_SPAWN_POINTS];

    count = 0;
    spot = NULL;

    while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL && count < MAX_SPAWN_POINTS) {
        if (SpotWouldTelefrag(spot))
            continue;

        if (((spot->flags & FL_NO_BOTS) && isbot) ||
            ((spot->flags & FL_NO_HUMANS) && !isbot)) {
            // spot is not for this human/bot player
            continue;
        }

        spots[count] = spot;
        count++;
    }

    if (!count) {  // no spots that won't telefrag
        return G_Find(NULL, FOFS(classname), "info_player_deathmatch");
    }

    selection = rand() % count;
    return spots[selection];
}

/*
===========
SelectRandomFurthestSpawnPoint

Chooses a player start, deathmatch start, etc
============
*/
gentity_t* SelectRandomFurthestSpawnPoint(vec3_t avoidPoint, vec3_t origin, vec3_t angles, qboolean isbot) {
    gentity_t* spot;
    vec3_t delta;
    float dist;
    float list_dist[MAX_SPAWN_POINTS];
    gentity_t* list_spot[MAX_SPAWN_POINTS];
    int numSpots, rnd, i, j;

    numSpots = 0;
    spot = NULL;

    while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
        if (SpotWouldTelefrag(spot))
            continue;

        if (((spot->flags & FL_NO_BOTS) && isbot) ||
            ((spot->flags & FL_NO_HUMANS) && !isbot)) {
            // spot is not for this human/bot player
            continue;
        }

        VectorSubtract(spot->s.origin, avoidPoint, delta);
        dist = VectorLength(delta);

        for (i = 0; i < numSpots; i++) {
            if (dist > list_dist[i]) {
                if (numSpots >= MAX_SPAWN_POINTS)
                    numSpots = MAX_SPAWN_POINTS - 1;

                for (j = numSpots; j > i; j--) {
                    list_dist[j] = list_dist[j - 1];
                    list_spot[j] = list_spot[j - 1];
                }

                list_dist[i] = dist;
                list_spot[i] = spot;

                numSpots++;
                break;
            }
        }

        if (i >= numSpots && numSpots < MAX_SPAWN_POINTS) {
            list_dist[numSpots] = dist;
            list_spot[numSpots] = spot;
            numSpots++;
        }
    }

    if (!numSpots) {
        spot = G_Find(NULL, FOFS(classname), "info_player_deathmatch");

        if (!spot)
            G_Error("Couldn't find a spawn point");

        VectorCopy(spot->s.origin, origin);
        origin[2] += 9;
        VectorCopy(spot->s.angles, angles);
        return spot;
    }

    // select a random spot from the spawn points furthest away
    rnd = random() * (numSpots / 2);

    VectorCopy(list_spot[rnd]->s.origin, origin);
    origin[2] += 9;
    VectorCopy(list_spot[rnd]->s.angles, angles);

    return list_spot[rnd];
}

/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, etc
============
*/
gentity_t* SelectSpawnPoint(vec3_t avoidPoint, vec3_t origin, vec3_t angles, qboolean isbot) {
    return SelectRandomFurthestSpawnPoint(avoidPoint, origin, angles, isbot);

    /*
    gentity_t	*spot;
    gentity_t	*nearestSpot;

    nearestSpot = SelectNearestDeathmatchSpawnPoint( avoidPoint );

    spot = SelectRandomDeathmatchSpawnPoint ( );
    if ( spot == nearestSpot ) {
        // roll again if it would be real close to point of death
        spot = SelectRandomDeathmatchSpawnPoint ( );
        if ( spot == nearestSpot ) {
            // last try
            spot = SelectRandomDeathmatchSpawnPoint ( );
        }
    }

    // find a single player start spot
    if (!spot) {
        G_Error( "Couldn't find a spawn point" );
    }

    VectorCopy (spot->s.origin, origin);
    origin[2] += 9;
    VectorCopy (spot->s.angles, angles);

    return spot;
    */
}

/*
===========
SelectInitialSpawnPoint

Try to find a spawn point marked 'initial', otherwise
use normal spawn selection.
============
*/
gentity_t* SelectInitialSpawnPoint(vec3_t origin, vec3_t angles, qboolean isbot) {
    gentity_t* spot;

    spot = NULL;

    while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
        if (((spot->flags & FL_NO_BOTS) && isbot) ||
            ((spot->flags & FL_NO_HUMANS) && !isbot)) {
            continue;
        }

        if ((spot->spawnflags & 0x01))
            break;
    }

    if (!spot || SpotWouldTelefrag(spot))
        return SelectSpawnPoint(vec3_origin, origin, angles, isbot);

    VectorCopy(spot->s.origin, origin);
    origin[2] += 9;
    VectorCopy(spot->s.angles, angles);

    return spot;
}

/*
===========
SelectSpectatorSpawnPoint

============
*/
gentity_t* SelectSpectatorSpawnPoint(vec3_t origin, vec3_t angles) {
    FindIntermissionPoint();

    VectorCopy(level.intermission_origin, origin);
    VectorCopy(level.intermission_angle, angles);

    return NULL;
}

/*
=======================================================================

BODYQUE

=======================================================================
*/

/*
===============
InitBodyQue
===============
*/
void InitBodyQue(void) {
    int i;
    gentity_t* ent;

    level.bodyQueIndex = 0;
    for (i = 0; i < BODY_QUEUE_SIZE; i++) {
        ent = G_Spawn();
        ent->classname = "bodyque";
        ent->neverFree = qtrue;
        level.bodyQue[i] = ent;
    }
}

/*
=============
BodySink

After sitting around for five seconds, fall into the ground and disappear
=============
*/
void BodySink(gentity_t* ent) {
    if (level.time - ent->timestamp > 6500) {
        // the body ques are never actually freed, they are just unlinked
        trap_UnlinkEntity(ent);
        ent->physicsObject = qfalse;
        return;
    }
    ent->nextthink = level.time + 100;
    ent->s.pos.trBase[2] -= 1;
}

/*
=============
CopyToBodyQue

A player is respawning, so make an entity that looks
just like the existing corpse to leave behind.
=============
*/
void CopyToBodyQue(gentity_t* ent) {
    gentity_t* e;
    int i;
    gentity_t* body;
    int contents;

    trap_UnlinkEntity(ent);

    // if client is in a nodrop area, don't leave the body
    contents = trap_PointContents(ent->s.origin, -1);
    if (contents & CONTENTS_NODROP) {
        return;
    }

    // grab a body que and cycle to the next one
    body = level.bodyQue[level.bodyQueIndex];
    level.bodyQueIndex = (level.bodyQueIndex + 1) % BODY_QUEUE_SIZE;

    body->s = ent->s;
    body->s.eFlags = EF_DEAD;  // clear EF_TALK, etc

    if (ent->s.eFlags & EF_KAMIKAZE) {
        body->s.eFlags |= EF_KAMIKAZE;

        // check if there is a kamikaze timer around for this owner
        for (i = 0; i < level.num_entities; i++) {
            e = &g_entities[i];
            if (!e->inuse)
                continue;
            if (e->activator != ent)
                continue;
            if (strcmp(e->classname, "kamikaze timer"))
                continue;
            e->activator = body;
            break;
        }
    }

    body->s.powerups = 0;   // clear powerups
    body->s.loopSound = 0;  // clear lava burning
    body->s.number = body - g_entities;
    body->timestamp = level.time;
    body->physicsObject = qtrue;
    body->physicsBounce = 0;  // don't bounce
    if (body->s.groundEntityNum == ENTITYNUM_NONE) {
        body->s.pos.trType = TR_GRAVITY;
        body->s.pos.trTime = level.time;
        VectorCopy(ent->client->ps.velocity, body->s.pos.trDelta);
    } else {
        body->s.pos.trType = TR_STATIONARY;
    }
    body->s.event = 0;

    // change the animation to the last-frame only, so the sequence
    // doesn't repeat anew for the body
    switch (body->s.legsAnim & ~ANIM_TOGGLEBIT) {
        case BOTH_DEATH1:
        case BOTH_DEAD1:
            body->s.torsoAnim = body->s.legsAnim = BOTH_DEAD1;
            break;
        case BOTH_DEATH2:
        case BOTH_DEAD2:
            body->s.torsoAnim = body->s.legsAnim = BOTH_DEAD2;
            break;
        case BOTH_DEATH3:
        case BOTH_DEAD3:
        default:
            body->s.torsoAnim = body->s.legsAnim = BOTH_DEAD3;
            break;
    }

    body->r.svFlags = ent->r.svFlags;
    VectorCopy(ent->r.mins, body->r.mins);
    VectorCopy(ent->r.maxs, body->r.maxs);
    VectorCopy(ent->r.absmin, body->r.absmin);
    VectorCopy(ent->r.absmax, body->r.absmax);

    body->clipmask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP;
    body->r.contents = CONTENTS_CORPSE;
    body->r.ownerNum = ent->s.number;

    body->nextthink = level.time + 5000;
    body->think = BodySink;

    body->die = body_die;

    // don't take more damage if already gibbed
    if (ent->health <= GIB_HEALTH) {
        body->takedamage = qfalse;
    } else {
        body->takedamage = qtrue;
    }

    VectorCopy(body->s.pos.trBase, body->r.currentOrigin);
    trap_LinkEntity(body);
}

//======================================================================

/*
==================
SetClientViewAngle

==================
*/
void SetClientViewAngle(gentity_t* ent, vec3_t angle) {
    int i;

    // set the delta angle
    for (i = 0; i < 3; i++) {
        int cmdAngle;

        cmdAngle = ANGLE2SHORT(angle[i]);
        ent->client->ps.delta_angles[i] = cmdAngle - ent->client->pers.cmd.angles[i];
    }
    VectorCopy(angle, ent->s.angles);
    VectorCopy(ent->s.angles, ent->client->ps.viewangles);
}

/*
================
ClientRespawn
================
*/
void ClientRespawn(gentity_t* ent) {
    CopyToBodyQue(ent);
    ClientSpawn(ent);
}

/*
================
TeamCount

Returns number of players on a team
================
*/
int TeamCount(int ignoreClientNum, team_t team) {
    int i;
    int count = 0;

    for (i = 0; i < level.maxclients; i++) {
        if (i == ignoreClientNum) {
            continue;
        }
        if (level.clients[i].pers.connected == CON_DISCONNECTED) {
            continue;
        }
        if (level.clients[i].sess.sessionTeam == team) {
            count++;
        }
    }

    return count;
}

/*
================
TeamLeader

Returns the client number of the team leader
================
*/
int TeamLeader(int team) {
    int i;

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected == CON_DISCONNECTED) {
            continue;
        }
        if (level.clients[i].sess.sessionTeam == team) {
            if (level.clients[i].sess.teamLeader)
                return i;
        }
    }

    return -1;
}

/*
================
PickTeam

================
*/
team_t PickTeam(int ignoreClientNum) {
    int counts[TEAM_NUM_TEAMS];

    counts[TEAM_BLUE] = TeamCount(ignoreClientNum, TEAM_BLUE);
    counts[TEAM_RED] = TeamCount(ignoreClientNum, TEAM_RED);

    if (counts[TEAM_BLUE] > counts[TEAM_RED]) {
        return TEAM_RED;
    }
    if (counts[TEAM_RED] > counts[TEAM_BLUE]) {
        return TEAM_BLUE;
    }
    // equal team count, so join the team with the lowest score
    if (level.teamScores[TEAM_BLUE] > level.teamScores[TEAM_RED]) {
        return TEAM_RED;
    }
    return TEAM_BLUE;
}

/*
===========
ForceClientSkin

Forces a client's skin (for teamplay)
===========
*/
/*
static void ForceClientSkin( gclient_t *client, char *model, const char *skin ) {
    char *p;

    if ((p = strrchr(model, '/')) != 0) {
        *p = 0;
    }

    Q_strcat(model, MAX_QPATH, "/");
    Q_strcat(model, MAX_QPATH, skin);
}
*/

/*
===========
ClientCleanName
============
*/
static void ClientCleanName(const char* in, char* out, int outSize) {
    int outpos = 0, colorlessLen = 0, spaces = 0;

    // discard leading spaces
    for (; *in == ' '; in++)
        ;

    for (; *in && outpos < outSize - 1; in++) {
        out[outpos] = *in;

        if (*in == ' ') {
            // don't allow too many consecutive spaces
            if (spaces > 2)
                continue;

            spaces++;
        } else if (outpos > 0 && out[outpos - 1] == Q_COLOR_ESCAPE) {
            if (Q_IsColorString(&out[outpos - 1])) {
                colorlessLen--;

                if (ColorIndex(*in) == 0) {
                    // Disallow color black in names to prevent players
                    // from getting advantage playing in front of black backgrounds
                    outpos--;
                    continue;
                }
            } else {
                spaces = 0;
                colorlessLen++;
            }
        } else {
            spaces = 0;
            colorlessLen++;
        }

        outpos++;
    }

    out[outpos] = '\0';

    // don't allow empty names
    if (*out == '\0' || colorlessLen == 0)
        Q_strncpyz(out, "UnnamedPlayer", outSize);
}

/*
===========
ClientUserInfoChanged

Called from ClientConnect when the player first connects and
directly by the server system when the player updates a userinfo variable.

The game can override any of the settings and call trap_SetUserinfo
if desired.
============
*/
void ClientUserinfoChanged(int clientNum) {
    gentity_t* ent;
    int teamTask, teamLeader, health;
    char* s;
    char model[MAX_QPATH];
    char headModel[MAX_QPATH];
    char oldname[MAX_STRING_CHARS];
    gclient_t* client;
    char c1[MAX_INFO_STRING];
    char c2[MAX_INFO_STRING];
    char redTeam[MAX_INFO_STRING];
    char blueTeam[MAX_INFO_STRING];
    char userinfo[MAX_INFO_STRING];

    ent = g_entities + clientNum;
    client = ent->client;

    trap_GetUserinfo(clientNum, userinfo, sizeof(userinfo));

    // check for malformed or illegal info strings
    if (!Info_Validate(userinfo)) {
        strcpy(userinfo, "\\name\\badinfo");
        // don't keep those clients and userinfo
        trap_DropClient(clientNum, "Invalid userinfo");
    }

    // check the item prediction
    s = Info_ValueForKey(userinfo, "cg_predictItems");
    if (!atoi(s)) {
        client->pers.predictItemPickup = qfalse;
    } else {
        client->pers.predictItemPickup = qtrue;
    }

    // set name
    Q_strncpyz(oldname, client->pers.netname, sizeof(oldname));
    s = Info_ValueForKey(userinfo, "name");
    ClientCleanName(s, client->pers.netname, sizeof(client->pers.netname));

    if (client->sess.sessionTeam == TEAM_SPECTATOR) {
        if (client->sess.spectatorState == SPECTATOR_SCOREBOARD) {
            Q_strncpyz(client->pers.netname, "scoreboard", sizeof(client->pers.netname));
        }
    }

    if (client->pers.connected == CON_CONNECTED) {
        if (strcmp(oldname, client->pers.netname)) {
            trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " renamed to %s\n\"", oldname,
                                          client->pers.netname));
        }
    }

    // set max health
    if (client->ps.powerups[PW_GUARD]) {
        client->pers.maxHealth = 200;
    } else {
        health = atoi(Info_ValueForKey(userinfo, "handicap"));
        client->pers.maxHealth = health;
        if (client->pers.maxHealth < 1 || client->pers.maxHealth > 100) {
            client->pers.maxHealth = 100;
        }
    }

    client->ps.stats[STAT_MAX_HEALTH] = client->pers.maxHealth;

    // [QL] set model - QL uses "model" for all gametypes (no team_model)
    Q_strncpyz(model, Info_ValueForKey(userinfo, "model"), sizeof(model));
    Q_strncpyz(headModel, Info_ValueForKey(userinfo, "headmodel"), sizeof(headModel));

    /*	NOTE: all client side now

        // team
        switch( team ) {
        case TEAM_RED:
            ForceClientSkin(client, model, "red");
    //		ForceClientSkin(client, headModel, "red");
            break;
        case TEAM_BLUE:
            ForceClientSkin(client, model, "blue");
    //		ForceClientSkin(client, headModel, "blue");
            break;
        }
        // don't ever use a default skin in teamplay, it would just waste memory
        // however bots will always join a team but they spawn in as spectator
        if ( g_gametype.integer >= GT_TEAM && team == TEAM_SPECTATOR) {
            ForceClientSkin(client, model, "red");
    //		ForceClientSkin(client, headModel, "red");
        }
    */

    if (g_gametype.integer >= GT_TEAM && !(ent->r.svFlags & SVF_BOT)) {
        client->pers.teamInfo = qtrue;
    } else {
        s = Info_ValueForKey(userinfo, "teamoverlay");
        if (!*s || atoi(s) != 0) {
            client->pers.teamInfo = qtrue;
        } else {
            client->pers.teamInfo = qfalse;
        }
    }

    /*
    s = Info_ValueForKey( userinfo, "cg_pmove_fixed" );
    if ( !*s || atoi( s ) == 0 ) {
        client->pers.pmoveFixed = qfalse;
    }
    else {
        client->pers.pmoveFixed = qtrue;
    }
    */

    // team task (0 = none, 1 = offence, 2 = defence)
    teamTask = atoi(Info_ValueForKey(userinfo, "teamtask"));
    // team Leader (1 = leader, 0 is normal player)
    teamLeader = client->sess.teamLeader;

    // colors
    Q_strncpyz(c1, Info_ValueForKey(userinfo, "color1"), sizeof(c1));
    Q_strncpyz(c2, Info_ValueForKey(userinfo, "color2"), sizeof(c2));

    Q_strncpyz(redTeam, Info_ValueForKey(userinfo, "g_redteam"), sizeof(redTeam));
    Q_strncpyz(blueTeam, Info_ValueForKey(userinfo, "g_blueteam"), sizeof(blueTeam));

    // send over a subset of the userinfo keys so other clients can
    // print scoreboards, display models, and play custom sounds
    if (ent->r.svFlags & SVF_BOT) {
        s = va("n\\%s\\t\\%i\\model\\%s\\hmodel\\%s\\c1\\%s\\c2\\%s\\hc\\%i\\w\\%i\\l\\%i\\skill\\%s\\tt\\%d\\tl\\%d",
               client->pers.netname, client->sess.sessionTeam, model, headModel, c1, c2,
               client->pers.maxHealth, client->sess.wins, client->sess.losses,
               Info_ValueForKey(userinfo, "skill"), teamTask, teamLeader);
    } else {
        s = va("n\\%s\\t\\%i\\model\\%s\\hmodel\\%s\\g_redteam\\%s\\g_blueteam\\%s\\c1\\%s\\c2\\%s\\hc\\%i\\w\\%i\\l\\%i\\tt\\%d\\tl\\%d",
               client->pers.netname, client->sess.sessionTeam, model, headModel, redTeam, blueTeam, c1, c2,
               client->pers.maxHealth, client->sess.wins, client->sess.losses, teamTask, teamLeader);
    }

    trap_SetConfigstring(CS_PLAYERS + clientNum, s);

    // this is not the userinfo, more like the configstring actually
    G_LogPrintf("ClientUserinfoChanged: %i %s\n", clientNum, s);
}

/*
===========
ClientConnect

Called when a player begins connecting to the server.
Called again for every map change or tournement restart.

The session information will be valid after exit.

Return NULL if the client should be allowed, otherwise return
a string with the reason for denial.

Otherwise, the client will be sent the current gamestate
and will eventually get to ClientBegin.

firstTime will be qtrue the very first time a client connects
to the server machine, but qfalse on map changes and tournement
restarts.
============
*/
char* ClientConnect(int clientNum, qboolean firstTime, qboolean isBot) {
    char* value;
    gclient_t* client;
    char userinfo[MAX_INFO_STRING];
    gentity_t* ent;

    ent = &g_entities[clientNum];

    trap_GetUserinfo(clientNum, userinfo, sizeof(userinfo));

    // [QL] Access level check (replaces IP banning and password)
    value = Info_ValueForKey(userinfo, "ip");
    if (!isBot) {
        int accessLevel = G_GetAccessLevel(value);
        if (accessLevel == -1) {
            return "You are banned from this server.";
        }
        // check for localhost
        if (strcmp(value, "localhost") != 0) {
            // store privilege level
            // (will be set after client struct is zeroed)
        }
    }

    // if a player reconnects quickly after a disconnect, the client disconnect may never be called
    if (ent->inuse) {
        G_LogPrintf("Forcing disconnect on active client: %i\n", clientNum);
        ClientDisconnect(clientNum);
    }

    // they can connect
    ent->client = level.clients + clientNum;
    client = ent->client;

    memset(client, 0, sizeof(*client));

    client->pers.connected = CON_CONNECTING;

    // check for local client
    value = Info_ValueForKey(userinfo, "ip");
    if (!strcmp(value, "localhost")) {
        client->pers.localClient = qtrue;
    }

    // [QL] initialize session data - always called on firstTime or newSession
    if (firstTime || level.newSession) {
        G_InitSessionData(client, userinfo);
        // [QL] initialize expanded stats timing
        {
            int r;
            memset(&client->expandedStats, 0, sizeof(client->expandedStats));
            client->expandedStats.teamJoinTime = level.time;
            client->expandedStats.lastThinkTime = level.time - level.startTime;
            r = rand();
            client->expandedStats.statId = level.time + r + 636 + (int)(intptr_t)client;
        }
    }
    G_ReadSessionData(client);

    if (isBot) {
        ent->r.svFlags |= SVF_BOT;
        ent->inuse = qtrue;
        if (!G_BotConnect(clientNum, !firstTime)) {
            return "BotConnectfailed";
        }
    }

    // get and distribute relevant parameters
    G_LogPrintf("ClientConnect: %i\n", clientNum);
    ClientUserinfoChanged(clientNum);

    // [QL] firstTime, non-bot: send privilege level, connected message, init stats
    if (firstTime && !g_isBotOnly.integer) {
        // send privilege level to client
        trap_SendServerCommand(clientNum, va("priv %i", client->sess.privileges));

        // [QL] gate "connected" message behind level.time > 5000
        if (level.time > 5000) {
            trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " connected\n\"", client->pers.netname));
        }

        STAT_InitClient(ent);

        if (g_gametype.integer > GT_RACE &&
            client->sess.sessionTeam != TEAM_SPECTATOR) {
            BroadcastTeamChange(client, -1);
        }

        if (!isBot) {
            STAT_SubscribeClient(ent);
        }
    }

    // count current clients and rank for scoreboard
    CalculateRanks();

    // [QL] Set EF_VOTED for spectators on map change (non-firstTime)
    if (!firstTime && client->sess.sessionTeam == TEAM_SPECTATOR) {
        ent->client->ps.eFlags |= EF_VOTED;
    }

    return NULL;
}

/*
===========
ClientBegin

called when a client has finished connecting, and is ready
to be placed into the level.  This will happen every level load,
and on transition between teams, but doesn't happen on respawns
============
*/
void ClientBegin(int clientNum) {
    gentity_t* ent;
    gclient_t* client;
    int flags;

    ent = g_entities + clientNum;

    client = level.clients + clientNum;

    if (ent->r.linked) {
        trap_UnlinkEntity(ent);
    }
    G_InitGentity(ent);
    ent->touch = 0;
    ent->pain = 0;
    ent->client = client;

    client->pers.connected = CON_CONNECTED;
    client->pers.enterTime = level.time;
    client->pers.teamState.state = TEAM_BEGIN;

    // save eflags around this, because changing teams will
    // cause this to happen with a valid entity, and we
    // want to make sure the teleport bit is set right
    // so the viewpoint doesn't interpolate through the
    // world to the new position
    flags = client->ps.eFlags;
    memset(&client->ps, 0, sizeof(client->ps));
    client->ps.eFlags = flags;

    // [QL] initialize kill tracking fields
    client->lastkilled_client = -1;
    client->lastClientKilled = -1;
    client->lasthurt_client[0] = -1;
    client->lasthurt_client[1] = -1;

    // [QL] set pm_type based on team: spectators get PM_SPECTATOR
    if (client->sess.sessionTeam != TEAM_SPECTATOR) {
        client->ps.pm_type = PM_NORMAL;
    } else {
        client->ps.pm_type = PM_SPECTATOR;
    }

    // [QL] gametype-specific begin dispatch
    switch (g_gametype.integer) {
    case GT_RACE:
        ClientBegin_Race(ent);
        break;
    case GT_CA:
    case GT_AD:
        ClientBegin_RoundBased(ent);
        break;
    case GT_FREEZE:
        ClientBegin_Freeze(ent);
        break;
    case GT_RR:
        ClientBegin_RedRover(ent);
        break;
    default:
        ClientSpawn(ent);
        break;
    }

    // [QL] Duel bots: clear losses, set join time
    if (g_gametype.integer == GT_TOURNAMENT && (ent->r.svFlags & SVF_BOT)) {
        client->sess.losses = 0;
        if (client->sess.joinTime == 0) {
            client->sess.joinTime = (int)time(NULL);
        }
        ClientUserinfoChanged(clientNum);
    }

    G_LogPrintf("ClientBegin: %i\n", clientNum);

    // count current clients and rank for scoreboard
    CalculateRanks();
}

/*
===========
ClientSpawn

Called every time a client is placed fresh in the world:
after the first ClientBegin, and after each respawn
Initializes all non-persistant parts of playerState
============
*/
void ClientSpawn(gentity_t* ent) {
    int index;
    vec3_t spawn_origin, spawn_angles;
    gclient_t* client;
    int i;
    clientPersistant_t saved;
    clientSession_t savedSess;
    int persistant[MAX_PERSISTANT];
    gentity_t* spawnPoint;
    int flags;
    int savedPing;
    //	char	*savedAreaBits;
    int accuracy_hits, accuracy_shots;
    int eventSequence;
    char userinfo[MAX_INFO_STRING];

    index = ent - g_entities;
    client = ent->client;

    VectorClear(spawn_origin);

    // [QL] find a spawn point - unified SelectSpawnPoint for all gametypes
    if (client->sess.sessionTeam == TEAM_SPECTATOR) {
        spawnPoint = SelectSpectatorSpawnPoint(
            spawn_origin, spawn_angles);
    } else {
        spawnPoint = SelectSpawnPoint(
            client->ps.origin,
            spawn_origin, spawn_angles, !!(ent->r.svFlags & SVF_BOT));
        // [QL] if no spawn point found, put player in spectator mode
        if (!spawnPoint) {
            client->respawnTime = level.time + 600;
            client->ps.pm_type = PM_SPECTATOR;
            client->pers.teamState.state = TEAM_ACTIVE;
            return;
        }
    }
    client->pers.teamState.state = TEAM_ACTIVE;

    // always clear the kamikaze flag
    ent->s.eFlags &= ~EF_KAMIKAZE;

    // toggle the teleport bit so the client knows to not lerp
    // and never clear the voted flag
    flags = ent->client->ps.eFlags & (EF_TELEPORT_BIT | EF_VOTED);
    flags ^= EF_TELEPORT_BIT;

    // clear everything but the persistant data
    saved = client->pers;
    savedSess = client->sess;
    savedPing = client->ps.ping;
    //	savedAreaBits = client->areabits;
    accuracy_hits = client->accuracy_hits;
    accuracy_shots = client->accuracy_shots;

    for (i = 0; i < MAX_PERSISTANT; i++) {
        persistant[i] = client->ps.persistant[i];
    }
    eventSequence = client->ps.eventSequence;

    Com_Memset(client, 0, sizeof(*client));

    client->pers = saved;
    client->sess = savedSess;
    client->ps.ping = savedPing;
    //	client->areabits = savedAreaBits;
    client->accuracy_hits = accuracy_hits;
    client->accuracy_shots = accuracy_shots;
    client->lastkilled_client = -1;

    for (i = 0; i < MAX_PERSISTANT; i++) {
        client->ps.persistant[i] = persistant[i];
    }
    client->ps.eventSequence = eventSequence;

    // increment the spawncount so the client will detect the respawn
    client->ps.persistant[PERS_SPAWN_COUNT]++;
    client->ps.persistant[PERS_TEAM] = client->sess.sessionTeam;

    client->airOutTime = level.time + 12000;

    trap_GetUserinfo(index, userinfo, sizeof(userinfo));
    // [QL] set max health: g_startingHealth * handicap factor
    {
        int handicap = atoi(Info_ValueForKey(userinfo, "handicap"));
        float hcFactor = 1.0f;
        int maxHealth;
        if (handicap >= 1 && handicap <= 100) {
            hcFactor = (float)handicap / 100.0f;
        }
        maxHealth = (int)(g_startingHealth.integer * hcFactor);
        if (maxHealth < 1 || maxHealth > g_startingHealth.integer) {
            maxHealth = g_startingHealth.integer;
        }
        client->pers.maxHealth = maxHealth;
    }
    // clear entity values
    client->ps.stats[STAT_MAX_HEALTH] = client->pers.maxHealth;
    client->ps.eFlags = flags;

    ent->s.groundEntityNum = ENTITYNUM_NONE;
    ent->client = &level.clients[index];
    ent->takedamage = qtrue;
    ent->inuse = qtrue;
    ent->classname = "player";
    if (client->sess.sessionTeam == TEAM_SPECTATOR) {
        ent->r.contents = 0;
        ent->clipmask = 0;
    } else {
        ent->r.contents = CONTENTS_BODY;
        ent->clipmask = MASK_PLAYERSOLID;
    }
    ent->die = player_die;
    ent->waterlevel = 0;
    ent->watertype = 0;
    ent->flags = 0;

    VectorCopy(playerMins, ent->r.mins);
    VectorCopy(playerMaxs, ent->r.maxs);

    client->ps.clientNum = index;

    // [QL] Starting weapons from g_startingWeapons bitmask + g_startingAmmo_* cvars
    {
        static vmCvar_t *startingAmmoCvars[WP_NUM_WEAPONS] = {
            NULL,                   // WP_NONE = 0
            &g_startingAmmo_g,      // WP_GAUNTLET = 1
            &g_startingAmmo_mg,     // WP_MACHINEGUN = 2
            &g_startingAmmo_sg,     // WP_SHOTGUN = 3
            &g_startingAmmo_gl,     // WP_GRENADE_LAUNCHER = 4
            &g_startingAmmo_rl,     // WP_ROCKET_LAUNCHER = 5
            &g_startingAmmo_lg,     // WP_LIGHTNING = 6
            &g_startingAmmo_rg,     // WP_RAILGUN = 7
            &g_startingAmmo_pg,     // WP_PLASMAGUN = 8
            &g_startingAmmo_bfg,    // WP_BFG = 9
            &g_startingAmmo_gh,     // WP_GRAPPLING_HOOK = 10
            &g_startingAmmo_ng,     // WP_NAILGUN = 11
            &g_startingAmmo_pl,     // WP_PROX_LAUNCHER = 12
            &g_startingAmmo_cg,     // WP_CHAINGUN = 13
            &g_startingAmmo_hmg,    // WP_HMG = 14
        };
        int w;

        // Binary uses bit (w-1) for weapon w: bit 0=gauntlet, bit 1=MG, etc.
        for (w = WP_GAUNTLET; w < WP_NUM_WEAPONS; w++) {
            if (g_startingWeapons.integer & (1 << (w - 1))) {
                client->ps.stats[STAT_WEAPONS] |= (1 << w);
                client->ps.ammo[w] = startingAmmoCvars[w]->integer;
            }
        }

        // Gauntlet and grapple always have infinite ammo when present
        if (client->ps.stats[STAT_WEAPONS] & (1 << WP_GAUNTLET)) {
            client->ps.ammo[WP_GAUNTLET] = -1;
        }
        if (client->ps.stats[STAT_WEAPONS] & (1 << WP_GRAPPLING_HOOK)) {
            client->ps.ammo[WP_GRAPPLING_HOOK] = -1;
        }

        // g_infiniteAmmo override: set all weapon ammo to -1
        if (g_infiniteAmmo.integer) {
            for (w = WP_GAUNTLET; w < WP_NUM_WEAPONS; w++) {
                client->ps.ammo[w] = -1;
            }
        }
    }

    // [QL] starting health: g_startingHealth + g_startingHealthBonus (default 100+25=125)
    {
        int startHealth = g_startingHealth.integer + g_startingHealthBonus.integer;
        if (startHealth < 1) {
            startHealth = client->pers.maxHealth;
        } else if (startHealth > 999) {
            startHealth = 999;
        }
        ent->health = client->ps.stats[STAT_HEALTH] = startHealth;
    }

    // [QL] starting armor from cvar
    client->ps.stats[STAT_ARMOR] = g_startingArmor.integer;

    G_SetOrigin(ent, spawn_origin);
    VectorCopy(spawn_origin, client->ps.origin);

    // [QL] Lag compensation - clear position history on spawn
    if (g_lagHaxMs.integer != 0 && g_lagHaxHistory.integer != 0) {
        HAX_Clear(ent);
    }

    // the respawned flag will be cleared after the attack and jump keys come up
    client->ps.pm_flags |= PMF_RESPAWNED;

    trap_GetUsercmd(client - level.clients, &ent->client->pers.cmd);
    SetClientViewAngle(ent, spawn_angles);
    // don't allow full run speed for a bit
    client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
    client->ps.pm_time = 100;

    // [QL] set movement mode flags from server cvars
    if (pmove_AirControl.integer) {
        client->ps.pm_flags |= PMF_PROMODE;
    }
    if (pmove_DoubleJump.integer) {
        client->ps.pm_flags |= PMF_DOUBLE_JUMPED;
    }
    if (pmove_CrouchSlide.integer) {
        client->ps.pm_flags |= PMF_CROUCH_SLIDE;
    }
    if (pmove_AutoHop.integer && g_gametype.integer != GT_CA) {
        // autoHop is opt-in per-client via userinfo "cg_autoHop"
        // [QL] binary skips this for GT_CA
        char *s = Info_ValueForKey(userinfo, "cg_autoHop");
        if (*s && atoi(s)) {
            client->ps.pm_flags &= ~PMF_NO_AUTOHOP;
        } else {
            client->ps.pm_flags |= PMF_NO_AUTOHOP;
        }
    }

    client->respawnTime = level.time;
    client->pers.inactivityTime = level.time + g_inactivity.integer * 1000;

    // set default animations
    client->ps.torsoAnim = TORSO_STAND;
    client->ps.legsAnim = LEGS_IDLE;

    // [QL] gametype-specific spawn logic (freeze during warmup/countdown)
    switch (g_gametype.integer) {
    case GT_RACE:
        // restore race info from saved data (preserved across respawn)
        break;
    case GT_CA:
    case GT_AD:
        // freeze during warmup or countdown (roundState 1)
        if (level.warmupTime > 0 || level.roundState.eCurrent == 1) {
            client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
        }
        break;
    case GT_FREEZE:
        // freeze during warmup/countdown, but only if roundState != 0
        if ((level.warmupTime > 0 || level.roundState.eCurrent == 1)
            && level.roundState.eCurrent != 0) {
            client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
        }
        break;
    default:
        break;
    }

    if (!level.intermissionTime) {
        if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
            // fire the targets of the spawn point
            G_UseTargets(spawnPoint, ent);

            // positively link the client, even if the command times are weird
            VectorCopy(ent->client->ps.origin, ent->r.currentOrigin);

            // [QL] G_KillBox after SetClientViewAngle (binary order)
            G_KillBox(ent);
            trap_LinkEntity(ent);
        }
    } else {
        // move players to intermission
        MoveClientToIntermission(ent);
    }

    // run a client frame to drop exactly to the floor,
    // initialize animations and other things
    client->ps.commandTime = level.time - 100;
    ent->client->pers.cmd.serverTime = level.time;
    trap_GetUsercmd(client - level.clients, &ent->client->pers.cmd);
    ent->client->pers.cmd.serverTime = level.time;
    ClientThink(ent - g_entities);

    // run the presend to set anything else, follow spectators wait
    // until all clients have been reconnected after map_restart
    if (!(ent->r.svFlags & SVF_BOT)) {
        ClientEndFrame(ent);
    }

    // [QL] g_spawnItems processing: give items listed in semicolon-delimited string
    if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
        if (g_spawnItems.string[0]) {
            char spawnItemsBuf[1024];
            char *tok;
            Q_strncpyz(spawnItemsBuf, g_spawnItems.string, sizeof(spawnItemsBuf));
            tok = strtok(spawnItemsBuf, ";");
            while (tok) {
                // GiveItem would parse "item count" pairs - stub for now
                tok = strtok(NULL, ";");
            }
        }

        // update origin after ClientThink
        VectorCopy(ent->client->ps.origin, ent->r.currentOrigin);
        trap_LinkEntity(ent);
    }

    // [QL] spectator auto-follow: if spawned as spectator in follow mode, attach to target
    if (client->sess.sessionTeam == TEAM_SPECTATOR
        && client->sess.spectatorState == SPECTATOR_FOLLOW
        && client->sess.spectatorClient >= 0) {
        client->ps.clientNum = client->sess.spectatorClient;
        client->ps.pm_flags |= PMF_FOLLOW;
    }

    // [QL] select best weapon after spawn (binary does this inside GiveDefaultWeapons)
    if (!level.intermissionTime && client->sess.sessionTeam != TEAM_SPECTATOR) {
        SelectSpawnWeapon(ent);
    }

    // [QL] team alive counting for team gametypes
    if (g_gametype.integer > GT_RACE) {
        UpdateTeamAliveCount();
    }

    // clear entity state values
    BG_PlayerStateToEntityState(&client->ps, &ent->s, qtrue);
}

/*
===========
ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.

This should NOT be called directly by any game logic,
call trap_DropClient(), which will call this and do
server system housekeeping.
============
*/
void ClientDisconnect(int clientNum) {
    gentity_t* ent;
    gentity_t* tent;
    int i;
    gclient_t* cl;

    ent = g_entities + clientNum;

    // [QL] Lag compensation - clear position history on disconnect
    if (g_lagHaxMs.integer != 0 && g_lagHaxHistory.integer != 0) {
        HAX_Clear(ent);
    }

    // [QL] publish disconnect stats before anything else (unless intermission)
    if (ent->client && ent->client->ps.pm_type != PM_INTERMISSION) {
        STAT_PublishClientDisconnect(ent->client, 1);
    }

    // cleanup if we are kicking a bot that hasn't spawned yet
    G_RemoveQueuedBotBegin(clientNum);

    if (!ent->client || ent->client->pers.connected == CON_DISCONNECTED) {
        return;
    }

    // [QL] duel: send scoreboard on disconnect
    if (g_gametype.integer == GT_TOURNAMENT) {
        DuelScoreboardMessage(ent);
    }

    // [QL] update alive counts immediately
    UpdateTeamAliveCount();

    // [QL] complex follow cleanup: stop followers, release grapple, clear complaint/damage tracking
    for (i = 0; i < level.maxclients; i++) {
        cl = &level.clients[i];

        // stop following disconnecting client
        if (cl->sess.sessionTeam == TEAM_SPECTATOR
            && cl->sess.spectatorState == SPECTATOR_FOLLOW
            && cl->sess.spectatorClient == clientNum) {
            // if disconnecting client is spectator, or warmup, or no players alive: full stop
            if (ent->client->sess.sessionTeam == TEAM_SPECTATOR
                || level.warmupTime
                || level.numPlayingClients < 1) {
                StopFollowing(&g_entities[i]);
            } else {
                // try to follow next player
                Cmd_FollowCycle_f(&g_entities[i], 1);
            }
        }

        // [QL] release grapple if target is disconnecting player
        if (level.clients[i].hook
            && level.clients[i].hook->enemy == ent) {
            G_ReleaseGrapple(&g_entities[i]);
        }

        // [QL] clear complaint tracking for this client
        if (cl->pers.complaintClient == clientNum) {
            cl->pers.complaintClient = -1;
            cl->pers.complaintEndTime = 0;
            trap_SendServerCommand(level.sortedClients[i], "complaint -2");
        }

        // [QL] clear per-client damage tracking
        cl->pers.damageFromTeammates = 0;
    }

    // send effect if they were completely connected
    if (ent->client->pers.connected == CON_CONNECTED
        && ent->client->sess.sessionTeam != TEAM_SPECTATOR
        && !level.intermissionQueued && !level.intermissionTime) {
        tent = G_TempEntity(ent->client->ps.origin, EV_PLAYER_TELEPORT_OUT);
        tent->s.clientNum = ent->s.clientNum;

        // They don't get to take powerups with them!
        TossClientItems(ent);

        // [QL] release own grapple
        if (ent->client->hook) {
            G_ReleaseGrapple(ent);
        }

        // [QL] respawn any held keys back into the world
        if (ent->client->ps.stats[STAT_KEY]) {
            if (ent->client->ps.stats[STAT_KEY] & KEY_SILVER) G_RespawnKey(KEY_SILVER);
            if (ent->client->ps.stats[STAT_KEY] & KEY_GOLD)   G_RespawnKey(KEY_GOLD);
            if (ent->client->ps.stats[STAT_KEY] & KEY_MASTER) G_RespawnKey(KEY_MASTER);
            ent->client->ps.stats[STAT_KEY] = 0;
        }
    }

    G_LogPrintf("ClientDisconnect: %i\n", clientNum);

    // if we are playing in tourney mode and losing, give a win to the other player
    if ((g_gametype.integer == GT_TOURNAMENT) && !level.intermissionTime && !level.warmupTime && level.sortedClients[1] == clientNum) {
        level.clients[level.sortedClients[0]].sess.wins++;
        ClientUserinfoChanged(level.sortedClients[0]);
    }

    STAT_LogDisconnect(clientNum);

    trap_UnlinkEntity(ent);
    ent->s.modelindex = 0;
    ent->inuse = qfalse;
    ent->classname = "disconnected";
    ent->client->pers.connected = CON_DISCONNECTED;
    ent->client->ps.persistant[PERS_TEAM] = TEAM_FREE;
    ent->client->sess.sessionTeam = TEAM_FREE;
    // [QL] clear privilege level
    ent->client->sess.privileges = 0;

    trap_SetConfigstring(CS_PLAYERS + clientNum, "");

    CalculateRanks();

    if (ent->r.svFlags & SVF_BOT) {
        BotAIShutdownClient(clientNum, qfalse);
    }

    // [QL] round-based gametypes: recount alive players after disconnect
    switch (g_gametype.integer) {
    case GT_CA:
    case GT_FREEZE:
    case GT_AD:
    case GT_RR:
        UpdateTeamAliveCount();
        break;
    }
}

/*
============
ClientBegin_Race

[QL] Race gametype begin - clear race timing data, send race_init
============
*/
void ClientBegin_Race(gentity_t* ent) {
    gclient_t* client = ent->client;

    // clear race info (timing, checkpoints)
    memset(&client->race, 0, sizeof(client->race));
    // sentinel value: no best time yet
    client->ps.persistant[PERS_SCORE] = 0x7FFFFFFF;

    // send race_init to the client
    trap_SendServerCommand(ent - g_entities, "race_init");
    ClientSpawn(ent);
}

/*
============
ClientBegin_RoundBased

[QL] CA/AD begin - spawn behavior depends on round state:
  roundState 0 (pre-round): normal spawn + weapon select
  roundState 1 (countdown): spawn + freeze (PMF_TIME_KNOCKBACK)
  roundState 2+ (active round): spectator mode until next round
============
*/
void ClientBegin_RoundBased(gentity_t* ent) {
    gclient_t* client = ent->client;

    if (level.roundState.eCurrent == 0) {
        // pre-round: normal spawn
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
    } else if (level.roundState.eCurrent == 1) {
        // countdown: spawn but freeze
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
        client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
    } else {
        // round active: force to spectator, wait for next round
        client->ps.pm_type = PM_SPECTATOR;
        ClientSpawn(ent);
        UpdateTeamAliveCount();
        // auto-follow if warmup is off, not spectator team, and both teams have players
        if (!level.warmupTime
            && client->sess.sessionTeam != TEAM_SPECTATOR
            && level.numPlayingClients > 0) {
            Cmd_FollowCycle_f(ent, 1);
        }
    }
}

/*
============
ClientBegin_Freeze

[QL] Freeze Tag begin - like RoundBased but clears freeze/dead pm_type first
============
*/
void ClientBegin_Freeze(gentity_t* ent) {
    gclient_t* client = ent->client;

    if (level.roundState.eCurrent == 0) {
        // pre-round: unfreeze if needed
        if (client->ps.pm_type == PM_FREEZE || client->ps.pm_type == PM_DEAD) {
            client->ps.pm_type = PM_NORMAL;
        }
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
    } else if (level.roundState.eCurrent == 1) {
        // countdown
        if (client->ps.pm_type == PM_FREEZE || client->ps.pm_type == PM_DEAD) {
            client->ps.pm_type = PM_NORMAL;
        }
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
        if (level.roundState.eCurrent != 0) {
            client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
        }
    } else {
        // round active: spectator mode
        client->ps.pm_type = PM_SPECTATOR;
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
        UpdateTeamAliveCount();
        if (!level.warmupTime
            && client->sess.sessionTeam != TEAM_SPECTATOR
            && level.numPlayingClients > 0) {
            Cmd_FollowCycle_f(ent, 1);
        }
    }
}

/*
============
ClientBegin_RedRover

[QL] Red Rover begin - spawn normally, freeze during countdown
============
*/
void ClientBegin_RedRover(gentity_t* ent) {
    gclient_t* client = ent->client;

    if (level.roundState.eCurrent != 1) {
        // not in countdown: normal spawn
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
        return;
    }
    // countdown: spawn + freeze
    ClientSpawn(ent);
    SelectSpawnWeapon(ent);
    client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
}

/*
============
SelectSpawnWeapon

[QL] Select best weapon after spawn. Checks:
1. If frozen (PMF_FREEZE flag 0x80000), force gauntlet
2. If loadout enabled and weaponPrimary is available, select it
3. Otherwise, select highest available weapon
============
*/
void SelectSpawnWeapon(gentity_t* ent) {
    gclient_t* client = ent->client;
    int i;

    // frozen players get gauntlet only
    if (client->ps.pm_flags & 0x80000) {
        client->ps.weapon = WP_GAUNTLET;
        return;
    }

    // loadout: prefer weaponPrimary if available
    if (g_loadout.integer && client->sess.weaponPrimary > 0
        && client->sess.weaponPrimary < WP_NUM_WEAPONS) {
        if (client->ps.stats[STAT_WEAPONS] & (1 << client->sess.weaponPrimary)) {
            client->ps.weapon = client->sess.weaponPrimary;
            return;
        }
    }

    // fallback: select highest weapon
    for (i = WP_NUM_WEAPONS - 1; i > 0; i--) {
        if (client->ps.stats[STAT_WEAPONS] & (1 << i)) {
            client->ps.weapon = i;
            return;
        }
    }
    client->ps.weapon = WP_GAUNTLET;
}

/*
============
UpdateTeamAliveCount

[QL] Count alive (non-spectator, non-dead, connected) players per team.
Sets configstring for each team's alive count.
============
*/
void UpdateTeamAliveCount(void) {
    int i;
    int aliveRed = 0, aliveBlue = 0;

    for (i = 0; i < level.maxclients; i++) {
        gclient_t* cl = &level.clients[i];
        if (cl->pers.connected != CON_CONNECTED) continue;
        if (cl->sess.sessionTeam == TEAM_SPECTATOR) continue;
        if (cl->ps.pm_type == PM_SPECTATOR) continue;
        if (cl->ps.stats[STAT_HEALTH] <= 0) continue;
        if (cl->sess.sessionTeam == TEAM_RED) aliveRed++;
        else if (cl->sess.sessionTeam == TEAM_BLUE) aliveBlue++;
    }

    trap_SetConfigstring(CS_TEAMCOUNT_RED, va("%i", aliveRed));
    trap_SetConfigstring(CS_TEAMCOUNT_BLUE, va("%i", aliveBlue));
}

/*
============
G_GetAccessLevel

[QL] Stub - Steam-based access level check.
Returns 0 (regular), 1 (mod), 2 (admin), -1 (banned).
Without Steam auth, always returns 0.
============
*/
int G_GetAccessLevel(const char* ip) {
    return 0;
}

/*
============
G_ReleaseGrapple

[QL] Release grapple hook if the entity has one active
============
*/
void G_ReleaseGrapple(gentity_t* ent) {
    if (!ent || !ent->client) return;
    if (ent->client->hook) {
        Weapon_HookFree(ent->client->hook);
    }
}

/*
============
DuelScoreboardMessage

[QL] Send duel-specific scoreboard to all clients
============
*/
void DuelScoreboardMessage(gentity_t* ent) {
    // In duel, the regular DeathmatchScoreboardMessage handles this
    DeathmatchScoreboardMessage(ent);
}

/*
============
STAT_* stubs

[QL] Stats reporting functions - stubs for now (original uses C++ jsoncpp)
============
*/
void STAT_InitClient(gentity_t* ent) { }
void STAT_PublishClientDisconnect(gclient_t* client, int reason) { }
void STAT_SubscribeClient(gentity_t* ent) { }
void STAT_LogDisconnect(int clientNum) { }
void STAT_MatchEnd(void) { }

/*
============
G_IsTeamLocked

[QL] Check if a team is locked (via /lock command by admin/referee)
Currently returns qfalse (unlocked) - lock state would need
a level-scope variable to track per-team lock status.
============
*/
qboolean G_IsTeamLocked(team_t team) {
    return qfalse;
}
