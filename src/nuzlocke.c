#include "global.h"
#include "battle.h"
#include "item.h"
#include "nuzlocke.h"
#include "pokedex.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "save.h"
#include "string_util.h"
#include "constants/battle.h"
#include "constants/items.h"

// Rules:
// - Only the first encounter in each map section can be caught. Wild battles,
//   static encounters, and gifts all use up the area.
// - Dupes clause: an encounter whose evolution family is already caught
//   doesn't use up the area, and can't be caught.
// - Shiny clause: shinies can always be caught and don't use up the area.
// - Pokémon that faint are moved to the graveyard (the last two boxes) after the
//   battle, and nothing can be moved in or out of it.
// - Whiteout: soft brings in the first Pokémon from the PC (or ends the run and
//   loads the last save if there is none), hard ends the run and deletes the save.
// Infinite candies is separate: Rare Candies aren't used up.

#define NUM_GRAVEYARD_BOXES 2
#define FIRST_GRAVEYARD_BOX (TOTAL_BOXES_COUNT - NUM_GRAVEYARD_BOXES)

static const u8 sText_GraveyardBoxNames[NUM_GRAVEYARD_BOXES][BOX_NAME_LENGTH + 1] = { _("GRAVES 1"), _("GRAVES 2") };

static EWRAM_DATA struct NuzlockeSave sPendingRules = {0};
static EWRAM_DATA bool8 sCanCatch = FALSE;
static EWRAM_DATA bool8 sRunIsOver = FALSE;

static const u8 sText_RunOverSaveLoaded[] = _("Your NUZLOCKE run is over…\nYour last save will be loaded.\p");
static const u8 sText_NoBattleItems[] = _("NUZLOCKE rules: only POKé BALLS\ncan be used in battle!\p");
static const u8 sText_RunOverSaveDeleted[] = _("Your NUZLOCKE run is over…\nYour save has been deleted.\p");

// Birch's speech runs before NewGameInitData clears SaveBlock3, so the choice is held here until then
void Nuzlocke_SetPendingRules(bool32 enabled, bool32 hardWhiteout, bool32 noBattleItems, bool32 infiniteCandies)
{
    sPendingRules.enabled = enabled;
    sPendingRules.hardWhiteout = hardWhiteout;
    sPendingRules.noBattleItems = noBattleItems;
    sPendingRules.infiniteCandies = infiniteCandies;
}

void Nuzlocke_NewGame(void)
{
    gSaveBlock3Ptr->nuzlocke = sPendingRules;
    sPendingRules = (struct NuzlockeSave){0};
    if (gSaveBlock3Ptr->nuzlocke.enabled)
    {
        for (u32 i = 0; i < NUM_GRAVEYARD_BOXES; i++)
            StringCopy(GetBoxNamePtr(FIRST_GRAVEYARD_BOX + i), sText_GraveyardBoxNames[i]);
    }
    if (gSaveBlock3Ptr->nuzlocke.infiniteCandies)
        AddBagItem(ITEM_RARE_CANDY, 1);
}

bool32 Nuzlocke_IsEnabled(void)
{
    return gSaveBlock3Ptr->nuzlocke.enabled;
}

bool32 Nuzlocke_HasInfiniteCandies(void)
{
    return gSaveBlock3Ptr->nuzlocke.infiniteCandies;
}

bool32 Nuzlocke_CanUseItemInBattle(enum Item itemId)
{
    return !Nuzlocke_IsEnabled()
        || !gSaveBlock3Ptr->nuzlocke.noBattleItems
        || GetItemBattleUsage(itemId) == EFFECT_ITEM_THROW_BALL;
}

const u8 *Nuzlocke_GetNoBattleItemsMessage(void)
{
    return sText_NoBattleItems;
}

bool32 Nuzlocke_IsMapsecUsed(mapsec_u8_t mapsec)
{
    return (gSaveBlock3Ptr->nuzlocke.usedMapsecs[mapsec / 8] >> (mapsec % 8)) & 1;
}

static bool32 IsFamilyCaught(enum Species species)
{
    const struct Evolution *evolutions = GetSpeciesEvolutions(species);

    if (GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_GET_CAUGHT))
        return TRUE;
    for (u32 i = 0; evolutions != NULL && evolutions[i].method != EVOLUTIONS_END; i++)
    {
        if (IsFamilyCaught(evolutions[i].targetSpecies))
            return TRUE;
    }
    return FALSE;
}

static bool32 IsDupe(enum Species species)
{
    for (enum Species preEvolution; (preEvolution = GetSpeciesPreEvolution(species)) != SPECIES_NONE;)
        species = preEvolution;
    return IsFamilyCaught(species);
}

// Uses up the current map section if this is its first encounter; returns whether it was
static bool32 TryUseAreaEncounter(enum Species species)
{
    mapsec_u8_t mapsec = gMapHeader.regionMapSectionId;

    if (Nuzlocke_IsMapsecUsed(mapsec) || IsDupe(species))
        return FALSE;
    gSaveBlock3Ptr->nuzlocke.usedMapsecs[mapsec / 8] |= 1 << (mapsec % 8);
    return TRUE;
}

void Nuzlocke_OnWildBattleStart(struct Pokemon *mon)
{
    if (!Nuzlocke_IsEnabled())
        return;
    if (IsMonShiny(mon))
        sCanCatch = TRUE;
    else
        sCanCatch = TryUseAreaEncounter(GetMonData(mon, MON_DATA_SPECIES));
}

bool32 Nuzlocke_CanCatch(void)
{
    return !Nuzlocke_IsEnabled() || sCanCatch;
}

// Gifts are always given, but use up the area like any other encounter
void Nuzlocke_OnGift(enum Species species)
{
    if (Nuzlocke_IsEnabled())
        TryUseAreaEncounter(species);
}

bool32 Nuzlocke_IsGraveyardBox(u32 boxId)
{
    return Nuzlocke_IsEnabled() && boxId >= FIRST_GRAVEYARD_BOX && boxId < TOTAL_BOXES_COUNT;
}

static bool32 IsFainted(struct Pokemon *mon)
{
    return GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE
        && !GetMonData(mon, MON_DATA_IS_EGG)
        && GetMonData(mon, MON_DATA_HP) == 0;
}

static u32 CountAlivePartyMons(void)
{
    u32 alive = 0;

    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        if (GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE && !GetMonData(mon, MON_DATA_IS_EGG) && !IsFainted(mon))
            alive++;
    }
    return alive;
}

static void BuryFaintedPartyMons(void)
{
    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        enum Item item = GetMonData(mon, MON_DATA_HELD_ITEM);

        if (!IsFainted(mon))
            continue;
        if (item != ITEM_NONE && AddBagItem(item, 1))
        {
            item = ITEM_NONE;
            SetMonData(mon, MON_DATA_HELD_ITEM, &item);
        }
        // A full graveyard (60 graves) just loses the Pokémon
        for (u32 grave = 0; grave < NUM_GRAVEYARD_BOXES * IN_BOX_COUNT; grave++)
        {
            u32 boxId = FIRST_GRAVEYARD_BOX + grave / IN_BOX_COUNT, pos = grave % IN_BOX_COUNT;
            if (GetBoxMonDataAt(boxId, pos, MON_DATA_SPECIES) == SPECIES_NONE)
            {
                SetBoxMonAt(boxId, pos, &mon->box);
                break;
            }
        }
        ZeroMonData(mon);
    }
    CompactPartySlots();
    CalculatePlayerPartyCount();
}

// Called after every battle that uses the player's own party. A whiteout is handled by Nuzlocke_OnWhiteOut.
void Nuzlocke_BuryFaintedPartyMons(void)
{
    if (Nuzlocke_IsEnabled() && !(gBattleTypeFlags & BATTLE_TYPE_FRONTIER) && CountAlivePartyMons() > 0)
        BuryFaintedPartyMons();
}

static bool32 TryWithdrawFirstStoredMon(void)
{
    for (u32 boxId = 0; boxId < FIRST_GRAVEYARD_BOX; boxId++)
    {
        for (u32 pos = 0; pos < IN_BOX_COUNT; pos++)
        {
            struct BoxPokemon *boxMon = GetBoxedMonPtr(boxId, pos);
            if (GetBoxMonData(boxMon, MON_DATA_SPECIES) == SPECIES_NONE || GetBoxMonData(boxMon, MON_DATA_IS_EGG))
                continue;
            for (u32 slot = 0; slot < PARTY_SIZE; slot++)
            {
                if (GetMonData(&gParties[B_TRAINER_PLAYER][slot], MON_DATA_SPECIES) == SPECIES_NONE)
                {
                    BoxMonToMon(boxMon, &gParties[B_TRAINER_PLAYER][slot]);
                    ZeroBoxMonAt(boxId, pos);
                    CalculatePlayerPartyCount();
                    return TRUE;
                }
            }
            return FALSE;
        }
    }
    return FALSE;
}

// Buries the party on a whiteout, and ends the run if there's no Pokémon left to carry on with.
// The whiteout screen then shows Nuzlocke_GetRunOverMessage and resets the game.
void Nuzlocke_OnWhiteOut(void)
{
    sRunIsOver = FALSE;
    if (!Nuzlocke_IsEnabled())
        return;
    BuryFaintedPartyMons();
    // Some Pokémon can sit out a lost battle, e.g. Steven's multi battle
    if (CountAlivePartyMons() > 0)
        return;
    if (!gSaveBlock3Ptr->nuzlocke.hardWhiteout && TryWithdrawFirstStoredMon())
        return;
    // Deleted before the message, so switching off during it can't keep the run
    if (gSaveBlock3Ptr->nuzlocke.hardWhiteout)
        ClearSaveData();
    sRunIsOver = TRUE;
}

bool32 Nuzlocke_IsRunOver(void)
{
    return sRunIsOver;
}

const u8 *Nuzlocke_GetRunOverMessage(void)
{
    return gSaveBlock3Ptr->nuzlocke.hardWhiteout ? sText_RunOverSaveDeleted : sText_RunOverSaveLoaded;
}
