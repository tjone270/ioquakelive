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
// g_syscalls.c -- [QL] trap wrappers using function pointer table
//
// In Quake Live, the engine passes a gameImport_t struct of function pointers
// via dllEntry, replacing Q3's integer syscall dispatch.

#include "g_local.h"

// The global imports pointer is set by dllEntry in g_main.c
// extern gameImport_t *imports;

// ============================================================
// Core engine traps
// ============================================================

// Q3 compatibility: trap_Print takes a simple string
void trap_Print(const char* text) {
    imports->trap_Printf(text);
}

void trap_Printf(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    imports->trap_Printf(buf);
}

void trap_Error(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    imports->trap_Error(buf);
    exit(1);  // shut up GCC warning, trap_Error doesn't return
}

int trap_Argc(void) {
    return imports->trap_Argc();
}

void trap_Argv(int n, char* buffer, int bufferLength) {
    imports->trap_Argv(n, buffer, bufferLength);
}

int trap_FS_FOpenFile(const char* qpath, fileHandle_t* f, fsMode_t mode) {
    return imports->trap_FS_FOpenFile(qpath, f, mode);
}

void trap_FS_Read(void* buffer, int len, fileHandle_t f) {
    imports->trap_FS_Read(buffer, len, f);
}

void trap_FS_Write(const void* buffer, int len, fileHandle_t f) {
    imports->trap_FS_Write(buffer, len, f);
}

void trap_FS_FCloseFile(fileHandle_t f) {
    imports->trap_FS_FCloseFile(f);
}

void trap_SendConsoleCommand(int exec_when, const char* text) {
    // [QL] QL's trap takes only the text, exec_when is implicit
    (void)exec_when;
    imports->trap_SendConsoleCommand(text);
}

// ============================================================
// Cvar traps
// ============================================================

void trap_Cvar_Register(vmCvar_t* cvar, const char* var_name, const char* value, int flags) {
    imports->trap_Cvar_Register(cvar, var_name, value, flags);
}

void trap_Cvar_Update(vmCvar_t* cvar) {
    imports->trap_Cvar_Update(cvar);
}

void trap_Cvar_Set(const char* var_name, const char* value) {
    imports->trap_Cvar_Set(var_name, value);
}

float trap_Cvar_VariableValue(const char* var_name) {
    return imports->trap_Cvar_VariableValue(var_name);
}

void trap_Cvar_VariableStringBuffer(const char* var_name, char* buffer, int bufsize) {
    imports->trap_Cvar_VariableStringBuffer(var_name, buffer, bufsize);
}

// ============================================================
// Server traps
// ============================================================

void trap_LocateGameData(gentity_t* gEnts, int numGEntities, int sizeofGEntity_t, playerState_t* clients, int sizeofGClient) {
    imports->trap_LocateGameData(gEnts, numGEntities, sizeofGEntity_t, (gclient_t*)clients, sizeofGClient);
}

void trap_DropClient(int clientNum, const char* reason) {
    imports->trap_DropClient(clientNum, reason);
}

void trap_SendServerCommand(int clientNum, const char* text) {
    imports->trap_SendServerCommand(clientNum, text);
}

void trap_SetConfigstring(int num, const char* string) {
    imports->trap_SetConfigstring(num, string);
}

void trap_GetConfigstring(int num, char* buffer, int bufferSize) {
    imports->trap_GetConfigstring(num, buffer, bufferSize);
}

void trap_GetUserinfo(int num, char* buffer, int bufferSize) {
    imports->trap_GetUserinfo(num, buffer, bufferSize);
}

void trap_SetUserinfo(int num, const char* buffer) {
    imports->trap_SetUserinfo(num, buffer);
}

void trap_GetServerinfo(char* buffer, int bufferSize) {
    imports->trap_GetServerinfo(buffer, bufferSize);
}

void trap_SetBrushModel(gentity_t* ent, const char* name) {
    imports->trap_SetBrushModel(ent, name);
}

void trap_GetUsercmd(int clientNum, usercmd_t* cmd) {
    imports->trap_GetUsercmd(clientNum, cmd);
}

// ============================================================
// Collision traps
// ============================================================

void trap_Trace(trace_t* results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask) {
    imports->trap_Trace(results, start, mins, maxs, end, passEntityNum, contentmask);
}

void trap_TraceCapsule(trace_t* results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask) {
    imports->trap_TraceCapsule(results, start, mins, maxs, end, passEntityNum, contentmask);
}

int trap_PointContents(const vec3_t point, int passEntityNum) {
    return imports->trap_PointContents(point, passEntityNum);
}

// ============================================================
// Entity traps
// ============================================================

void trap_AdjustAreaPortalState(gentity_t* ent, qboolean open) {
    imports->trap_AdjustAreaPortalState(ent, open);
}

void trap_LinkEntity(gentity_t* ent) {
    imports->trap_LinkEntity(ent);
}

void trap_UnlinkEntity(gentity_t* ent) {
    imports->trap_UnlinkEntity(ent);
}

int trap_EntitiesInBox(const vec3_t mins, const vec3_t maxs, int* list, int maxcount) {
    return imports->trap_EntitiesInBox(mins, maxs, list, maxcount);
}

qboolean trap_EntityContact(const vec3_t mins, const vec3_t maxs, const gentity_t* ent) {
    return imports->trap_EntityContact(mins, maxs, ent);
}

// ============================================================
// Bot traps
// ============================================================

int trap_BotAllocateClient(void) {
    return imports->trap_BotAllocateClient();
}

int trap_BotLibSetup(void) {
    return imports->trap_BotLibSetup();
}

int trap_BotLibShutdown(void) {
    return imports->trap_BotLibShutdown();
}

int trap_BotLibVarSet(char* var_name, char* value) {
    return imports->trap_BotLibVarSet(var_name, value);
}

int trap_BotLibDefine(char* string) {
    return imports->trap_BotLibDefine(string);
}

int trap_BotLibStartFrame(float time) {
    return imports->trap_BotLibStartFrame(time);
}

int trap_BotLibLoadMap(const char* mapname) {
    return imports->trap_BotLibLoadMap(mapname);
}

int trap_BotLibUpdateEntity(int ent, void* bue) {
    return imports->trap_BotLibUpdateEntity(ent, bue);
}

int trap_BotGetSnapshotEntity(int clientNum, int sequence) {
    return imports->trap_BotGetSnapshotEntity(clientNum, sequence);
}

void trap_BotUserCommand(int clientNum, usercmd_t* ucmd) {
    imports->trap_BotUserCommand(clientNum, ucmd);
}

// ============================================================
// Bot AI traps
// ============================================================

void trap_AAS_EntityInfo(int entnum, void* info) {
    imports->trap_AAS_EntityInfo(entnum, info);
}

int trap_AAS_Initialized(void) {
    return imports->trap_AAS_Initialized();
}

float trap_AAS_Time(void) {
    return imports->trap_AAS_Time();
}

int trap_AAS_PointAreaNum(vec3_t point) {
    return imports->trap_AAS_PointAreaNum(point);
}

int trap_AAS_TraceAreas(vec3_t start, vec3_t end, int* areas, vec3_t* points, int maxareas) {
    return imports->trap_AAS_TraceAreas(start, end, areas, (void*)points, maxareas);
}

float trap_Characteristic_Float(int character, int index) {
    return imports->trap_Characteristic_Float(character, index);
}

float trap_Characteristic_BFloat(int character, int index, float min, float max) {
    return imports->trap_Characteristic_BFloat(character, index, min, max);
}

int trap_Characteristic_Integer(int character, int index) {
    return imports->trap_Characteristic_Integer(character, index);
}

int trap_Characteristic_BInteger(int character, int index, int min, int max) {
    return imports->trap_Characteristic_BInteger(character, index, min, max);
}

void trap_Characteristic_String(int character, int index, char* buf, int size) {
    imports->trap_Characteristic_String(character, index, buf, size);
}

int trap_BotAllocChatState(void) {
    return imports->trap_BotAllocChatState();
}

void trap_BotFreeChatState(int handle) {
    imports->trap_BotFreeChatState(handle);
}

void trap_BotQueueConsoleMessage(int chatstate, int type, char* message) {
    imports->trap_BotQueueConsoleMessage(chatstate, type, message);
}

void trap_BotRemoveConsoleMessage(int chatstate, int handle) {
    imports->trap_BotRemoveConsoleMessage(chatstate, handle);
}

int trap_BotNextConsoleMessage(int chatstate, void* cm) {
    return imports->trap_BotNextConsoleMessage(chatstate, cm);
}

int trap_BotNumConsoleMessages(int chatstate) {
    return imports->trap_BotNumConsoleMessages(chatstate);
}

void trap_BotInitialChat(int chatstate, char* type, int mcontext, char* var0, char* var1, char* var2, char* var3, char* var4, char* var5, char* var6, char* var7) {
    imports->trap_BotInitialChat(chatstate, type, mcontext, var0, var1, var2, var3, var4, var5, var6, var7);
}

int trap_BotNumInitialChats(int chatstate, char* type) {
    return imports->trap_BotNumInitialChats(chatstate, type);
}

int trap_BotReplyChat(int chatstate, char* message, int mcontext, int vcontext, char* var0, char* var1, char* var2, char* var3, char* var4, char* var5, char* var6, char* var7) {
    return imports->trap_BotReplyChat(chatstate, message, mcontext, vcontext, var0, var1, var2, var3, var4, var5, var6, var7);
}

int trap_BotChatLength(int chatstate) {
    return imports->trap_BotChatLength(chatstate);
}

void trap_BotEnterChat(int chatstate, int client, int sendto) {
    imports->trap_BotEnterChat(chatstate, client, sendto);
}

void trap_BotGetChatMessage(int chatstate, char* buf, int size) {
    imports->trap_BotGetChatMessage(chatstate, buf, size);
}

int trap_StringContains(char* str1, char* str2, int casesensitive) {
    return imports->trap_StringContains(str1, str2, casesensitive);
}

int trap_BotFindMatch(char* str, void* match, unsigned long int context) {
    return imports->trap_BotFindMatch(str, match, context);
}

void trap_BotMatchVariable(void* match, int variable, char* buf, int size) {
    imports->trap_BotMatchVariable(match, variable, buf, size);
}

void trap_UnifyWhiteSpaces(char* string) {
    imports->trap_UnifyWhiteSpaces(string);
}

void trap_BotReplaceSynonyms(char* string, unsigned long int context) {
    imports->trap_BotReplaceSynonyms(string, context);
}

void trap_BotSetChatGender(int chatstate, int gender) {
    imports->trap_BotSetChatGender(chatstate, gender);
}

void trap_BotSetChatName(int chatstate, char* name, int client) {
    imports->trap_BotSetChatName(chatstate, name, client);
}

int trap_BotLoadChatFile(int chatstate, char* chatfile, char* chatname) {
    return imports->trap_BotLoadChatFile(chatstate, chatfile, chatname);
}

void trap_BotResetGoalState(int goalstate) {
    imports->trap_BotResetGoalState(goalstate);
}

void trap_BotResetAvoidGoals(int goalstate) {
    imports->trap_BotResetAvoidGoals(goalstate);
}

void trap_BotRemoveFromAvoidGoals(int goalstate, int number) {
    imports->trap_BotRemoveFromAvoidGoals(goalstate, number);
}

void trap_BotPushGoal(int goalstate, void* goal) {
    imports->trap_BotPushGoal(goalstate, goal);
}

void trap_BotPopGoal(int goalstate) {
    imports->trap_BotPopGoal(goalstate);
}

void trap_BotEmptyGoalStack(int goalstate) {
    imports->trap_BotEmptyGoalStack(goalstate);
}

void trap_BotDumpAvoidGoals(int goalstate) {
    imports->trap_BotDumpAvoidGoals(goalstate);
}

void trap_BotDumpGoalStack(int goalstate) {
    imports->trap_BotDumpGoalStack(goalstate);
}

void trap_BotGoalName(int number, char* name, int size) {
    imports->trap_BotGoalName(number, name, size);
}

int trap_BotGetTopGoal(int goalstate, void* goal) {
    return imports->trap_BotGetTopGoal(goalstate, goal);
}

int trap_BotGetSecondGoal(int goalstate, void* goal) {
    return imports->trap_BotGetSecondGoal(goalstate, goal);
}

int trap_BotChooseLTGItem(int goalstate, vec3_t origin, int* inventory, int travelflags) {
    return imports->trap_BotChooseLTGItem(goalstate, origin, inventory, travelflags);
}

int trap_BotChooseNBGItem(int goalstate, vec3_t origin, int* inventory, int travelflags, void* ltg, float maxtime) {
    return imports->trap_BotChooseNBGItem(goalstate, origin, inventory, travelflags, ltg, maxtime);
}

int trap_BotTouchingGoal(vec3_t origin, void* goal) {
    return imports->trap_BotTouchingGoal(origin, goal);
}

int trap_BotItemGoalInVisButNotVisible(int viewer, vec3_t eye, vec3_t viewangles, void* goal) {
    return imports->trap_BotItemGoalInVisButNotVisible(viewer, eye, viewangles, goal);
}

int trap_BotGetLevelItemGoal(int index, char* classname, void* goal) {
    return imports->trap_BotGetLevelItemGoal(index, classname, goal);
}

int trap_BotGetNextCampSpotGoal(int num, void* goal) {
    return imports->trap_BotGetNextCampSpotGoal(num, goal);
}

int trap_BotGetMapLocationGoal(char* name, void* goal) {
    return imports->trap_BotGetMapLocationGoal(name, goal);
}

float trap_BotAvoidGoalTime(int goalstate, int number) {
    return imports->trap_BotAvoidGoalTime(goalstate, number);
}

void trap_BotSetAvoidGoalTime(int goalstate, int number, float avoidtime) {
    imports->trap_BotSetAvoidGoalTime(goalstate, number, avoidtime);
}

void trap_BotInitLevelItems(void) {
    imports->trap_BotInitLevelItems();
}

void trap_BotUpdateEntityItems(void) {
    imports->trap_BotUpdateEntityItems();
}

int trap_BotLoadItemWeights(int goalstate, char* filename) {
    return imports->trap_BotLoadItemWeights(goalstate, filename);
}

void trap_BotFreeItemWeights(int goalstate) {
    imports->trap_BotFreeItemWeights(goalstate);
}

void trap_BotInterbreedGoalFuzzyLogic(int parent1, int parent2, int child) {
    imports->trap_BotInterbreedGoalFuzzyLogic(parent1, parent2, child);
}

void trap_BotSaveGoalFuzzyLogic(int goalstate, char* filename) {
    imports->trap_BotSaveGoalFuzzyLogic(goalstate, filename);
}

void trap_BotMutateGoalFuzzyLogic(int goalstate, float range) {
    imports->trap_BotMutateGoalFuzzyLogic(goalstate, range);
}

int trap_BotAllocGoalState(int state) {
    return imports->trap_BotAllocGoalState(state);
}

void trap_BotFreeGoalState(int handle) {
    imports->trap_BotFreeGoalState(handle);
}

void trap_BotResetMoveState(int movestate) {
    imports->trap_BotResetMoveState(movestate);
}

void trap_BotMoveToGoal(void* result, int movestate, void* goal, int travelflags) {
    imports->trap_BotMoveToGoal(result, movestate, goal, travelflags);
}

int trap_BotMoveInDirection(int movestate, vec3_t dir, float speed, int type) {
    return imports->trap_BotMoveInDirection(movestate, dir, speed, type);
}

void trap_BotResetAvoidReach(int movestate) {
    imports->trap_BotResetAvoidReach(movestate);
}

void trap_BotResetLastAvoidReach(int movestate) {
    imports->trap_BotResetLastAvoidReach(movestate);
}

int trap_BotReachabilityArea(vec3_t origin, int testground) {
    return imports->trap_BotReachabilityArea(origin, testground);
}

int trap_BotMovementViewTarget(int movestate, void* goal, int travelflags, float lookahead, vec3_t target) {
    return imports->trap_BotMovementViewTarget(movestate, goal, travelflags, lookahead, target);
}

int trap_BotPredictVisiblePosition(vec3_t origin, int areanum, void* goal, int travelflags, vec3_t target) {
    return imports->trap_BotPredictVisiblePosition(origin, areanum, goal, travelflags, target);
}

int trap_BotAllocMoveState(void) {
    return imports->trap_BotAllocMoveState();
}

void trap_BotFreeMoveState(int handle) {
    imports->trap_BotFreeMoveState(handle);
}

void trap_BotInitMoveState(int handle, void* initmove) {
    imports->trap_BotInitMoveState(handle, initmove);
}

void trap_BotAddAvoidSpot(int movestate, vec3_t origin, float radius, int type) {
    imports->trap_BotAddAvoidSpot(movestate, origin, radius, type);
}

int trap_BotChooseBestFightWeapon(int weaponstate, int* inventory) {
    return imports->trap_BotChooseBestFightWeapon(weaponstate, inventory);
}

void trap_BotGetWeaponInfo(int weaponstate, int weapon, void* weaponinfo) {
    imports->trap_BotGetWeaponInfo(weaponstate, weapon, weaponinfo);
}

int trap_BotLoadWeaponWeights(int weaponstate, char* filename) {
    return imports->trap_BotLoadWeaponWeights(weaponstate, filename);
}

int trap_BotAllocWeaponState(void) {
    return imports->trap_BotAllocWeaponState();
}

void trap_BotFreeWeaponState(int weaponstate) {
    imports->trap_BotFreeWeaponState(weaponstate);
}

void trap_BotResetWeaponState(int weaponstate) {
    imports->trap_BotResetWeaponState(weaponstate);
}

int trap_GeneticParentsAndChildSelection(int numranks, float* ranks, int* parent1, int* parent2, int* child) {
    return imports->trap_GeneticParentsAndChildSelection(numranks, ranks, parent1, parent2, child);
}

// ============================================================
// [QL] Steam integration traps
// ============================================================

uint64_t trap_GetSteamID(int clientNum) {
    return imports->trap_GetSteamID(clientNum);
}

void trap_GetServerSteamID(char* buf, int size) {
    imports->trap_GetServerSteamID(buf, size);
}

void trap_IncrementSteamStat(int clientNum, int statID) {
    imports->trap_IncrementSteamStat(clientNum, statID);
}

int trap_GetSteamStat(int clientNum, int statID) {
    return imports->trap_GetSteamStat(clientNum, statID);
}

qboolean trap_VerifySteamAuth(int clientNum) {
    return imports->trap_VerifySteamAuth(clientNum);
}

// ============================================================
// Stubs for Q3 traps not in QL's import table
// ============================================================

int trap_Milliseconds(void) { return 0; }
qboolean trap_InPVS(const vec3_t p1, const vec3_t p2) { (void)p1; (void)p2; return qfalse; }
qboolean trap_InPVSIgnorePortals(const vec3_t p1, const vec3_t p2) { (void)p1; (void)p2; return qfalse; }
qboolean trap_AreasConnected(int area1, int area2) { (void)area1; (void)area2; return qfalse; }
qboolean trap_GetEntityToken(char* buffer, int bufferSize) {
    return imports->trap_GetEntityToken(buffer, bufferSize);
}
int trap_FS_GetFileList(const char* path, const char* extension, char* listbuf, int bufsize) { (void)path; (void)extension; (void)listbuf; (void)bufsize; return 0; }
int trap_FS_Seek(fileHandle_t f, long offset, int origin) { (void)f; (void)offset; (void)origin; return 0; }
int trap_DebugPolygonCreate(int color, int numPoints, vec3_t* points) { (void)color; (void)numPoints; (void)points; return 0; }
void trap_DebugPolygonDelete(int id) { (void)id; }
int trap_RealTime(qtime_t* qtime) { (void)qtime; return 0; }
void trap_SnapVector(float* v) { (void)v; }
int trap_Cvar_VariableIntegerValue(const char* var_name) { return (int)imports->trap_Cvar_VariableValue(var_name); }
qboolean trap_EntityContactCapsule(const vec3_t mins, const vec3_t maxs, const gentity_t* ent) { (void)mins; (void)maxs; (void)ent; return qfalse; }

// Bot traps wired through import table

void trap_BotFreeClient(int clientNum) {
    imports->trap_BotFreeClient(clientNum);
}

void trap_AAS_PresenceTypeBoundingBox(int presencetype, vec3_t mins, vec3_t maxs) {
    imports->trap_AAS_PresenceTypeBoundingBox(presencetype, mins, maxs);
}

int trap_AAS_PointReachabilityAreaIndex(vec3_t point) {
    return imports->trap_AAS_PointReachabilityAreaIndex(point);
}

int trap_AAS_BBoxAreas(vec3_t absmins, vec3_t absmaxs, int* areas, int maxareas) {
    return imports->trap_AAS_BBoxAreas(absmins, absmaxs, areas, maxareas);
}

int trap_AAS_AreaInfo(int areanum, void* info) {
    return imports->trap_AAS_AreaInfo(areanum, info);
}

int trap_AAS_PointContents(vec3_t point) {
    return imports->trap_AAS_PointContents(point);
}

int trap_AAS_NextBSPEntity(int ent) {
    return imports->trap_AAS_NextBSPEntity(ent);
}

int trap_AAS_ValueForBSPEpairKey(int ent, char* key, char* value, int size) {
    return imports->trap_AAS_ValueForBSPEpairKey(ent, key, value, size);
}

int trap_AAS_VectorForBSPEpairKey(int ent, char* key, vec3_t v) {
    return imports->trap_AAS_VectorForBSPEpairKey(ent, key, v);
}

int trap_AAS_FloatForBSPEpairKey(int ent, char* key, float* value) {
    return imports->trap_AAS_FloatForBSPEpairKey(ent, key, value);
}

int trap_AAS_IntForBSPEpairKey(int ent, char* key, int* value) {
    return imports->trap_AAS_IntForBSPEpairKey(ent, key, value);
}

int trap_AAS_AreaReachability(int areanum) {
    return imports->trap_AAS_AreaReachability(areanum);
}

int trap_AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags) {
    return imports->trap_AAS_AreaTravelTimeToGoalArea(areanum, origin, goalareanum, travelflags);
}

int trap_AAS_EnableRoutingArea(int areanum, int enable) {
    return imports->trap_AAS_EnableRoutingArea(areanum, enable);
}

int trap_AAS_PredictRoute(void* route, int areanum, vec3_t origin, int goalareanum, int travelflags, int maxareas, int maxtime, int stopevent, int stopcontents, int stoptfl, int stopareanum) {
    return imports->trap_AAS_PredictRoute(route, areanum, origin, goalareanum, travelflags, maxareas, maxtime, stopevent, stopcontents, stoptfl, stopareanum);
}

int trap_AAS_AlternativeRouteGoals(vec3_t start, int startareanum, vec3_t goal, int goalareanum, int travelflags, void* altroutegoals, int maxaltroutegoals, int type) {
    return imports->trap_AAS_AlternativeRouteGoals(start, startareanum, goal, goalareanum, travelflags, altroutegoals, maxaltroutegoals, type);
}

int trap_AAS_Swimming(vec3_t origin) {
    return imports->trap_AAS_Swimming(origin);
}

int trap_AAS_PredictClientMovement(void* move, int entnum, vec3_t origin, int presencetype, int onground, vec3_t velocity, vec3_t cmdmove, int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum, int visualize) {
    return imports->trap_AAS_PredictClientMovement(move, entnum, origin, presencetype, onground, velocity, cmdmove, cmdframes, maxframes, frametime, stopevent, stopareanum, visualize);
}

void trap_EA_Say(int client, char* str) {
    imports->trap_EA_Say(client, str);
}

void trap_EA_SayTeam(int client, char* str) {
    imports->trap_EA_SayTeam(client, str);
}

void trap_EA_Command(int client, char* command) {
    imports->trap_EA_Command(client, command);
}

void trap_EA_Action(int client, int action) {
    imports->trap_EA_Action(client, action);
}

void trap_EA_Gesture(int client) {
    imports->trap_EA_Gesture(client);
}

void trap_EA_Talk(int client) {
    imports->trap_EA_Talk(client);
}

void trap_EA_Attack(int client) {
    imports->trap_EA_Attack(client);
}

void trap_EA_Use(int client) {
    imports->trap_EA_Use(client);
}

void trap_EA_Respawn(int client) {
    imports->trap_EA_Respawn(client);
}

void trap_EA_MoveUp(int client) {
    imports->trap_EA_MoveUp(client);
}

void trap_EA_MoveDown(int client) {
    imports->trap_EA_MoveDown(client);
}

void trap_EA_MoveForward(int client) {
    imports->trap_EA_MoveForward(client);
}

void trap_EA_MoveBack(int client) {
    imports->trap_EA_MoveBack(client);
}

void trap_EA_MoveLeft(int client) {
    imports->trap_EA_MoveLeft(client);
}

void trap_EA_MoveRight(int client) {
    imports->trap_EA_MoveRight(client);
}

void trap_EA_Crouch(int client) {
    imports->trap_EA_Crouch(client);
}

void trap_EA_SelectWeapon(int client, int weapon) {
    imports->trap_EA_SelectWeapon(client, weapon);
}

void trap_EA_Jump(int client) {
    imports->trap_EA_Jump(client);
}

void trap_EA_DelayedJump(int client) {
    imports->trap_EA_DelayedJump(client);
}

void trap_EA_Move(int client, vec3_t dir, float speed) {
    imports->trap_EA_Move(client, dir, speed);
}

void trap_EA_View(int client, vec3_t viewangles) {
    imports->trap_EA_View(client, viewangles);
}

void trap_EA_EndRegular(int client, float thinktime) {
    imports->trap_EA_EndRegular(client, thinktime);
}

void trap_EA_GetInput(int client, float thinktime, void* input) {
    imports->trap_EA_GetInput(client, thinktime, input);
}

void trap_EA_ResetInput(int client) {
    imports->trap_EA_ResetInput(client);
}

int trap_BotLoadCharacter(char* charfile, float skill) {
    return imports->trap_BotLoadCharacter(charfile, skill);
}

void trap_BotFreeCharacter(int character) {
    imports->trap_BotFreeCharacter(character);
}

int trap_BotGetServerCommand(int clientNum, char* message, int size) {
    return imports->trap_BotGetServerCommand(clientNum, message, size);
}

int trap_BotLibVarGet(char* var_name, char* value, int size) {
    return imports->trap_BotLibVarGet(var_name, value, size);
}

int trap_BotLibTest(int parm0, char* parm1, vec3_t parm2, vec3_t parm3) {
    return imports->trap_BotLibTest(parm0, parm1, parm2, parm3);
}

int trap_PC_LoadSource(const char* filename) {
    return imports->trap_PC_LoadSource(filename);
}

int trap_PC_FreeSource(int handle) {
    return imports->trap_PC_FreeSource(handle);
}

int trap_PC_ReadToken(int handle, pc_token_t* pc_token) {
    return imports->trap_PC_ReadToken(handle, pc_token);
}

int trap_PC_SourceFileAndLine(int handle, char* filename, int* line) {
    return imports->trap_PC_SourceFileAndLine(handle, filename, line);
}
