#include "global.h"
#include "item.h"
#include "nuzlocke.h"
#include "pokedex.h"
#include "pokemon.h"
#include "constants/items.h"

// Rules:
// - Only the first encounter in each map section can be caught. Wild battles,
//   static encounters, and gifts all use up the area.
// - Dupes clause: an encounter whose evolution family is already caught
//   doesn't use up the area, and can't be caught.
// - Shiny clause: shinies can always be caught and don't use up the area.
// Infinite candies is separate: Rare Candies aren't used up.

static EWRAM_DATA struct NuzlockeSave sPendingRules = {0};
static EWRAM_DATA bool8 sCanCatch = FALSE;

// Birch's speech runs before NewGameInitData clears SaveBlock3, so the choice is held here until then
void Nuzlocke_SetPendingRules(bool32 enabled, bool32 infiniteCandies)
{
    sPendingRules.enabled = enabled;
    sPendingRules.infiniteCandies = infiniteCandies;
}

void Nuzlocke_NewGame(void)
{
    gSaveBlock3Ptr->nuzlocke = sPendingRules;
    sPendingRules = (struct NuzlockeSave){0};
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
