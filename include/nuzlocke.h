#ifndef GUARD_NUZLOCKE_H
#define GUARD_NUZLOCKE_H

extern const u8 gText_NuzlockeOffType[];

void Nuzlocke_SetPendingRules(struct NuzlockeSave rules);
void Nuzlocke_NewGame(void);
bool32 Nuzlocke_IsEnabled(void);
bool32 Nuzlocke_HasInfiniteCandies(void);
bool32 Nuzlocke_CanUseItemInBattle(enum Item itemId);
const u8 *Nuzlocke_GetNoBattleItemsMessage(void);
bool32 Nuzlocke_ForcesSetMode(void);
bool32 Nuzlocke_IsSpeciesAllowed(enum Species species);
void Nuzlocke_CheckTransferredSpecies(void);
enum Species Nuzlocke_GetStarter(u32 index, enum Species defaultStarter);
bool32 Nuzlocke_IsMapsecUsed(mapsec_u8_t mapsec);
void Nuzlocke_OnWildBattleStart(struct Pokemon *mon);
bool32 Nuzlocke_CanCatch(void);
bool32 Nuzlocke_OnGift(enum Species species);
bool32 Nuzlocke_IsGraveyardBox(u32 boxId);
void Nuzlocke_BuryFaintedPartyMons(void);
void Nuzlocke_OnWhiteOut(void);
bool32 Nuzlocke_IsRunOver(void);
const u8 *Nuzlocke_GetRunOverMessage(void);

#endif // GUARD_NUZLOCKE_H
