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
// sv_game.c -- interface to the game dll

#include "server.h"

#include "../botlib/botlib.h"

botlib_export_t* botlib_export;

// these functions must be used instead of pointer arithmetic, because
// the game allocates gentities with private information after the server shared part
int SV_NumForGentity(sharedEntity_t* ent) {
    int num;

    num = ((byte*)ent - (byte*)sv.gentities) / sv.gentitySize;

    return num;
}

sharedEntity_t* SV_GentityNum(int num) {
    sharedEntity_t* ent;

    ent = (sharedEntity_t*)((byte*)sv.gentities + sv.gentitySize * (num));

    return ent;
}

playerState_t* SV_GameClientNum(int num) {
    playerState_t* ps;

    ps = (playerState_t*)((byte*)sv.gameClients + sv.gameClientSize * (num));

    return ps;
}

svEntity_t* SV_SvEntityForGentity(sharedEntity_t* gEnt) {
    if (!gEnt || gEnt->s.number < 0 || gEnt->s.number >= MAX_GENTITIES) {
        Com_Error(ERR_DROP, "SV_SvEntityForGentity: bad gEnt");
    }
    return &sv.svEntities[gEnt->s.number];
}

sharedEntity_t* SV_GEntityForSvEntity(svEntity_t* svEnt) {
    int num;

    num = svEnt - sv.svEntities;
    return SV_GentityNum(num);
}

/*
===============
SV_GameSendServerCommand

Sends a command string to a client
===============
*/
void SV_GameSendServerCommand(int clientNum, const char* text) {
    if (clientNum == -1) {
        SV_SendServerCommand(NULL, "%s", text);
    } else {
        if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
            return;
        }
        SV_SendServerCommand(svs.clients + clientNum, "%s", text);
    }
}

/*
===============
SV_GameDropClient

Disconnects the client with a message
===============
*/
void SV_GameDropClient(int clientNum, const char* reason) {
    if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
        return;
    }
    SV_DropClient(svs.clients + clientNum, reason);
}

/*
=================
SV_SetBrushModel

sets mins and maxs for inline bmodels
=================
*/
void SV_SetBrushModel(sharedEntity_t* ent, const char* name) {
    clipHandle_t h;
    vec3_t mins, maxs;

    if (!name) {
        Com_Error(ERR_DROP, "SV_SetBrushModel: NULL");
    }

    if (name[0] != '*') {
        Com_Error(ERR_DROP, "SV_SetBrushModel: %s isn't a brush model", name);
    }

    ent->s.modelindex = atoi(name + 1);

    h = CM_InlineModel(ent->s.modelindex);
    CM_ModelBounds(h, mins, maxs);
    VectorCopy(mins, ent->r.mins);
    VectorCopy(maxs, ent->r.maxs);
    ent->r.bmodel = qtrue;

    ent->r.contents = -1;  // we don't know exactly what is in the brushes

    SV_LinkEntity(ent);  // FIXME: remove
}

/*
=================
SV_inPVS

Also checks portalareas so that doors block sight
=================
*/
qboolean SV_inPVS(const vec3_t p1, const vec3_t p2) {
    int leafnum;
    int cluster;
    int area1, area2;
    byte* mask;

    leafnum = CM_PointLeafnum(p1);
    cluster = CM_LeafCluster(leafnum);
    area1 = CM_LeafArea(leafnum);
    mask = CM_ClusterPVS(cluster);

    leafnum = CM_PointLeafnum(p2);
    cluster = CM_LeafCluster(leafnum);
    area2 = CM_LeafArea(leafnum);
    if (mask && (!(mask[cluster >> 3] & (1 << (cluster & 7)))))
        return qfalse;
    if (!CM_AreasConnected(area1, area2))
        return qfalse;  // a door blocks sight
    return qtrue;
}

/*
=================
SV_inPVSIgnorePortals

Does NOT check portalareas
=================
*/
qboolean SV_inPVSIgnorePortals(const vec3_t p1, const vec3_t p2) {
    int leafnum;
    int cluster;
    byte* mask;

    leafnum = CM_PointLeafnum(p1);
    cluster = CM_LeafCluster(leafnum);
    mask = CM_ClusterPVS(cluster);

    leafnum = CM_PointLeafnum(p2);
    cluster = CM_LeafCluster(leafnum);

    if (mask && (!(mask[cluster >> 3] & (1 << (cluster & 7)))))
        return qfalse;

    return qtrue;
}

/*
========================
SV_AdjustAreaPortalState
========================
*/
void SV_AdjustAreaPortalState(sharedEntity_t* ent, qboolean open) {
    svEntity_t* svEnt;

    svEnt = SV_SvEntityForGentity(ent);
    if (svEnt->areanum2 == -1) {
        return;
    }
    CM_AdjustAreaPortalState(svEnt->areanum, svEnt->areanum2, open);
}

/*
==================
SV_EntityContact
==================
*/
qboolean SV_EntityContact(vec3_t mins, vec3_t maxs, const sharedEntity_t* gEnt, int capsule) {
    const float *origin, *angles;
    clipHandle_t ch;
    trace_t trace;

    // check for exact collision
    origin = gEnt->r.currentOrigin;
    angles = gEnt->r.currentAngles;

    ch = SV_ClipHandleForEntity(gEnt);
    CM_TransformedBoxTrace(&trace, vec3_origin, vec3_origin, mins, maxs,
                           ch, -1, origin, angles, capsule);

    return trace.startsolid;
}

/*
===============
SV_GetServerinfo

===============
*/
void SV_GetServerinfo(char* buffer, int bufferSize) {
    if (bufferSize < 1) {
        Com_Error(ERR_DROP, "SV_GetServerinfo: bufferSize == %i", bufferSize);
    }
    Q_strncpyz(buffer, Cvar_InfoString(CVAR_SERVERINFO), bufferSize);
}

/*
===============
SV_LocateGameData

===============
*/
void SV_LocateGameData(sharedEntity_t* gEnts, int numGEntities, int sizeofGEntity_t, playerState_t* clients, int sizeofGameClient) {
    sv.gentities = gEnts;
    sv.gentitySize = sizeofGEntity_t;
    sv.num_entities = numGEntities;

    sv.gameClients = clients;
    sv.gameClientSize = sizeofGameClient;
}

/*
===============
SV_GetUsercmd

===============
*/
void SV_GetUsercmd(int clientNum, usercmd_t* cmd) {
    if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
        Com_Error(ERR_DROP, "SV_GetUsercmd: bad clientNum:%i", clientNum);
    }
    *cmd = svs.clients[clientNum].lastUsercmd;
}

//==============================================

//==============================================
//
// [QL] Game import wrappers - adapt engine functions to gameImport_t signatures
//

#include "../sys/sys_loadlib.h"

// Game module state
static void *sv_gameLibHandle;
static void *sv_vmMainTable[10];
static gameImport_t sv_gameImports;
static int sv_apiVersion;

// --- Core trap wrappers ---

static void SV_GI_Printf(const char *text) {
    Com_Printf("%s", text);
}

static void SV_GI_Error(const char *fmt, ...) {
    va_list ap;
    char text[1024];
    va_start(ap, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    Com_Error(ERR_DROP, "%s", text);
}

static void SV_GI_SendConsoleCommand(const char *text) {
    Cbuf_ExecuteText(EXEC_APPEND, text);
}

static void SV_GI_Trace(trace_t *results, const vec3_t start, const vec3_t mins,
                         const vec3_t maxs, const vec3_t end, int passEntityNum, int contentMask) {
    SV_Trace(results, start, (float *)mins, (float *)maxs, end, passEntityNum, contentMask, qfalse);
}

static void SV_GI_TraceCapsule(trace_t *results, const vec3_t start, const vec3_t mins,
                                const vec3_t maxs, const vec3_t end, int passEntityNum, int contentMask) {
    SV_Trace(results, start, (float *)mins, (float *)maxs, end, passEntityNum, contentMask, qtrue);
}

static qboolean SV_GI_EntityContact(const vec3_t mins, const vec3_t maxs, const gentity_t *ent) {
    return SV_EntityContact((float *)mins, (float *)maxs, (const sharedEntity_t *)ent, qfalse);
}

static qboolean SV_GI_GetEntityToken(char *buf, int size) {
    const char *s = COM_Parse(&sv.entityParsePoint);
    Q_strncpyz(buf, s, size);
    return (sv.entityParsePoint || s[0]) ? qtrue : qfalse;
}

// --- Bot library wrappers (runtime botlib_export check) ---

static int SV_GI_BotLibVarSet(const char *n, const char *v) {
    return botlib_export ? botlib_export->BotLibVarSet((char*)n, (char*)v) : 0;
}

static int SV_GI_BotLibDefine(const char *s) {
    return botlib_export ? botlib_export->PC_AddGlobalDefine((char*)s) : 0;
}

static int SV_GI_BotLibStartFrame(float t) {
    return botlib_export ? botlib_export->BotLibStartFrame(t) : 0;
}

static int SV_GI_BotLibLoadMap(const char *name) {
    return botlib_export ? botlib_export->BotLibLoadMap((char*)name) : 0;
}

static int SV_GI_BotLibUpdateEntity(int ent, void *info) {
    return botlib_export ? botlib_export->BotLibUpdateEntity(ent, info) : 0;
}

static void SV_GI_AAS_EntityInfo(int ent, void *info) {
    if (botlib_export) botlib_export->aas.AAS_EntityInfo(ent, info);
}

static qboolean SV_GI_AAS_Initialized(void) {
    return botlib_export ? botlib_export->aas.AAS_Initialized() : qfalse;
}

static float SV_GI_AAS_Time(void) {
    return botlib_export ? botlib_export->aas.AAS_Time() : 0.0f;
}

static int SV_GI_AAS_PointAreaNum(const vec3_t point) {
    return botlib_export ? botlib_export->aas.AAS_PointAreaNum((float*)point) : 0;
}

static int SV_GI_AAS_TraceAreas(const vec3_t start, const vec3_t end, int *areas, void *points, int maxareas) {
    return botlib_export ? botlib_export->aas.AAS_TraceAreas((float*)start, (float*)end, areas, (vec3_t*)points, maxareas) : 0;
}

// --- AAS Extended wrappers ---

static void SV_GI_AAS_PresenceTypeBoundingBox(int presencetype, vec3_t mins, vec3_t maxs) {
    if (botlib_export) botlib_export->aas.AAS_PresenceTypeBoundingBox(presencetype, mins, maxs);
}

static int SV_GI_AAS_PointReachabilityAreaIndex(vec3_t point) {
    return botlib_export ? botlib_export->aas.AAS_PointReachabilityAreaIndex(point) : 0;
}

static int SV_GI_AAS_BBoxAreas(vec3_t absmins, vec3_t absmaxs, int *areas, int maxareas) {
    return botlib_export ? botlib_export->aas.AAS_BBoxAreas(absmins, absmaxs, areas, maxareas) : 0;
}

static int SV_GI_AAS_AreaInfo(int areanum, void *info) {
    return botlib_export ? botlib_export->aas.AAS_AreaInfo(areanum, info) : 0;
}

static int SV_GI_AAS_PointContents(vec3_t point) {
    return botlib_export ? botlib_export->aas.AAS_PointContents(point) : 0;
}

static int SV_GI_AAS_NextBSPEntity(int ent) {
    return botlib_export ? botlib_export->aas.AAS_NextBSPEntity(ent) : 0;
}

static int SV_GI_AAS_ValueForBSPEpairKey(int ent, char *key, char *value, int size) {
    return botlib_export ? botlib_export->aas.AAS_ValueForBSPEpairKey(ent, key, value, size) : 0;
}

static int SV_GI_AAS_VectorForBSPEpairKey(int ent, char *key, vec3_t v) {
    return botlib_export ? botlib_export->aas.AAS_VectorForBSPEpairKey(ent, key, v) : 0;
}

static int SV_GI_AAS_FloatForBSPEpairKey(int ent, char *key, float *value) {
    return botlib_export ? botlib_export->aas.AAS_FloatForBSPEpairKey(ent, key, value) : 0;
}

static int SV_GI_AAS_IntForBSPEpairKey(int ent, char *key, int *value) {
    return botlib_export ? botlib_export->aas.AAS_IntForBSPEpairKey(ent, key, value) : 0;
}

static int SV_GI_AAS_AreaReachability(int areanum) {
    return botlib_export ? botlib_export->aas.AAS_AreaReachability(areanum) : 0;
}

static int SV_GI_AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags) {
    return botlib_export ? botlib_export->aas.AAS_AreaTravelTimeToGoalArea(areanum, origin, goalareanum, travelflags) : 0;
}

static int SV_GI_AAS_EnableRoutingArea(int areanum, int enable) {
    return botlib_export ? botlib_export->aas.AAS_EnableRoutingArea(areanum, enable) : 0;
}

static int SV_GI_AAS_PredictRoute(void *route, int areanum, vec3_t origin, int goalareanum, int travelflags, int maxareas, int maxtime, int stopevent, int stopcontents, int stoptfl, int stopareanum) {
    return botlib_export ? botlib_export->aas.AAS_PredictRoute(route, areanum, origin, goalareanum, travelflags, maxareas, maxtime, stopevent, stopcontents, stoptfl, stopareanum) : 0;
}

static int SV_GI_AAS_AlternativeRouteGoals(vec3_t start, int startareanum, vec3_t goal, int goalareanum, int travelflags, void *altroutegoals, int maxaltroutegoals, int type) {
    return botlib_export ? botlib_export->aas.AAS_AlternativeRouteGoals(start, startareanum, goal, goalareanum, travelflags, altroutegoals, maxaltroutegoals, type) : 0;
}

static int SV_GI_AAS_Swimming(vec3_t origin) {
    return botlib_export ? botlib_export->aas.AAS_Swimming(origin) : 0;
}

static int SV_GI_AAS_PredictClientMovement(void *move, int entnum, vec3_t origin, int presencetype, int onground, vec3_t velocity, vec3_t cmdmove, int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum, int visualize) {
    return botlib_export ? botlib_export->aas.AAS_PredictClientMovement(move, entnum, origin, presencetype, onground, velocity, cmdmove, cmdframes, maxframes, frametime, stopevent, stopareanum, visualize) : 0;
}

// --- EA wrappers ---

static void SV_GI_EA_Command(int client, char *command) {
    if (botlib_export) botlib_export->ea.EA_Command(client, command);
}

static void SV_GI_EA_Say(int client, char *str) {
    if (botlib_export) botlib_export->ea.EA_Say(client, str);
}

static void SV_GI_EA_SayTeam(int client, char *str) {
    if (botlib_export) botlib_export->ea.EA_SayTeam(client, str);
}

static void SV_GI_EA_Action(int client, int action) {
    if (botlib_export) botlib_export->ea.EA_Action(client, action);
}

static void SV_GI_EA_Gesture(int client) {
    if (botlib_export) botlib_export->ea.EA_Gesture(client);
}

static void SV_GI_EA_Talk(int client) {
    if (botlib_export) botlib_export->ea.EA_Talk(client);
}

static void SV_GI_EA_Attack(int client) {
    if (botlib_export) botlib_export->ea.EA_Attack(client);
}

static void SV_GI_EA_Use(int client) {
    if (botlib_export) botlib_export->ea.EA_Use(client);
}

static void SV_GI_EA_Respawn(int client) {
    if (botlib_export) botlib_export->ea.EA_Respawn(client);
}

static void SV_GI_EA_MoveUp(int client) {
    if (botlib_export) botlib_export->ea.EA_MoveUp(client);
}

static void SV_GI_EA_MoveDown(int client) {
    if (botlib_export) botlib_export->ea.EA_MoveDown(client);
}

static void SV_GI_EA_MoveForward(int client) {
    if (botlib_export) botlib_export->ea.EA_MoveForward(client);
}

static void SV_GI_EA_MoveBack(int client) {
    if (botlib_export) botlib_export->ea.EA_MoveBack(client);
}

static void SV_GI_EA_MoveLeft(int client) {
    if (botlib_export) botlib_export->ea.EA_MoveLeft(client);
}

static void SV_GI_EA_MoveRight(int client) {
    if (botlib_export) botlib_export->ea.EA_MoveRight(client);
}

static void SV_GI_EA_Crouch(int client) {
    if (botlib_export) botlib_export->ea.EA_Crouch(client);
}

static void SV_GI_EA_SelectWeapon(int client, int weapon) {
    if (botlib_export) botlib_export->ea.EA_SelectWeapon(client, weapon);
}

static void SV_GI_EA_Jump(int client) {
    if (botlib_export) botlib_export->ea.EA_Jump(client);
}

static void SV_GI_EA_DelayedJump(int client) {
    if (botlib_export) botlib_export->ea.EA_DelayedJump(client);
}

static void SV_GI_EA_Move(int client, vec3_t dir, float speed) {
    if (botlib_export) botlib_export->ea.EA_Move(client, dir, speed);
}

static void SV_GI_EA_View(int client, vec3_t viewangles) {
    if (botlib_export) botlib_export->ea.EA_View(client, viewangles);
}

static void SV_GI_EA_EndRegular(int client, float thinktime) {
    if (botlib_export) botlib_export->ea.EA_EndRegular(client, thinktime);
}

static void SV_GI_EA_GetInput(int client, float thinktime, void *input) {
    if (botlib_export) botlib_export->ea.EA_GetInput(client, thinktime, input);
}

static void SV_GI_EA_ResetInput(int client) {
    if (botlib_export) botlib_export->ea.EA_ResetInput(client);
}

// --- Bot Character wrappers ---

static int SV_GI_BotLoadCharacter(char *charfile, float skill) {
    return botlib_export ? botlib_export->ai.BotLoadCharacter(charfile, skill) : 0;
}

static void SV_GI_BotFreeCharacter(int character) {
    if (botlib_export) botlib_export->ai.BotFreeCharacter(character);
}

static float SV_GI_Characteristic_Float(int character, int index) {
    return botlib_export ? botlib_export->ai.Characteristic_Float(character, index) : 0.0f;
}

static float SV_GI_Characteristic_BFloat(int character, int index, float min, float max) {
    return botlib_export ? botlib_export->ai.Characteristic_BFloat(character, index, min, max) : 0.0f;
}

static int SV_GI_Characteristic_Integer(int character, int index) {
    return botlib_export ? botlib_export->ai.Characteristic_Integer(character, index) : 0;
}

static int SV_GI_Characteristic_BInteger(int character, int index, int min, int max) {
    return botlib_export ? botlib_export->ai.Characteristic_BInteger(character, index, min, max) : 0;
}

static void SV_GI_Characteristic_String(int character, int index, char *buf, int size) {
    if (botlib_export) botlib_export->ai.Characteristic_String(character, index, buf, size);
}

// --- Bot Chat wrappers ---

static int SV_GI_BotAllocChatState(void) {
    return botlib_export ? botlib_export->ai.BotAllocChatState() : 0;
}

static void SV_GI_BotFreeChatState(int handle) {
    if (botlib_export) botlib_export->ai.BotFreeChatState(handle);
}

static void SV_GI_BotQueueConsoleMessage(int chatstate, int type, char *message) {
    if (botlib_export) botlib_export->ai.BotQueueConsoleMessage(chatstate, type, message);
}

static void SV_GI_BotRemoveConsoleMessage(int chatstate, int handle) {
    if (botlib_export) botlib_export->ai.BotRemoveConsoleMessage(chatstate, handle);
}

static int SV_GI_BotNextConsoleMessage(int chatstate, void *cm) {
    return botlib_export ? botlib_export->ai.BotNextConsoleMessage(chatstate, cm) : 0;
}

static int SV_GI_BotNumConsoleMessages(int chatstate) {
    return botlib_export ? botlib_export->ai.BotNumConsoleMessages(chatstate) : 0;
}

static void SV_GI_BotInitialChat(int chatstate, char *type, int mcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7) {
    if (botlib_export) botlib_export->ai.BotInitialChat(chatstate, type, mcontext, var0, var1, var2, var3, var4, var5, var6, var7);
}

static int SV_GI_BotNumInitialChats(int chatstate, char *type) {
    return botlib_export ? botlib_export->ai.BotNumInitialChats(chatstate, type) : 0;
}

static int SV_GI_BotReplyChat(int chatstate, char *message, int mcontext, int vcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7) {
    return botlib_export ? botlib_export->ai.BotReplyChat(chatstate, message, mcontext, vcontext, var0, var1, var2, var3, var4, var5, var6, var7) : 0;
}

static int SV_GI_BotChatLength(int chatstate) {
    return botlib_export ? botlib_export->ai.BotChatLength(chatstate) : 0;
}

static void SV_GI_BotEnterChat(int chatstate, int client, int sendto) {
    if (botlib_export) botlib_export->ai.BotEnterChat(chatstate, client, sendto);
}

static void SV_GI_BotGetChatMessage(int chatstate, char *buf, int size) {
    if (botlib_export) botlib_export->ai.BotGetChatMessage(chatstate, buf, size);
}

static int SV_GI_StringContains(char *str1, char *str2, int casesensitive) {
    return botlib_export ? botlib_export->ai.StringContains(str1, str2, casesensitive) : -1;
}

static int SV_GI_BotFindMatch(char *str, void *match, unsigned long context) {
    return botlib_export ? botlib_export->ai.BotFindMatch(str, match, context) : 0;
}

static void SV_GI_BotMatchVariable(void *match, int variable, char *buf, int size) {
    if (botlib_export) botlib_export->ai.BotMatchVariable(match, variable, buf, size);
}

static void SV_GI_UnifyWhiteSpaces(char *string) {
    if (botlib_export) botlib_export->ai.UnifyWhiteSpaces(string);
}

static void SV_GI_BotReplaceSynonyms(char *string, unsigned long context) {
    if (botlib_export) botlib_export->ai.BotReplaceSynonyms(string, context);
}

static void SV_GI_BotSetChatGender(int chatstate, int gender) {
    if (botlib_export) botlib_export->ai.BotSetChatGender(chatstate, gender);
}

static void SV_GI_BotSetChatName(int chatstate, char *name, int client) {
    if (botlib_export) botlib_export->ai.BotSetChatName(chatstate, name, client);
}

static int SV_GI_BotLoadChatFile(int chatstate, char *chatfile, char *chatname) {
    return botlib_export ? botlib_export->ai.BotLoadChatFile(chatstate, chatfile, chatname) : 0;
}

// --- Bot Goal wrappers ---

static void SV_GI_BotResetGoalState(int goalstate) {
    if (botlib_export) botlib_export->ai.BotResetGoalState(goalstate);
}

static void SV_GI_BotResetAvoidGoals(int goalstate) {
    if (botlib_export) botlib_export->ai.BotResetAvoidGoals(goalstate);
}

static void SV_GI_BotRemoveFromAvoidGoals(int goalstate, int number) {
    if (botlib_export) botlib_export->ai.BotRemoveFromAvoidGoals(goalstate, number);
}

static void SV_GI_BotPushGoal(int goalstate, void *goal) {
    if (botlib_export) botlib_export->ai.BotPushGoal(goalstate, goal);
}

static void SV_GI_BotPopGoal(int goalstate) {
    if (botlib_export) botlib_export->ai.BotPopGoal(goalstate);
}

static void SV_GI_BotEmptyGoalStack(int goalstate) {
    if (botlib_export) botlib_export->ai.BotEmptyGoalStack(goalstate);
}

static void SV_GI_BotDumpAvoidGoals(int goalstate) {
    if (botlib_export) botlib_export->ai.BotDumpAvoidGoals(goalstate);
}

static void SV_GI_BotDumpGoalStack(int goalstate) {
    if (botlib_export) botlib_export->ai.BotDumpGoalStack(goalstate);
}

static void SV_GI_BotGoalName(int number, char *name, int size) {
    if (botlib_export) botlib_export->ai.BotGoalName(number, name, size);
}

static int SV_GI_BotGetTopGoal(int goalstate, void *goal) {
    return botlib_export ? botlib_export->ai.BotGetTopGoal(goalstate, goal) : 0;
}

static int SV_GI_BotGetSecondGoal(int goalstate, void *goal) {
    return botlib_export ? botlib_export->ai.BotGetSecondGoal(goalstate, goal) : 0;
}

static int SV_GI_BotChooseLTGItem(int goalstate, vec3_t origin, int *inventory, int travelflags) {
    return botlib_export ? botlib_export->ai.BotChooseLTGItem(goalstate, origin, inventory, travelflags) : 0;
}

static int SV_GI_BotChooseNBGItem(int goalstate, vec3_t origin, int *inventory, int travelflags, void *ltg, float maxtime) {
    return botlib_export ? botlib_export->ai.BotChooseNBGItem(goalstate, origin, inventory, travelflags, ltg, maxtime) : 0;
}

static int SV_GI_BotTouchingGoal(vec3_t origin, void *goal) {
    return botlib_export ? botlib_export->ai.BotTouchingGoal(origin, goal) : 0;
}

static int SV_GI_BotItemGoalInVisButNotVisible(int viewer, vec3_t eye, vec3_t viewangles, void *goal) {
    return botlib_export ? botlib_export->ai.BotItemGoalInVisButNotVisible(viewer, eye, viewangles, goal) : 0;
}

static int SV_GI_BotGetLevelItemGoal(int index, char *classname, void *goal) {
    return botlib_export ? botlib_export->ai.BotGetLevelItemGoal(index, classname, goal) : 0;
}

static int SV_GI_BotGetNextCampSpotGoal(int num, void *goal) {
    return botlib_export ? botlib_export->ai.BotGetNextCampSpotGoal(num, goal) : 0;
}

static int SV_GI_BotGetMapLocationGoal(char *name, void *goal) {
    return botlib_export ? botlib_export->ai.BotGetMapLocationGoal(name, goal) : 0;
}

static float SV_GI_BotAvoidGoalTime(int goalstate, int number) {
    return botlib_export ? botlib_export->ai.BotAvoidGoalTime(goalstate, number) : 0.0f;
}

static void SV_GI_BotSetAvoidGoalTime(int goalstate, int number, float avoidtime) {
    if (botlib_export) botlib_export->ai.BotSetAvoidGoalTime(goalstate, number, avoidtime);
}

static void SV_GI_BotInitLevelItems(void) {
    if (botlib_export) botlib_export->ai.BotInitLevelItems();
}

static void SV_GI_BotUpdateEntityItems(void) {
    if (botlib_export) botlib_export->ai.BotUpdateEntityItems();
}

static int SV_GI_BotLoadItemWeights(int goalstate, char *filename) {
    return botlib_export ? botlib_export->ai.BotLoadItemWeights(goalstate, filename) : 0;
}

static void SV_GI_BotFreeItemWeights(int goalstate) {
    if (botlib_export) botlib_export->ai.BotFreeItemWeights(goalstate);
}

static void SV_GI_BotInterbreedGoalFuzzyLogic(int parent1, int parent2, int child) {
    if (botlib_export) botlib_export->ai.BotInterbreedGoalFuzzyLogic(parent1, parent2, child);
}

static void SV_GI_BotSaveGoalFuzzyLogic(int goalstate, char *filename) {
    if (botlib_export) botlib_export->ai.BotSaveGoalFuzzyLogic(goalstate, filename);
}

static void SV_GI_BotMutateGoalFuzzyLogic(int goalstate, float range) {
    if (botlib_export) botlib_export->ai.BotMutateGoalFuzzyLogic(goalstate, range);
}

static int SV_GI_BotAllocGoalState(int client) {
    return botlib_export ? botlib_export->ai.BotAllocGoalState(client) : 0;
}

static void SV_GI_BotFreeGoalState(int handle) {
    if (botlib_export) botlib_export->ai.BotFreeGoalState(handle);
}

// --- Bot Movement wrappers ---

static void SV_GI_BotResetMoveState(int movestate) {
    if (botlib_export) botlib_export->ai.BotResetMoveState(movestate);
}

static void SV_GI_BotMoveToGoal(void *result, int movestate, void *goal, int travelflags) {
    if (botlib_export) botlib_export->ai.BotMoveToGoal(result, movestate, goal, travelflags);
}

static int SV_GI_BotMoveInDirection(int movestate, vec3_t dir, float speed, int type) {
    return botlib_export ? botlib_export->ai.BotMoveInDirection(movestate, dir, speed, type) : 0;
}

static void SV_GI_BotResetAvoidReach(int movestate) {
    if (botlib_export) botlib_export->ai.BotResetAvoidReach(movestate);
}

static void SV_GI_BotResetLastAvoidReach(int movestate) {
    if (botlib_export) botlib_export->ai.BotResetLastAvoidReach(movestate);
}

static int SV_GI_BotReachabilityArea(vec3_t origin, int testground) {
    return botlib_export ? botlib_export->ai.BotReachabilityArea(origin, testground) : 0;
}

static int SV_GI_BotMovementViewTarget(int movestate, void *goal, int travelflags, float lookahead, vec3_t target) {
    return botlib_export ? botlib_export->ai.BotMovementViewTarget(movestate, goal, travelflags, lookahead, target) : 0;
}

static int SV_GI_BotPredictVisiblePosition(vec3_t origin, int areanum, void *goal, int travelflags, vec3_t target) {
    return botlib_export ? botlib_export->ai.BotPredictVisiblePosition(origin, areanum, goal, travelflags, target) : 0;
}

static int SV_GI_BotAllocMoveState(void) {
    return botlib_export ? botlib_export->ai.BotAllocMoveState() : 0;
}

static void SV_GI_BotFreeMoveState(int handle) {
    if (botlib_export) botlib_export->ai.BotFreeMoveState(handle);
}

static void SV_GI_BotInitMoveState(int handle, void *initmove) {
    if (botlib_export) botlib_export->ai.BotInitMoveState(handle, initmove);
}

static void SV_GI_BotAddAvoidSpot(int movestate, vec3_t origin, float radius, int type) {
    if (botlib_export) botlib_export->ai.BotAddAvoidSpot(movestate, origin, radius, type);
}

// --- Bot Weapon wrappers ---

static int SV_GI_BotChooseBestFightWeapon(int weaponstate, int *inventory) {
    return botlib_export ? botlib_export->ai.BotChooseBestFightWeapon(weaponstate, inventory) : 0;
}

static void SV_GI_BotGetWeaponInfo(int weaponstate, int weapon, void *weaponinfo) {
    if (botlib_export) botlib_export->ai.BotGetWeaponInfo(weaponstate, weapon, weaponinfo);
}

static int SV_GI_BotLoadWeaponWeights(int weaponstate, char *filename) {
    return botlib_export ? botlib_export->ai.BotLoadWeaponWeights(weaponstate, filename) : 0;
}

static int SV_GI_BotAllocWeaponState(void) {
    return botlib_export ? botlib_export->ai.BotAllocWeaponState() : 0;
}

static void SV_GI_BotFreeWeaponState(int weaponstate) {
    if (botlib_export) botlib_export->ai.BotFreeWeaponState(weaponstate);
}

static void SV_GI_BotResetWeaponState(int weaponstate) {
    if (botlib_export) botlib_export->ai.BotResetWeaponState(weaponstate);
}

// --- Misc Bot wrappers ---

static int SV_GI_GeneticParentsAndChildSelection(int numranks, float *ranks, int *parent1, int *parent2, int *child) {
    return botlib_export ? botlib_export->ai.GeneticParentsAndChildSelection(numranks, ranks, parent1, parent2, child) : 0;
}

static int SV_GI_BotGetServerCommand(int clientNum, char *message, int size) {
    return SV_BotGetConsoleMessage(clientNum, message, size);
}

static int SV_GI_BotLibVarGet(char *var_name, char *value, int size) {
    return botlib_export ? botlib_export->BotLibVarGet(var_name, value, size) : 0;
}

static int SV_GI_BotLibTest(int parm0, char *parm1, vec3_t parm2, vec3_t parm3) {
    return botlib_export ? botlib_export->Test(parm0, parm1, parm2, parm3) : 0;
}

static int SV_GI_PC_LoadSource(const char *filename) {
    return botlib_export ? botlib_export->PC_LoadSourceHandle(filename) : 0;
}

static int SV_GI_PC_FreeSource(int handle) {
    return botlib_export ? botlib_export->PC_FreeSourceHandle(handle) : 0;
}

static int SV_GI_PC_ReadToken(int handle, pc_token_t *pc_token) {
    return botlib_export ? botlib_export->PC_ReadTokenHandle(handle, pc_token) : 0;
}

static int SV_GI_PC_SourceFileAndLine(int handle, char *filename, int *line) {
    return botlib_export ? botlib_export->PC_SourceFileAndLine(handle, filename, line) : 0;
}

static void SV_GI_BotUserCommand(int clientNum, usercmd_t *cmd) {
    if (clientNum >= 0 && clientNum < sv_maxclients->integer) {
        SV_ClientThink(&svs.clients[clientNum], cmd);
    }
}

// --- Steam trap stubs (not implemented in open-source engine) ---

static uint64_t SV_GI_GetSteamID(int clientNum) {
    if (clientNum >= 0 && clientNum < sv_maxclients->integer)
        return svs.clients[clientNum].steam_id;
    return 0;
}

static void SV_GI_GetServerSteamID(char *buf, int size) {
    if (buf && size > 0) buf[0] = '\0';
}

static void SV_GI_IncrementSteamStat(int clientNum, int statID) { (void)clientNum; (void)statID; }
static int SV_GI_GetSteamStat(int clientNum, int statID) { (void)clientNum; (void)statID; return 0; }
static qboolean SV_GI_VerifySteamAuth(int clientNum) { (void)clientNum; return qtrue; }

/*
===============
SV_PopulateGameImports

Fill in the gameImport_t function pointer table that the game module receives.
===============
*/
static void SV_PopulateGameImports(void) {
    memset(&sv_gameImports, 0, sizeof(sv_gameImports));

    // Core engine traps
    sv_gameImports.trap_SendConsoleCommand    = SV_GI_SendConsoleCommand;
    sv_gameImports.trap_Printf                = SV_GI_Printf;
    sv_gameImports.trap_FS_Write              = (int (*)(const void*, int, fileHandle_t))FS_Write;
    sv_gameImports.trap_FS_Read               = (int (*)(void*, int, fileHandle_t))FS_Read;
    sv_gameImports.trap_FS_FOpenFile          = (int (*)(const char*, fileHandle_t*, fsMode_t))FS_FOpenFileByMode;
    sv_gameImports.trap_FS_FCloseFile         = (void (*)(fileHandle_t))FS_FCloseFile;
    sv_gameImports.trap_Error                 = SV_GI_Error;
    sv_gameImports.trap_Cvar_VariableValue    = Cvar_VariableValue;
    sv_gameImports.trap_Cvar_Update           = Cvar_Update;
    sv_gameImports.trap_Cvar_VariableStringBuffer = Cvar_VariableStringBuffer;
    sv_gameImports.trap_Cvar_Set              = Cvar_SetSafe;
    sv_gameImports.trap_Cvar_Register         = Cvar_Register;
    sv_gameImports.trap_Argv                  = Cmd_ArgvBuffer;
    sv_gameImports.trap_Argc                  = Cmd_Argc;

    // Game data and client management
    sv_gameImports.trap_LocateGameData         = (void (*)(gentity_t*, int, int, gclient_t*, int))SV_LocateGameData;
    sv_gameImports.trap_DropClient             = SV_GameDropClient;
    sv_gameImports.trap_SendServerCommand      = SV_GameSendServerCommand;
    sv_gameImports.trap_SetConfigstring        = SV_SetConfigstring;
    sv_gameImports.trap_GetConfigstring        = SV_GetConfigstring;
    sv_gameImports.trap_GetUserinfo            = SV_GetUserinfo;
    sv_gameImports.trap_SetUserinfo            = SV_SetUserinfo;
    sv_gameImports.trap_GetServerinfo          = SV_GetServerinfo;

    // Collision and world
    sv_gameImports.trap_SetBrushModel          = (void (*)(gentity_t*, const char*))SV_SetBrushModel;
    sv_gameImports.trap_Trace                  = SV_GI_Trace;
    sv_gameImports.trap_TraceCapsule           = SV_GI_TraceCapsule;
    sv_gameImports.trap_PointContents          = SV_PointContents;
    sv_gameImports.trap_AdjustAreaPortalState  = (void (*)(gentity_t*, qboolean))SV_AdjustAreaPortalState;
    sv_gameImports.trap_LinkEntity             = (void (*)(gentity_t*))SV_LinkEntity;
    sv_gameImports.trap_UnlinkEntity           = (void (*)(gentity_t*))SV_UnlinkEntity;
    sv_gameImports.trap_EntitiesInBox          = SV_AreaEntities;
    sv_gameImports.trap_EntityContact          = SV_GI_EntityContact;

    // Bot client management
    sv_gameImports.trap_BotAllocateClient      = SV_BotAllocateClient;
    sv_gameImports.trap_BotFreeClient          = SV_BotFreeClient;
    sv_gameImports.trap_GetUsercmd             = SV_GetUsercmd;
    sv_gameImports.trap_GetEntityToken         = SV_GI_GetEntityToken;

    // Bot library
    sv_gameImports.trap_BotLibSetup            = SV_BotLibSetup;
    sv_gameImports.trap_BotLibShutdown         = SV_BotLibShutdown;
    sv_gameImports.trap_BotLibVarSet           = SV_GI_BotLibVarSet;
    sv_gameImports.trap_BotLibDefine           = SV_GI_BotLibDefine;
    sv_gameImports.trap_BotLibStartFrame       = SV_GI_BotLibStartFrame;
    sv_gameImports.trap_BotLibLoadMap          = SV_GI_BotLibLoadMap;
    sv_gameImports.trap_BotLibUpdateEntity     = SV_GI_BotLibUpdateEntity;
    sv_gameImports.trap_BotGetSnapshotEntity   = SV_BotGetSnapshotEntity;
    sv_gameImports.trap_BotUserCommand         = SV_GI_BotUserCommand;

    // AAS (base)
    sv_gameImports.trap_AAS_EntityInfo         = SV_GI_AAS_EntityInfo;
    sv_gameImports.trap_AAS_Initialized        = SV_GI_AAS_Initialized;
    sv_gameImports.trap_AAS_Time               = SV_GI_AAS_Time;
    sv_gameImports.trap_AAS_PointAreaNum       = SV_GI_AAS_PointAreaNum;
    sv_gameImports.trap_AAS_TraceAreas         = SV_GI_AAS_TraceAreas;

    // AAS (extended)
    sv_gameImports.trap_AAS_PresenceTypeBoundingBox = SV_GI_AAS_PresenceTypeBoundingBox;
    sv_gameImports.trap_AAS_PointReachabilityAreaIndex = SV_GI_AAS_PointReachabilityAreaIndex;
    sv_gameImports.trap_AAS_BBoxAreas          = SV_GI_AAS_BBoxAreas;
    sv_gameImports.trap_AAS_AreaInfo           = SV_GI_AAS_AreaInfo;
    sv_gameImports.trap_AAS_PointContents      = SV_GI_AAS_PointContents;
    sv_gameImports.trap_AAS_NextBSPEntity      = SV_GI_AAS_NextBSPEntity;
    sv_gameImports.trap_AAS_ValueForBSPEpairKey = SV_GI_AAS_ValueForBSPEpairKey;
    sv_gameImports.trap_AAS_VectorForBSPEpairKey = SV_GI_AAS_VectorForBSPEpairKey;
    sv_gameImports.trap_AAS_FloatForBSPEpairKey = SV_GI_AAS_FloatForBSPEpairKey;
    sv_gameImports.trap_AAS_IntForBSPEpairKey  = SV_GI_AAS_IntForBSPEpairKey;
    sv_gameImports.trap_AAS_AreaReachability   = SV_GI_AAS_AreaReachability;
    sv_gameImports.trap_AAS_AreaTravelTimeToGoalArea = SV_GI_AAS_AreaTravelTimeToGoalArea;
    sv_gameImports.trap_AAS_EnableRoutingArea  = SV_GI_AAS_EnableRoutingArea;
    sv_gameImports.trap_AAS_PredictRoute       = SV_GI_AAS_PredictRoute;
    sv_gameImports.trap_AAS_AlternativeRouteGoals = SV_GI_AAS_AlternativeRouteGoals;
    sv_gameImports.trap_AAS_Swimming           = SV_GI_AAS_Swimming;
    sv_gameImports.trap_AAS_PredictClientMovement = SV_GI_AAS_PredictClientMovement;

    // Elementary Actions
    sv_gameImports.trap_EA_Command             = SV_GI_EA_Command;
    sv_gameImports.trap_EA_Say                 = SV_GI_EA_Say;
    sv_gameImports.trap_EA_SayTeam             = SV_GI_EA_SayTeam;
    sv_gameImports.trap_EA_Action              = SV_GI_EA_Action;
    sv_gameImports.trap_EA_Gesture             = SV_GI_EA_Gesture;
    sv_gameImports.trap_EA_Talk                = SV_GI_EA_Talk;
    sv_gameImports.trap_EA_Attack              = SV_GI_EA_Attack;
    sv_gameImports.trap_EA_Use                 = SV_GI_EA_Use;
    sv_gameImports.trap_EA_Respawn             = SV_GI_EA_Respawn;
    sv_gameImports.trap_EA_MoveUp              = SV_GI_EA_MoveUp;
    sv_gameImports.trap_EA_MoveDown            = SV_GI_EA_MoveDown;
    sv_gameImports.trap_EA_MoveForward         = SV_GI_EA_MoveForward;
    sv_gameImports.trap_EA_MoveBack            = SV_GI_EA_MoveBack;
    sv_gameImports.trap_EA_MoveLeft            = SV_GI_EA_MoveLeft;
    sv_gameImports.trap_EA_MoveRight           = SV_GI_EA_MoveRight;
    sv_gameImports.trap_EA_Crouch              = SV_GI_EA_Crouch;
    sv_gameImports.trap_EA_SelectWeapon        = SV_GI_EA_SelectWeapon;
    sv_gameImports.trap_EA_Jump                = SV_GI_EA_Jump;
    sv_gameImports.trap_EA_DelayedJump         = SV_GI_EA_DelayedJump;
    sv_gameImports.trap_EA_Move                = SV_GI_EA_Move;
    sv_gameImports.trap_EA_View                = SV_GI_EA_View;
    sv_gameImports.trap_EA_EndRegular          = SV_GI_EA_EndRegular;
    sv_gameImports.trap_EA_GetInput            = SV_GI_EA_GetInput;
    sv_gameImports.trap_EA_ResetInput          = SV_GI_EA_ResetInput;

    // Bot Character
    sv_gameImports.trap_BotLoadCharacter       = SV_GI_BotLoadCharacter;
    sv_gameImports.trap_BotFreeCharacter       = SV_GI_BotFreeCharacter;
    sv_gameImports.trap_Characteristic_Float   = SV_GI_Characteristic_Float;
    sv_gameImports.trap_Characteristic_BFloat  = SV_GI_Characteristic_BFloat;
    sv_gameImports.trap_Characteristic_Integer = SV_GI_Characteristic_Integer;
    sv_gameImports.trap_Characteristic_BInteger = SV_GI_Characteristic_BInteger;
    sv_gameImports.trap_Characteristic_String  = SV_GI_Characteristic_String;

    // Bot Chat
    sv_gameImports.trap_BotAllocChatState      = SV_GI_BotAllocChatState;
    sv_gameImports.trap_BotFreeChatState       = SV_GI_BotFreeChatState;
    sv_gameImports.trap_BotQueueConsoleMessage = SV_GI_BotQueueConsoleMessage;
    sv_gameImports.trap_BotRemoveConsoleMessage = SV_GI_BotRemoveConsoleMessage;
    sv_gameImports.trap_BotNextConsoleMessage  = SV_GI_BotNextConsoleMessage;
    sv_gameImports.trap_BotNumConsoleMessages  = SV_GI_BotNumConsoleMessages;
    sv_gameImports.trap_BotInitialChat         = SV_GI_BotInitialChat;
    sv_gameImports.trap_BotNumInitialChats     = SV_GI_BotNumInitialChats;
    sv_gameImports.trap_BotReplyChat           = SV_GI_BotReplyChat;
    sv_gameImports.trap_BotChatLength          = SV_GI_BotChatLength;
    sv_gameImports.trap_BotEnterChat           = SV_GI_BotEnterChat;
    sv_gameImports.trap_BotGetChatMessage      = SV_GI_BotGetChatMessage;
    sv_gameImports.trap_StringContains         = SV_GI_StringContains;
    sv_gameImports.trap_BotFindMatch           = SV_GI_BotFindMatch;
    sv_gameImports.trap_BotMatchVariable       = SV_GI_BotMatchVariable;
    sv_gameImports.trap_UnifyWhiteSpaces       = SV_GI_UnifyWhiteSpaces;
    sv_gameImports.trap_BotReplaceSynonyms     = SV_GI_BotReplaceSynonyms;
    sv_gameImports.trap_BotSetChatGender       = SV_GI_BotSetChatGender;
    sv_gameImports.trap_BotSetChatName         = SV_GI_BotSetChatName;
    sv_gameImports.trap_BotLoadChatFile        = SV_GI_BotLoadChatFile;

    // Bot Goals
    sv_gameImports.trap_BotResetGoalState      = SV_GI_BotResetGoalState;
    sv_gameImports.trap_BotResetAvoidGoals     = SV_GI_BotResetAvoidGoals;
    sv_gameImports.trap_BotRemoveFromAvoidGoals = SV_GI_BotRemoveFromAvoidGoals;
    sv_gameImports.trap_BotPushGoal            = SV_GI_BotPushGoal;
    sv_gameImports.trap_BotPopGoal             = SV_GI_BotPopGoal;
    sv_gameImports.trap_BotEmptyGoalStack      = SV_GI_BotEmptyGoalStack;
    sv_gameImports.trap_BotDumpAvoidGoals      = SV_GI_BotDumpAvoidGoals;
    sv_gameImports.trap_BotDumpGoalStack       = SV_GI_BotDumpGoalStack;
    sv_gameImports.trap_BotGoalName            = SV_GI_BotGoalName;
    sv_gameImports.trap_BotGetTopGoal          = SV_GI_BotGetTopGoal;
    sv_gameImports.trap_BotGetSecondGoal       = SV_GI_BotGetSecondGoal;
    sv_gameImports.trap_BotChooseLTGItem       = SV_GI_BotChooseLTGItem;
    sv_gameImports.trap_BotChooseNBGItem       = SV_GI_BotChooseNBGItem;
    sv_gameImports.trap_BotTouchingGoal        = SV_GI_BotTouchingGoal;
    sv_gameImports.trap_BotItemGoalInVisButNotVisible = SV_GI_BotItemGoalInVisButNotVisible;
    sv_gameImports.trap_BotGetLevelItemGoal    = SV_GI_BotGetLevelItemGoal;
    sv_gameImports.trap_BotGetNextCampSpotGoal = SV_GI_BotGetNextCampSpotGoal;
    sv_gameImports.trap_BotGetMapLocationGoal  = SV_GI_BotGetMapLocationGoal;
    sv_gameImports.trap_BotAvoidGoalTime       = SV_GI_BotAvoidGoalTime;
    sv_gameImports.trap_BotSetAvoidGoalTime    = SV_GI_BotSetAvoidGoalTime;
    sv_gameImports.trap_BotInitLevelItems      = SV_GI_BotInitLevelItems;
    sv_gameImports.trap_BotUpdateEntityItems   = SV_GI_BotUpdateEntityItems;
    sv_gameImports.trap_BotLoadItemWeights     = SV_GI_BotLoadItemWeights;
    sv_gameImports.trap_BotFreeItemWeights     = SV_GI_BotFreeItemWeights;
    sv_gameImports.trap_BotInterbreedGoalFuzzyLogic = SV_GI_BotInterbreedGoalFuzzyLogic;
    sv_gameImports.trap_BotSaveGoalFuzzyLogic  = SV_GI_BotSaveGoalFuzzyLogic;
    sv_gameImports.trap_BotMutateGoalFuzzyLogic = SV_GI_BotMutateGoalFuzzyLogic;
    sv_gameImports.trap_BotAllocGoalState      = SV_GI_BotAllocGoalState;
    sv_gameImports.trap_BotFreeGoalState       = SV_GI_BotFreeGoalState;

    // Bot Movement
    sv_gameImports.trap_BotResetMoveState      = SV_GI_BotResetMoveState;
    sv_gameImports.trap_BotMoveToGoal          = SV_GI_BotMoveToGoal;
    sv_gameImports.trap_BotMoveInDirection     = SV_GI_BotMoveInDirection;
    sv_gameImports.trap_BotResetAvoidReach     = SV_GI_BotResetAvoidReach;
    sv_gameImports.trap_BotResetLastAvoidReach = SV_GI_BotResetLastAvoidReach;
    sv_gameImports.trap_BotReachabilityArea    = SV_GI_BotReachabilityArea;
    sv_gameImports.trap_BotMovementViewTarget  = SV_GI_BotMovementViewTarget;
    sv_gameImports.trap_BotPredictVisiblePosition = SV_GI_BotPredictVisiblePosition;
    sv_gameImports.trap_BotAllocMoveState      = SV_GI_BotAllocMoveState;
    sv_gameImports.trap_BotFreeMoveState       = SV_GI_BotFreeMoveState;
    sv_gameImports.trap_BotInitMoveState       = SV_GI_BotInitMoveState;
    sv_gameImports.trap_BotAddAvoidSpot        = SV_GI_BotAddAvoidSpot;

    // Bot Weapons
    sv_gameImports.trap_BotChooseBestFightWeapon = SV_GI_BotChooseBestFightWeapon;
    sv_gameImports.trap_BotGetWeaponInfo       = SV_GI_BotGetWeaponInfo;
    sv_gameImports.trap_BotLoadWeaponWeights   = SV_GI_BotLoadWeaponWeights;
    sv_gameImports.trap_BotAllocWeaponState    = SV_GI_BotAllocWeaponState;
    sv_gameImports.trap_BotFreeWeaponState     = SV_GI_BotFreeWeaponState;
    sv_gameImports.trap_BotResetWeaponState    = SV_GI_BotResetWeaponState;

    // Misc Bot
    sv_gameImports.trap_GeneticParentsAndChildSelection = SV_GI_GeneticParentsAndChildSelection;
    sv_gameImports.trap_BotGetServerCommand    = SV_GI_BotGetServerCommand;
    sv_gameImports.trap_BotLibVarGet           = SV_GI_BotLibVarGet;
    sv_gameImports.trap_BotLibTest             = SV_GI_BotLibTest;
    sv_gameImports.trap_PC_LoadSource          = SV_GI_PC_LoadSource;
    sv_gameImports.trap_PC_FreeSource          = SV_GI_PC_FreeSource;
    sv_gameImports.trap_PC_ReadToken           = SV_GI_PC_ReadToken;
    sv_gameImports.trap_PC_SourceFileAndLine   = SV_GI_PC_SourceFileAndLine;

    // QL extensions
    sv_gameImports.trap_GetSteamID             = SV_GI_GetSteamID;
    sv_gameImports.trap_GetServerSteamID       = SV_GI_GetServerSteamID;
    sv_gameImports.trap_IncrementSteamStat     = SV_GI_IncrementSteamStat;
    sv_gameImports.trap_GetSteamStat           = SV_GI_GetSteamStat;
    sv_gameImports.trap_VerifySteamAuth        = SV_GI_VerifySteamAuth;
}

//==============================================
//
// [QL] Game export call functions - typed wrappers for vmMainTable
//

void SV_GameShutdown(int restart) {
    if (sv_vmMainTable[GAME_SHUTDOWN])
        ((void (*)(int))sv_vmMainTable[GAME_SHUTDOWN])(restart);
}

void SV_GameRunFrame(int serverTime) {
    if (sv_vmMainTable[GAME_RUN_FRAME])
        ((void (*)(int))sv_vmMainTable[GAME_RUN_FRAME])(serverTime);
}

void SV_GameRegisterCvars(void) {
    if (sv_vmMainTable[GAME_REGISTER_CVARS])
        ((void (*)(void))sv_vmMainTable[GAME_REGISTER_CVARS])();
}

void SV_GameInit(int levelTime, int randomSeed, int restart) {
    if (sv_vmMainTable[GAME_INIT])
        ((void (*)(int, int, int))sv_vmMainTable[GAME_INIT])(levelTime, randomSeed, restart);
}

qboolean SV_GameConsoleCommand(void) {
    if (sv_vmMainTable[GAME_CONSOLE_COMMAND])
        return ((int (*)(void))sv_vmMainTable[GAME_CONSOLE_COMMAND])();
    return qfalse;
}

void SV_GameClientUserinfoChanged(int clientNum) {
    if (sv_vmMainTable[GAME_CLIENT_USERINFO_CHANGED])
        ((void (*)(int))sv_vmMainTable[GAME_CLIENT_USERINFO_CHANGED])(clientNum);
}

void SV_GameClientThink(int clientNum) {
    if (sv_vmMainTable[GAME_CLIENT_THINK])
        ((void (*)(int))sv_vmMainTable[GAME_CLIENT_THINK])(clientNum);
}

void SV_GameClientDisconnect(int clientNum) {
    if (sv_vmMainTable[GAME_CLIENT_DISCONNECT])
        ((void (*)(int))sv_vmMainTable[GAME_CLIENT_DISCONNECT])(clientNum);
}

const char *SV_GameClientConnect(int clientNum, qboolean firstTime, qboolean isBot) {
    if (sv_vmMainTable[GAME_CLIENT_CONNECT])
        return ((const char *(*)(int, qboolean, qboolean))sv_vmMainTable[GAME_CLIENT_CONNECT])(clientNum, firstTime, isBot);
    return "Game module not loaded";
}

void SV_GameClientCommand(int clientNum) {
    if (sv_vmMainTable[GAME_CLIENT_COMMAND])
        ((void (*)(int))sv_vmMainTable[GAME_CLIENT_COMMAND])(clientNum);
}

//==============================================

/*
===============
SV_ShutdownGameProgs

Called every time a map changes
===============
*/
void SV_ShutdownGameProgs(void) {
    if (!sv_gameLibHandle) {
        return;
    }
    SV_GameShutdown(qfalse);
    Sys_UnloadLibrary(sv_gameLibHandle);
    sv_gameLibHandle = NULL;
    memset(sv_vmMainTable, 0, sizeof(sv_vmMainTable));
}

/*
==================
SV_InitGameVM

Called for both a full init and a restart
==================
*/
static void SV_InitGameVM(qboolean restart) {
    int i;

    // start the entity parsing at the beginning
    sv.entityParsePoint = CM_EntityString();

    // clear all gentity pointers that might still be set from a previous level
    for (i = 0; i < sv_maxclients->integer; i++) {
        svs.clients[i].gentity = NULL;
    }

    // use the current msec count for a random seed
    SV_GameInit(sv.time, Com_Milliseconds(), restart);
}

/*
===================
SV_RestartGameProgs

Called on a map_restart, but not on a normal map change
===================
*/
void SV_RestartGameProgs(void) {
    if (!sv_gameLibHandle) {
        return;
    }
    SV_GameShutdown(qtrue);

    // Unload and reload the DLL
    Sys_UnloadLibrary(sv_gameLibHandle);
    sv_gameLibHandle = NULL;
    memset(sv_vmMainTable, 0, sizeof(sv_vmMainTable));

    // Reload
    SV_InitGameProgs();
    SV_InitGameVM(qtrue);
}

/*
===============
SV_InitGameProgs

Called on a normal map change, not on a map_restart
===============
*/
void SV_InitGameProgs(void) {
    cvar_t *var;
    extern int bot_enable;
    typedef void (*dllEntryProc)(void **vmMainTable, gameImport_t *imports, int *apiVersion);
    dllEntryProc dllEntry;
    char dllPath[MAX_OSPATH];

    var = Cvar_Get("bot_enable", "1", CVAR_LATCH);
    bot_enable = var ? var->integer : 0;

    // Extract qagame DLL from bin.pk3 (same mechanism as cgame/ui via VM_Create)
    if (FS_ExtractGamecode("qagame", dllPath) && dllPath[0] != '\0') {
        Com_Printf("Loading game DLL from pk3: %s\n", dllPath);
        sv_gameLibHandle = Sys_LoadLibrary(dllPath);
    }

#ifdef _DEBUG
    if (!sv_gameLibHandle) {
        Com_sprintf(dllPath, sizeof(dllPath), "%s/qagame" ARCH_STRING DLL_EXT, FS_GetCurrentGameDir());
        Com_Printf("Loading game DLL: %s\n", dllPath);
        sv_gameLibHandle = Sys_LoadLibrary(dllPath);
    }
    if (!sv_gameLibHandle) {
        const char *basePath = Cvar_VariableString("fs_basepath");
        Com_sprintf(dllPath, sizeof(dllPath), "%s/%s/qagame" ARCH_STRING DLL_EXT, basePath, FS_GetCurrentGameDir());
        Com_Printf("Retrying: %s\n", dllPath);
        sv_gameLibHandle = Sys_LoadLibrary(dllPath);
    }
#endif
    if (!sv_gameLibHandle) {
        Com_Error(ERR_FATAL, "Failed to load game DLL");
    }

    dllEntry = (dllEntryProc)Sys_LoadFunction(sv_gameLibHandle, "dllEntry");
    if (!dllEntry) {
        Sys_UnloadLibrary(sv_gameLibHandle);
        sv_gameLibHandle = NULL;
        Com_Error(ERR_FATAL, "Game DLL missing dllEntry export");
    }

    // Populate the import table and call dllEntry
    SV_PopulateGameImports();
    memset(sv_vmMainTable, 0, sizeof(sv_vmMainTable));
    sv_apiVersion = 0;
    dllEntry(sv_vmMainTable, &sv_gameImports, &sv_apiVersion);

    Com_Printf("Game DLL loaded, API version %d\n", sv_apiVersion);

    SV_InitGameVM(qfalse);
}

/*
====================
SV_GameCommand

See if the current console command is claimed by the game
====================
*/
qboolean SV_GameCommand(void) {
    if (sv.state != SS_GAME) {
        return qfalse;
    }

    return SV_GameConsoleCommand();
}
