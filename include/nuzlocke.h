#ifndef GUARD_NUZLOCKE_H
#define GUARD_NUZLOCKE_H

void Nuzlocke_SetPendingRules(bool32 enabled, bool32 hardWhiteout, bool32 noBattleItems, bool32 infiniteCandies);
void Nuzlocke_NewGame(void);
bool32 Nuzlocke_IsEnabled(void);
bool32 Nuzlocke_HasInfiniteCandies(void);
bool32 Nuzlocke_CanUseItemInBattle(enum Item itemId);
const u8 *Nuzlocke_GetNoBattleItemsMessage(void);
bool32 Nuzlocke_IsMapsecUsed(mapsec_u8_t mapsec);
void Nuzlocke_OnWildBattleStart(struct Pokemon *mon);
bool32 Nuzlocke_CanCatch(void);
void Nuzlocke_OnGift(enum Species species);
bool32 Nuzlocke_IsGraveyardBox(u32 boxId);
void Nuzlocke_BuryFaintedPartyMons(void);
void Nuzlocke_OnWhiteOut(void);
bool32 Nuzlocke_IsRunOver(void);
const u8 *Nuzlocke_GetRunOverMessage(void);

#endif // GUARD_NUZLOCKE_H
