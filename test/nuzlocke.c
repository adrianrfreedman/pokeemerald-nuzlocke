#include "global.h"
#include "battle.h"
#include "caps.h"
#include "event_data.h"
#include "item.h"
#include "nuzlocke.h"
#include "pokedex.h"
#include "pokemon_storage_system.h"
#include "starter_choose.h"
#include "string_util.h"
#include "constants/battle.h"
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

    Nuzlocke_SetPendingRules((struct NuzlockeSave){ .enabled = enabled, .infiniteCandies = infiniteCandies });
    Nuzlocke_NewGame();

    EXPECT_EQ(Nuzlocke_IsEnabled(), enabled);
    EXPECT_EQ(Nuzlocke_HasInfiniteCandies(), infiniteCandies);
    EXPECT_EQ(CheckBagHasItem(ITEM_RARE_CANDY, 1), infiniteCandies);
}

TEST("Nuzlocke: Banning battle items still allows Poké Balls")
{
    bool32 enabled, noBattleItems;
    PARAMETRIZE { enabled = TRUE; noBattleItems = TRUE; }
    PARAMETRIZE { enabled = TRUE; noBattleItems = FALSE; }
    PARAMETRIZE { enabled = FALSE; noBattleItems = TRUE; }

    Nuzlocke_SetPendingRules((struct NuzlockeSave){ .enabled = enabled, .noBattleItems = noBattleItems });
    Nuzlocke_NewGame();

    EXPECT(Nuzlocke_CanUseItemInBattle(ITEM_POKE_BALL));
    EXPECT(Nuzlocke_CanUseItemInBattle(ITEM_ULTRA_BALL));
    EXPECT_EQ(Nuzlocke_CanUseItemInBattle(ITEM_POTION), !(enabled && noBattleItems));
    EXPECT_EQ(Nuzlocke_CanUseItemInBattle(ITEM_X_ATTACK), !(enabled && noBattleItems));
    EXPECT_EQ(Nuzlocke_CanUseItemInBattle(ITEM_POKE_DOLL), !(enabled && noBattleItems));
}

TEST("Nuzlocke: SET mode is forced only when chosen")
{
    bool32 enabled, setMode;
    PARAMETRIZE { enabled = TRUE; setMode = TRUE; }
    PARAMETRIZE { enabled = TRUE; setMode = FALSE; }
    PARAMETRIZE { enabled = FALSE; setMode = TRUE; }
    gSaveBlock2Ptr->optionsBattleStyle = OPTIONS_BATTLE_STYLE_SHIFT;

    Nuzlocke_SetPendingRules((struct NuzlockeSave){ .enabled = enabled, .setMode = setMode });
    Nuzlocke_NewGame();

    EXPECT_EQ(Nuzlocke_ForcesSetMode(), enabled && setMode);
    EXPECT_EQ((u32)gSaveBlock2Ptr->optionsBattleStyle, (enabled && setMode) ? OPTIONS_BATTLE_STYLE_SET : OPTIONS_BATTLE_STYLE_SHIFT);
}

static void StartMonotype(enum Type type)
{
    StartNuzlocke(TRUE);
    gSaveBlock3Ptr->nuzlocke.monotype = type;
}

TEST("Nuzlocke: Monotype off-type encounters can't be caught and don't use up the area")
{
    bool32 isShiny;
    PARAMETRIZE { isShiny = FALSE; }
    PARAMETRIZE { isShiny = TRUE; }
    StartMonotype(TYPE_WATER);

    Encounter(SPECIES_ZIGZAGOON, isShiny);
    EXPECT(!Nuzlocke_CanCatch());
    EXPECT(!Nuzlocke_IsMapsecUsed(MAPSEC_ROUTE_101));

    Encounter(SPECIES_WINGULL, FALSE);
    EXPECT(Nuzlocke_CanCatch());
    EXPECT(Nuzlocke_IsMapsecUsed(MAPSEC_ROUTE_101));
}

TEST("Nuzlocke: Monotype off-type gifts are refused and don't use up the area")
{
    StartMonotype(TYPE_WATER);
    ZeroPlayerPartyMons();

    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_CASTFORM, 25
    );
    EXPECT_EQ(gSpecialVar_Result, MON_CANT_GIVE);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES), SPECIES_NONE);
    EXPECT(!Nuzlocke_IsMapsecUsed(MAPSEC_ROUTE_101));

    RUN_OVERWORLD_SCRIPT(
        giveegg SPECIES_WYNAUT
    );
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES), SPECIES_NONE);

    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_LOTAD, 5
    );
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES), SPECIES_LOTAD);
}

TEST("Nuzlocke: Monotype blocks evolving into an off-type Pokémon")
{
    enum Type type;
    enum Species target;
    PARAMETRIZE { type = TYPE_GROUND; target = SPECIES_NONE; }
    PARAMETRIZE { type = TYPE_BUG; target = SPECIES_NINJASK; }
    PARAMETRIZE { type = TYPE_NONE; target = SPECIES_NINJASK; }
    struct Pokemon mon;
    StartMonotype(type);

    CreateRandomMon(&mon, SPECIES_NINCADA, 20);
    EXPECT_EQ(GetEvolutionTargetSpecies(&mon, EVO_MODE_NORMAL, ITEM_NONE, NULL, NULL, CHECK_EVO), target);
}

TEST("Nuzlocke: Monotype starters all have the chosen type")
{
    for (enum Type type = TYPE_NORMAL; type <= TYPE_FAIRY; type++)
    {
        if (type == TYPE_MYSTERY)
            continue;
        StartMonotype(type);
        for (u32 i = 0; i < 3; i++)
        {
            enum Species species = GetStarterPokemon(i);
            EXPECT(GetSpeciesType(species, 0) == type || GetSpeciesType(species, 1) == type);
            EXPECT_EQ(GetSpeciesPreEvolution(species), SPECIES_NONE);
        }
    }
    StartMonotype(TYPE_NONE);
    EXPECT_EQ(GetStarterPokemon(0), SPECIES_TREECKO);
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

#define GRAVEYARD_BOX (TOTAL_BOXES_COUNT - 2)

static void GivePartyMon(u32 slot, enum Species species, u32 hp, enum Item item)
{
    CreateRandomMon(&gParties[B_TRAINER_PLAYER][slot], species, 5);
    SetMonData(&gParties[B_TRAINER_PLAYER][slot], MON_DATA_HP, &hp);
    SetMonData(&gParties[B_TRAINER_PLAYER][slot], MON_DATA_HELD_ITEM, &item);
}

static void StartGraveyardTest(bool32 enabled)
{
    StartNuzlocke(enabled);
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    ClearBag();
    gBattleTypeFlags = 0;
    GivePartyMon(0, SPECIES_ZIGZAGOON, 0, ITEM_ORAN_BERRY);
    GivePartyMon(1, SPECIES_WURMPLE, 10, ITEM_NONE);
    CalculatePlayerPartyCount();
}

TEST("Nuzlocke: Fainted Pokémon are moved to the graveyard box after battle")
{
    StartGraveyardTest(TRUE);

    Nuzlocke_BuryFaintedPartyMons();

    EXPECT_EQ(gPartiesCount[B_TRAINER_PLAYER], 1);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES), SPECIES_WURMPLE);
    EXPECT_EQ(GetBoxMonDataAt(GRAVEYARD_BOX, 0, MON_DATA_SPECIES), SPECIES_ZIGZAGOON);
    EXPECT_EQ(GetBoxMonDataAt(GRAVEYARD_BOX, 0, MON_DATA_HELD_ITEM), ITEM_NONE);
    EXPECT(CheckBagHasItem(ITEM_ORAN_BERRY, 1));
}

TEST("Nuzlocke: Nothing is buried when the rules are off, after a whiteout, or in the Battle Frontier")
{
    u32 setup;
    PARAMETRIZE { setup = 0; }
    PARAMETRIZE { setup = 1; }
    PARAMETRIZE { setup = 2; }
    StartGraveyardTest(setup != 0);
    if (setup == 1)
        GivePartyMon(1, SPECIES_WURMPLE, 0, ITEM_NONE);
    if (setup == 2)
        gBattleTypeFlags = BATTLE_TYPE_PYRAMID;

    Nuzlocke_BuryFaintedPartyMons();

    EXPECT_EQ(gPartiesCount[B_TRAINER_PLAYER], 2);
    EXPECT_EQ(GetBoxMonDataAt(GRAVEYARD_BOX, 0, MON_DATA_SPECIES), SPECIES_NONE);
}

TEST("Nuzlocke: Caught Pokémon are never sent to the graveyard box")
{
    struct Pokemon mon;
    StartGraveyardTest(TRUE);
    gPokemonStoragePtr->currentBox = GRAVEYARD_BOX;
    VarSet(VAR_PC_BOX_TO_SEND_MON, GRAVEYARD_BOX);
    CreateRandomMon(&mon, SPECIES_POOCHYENA, 5);

    EXPECT_EQ(CopyMonToPC(&mon), MON_GIVEN_TO_PC);
    EXPECT_NE(gSpecialVar_MonBoxId, GRAVEYARD_BOX);
    EXPECT_EQ(GetBoxMonDataAt(GRAVEYARD_BOX, 0, MON_DATA_SPECIES), SPECIES_NONE);
}

TEST("Nuzlocke: A new game names the two graveyard boxes")
{
    static const u8 sGraves1[] = _("GRAVES 1");
    static const u8 sGraves2[] = _("GRAVES 2");
    ResetPokemonStorageSystem();
    ClearBag();

    Nuzlocke_SetPendingRules((struct NuzlockeSave){ .enabled = TRUE });
    Nuzlocke_NewGame();

    EXPECT_EQ(StringCompare(GetBoxNamePtr(GRAVEYARD_BOX), sGraves1), 0);
    EXPECT_EQ(StringCompare(GetBoxNamePtr(GRAVEYARD_BOX + 1), sGraves2), 0);
    EXPECT(!Nuzlocke_IsGraveyardBox(GRAVEYARD_BOX - 1));
    EXPECT(Nuzlocke_IsGraveyardBox(GRAVEYARD_BOX + 1));
}

TEST("Nuzlocke: The graveyard holds 60 Pokémon across two boxes")
{
    StartGraveyardTest(TRUE);
    for (u32 pos = 0; pos < IN_BOX_COUNT; pos++)
        SetBoxMonAt(GRAVEYARD_BOX, pos, &gParties[B_TRAINER_PLAYER][1].box);

    Nuzlocke_BuryFaintedPartyMons();

    EXPECT_EQ(GetBoxMonDataAt(GRAVEYARD_BOX + 1, 0, MON_DATA_SPECIES), SPECIES_ZIGZAGOON);
}

TEST("Nuzlocke: A soft whiteout buries the party and brings in the first Pokémon from the PC")
{
    struct Pokemon stored;
    StartGraveyardTest(TRUE);
    GivePartyMon(1, SPECIES_WURMPLE, 0, ITEM_NONE);
    CreateRandomMon(&stored, SPECIES_POOCHYENA, 5);
    SetBoxMonAt(0, 3, &stored.box);

    Nuzlocke_OnWhiteOut();

    EXPECT(!Nuzlocke_IsRunOver());
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES), SPECIES_POOCHYENA);
    EXPECT_EQ(GetBoxMonDataAt(0, 3, MON_DATA_SPECIES), SPECIES_NONE);
    EXPECT_EQ(GetBoxMonDataAt(GRAVEYARD_BOX, 0, MON_DATA_SPECIES), SPECIES_ZIGZAGOON);
    EXPECT_EQ(GetBoxMonDataAt(GRAVEYARD_BOX, 1, MON_DATA_SPECIES), SPECIES_WURMPLE);
}

TEST("Nuzlocke: A whiteout with Pokémon that sat out the battle only buries the fainted ones")
{
    StartGraveyardTest(TRUE);

    Nuzlocke_OnWhiteOut();

    EXPECT(!Nuzlocke_IsRunOver());
    EXPECT_EQ(gPartiesCount[B_TRAINER_PLAYER], 1);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES), SPECIES_WURMPLE);
}

// ponytail: the hard whiteout isn't tested here, since ClearSaveData would erase the test runner's save
TEST("Nuzlocke: A soft whiteout with an empty PC ends the run")
{
    static const u8 sRunOver[] = _("Your NUZLOCKE run is over…\nYour last save will be loaded.\p");
    StartGraveyardTest(TRUE);
    GivePartyMon(1, SPECIES_WURMPLE, 0, ITEM_NONE);

    Nuzlocke_OnWhiteOut();

    EXPECT(Nuzlocke_IsRunOver());
    EXPECT_EQ(StringCompare(Nuzlocke_GetRunOverMessage(), sRunOver), 0);
    EXPECT_EQ(GetBoxMonDataAt(GRAVEYARD_BOX, 1, MON_DATA_SPECIES), SPECIES_WURMPLE);
}
