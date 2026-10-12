#include "global.h"
#include "caps.h"
#include "event_data.h"
#include "item.h"
#include "nuzlocke.h"
#include "pokedex.h"
#include "test/overworld_script.h"
#include "test/test.h"
#include "constants/items.h"

static void StartNuzlocke(bool32 enabled)
{
    gSaveBlock3Ptr->nuzlocke = (struct NuzlockeSave){ .enabled = enabled };
    ResetPokedex();
    gMapHeader.regionMapSectionId = MAPSEC_ROUTE_101;
}

static void Encounter(enum Species species, bool32 isShiny)
{
    struct Pokemon mon;

    CreateRandomMon(&mon, species, 5);
    SetMonData(&mon, MON_DATA_IS_SHINY, &isShiny);
    Nuzlocke_OnWildBattleStart(&mon);
}

TEST("Nuzlocke: Only the first encounter in an area can be caught")
{
    StartNuzlocke(TRUE);

    Encounter(SPECIES_ZIGZAGOON, FALSE);
    EXPECT(Nuzlocke_CanCatch());
    EXPECT(Nuzlocke_IsMapsecUsed(MAPSEC_ROUTE_101));

    Encounter(SPECIES_WURMPLE, FALSE);
    EXPECT(!Nuzlocke_CanCatch());

    gMapHeader.regionMapSectionId = MAPSEC_ROUTE_102;
    Encounter(SPECIES_WURMPLE, FALSE);
    EXPECT(Nuzlocke_CanCatch());
}

TEST("Nuzlocke: Dupes don't use up the area and can't be caught")
{
    StartNuzlocke(TRUE);
    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_POOCHYENA), FLAG_SET_CAUGHT);

    Encounter(SPECIES_MIGHTYENA, FALSE);
    EXPECT(!Nuzlocke_CanCatch());
    EXPECT(!Nuzlocke_IsMapsecUsed(MAPSEC_ROUTE_101));

    Encounter(SPECIES_WURMPLE, FALSE);
    EXPECT(Nuzlocke_CanCatch());
}

TEST("Nuzlocke: Dupes include the whole evolution family")
{
    enum Species caught, encountered;
    PARAMETRIZE { caught = SPECIES_PICHU; encountered = SPECIES_RAICHU; }
    PARAMETRIZE { caught = SPECIES_RAICHU; encountered = SPECIES_PICHU; }
    PARAMETRIZE { caught = SPECIES_VAPOREON; encountered = SPECIES_FLAREON; }
    StartNuzlocke(TRUE);
    GetSetPokedexFlag(SpeciesToNationalPokedexNum(caught), FLAG_SET_CAUGHT);

    Encounter(encountered, FALSE);
    EXPECT(!Nuzlocke_CanCatch());
    EXPECT(!Nuzlocke_IsMapsecUsed(MAPSEC_ROUTE_101));
}

TEST("Nuzlocke: Shinies can always be caught and don't use up the area")
{
    StartNuzlocke(TRUE);

    Encounter(SPECIES_ZIGZAGOON, TRUE);
    EXPECT(Nuzlocke_CanCatch());
    EXPECT(!Nuzlocke_IsMapsecUsed(MAPSEC_ROUTE_101));

    Encounter(SPECIES_WURMPLE, FALSE);
    Encounter(SPECIES_POOCHYENA, TRUE);
    EXPECT(Nuzlocke_CanCatch());
}

TEST("Nuzlocke: Gifts use up the area")
{
    StartNuzlocke(TRUE);
    ZeroPlayerPartyMons();

    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_CASTFORM, 25
    );
    EXPECT(Nuzlocke_IsMapsecUsed(MAPSEC_ROUTE_101));

    Encounter(SPECIES_WURMPLE, FALSE);
    EXPECT(!Nuzlocke_CanCatch());
}

TEST("Nuzlocke: Gifted dupes don't use up the area")
{
    StartNuzlocke(TRUE);
    ZeroPlayerPartyMons();
    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_WYNAUT), FLAG_SET_CAUGHT);

    RUN_OVERWORLD_SCRIPT(
        giveegg SPECIES_WOBBUFFET
    );
    EXPECT(!Nuzlocke_IsMapsecUsed(MAPSEC_ROUTE_101));
}

TEST("Nuzlocke: Catching is never blocked when the rules are off")
{
    StartNuzlocke(FALSE);

    Encounter(SPECIES_ZIGZAGOON, FALSE);
    Encounter(SPECIES_WURMPLE, FALSE);
    EXPECT(Nuzlocke_CanCatch());
    EXPECT(!Nuzlocke_IsMapsecUsed(MAPSEC_ROUTE_101));
}

TEST("Nuzlocke: A new game applies the rules chosen in Birch's speech")
{
    bool32 enabled, infiniteCandies;
    PARAMETRIZE { enabled = FALSE; infiniteCandies = FALSE; }
    PARAMETRIZE { enabled = TRUE; infiniteCandies = FALSE; }
    PARAMETRIZE { enabled = FALSE; infiniteCandies = TRUE; }
    ClearBag();

    Nuzlocke_SetPendingRules(enabled, infiniteCandies);
    Nuzlocke_NewGame();

    EXPECT_EQ(Nuzlocke_IsEnabled(), enabled);
    EXPECT_EQ(Nuzlocke_HasInfiniteCandies(), infiniteCandies);
    EXPECT_EQ(CheckBagHasItem(ITEM_RARE_CANDY, 1), infiniteCandies);
}

TEST("Nuzlocke: Infinite candies use the badge level caps")
{
    gSaveBlock3Ptr->nuzlocke = (struct NuzlockeSave){ .infiniteCandies = TRUE };
    FlagClear(FLAG_BADGE01_GET);
    EXPECT_EQ(GetCurrentLevelCap(), 15);
    FlagSet(FLAG_BADGE01_GET);
    EXPECT_EQ(GetCurrentLevelCap(), 19);
    FlagClear(FLAG_BADGE01_GET);
}
