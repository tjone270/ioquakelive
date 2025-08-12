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

// g_public.h -- game module information visible to server

#define GAME_API_VERSION 10  // [QL] was 8 in Q3

// entity->svFlags
// the server does not know how to interpret most of the values
// in entityStates (level eType), so the game must explicitly flag
// special server behaviors
#define SVF_NOCLIENT 0x00000001  // don't send entity to clients, even if it has effects

// TTimo
// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=551
#define SVF_CLIENTMASK 0x00000002

#define SVF_BOT 0x00000008                 // set if the entity is a bot
#define SVF_BROADCAST 0x00000020           // send to all connected clients
#define SVF_PORTAL 0x00000040              // merge a second pvs at origin2 into snapshots
#define SVF_USE_CURRENT_ORIGIN 0x00000080  // entity->r.currentOrigin instead of entity->s.origin
                                           // for link position (missiles and movers)
#define SVF_SINGLECLIENT 0x00000100        // only send to a single client (entityShared_t->singleClient)
#define SVF_NOSERVERINFO 0x00000200        // don't send CS_SERVERINFO updates to this client
                                           // so that it can be updated for ping tools without
                                           // lagging clients
#define SVF_CAPSULE 0x00000400             // use capsule for collision detection instead of bbox
#define SVF_NOTSINGLECLIENT 0x00000800     // send entity to everyone but one client
                                           // (entityShared_t->singleClient)

//===============================================================

typedef struct {
    entityState_t unused;  // apparently this field was put here accidentally
                           //  (and is kept only for compatibility, as a struct pad)

    qboolean linked;  // qfalse if not in any good cluster
    int linkcount;

    int svFlags;  // SVF_NOCLIENT, SVF_BROADCAST, etc

    // only send to this client when SVF_SINGLECLIENT is set
    // if SVF_CLIENTMASK is set, use bitmask for clients to send to (maxclients must be <= 32, up to the mod to enforce this)
    int singleClient;

    qboolean bmodel;  // if false, assume an explicit mins / maxs bounding box
                      // only set by trap_SetBrushModel
    vec3_t mins, maxs;
    int contents;  // CONTENTS_TRIGGER, CONTENTS_SOLID, CONTENTS_BODY, etc
                   // a non-solid entity should set to 0

    vec3_t absmin, absmax;  // derived from mins/maxs and origin + rotation

    // currentOrigin will be used for all collision detection and world linking.
    // it will not necessarily be the same as the trajectory evaluation for the current
    // time, because each entity must be moved one at a time after time is advanced
    // to avoid simultanious collision issues
    vec3_t currentOrigin;
    vec3_t currentAngles;

    // when a trace call is made and passEntityNum != ENTITYNUM_NONE,
    // an ent will be excluded from testing if:
    // ent->s.number == passEntityNum	(don't interact with self)
    // ent->r.ownerNum == passEntityNum	(don't interact with your own missiles)
    // entity[ent->r.ownerNum].r.ownerNum == passEntityNum	(don't interact with other missiles from owner)
    int ownerNum;
} entityShared_t;

// the server looks at a sharedEntity, which is the start of the game's gentity_t structure
typedef struct {
    entityState_t s;   // communicated by server to clients
    entityShared_t r;  // shared by both the server system and game
} sharedEntity_t;

// [QL] Forward declarations needed by gameImport_t function pointers
typedef struct gentity_s gentity_t;
typedef struct gclient_s gclient_t;

//===============================================================

// [QL] The engine passes a struct of function pointers instead of Q3's integer syscall IDs.
// Each slot is an 8-byte function pointer (x86_64). Gaps exist where traps were removed/added.
// Offsets verified from decompiled binary.
typedef struct {
    /* --- Core Engine Traps --- */
    /* 0x00  */ void    (*trap_SendConsoleCommand)(const char *text);
    /* 0x08  */ void    *_unused_01;
    /* 0x10  */ void    (*trap_Printf)(const char *fmt, ...);
    /* 0x18  */ void    *_unused_03;
    /* 0x20  */ int     (*trap_FS_Write)(const void *buf, int len, fileHandle_t f);
    /* 0x28  */ void    *_unused_05;
    /* 0x30  */ int     (*trap_FS_Read)(void *buf, int len, fileHandle_t f);
    /* 0x38  */ void    *_unused_07;
    /* 0x40  */ int     (*trap_FS_FOpenFile)(const char *name, fileHandle_t *f, fsMode_t mode);
    /* 0x48  */ void    (*trap_FS_FCloseFile)(fileHandle_t f);
    /* 0x50  */ void    (*trap_Error)(const char *fmt, ...);
    /* 0x58  */ float   (*trap_Cvar_VariableValue)(const char *name);
    /* 0x60  */ void    (*trap_Cvar_Update)(vmCvar_t *cv);
    /* 0x68  */ void    (*trap_Cvar_VariableStringBuffer)(const char *name, char *buf, int size);
    /* 0x70  */ void    *_unused_0e;
    /* 0x78  */ void    (*trap_Cvar_Set)(const char *name, const char *value);
    /* 0x80  */ void    *_unused_10;
    /* 0x88  */ void    (*trap_Cvar_Register)(vmCvar_t *cv, const char *name, const char *defaultValue, int flags);
    /* 0x90  */ void    (*trap_Argv)(int n, char *buf, int size);
    /* 0x98  */ void    *_unused_13;
    /* 0xA0  */ int     (*trap_Argc)(void);
    /* 0xA8  */ void    *_unused_15;
    /* 0xB0  */ void    (*trap_LocateGameData)(gentity_t *gEnts, int numGEntities, int sizeofGEntity, gclient_t *clients, int sizeofGClient);
    /* 0xB8  */ void    (*trap_DropClient)(int clientNum, const char *reason);
    /* 0xC0  */ void    (*trap_SendServerCommand)(int clientNum, const char *text);
    /* 0xC8  */ void    (*trap_SetConfigstring)(int index, const char *value);
    /* 0xD0  */ void    (*trap_GetConfigstring)(int index, char *buf, int size);
    /* 0xD8  */ void    *_unused_1b;
    /* 0xE0  */ void    (*trap_GetUserinfo)(int clientNum, char *buf, int size);
    /* 0xE8  */ void    (*trap_SetUserinfo)(int clientNum, const char *info);
    /* 0xF0  */ void    (*trap_GetServerinfo)(char *buf, int size);
    /* 0xF8  */ void    (*trap_SetBrushModel)(gentity_t *ent, const char *model);
    /* 0x100 */ void    (*trap_Trace)(trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentMask);
    /* 0x108 */ void    (*trap_TraceCapsule)(trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentMask);
    /* 0x110 */ int     (*trap_PointContents)(const vec3_t point, int passEntityNum);
    /* 0x118 */ void    *_unused_23;
    /* 0x120 */ void    *_unused_24;
    /* 0x128 */ void    (*trap_AdjustAreaPortalState)(gentity_t *ent, qboolean open);
    /* 0x130 */ void    *_unused_26;
    /* 0x138 */ void    (*trap_LinkEntity)(gentity_t *ent);
    /* 0x140 */ void    (*trap_UnlinkEntity)(gentity_t *ent);
    /* 0x148 */ int     (*trap_EntitiesInBox)(const vec3_t mins, const vec3_t maxs, int *list, int maxcount);
    /* 0x150 */ qboolean(*trap_EntityContact)(const vec3_t mins, const vec3_t maxs, const gentity_t *ent);
    /* 0x158 */ int     (*trap_BotAllocateClient)(void);
    /* 0x160 */ void    (*trap_BotFreeClient)(int clientNum);
    /* 0x168 */ void    (*trap_GetUsercmd)(int clientNum, usercmd_t *cmd);
    /* 0x170 */ qboolean(*trap_GetEntityToken)(char *buf, int size);
    /* 0x178 */ void    *_unused_2f;
    /* 0x180 */ void    *_unused_30;
    /* 0x188 */ int     (*trap_BotLibSetup)(void);
    /* 0x190 */ int     (*trap_BotLibShutdown)(void);
    /* 0x198 */ int     (*trap_BotLibVarSet)(const char *name, const char *value);
    /* 0x1A0 */ void    *_unused_34;
    /* 0x1A8 */ int     (*trap_BotLibDefine)(const char *symbol);
    /* 0x1B0 */ int     (*trap_BotLibStartFrame)(float time);
    /* 0x1B8 */ int     (*trap_BotLibLoadMap)(const char *mapname);
    /* 0x1C0 */ int     (*trap_BotLibUpdateEntity)(int entNum, void *entityInfo);
    /* 0x1C8 */ void    *_unused_39;
    /* 0x1D0 */ int     (*trap_BotGetSnapshotEntity)(int clientNum, int sequence);
    /* 0x1D8 */ void    *_unused_3b;
    /* 0x1E0 */ void    (*trap_BotUserCommand)(int clientNum, usercmd_t *cmd);
    /* 0x1E8 */ void    *_unused_3d;
    /* 0x1F0 */ void    *_unused_3e;
    /* 0x1F8 */ void    (*trap_AAS_EntityInfo)(int entNum, void *info);
    /* 0x200 */ qboolean(*trap_AAS_Initialized)(void);
    /* 0x208 */ void    *_unused_41;
    /* 0x210 */ float   (*trap_AAS_Time)(void);
    /* 0x218 */ int     (*trap_AAS_PointAreaNum)(const vec3_t point);
    /* 0x220 */ void    *_unused_44;
    /* 0x228 */ int     (*trap_AAS_TraceAreas)(const vec3_t start, const vec3_t end, int *areas, void *points, int maxareas);

    /* --- AAS Extended --- */
    void    (*trap_AAS_PresenceTypeBoundingBox)(int presencetype, vec3_t mins, vec3_t maxs);
    int     (*trap_AAS_PointReachabilityAreaIndex)(vec3_t point);
    int     (*trap_AAS_BBoxAreas)(vec3_t absmins, vec3_t absmaxs, int *areas, int maxareas);
    int     (*trap_AAS_AreaInfo)(int areanum, void *info);
    int     (*trap_AAS_PointContents)(vec3_t point);
    int     (*trap_AAS_NextBSPEntity)(int ent);
    int     (*trap_AAS_ValueForBSPEpairKey)(int ent, char *key, char *value, int size);
    int     (*trap_AAS_VectorForBSPEpairKey)(int ent, char *key, vec3_t v);
    int     (*trap_AAS_FloatForBSPEpairKey)(int ent, char *key, float *value);
    int     (*trap_AAS_IntForBSPEpairKey)(int ent, char *key, int *value);
    int     (*trap_AAS_AreaReachability)(int areanum);
    int     (*trap_AAS_AreaTravelTimeToGoalArea)(int areanum, vec3_t origin, int goalareanum, int travelflags);
    int     (*trap_AAS_EnableRoutingArea)(int areanum, int enable);
    int     (*trap_AAS_PredictRoute)(void *route, int areanum, vec3_t origin, int goalareanum, int travelflags, int maxareas, int maxtime, int stopevent, int stopcontents, int stoptfl, int stopareanum);
    int     (*trap_AAS_AlternativeRouteGoals)(vec3_t start, int startareanum, vec3_t goal, int goalareanum, int travelflags, void *altroutegoals, int maxaltroutegoals, int type);
    int     (*trap_AAS_Swimming)(vec3_t origin);
    int     (*trap_AAS_PredictClientMovement)(void *move, int entnum, vec3_t origin, int presencetype, int onground, vec3_t velocity, vec3_t cmdmove, int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum, int visualize);

    /* --- Elementary Actions --- */
    void    (*trap_EA_Command)(int client, char *command);
    void    (*trap_EA_Say)(int client, char *str);
    void    (*trap_EA_SayTeam)(int client, char *str);
    void    (*trap_EA_Action)(int client, int action);
    void    (*trap_EA_Gesture)(int client);
    void    (*trap_EA_Talk)(int client);
    void    (*trap_EA_Attack)(int client);
    void    (*trap_EA_Use)(int client);
    void    (*trap_EA_Respawn)(int client);
    void    (*trap_EA_MoveUp)(int client);
    void    (*trap_EA_MoveDown)(int client);
    void    (*trap_EA_MoveForward)(int client);
    void    (*trap_EA_MoveBack)(int client);
    void    (*trap_EA_MoveLeft)(int client);
    void    (*trap_EA_MoveRight)(int client);
    void    (*trap_EA_Crouch)(int client);
    void    (*trap_EA_SelectWeapon)(int client, int weapon);
    void    (*trap_EA_Jump)(int client);
    void    (*trap_EA_DelayedJump)(int client);
    void    (*trap_EA_Move)(int client, vec3_t dir, float speed);
    void    (*trap_EA_View)(int client, vec3_t viewangles);
    void    (*trap_EA_EndRegular)(int client, float thinktime);
    void    (*trap_EA_GetInput)(int client, float thinktime, void *input);
    void    (*trap_EA_ResetInput)(int client);

    /* --- Bot Character --- */
    int     (*trap_BotLoadCharacter)(char *charfile, float skill);
    void    (*trap_BotFreeCharacter)(int character);
    float   (*trap_Characteristic_Float)(int character, int index);
    float   (*trap_Characteristic_BFloat)(int character, int index, float min, float max);
    int     (*trap_Characteristic_Integer)(int character, int index);
    int     (*trap_Characteristic_BInteger)(int character, int index, int min, int max);
    void    (*trap_Characteristic_String)(int character, int index, char *buf, int size);

    /* --- Bot Chat --- */
    int     (*trap_BotAllocChatState)(void);
    void    (*trap_BotFreeChatState)(int handle);
    void    (*trap_BotQueueConsoleMessage)(int chatstate, int type, char *message);
    void    (*trap_BotRemoveConsoleMessage)(int chatstate, int handle);
    int     (*trap_BotNextConsoleMessage)(int chatstate, void *cm);
    int     (*trap_BotNumConsoleMessages)(int chatstate);
    void    (*trap_BotInitialChat)(int chatstate, char *type, int mcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7);
    int     (*trap_BotNumInitialChats)(int chatstate, char *type);
    int     (*trap_BotReplyChat)(int chatstate, char *message, int mcontext, int vcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7);
    int     (*trap_BotChatLength)(int chatstate);
    void    (*trap_BotEnterChat)(int chatstate, int client, int sendto);
    void    (*trap_BotGetChatMessage)(int chatstate, char *buf, int size);
    int     (*trap_StringContains)(char *str1, char *str2, int casesensitive);
    int     (*trap_BotFindMatch)(char *str, void *match, unsigned long context);
    void    (*trap_BotMatchVariable)(void *match, int variable, char *buf, int size);
    void    (*trap_UnifyWhiteSpaces)(char *string);
    void    (*trap_BotReplaceSynonyms)(char *string, unsigned long context);
    void    (*trap_BotSetChatGender)(int chatstate, int gender);
    void    (*trap_BotSetChatName)(int chatstate, char *name, int client);
    int     (*trap_BotLoadChatFile)(int chatstate, char *chatfile, char *chatname);

    /* --- Bot Goals --- */
    void    (*trap_BotResetGoalState)(int goalstate);
    void    (*trap_BotResetAvoidGoals)(int goalstate);
    void    (*trap_BotRemoveFromAvoidGoals)(int goalstate, int number);
    void    (*trap_BotPushGoal)(int goalstate, void *goal);
    void    (*trap_BotPopGoal)(int goalstate);
    void    (*trap_BotEmptyGoalStack)(int goalstate);
    void    (*trap_BotDumpAvoidGoals)(int goalstate);
    void    (*trap_BotDumpGoalStack)(int goalstate);
    void    (*trap_BotGoalName)(int number, char *name, int size);
    int     (*trap_BotGetTopGoal)(int goalstate, void *goal);
    int     (*trap_BotGetSecondGoal)(int goalstate, void *goal);
    int     (*trap_BotChooseLTGItem)(int goalstate, vec3_t origin, int *inventory, int travelflags);
    int     (*trap_BotChooseNBGItem)(int goalstate, vec3_t origin, int *inventory, int travelflags, void *ltg, float maxtime);
    int     (*trap_BotTouchingGoal)(vec3_t origin, void *goal);
    int     (*trap_BotItemGoalInVisButNotVisible)(int viewer, vec3_t eye, vec3_t viewangles, void *goal);
    int     (*trap_BotGetLevelItemGoal)(int index, char *classname, void *goal);
    int     (*trap_BotGetNextCampSpotGoal)(int num, void *goal);
    int     (*trap_BotGetMapLocationGoal)(char *name, void *goal);
    float   (*trap_BotAvoidGoalTime)(int goalstate, int number);
    void    (*trap_BotSetAvoidGoalTime)(int goalstate, int number, float avoidtime);
    void    (*trap_BotInitLevelItems)(void);
    void    (*trap_BotUpdateEntityItems)(void);
    int     (*trap_BotLoadItemWeights)(int goalstate, char *filename);
    void    (*trap_BotFreeItemWeights)(int goalstate);
    void    (*trap_BotInterbreedGoalFuzzyLogic)(int parent1, int parent2, int child);
    void    (*trap_BotSaveGoalFuzzyLogic)(int goalstate, char *filename);
    void    (*trap_BotMutateGoalFuzzyLogic)(int goalstate, float range);
    int     (*trap_BotAllocGoalState)(int client);
    void    (*trap_BotFreeGoalState)(int handle);

    /* --- Bot Movement --- */
    void    (*trap_BotResetMoveState)(int movestate);
    void    (*trap_BotMoveToGoal)(void *result, int movestate, void *goal, int travelflags);
    int     (*trap_BotMoveInDirection)(int movestate, vec3_t dir, float speed, int type);
    void    (*trap_BotResetAvoidReach)(int movestate);
    void    (*trap_BotResetLastAvoidReach)(int movestate);
    int     (*trap_BotReachabilityArea)(vec3_t origin, int testground);
    int     (*trap_BotMovementViewTarget)(int movestate, void *goal, int travelflags, float lookahead, vec3_t target);
    int     (*trap_BotPredictVisiblePosition)(vec3_t origin, int areanum, void *goal, int travelflags, vec3_t target);
    int     (*trap_BotAllocMoveState)(void);
    void    (*trap_BotFreeMoveState)(int handle);
    void    (*trap_BotInitMoveState)(int handle, void *initmove);
    void    (*trap_BotAddAvoidSpot)(int movestate, vec3_t origin, float radius, int type);

    /* --- Bot Weapons --- */
    int     (*trap_BotChooseBestFightWeapon)(int weaponstate, int *inventory);
    void    (*trap_BotGetWeaponInfo)(int weaponstate, int weapon, void *weaponinfo);
    int     (*trap_BotLoadWeaponWeights)(int weaponstate, char *filename);
    int     (*trap_BotAllocWeaponState)(void);
    void    (*trap_BotFreeWeaponState)(int weaponstate);
    void    (*trap_BotResetWeaponState)(int weaponstate);

    /* --- Misc Bot/Engine --- */
    int     (*trap_GeneticParentsAndChildSelection)(int numranks, float *ranks, int *parent1, int *parent2, int *child);
    int     (*trap_BotGetServerCommand)(int clientNum, char *message, int size);
    int     (*trap_BotLibVarGet)(char *var_name, char *value, int size);
    int     (*trap_BotLibTest)(int parm0, char *parm1, vec3_t parm2, vec3_t parm3);
    int     (*trap_PC_LoadSource)(const char *filename);
    int     (*trap_PC_FreeSource)(int handle);
    int     (*trap_PC_ReadToken)(int handle, pc_token_t *pc_token);
    int     (*trap_PC_SourceFileAndLine)(int handle, char *filename, int *line);

    /* --- Reserved --- */
    void    *_reserved[7];

    /* --- Quake Live Extensions --- */
    /* 0x640 */ uint64_t(*trap_GetSteamID)(int clientNum);
    /* 0x648 */ void    *_unused_c9;
    /* 0x650 */ void    (*trap_GetServerSteamID)(char *buf, int size);
    /* 0x658 */ void    (*trap_IncrementSteamStat)(int clientNum, int statID);
    /* 0x660 */ int     (*trap_GetSteamStat)(int clientNum, int statID);
    /* 0x668 */ qboolean(*trap_VerifySteamAuth)(int clientNum);

    /* 0x670 */ /* end - total 206 slots × 8 = 1648 bytes */
} gameImport_t;

// [QL] vmMain export table - completely reordered vs Q3.
// QL uses a function pointer table instead of Q3's vmMain dispatcher.
// QL drops GAME_CLIENT_BEGIN and BOTAI_START_FRAME entirely.
typedef enum {
    GAME_SHUTDOWN               = 0,    // G_ShutdownGame()
    GAME_RUN_FRAME              = 1,    // G_RunFrame()
    GAME_REGISTER_CVARS         = 2,    // [QL] G_RegisterCvars()
    GAME_INIT                   = 3,    // G_InitGame()
    GAME_CONSOLE_COMMAND        = 4,    // ConsoleCommand()
    GAME_CLIENT_USERINFO_CHANGED = 5,   // ClientUserinfoChanged()
    GAME_CLIENT_THINK           = 6,    // ClientThink()
    GAME_CLIENT_DISCONNECT      = 7,    // ClientDisconnect()
    GAME_CLIENT_CONNECT         = 8,    // ClientConnect()
    GAME_CLIENT_COMMAND         = 9,    // ClientCommand()
} gameExport_t;
