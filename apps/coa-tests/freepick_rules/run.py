CLI_DESCRIPTION = """Check the free-pick Hero's Character Advancement rules without a server.

Compiles the free-pick rule code against the client DBC set the server loads (--dbc-dir), then checks the Hero
essence budget, class and realm admission, masteries, choice groups, investments, the unit rules, and how a
known-entries upload is accepted, reordered, refused and charged, and which entries a followed Hero Architect
build asks for at a level.
No database, server build or game client is needed.
"""

import argparse
import os
import re
import struct
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(HERE.parent))
from coa_talent_catalog import STUBS  # noqa: E402
from client_data import dbc_dir  # noqa: E402

MAIN = r"""
#include "AscensionFreepickRules.h"
#include "DBCStores.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>

using namespace AscensionFreepick;

namespace
{
int failures = 0;
void Check(bool value, char const* name)
{
    failures += !value;
    std::printf("%s: %s\n", value ? "PASS" : "FAIL", name);
}

constexpr std::uint32_t PURIFY = 102;
constexpr std::uint32_t FERAL_SHAPESHIFT_MASTERY = 42016;
constexpr std::uint32_t BEAR_FORM = 42083;
constexpr std::uint32_t LIVE_FERAL_SHAPESHIFT_MASTERY = 1370;
constexpr std::uint32_t LIVE_BEAR_FORM = 2855;
constexpr std::uint32_t LIVE_CAT_FORM = 2728;
constexpr std::uint32_t PRESENCE_MASTERY = 1322;
constexpr std::uint32_t AIR_TOTEM_MASTERY = 1317;
constexpr std::uint32_t BLOOD_PRESENCE = 1169;
constexpr std::uint32_t TRACKING_MASTERY = 3042;
constexpr std::uint32_t TRACK_BEASTS = 1379;
constexpr std::uint32_t TRACK_BEASTS_SPELL = 1494;
constexpr std::uint32_t TAME_BEAST_ABILITIES = 40308;
constexpr std::uint32_t DOMINATE_UNDEAD = 41916;
constexpr std::uint32_t PATH_OF_STRENGTH = 1149;
constexpr std::uint32_t PATH_OF_AGILITY = 1150;
constexpr std::uint32_t BLOOD_GORGED_RANK_5 = 61158;
constexpr std::uint32_t WILL_OF_THE_NECROPOLIS_RANK_3 = 50150;
constexpr std::uint32_t PURIFY_SPELL = 1152;
constexpr std::uint32_t IMPROVED_HEROIC_STRIKE = 12228;
constexpr std::uint32_t WARRIOR = 1;
constexpr std::uint32_t DEATH_KNIGHT = 6;
constexpr std::uint32_t DRUID = 11;

bool Holds(std::vector<Entry> const& entries, std::uint32_t id, std::uint32_t rank)
{
    for (Entry const& entry : entries)
        if (entry.EntryId == id)
            return entry.Rank == rank;
    return false;
}

std::size_t PositionOf(std::vector<Entry> const& entries, std::uint32_t id)
{
    for (std::size_t index = 0; index < entries.size(); ++index)
        if (entries[index].EntryId == id)
            return index;
    return entries.size();
}

std::uint32_t DwordAt(std::vector<std::uint8_t> const& bytes, std::size_t at)
{
    if (at + 4 > bytes.size())
        return 0;
    return bytes[at] | bytes[at + 1] << 8 | bytes[at + 2] << 16 | std::uint32_t(bytes[at + 3]) << 24;
}

std::uint32_t PatchedFlags(std::vector<std::uint8_t> const& patch)
{
    std::string const expansion = "EXPANSION_";
    auto const text = std::search(patch.begin(), patch.end(), expansion.begin(), expansion.end());
    return DwordAt(patch, std::find(text, patch.end(), 0) - patch.begin() + 1);
}

std::uint32_t PatchedMastery(std::vector<std::uint8_t> const& patch)
{
    return patch.size() < 19 * 4 ? 0 : DwordAt(patch, patch.size() - 19 * 4);
}
}

int main(int, char** argv)
{
    DbcDirectory = std::string(argv[1]) + "/";
    Catalog catalog;
    Check(LoadCatalog(catalog), "the advancement, class type and essence tables load");
    std::vector<std::uint32_t> const priced = PriceLiveMasteries(catalog);
    std::size_t const pricedMasteries = std::count_if(priced.begin(), priced.end(),
        [&catalog](std::uint32_t id) { return catalog.Find(id)->Has(ROW_MASTERY); });
    std::printf("re-priced %zu live mastery members and %zu masteries\n", priced.size() - pricedMasteries,
        pricedMasteries);
    Check(priced.size() == 113 && pricedMasteries == 22 && RowPatches(catalog, priced).size() == 113,
        "the 91 live members of a catalog mastery and 22 masteries are patched as the client prices them");
    std::size_t liveMasteries = 0, twoAEMasteries = 0;
    for (auto const& [id, row] : catalog.Rows)
        if (row.Has(ROW_MASTERY) && row.Realms[0])
        {
            ++liveMasteries;
            twoAEMasteries += row.AECost == 2;
        }
    Check(liveMasteries == 22 && twoAEMasteries == 22, "the 21 live masteries and Tracking Mastery all cost 2 AE");
    Row const* tracking = catalog.Find(TRACKING_MASTERY);
    std::vector<std::uint32_t> tracks;
    bool tracksFree = true;
    for (std::uint32_t id : catalog.RowOrder)
    {
        Row const& row = catalog.Rows.at(id);
        if (std::find(row.Masteries.begin(), row.Masteries.end(), TRACKING_MASTERY) == row.Masteries.end())
            continue;
        tracks.push_back(id);
        tracksFree = tracksFree && !row.AECost &&
            std::find(row.Required.begin(), row.Required.end(), TRACKING_MASTERY) != row.Required.end();
    }
    Check(tracking && tracking->Has(ROW_MASTERY) && tracking->AECost == 2 && tracking->RequiredLevel == 1 &&
            tracks.size() == 8 && tracksFree,
        "Tracking Mastery is a 2 AE mastery at level 1 with eight Track members, each free and requiring it");
    std::vector<std::vector<std::uint8_t>> const trackingPatches =
        RowPatches(catalog, { TRACKING_MASTERY, TRACK_BEASTS });
    Check(trackingPatches.size() == 2 && PatchedFlags(trackingPatches[0]) == 0x1010 &&
            PatchedFlags(trackingPatches[1]) == 0x100000 && PatchedMastery(trackingPatches[1]) == TRACKING_MASTERY,
        "the Tracking Mastery patch carries flags 0x1010, and Track Beasts' 0x100000 and Tracking Mastery");
    Check(catalog.Find(PRESENCE_MASTERY)->RequiredLevel == 1 && catalog.Find(AIR_TOTEM_MASTERY)->RequiredLevel == 30,
        "Presence Mastery is lowered to Blood Presence's level 1 and Air Totem Mastery to level 30");

    Realm realm;
    realm.Live = true;
    realm.Seasonal = true;

    Check(Build(catalog, realm, 12).AEBudget() == 32 && Build(catalog, realm, 12).TEBudget() == 3,
        "a level 12 Hero has the 32 AE and 3 TE seen on live Ascension");
    Check(Build(catalog, realm, 80).AEBudget() == 140 && Build(catalog, realm, 80).TEBudget() == 71,
        "a level 80 Hero has 140 AE and 71 TE");

    Row const* purify = catalog.Find(PURIFY);
    Row const* bear = catalog.Find(BEAR_FORM);
    Row const* tame = catalog.Find(TAME_BEAST_ABILITIES);
    Check(purify && bear && tame && catalog.Find(FERAL_SHAPESHIFT_MASTERY) && catalog.Find(DOMINATE_UNDEAD),
        "the entries the checks use are in the catalog");
    if (!purify || !bear || !tame)
        return 1;

    Check(ClassAdmits(catalog, *purify, HERO_CLASS) && ClassAdmits(catalog, *bear, HERO_CLASS),
        "the classic class trees admit a Hero");
    Check(!ClassAdmits(catalog, *purify, 2), "a Hero tree entry refuses a stock class");
    Check(Visible(catalog, realm, *purify), "Purify is offered on a live seasonal free-pick realm");
    Realm seasonal;
    seasonal.Seasonal = true;
    Check(purify->Realms[0] && !purify->Realms[1] && !Visible(catalog, seasonal, *purify),
        "an entry flagged for live realms only is hidden on a seasonal-only realm");
    std::size_t heroEntries = 0;
    for (auto const& [id, row] : catalog.Rows)
        heroEntries += ClassAdmits(catalog, row, HERO_CLASS) && Visible(catalog, realm, row);
    Check(heroEntries > 3000, "a live seasonal free-pick realm offers the Hero thousands of entries");

    Check(Build(catalog, realm, 12).ValidateLearn(PURIFY, nullptr) == LEARN_OK, "Purify is learnable at level 12");
    Check(Build(catalog, realm, 9).ValidateLearn(PURIFY, nullptr) == LEARN_LOW_LEVEL, "Purify is refused at level 9");
    Check(Build(catalog, realm, 12, { { PURIFY, purify->MaxRank() } }).ValidateLearn(PURIFY, nullptr) ==
        LEARN_ALREADY_KNOWN, "a rank past the entry's last is refused");

    Check(Build(catalog, realm, 12).ValidateLearn(BEAR_FORM, nullptr) == LEARN_MISSING_REQUIRED_ID,
        "Bear Form needs its mastery");
    Build druid(catalog, realm, 12, { { FERAL_SHAPESHIFT_MASTERY, 1 } });
    Check(druid.AutoLearn(nullptr) >= 1 && druid.RankOf(BEAR_FORM) == 1,
        "the mastery hands over Bear Form, which costs nothing");
    Check(druid.GlobalAE(0) == catalog.Find(FERAL_SHAPESHIFT_MASTERY)->AECost,
        "only the mastery is charged");

    Realm live;
    live.Live = true;
    Row const* liveMastery = catalog.Find(LIVE_FERAL_SHAPESHIFT_MASTERY);
    Row const* liveBear = catalog.Find(LIVE_BEAR_FORM);
    Row const* liveCat = catalog.Find(LIVE_CAT_FORM);
    Check(liveMastery && liveBear && liveCat && liveBear->Masteries[0] == LIVE_FERAL_SHAPESHIFT_MASTERY &&
            liveCat->Masteries[0] == LIVE_FERAL_SHAPESHIFT_MASTERY,
        "the live Bear Form and Cat Form name the live Feral Shapeshift Mastery");
    if (!liveMastery || !liveBear || !liveCat)
        return 1;
    Check(Build(catalog, live, 12).ValidateLearn(LIVE_BEAR_FORM, nullptr) == LEARN_MISSING_REQUIRED_ID,
        "on a live realm Bear Form without its mastery is refused");
    Build liveDruid(catalog, live, 12, { { LIVE_FERAL_SHAPESHIFT_MASTERY, 1 } });
    liveDruid.AutoLearn(nullptr);
    Check(liveDruid.RankOf(LIVE_BEAR_FORM) == 1 && liveDruid.RankOf(LIVE_CAT_FORM) == 1,
        "on a live realm the mastery hands over Bear Form and Cat Form");
    Check(liveDruid.GlobalAE(0) == 2, "on a live realm only the mastery's 2 AE is charged");
    std::vector<std::vector<std::uint8_t>> const patches = RowPatches(catalog, { LIVE_BEAR_FORM });
    std::string const bearText = "Bear Form" + std::string(1, '\0') + "Ability_Racial_BearForm" + std::string(3, '\0');
    Check(patches.size() == 1 &&
            std::search(patches[0].begin(), patches[0].end(), bearText.begin(), bearText.end()) != patches[0].end(),
        "the live Bear Form row patch carries its name and icon, and the two empty texts after them, from its record");
    ApplyCheck const liveUpload = CheckApply(Build(catalog, live, 12),
        { { LIVE_BEAR_FORM, 1 }, { LIVE_FERAL_SHAPESHIFT_MASTERY, 1 } }, nullptr, {});
    Check(liveUpload.Result == UPDATE_OK && Holds(liveUpload.Entries, LIVE_BEAR_FORM, 1) &&
            PositionOf(liveUpload.Entries, LIVE_FERAL_SHAPESHIFT_MASTERY) <
                PositionOf(liveUpload.Entries, LIVE_BEAR_FORM) &&
            Build(catalog, live, 12, liveUpload.Entries).GlobalAE(0) == 2,
        "on a live realm an upload naming Bear Form before its mastery is reordered and charges the mastery's 2 AE");
    std::vector<Entry> const liveGuide = FitGuide(catalog, live, 80, HERO_CLASS, GuideEntries(catalog, live, 80,
        HERO_CLASS, { { liveBear->Spells[0], 10 }, { liveCat->Spells[0], 10 }, { liveMastery->Spells[0], 10 } }, 2),
        {});
    Check(liveGuide.size() == 3 && Holds(liveGuide, LIVE_FERAL_SHAPESHIFT_MASTERY, 1) &&
            Holds(liveGuide, LIVE_BEAR_FORM, 1) && Holds(liveGuide, LIVE_CAT_FORM, 1) &&
            Build(catalog, live, 80, liveGuide).GlobalAE(0) == liveMastery->AECost &&
            CheckApply(Build(catalog, live, 80), liveGuide, nullptr, {}).Result == UPDATE_OK,
        "a followed build listing the forms before their live mastery holds all three for the mastery's cost");
    ApplyCheck const bloodPresence = CheckApply(Build(catalog, live, 1),
        FitGuide(catalog, live, 1, HERO_CLASS, { { BLOOD_PRESENCE, 1 } }, {}), nullptr, {});
    Check(bloodPresence.Result == UPDATE_OK && Holds(bloodPresence.Entries, PRESENCE_MASTERY, 1) &&
            Holds(bloodPresence.Entries, BLOOD_PRESENCE, 1) &&
            Build(catalog, live, 1, bloodPresence.Entries).GlobalAE(0) == 2,
        "a level 1 Hero learns Blood Presence through Presence Mastery for 2 AE");
    UnitCheck const anyone = [](std::uint32_t, Row const&) { return true; };
    auto const followGuide = [&catalog, &live, &anyone](Build const& stored, std::vector<GuideSpell> const& spells)
    {
        ApplyCheck check = CheckApply(stored, FitGuide(catalog, live, stored.Level(), HERO_CLASS,
            GuideEntries(catalog, live, stored.Level(), HERO_CLASS, spells, 2), GuideKeeps(catalog, stored.Entries())),
            anyone, { 1000000, 0 });
        Build built(stored);
        built.SetEntries(check.Entries);
        built.AutoLearn(anyone);
        check.Entries = built.Entries();
        return check;
    };
    ApplyCheck const bearGuide = followGuide(Build(catalog, live, 20), { { liveBear->Spells[0], 10 } });
    Check(bearGuide.Result == UPDATE_OK && bearGuide.Entries.size() == 3 &&
            Holds(bearGuide.Entries, LIVE_FERAL_SHAPESHIFT_MASTERY, 1) && Holds(bearGuide.Entries, LIVE_BEAR_FORM, 1) &&
            Holds(bearGuide.Entries, LIVE_CAT_FORM, 1) && Build(catalog, live, 20, bearGuide.Entries).GlobalAE(0) == 2,
        "a followed build listing Bear Form's spell alone ends holding the mastery and both forms for 2 AE");
    Build tracker(catalog, live, 1, { { TRACKING_MASTERY, 1 } });
    tracker.AutoLearn(nullptr);
    Build trackerAt50(catalog, live, 50, tracker.Entries());
    trackerAt50.AutoLearn(nullptr);
    Check(tracker.Entries().size() == 2 && tracker.Has(TRACK_BEASTS) && trackerAt50.Entries().size() == 9,
        "a level 1 Hero holding Tracking Mastery gets Track Beasts only, and all eight tracks at level 50");
    ApplyCheck const trackGuide = followGuide(Build(catalog, live, 1), { { TRACK_BEASTS_SPELL, 1 } });
    Check(catalog.Find(TRACK_BEASTS)->Spells[0] == TRACK_BEASTS_SPELL && trackGuide.Result == UPDATE_OK &&
            trackGuide.Entries.size() == 2 && Holds(trackGuide.Entries, TRACKING_MASTERY, 1) &&
            Holds(trackGuide.Entries, TRACK_BEASTS, 1) && Build(catalog, live, 1, trackGuide.Entries).GlobalAE(0) == 2,
        "a followed build listing Track Beasts' spell alone ends holding it and Tracking Mastery for 2 AE");
    Build feralHero(catalog, live, 20, { { LIVE_FERAL_SHAPESHIFT_MASTERY, 1 } });
    feralHero.AutoLearn(anyone);
    ApplyCheck const leftGuide = followGuide(feralHero, { { PURIFY_SPELL, 12 } });
    Check(feralHero.Entries().size() == 3 && leftGuide.Result == UPDATE_OK && Holds(leftGuide.Entries, PURIFY, 1) &&
            !Holds(leftGuide.Entries, LIVE_FERAL_SHAPESHIFT_MASTERY, 1) &&
            !Holds(leftGuide.Entries, LIVE_BEAR_FORM, 1) && !Holds(leftGuide.Entries, LIVE_CAT_FORM, 1),
        "a character holding the live mastery follows a build without it, which drops the mastery and both forms");
    Build spentDown(catalog, live, 20);
    std::vector<Entry> spentEntries;
    for (std::uint32_t id : catalog.RowOrder)
    {
        Row const& row = catalog.Rows.at(id);
        if (spentDown.RemainingAE() <= liveMastery->AECost)
            break;
        if (row.AECost && row.AECost + liveMastery->AECost <= spentDown.RemainingAE() && !row.Group &&
            !row.Has(ROW_MASTERY) && !row.Masteries[0] && spentDown.ValidateLearn(id, nullptr) == LEARN_OK)
        {
            spentDown.AddRank(id);
            spentEntries.push_back({ id, 1 });
        }
    }
    std::vector<Entry> const tightGuide = FitGuide(catalog, live, 20, HERO_CLASS,
        { { LIVE_BEAR_FORM, 1 }, { LIVE_CAT_FORM, 1 }, { LIVE_FERAL_SHAPESHIFT_MASTERY, 1 } }, spentEntries);
    Check(spentDown.RemainingAE() == liveMastery->AECost && Holds(tightGuide, LIVE_FERAL_SHAPESHIFT_MASTERY, 1) &&
            Holds(tightGuide, LIVE_BEAR_FORM, 1) && Holds(tightGuide, LIVE_CAT_FORM, 1),
        "a followed build with only the mastery's essence left learns the mastery before the forms it lists first");

    Check(Build(catalog, realm, 20).ValidateLearn(TAME_BEAST_ABILITIES, nullptr) == LEARN_NOT_ENOUGH_INVESTED_AE,
        "a taming ability needs essence invested first");
    Check(Build(catalog, realm, 20, { { TAME_BEAST_ABILITIES, 1 } }).ValidateLearn(DOMINATE_UNDEAD, nullptr) ==
        LEARN_GROUP, "a second taming ability is refused: they share a choice group");

    UnitCheck const dead = [](std::uint32_t slot, Row const&) { return slot != LEARN_NOT_WHILE_DEAD; };
    Check(Build(catalog, realm, 12).ValidateLearn(PURIFY, dead) == LEARN_NOT_WHILE_DEAD, "the dead learn nothing");

    Build const base(catalog, realm, 12);
    ApplyCheck applied = CheckApply(base, { { PURIFY, 1 } }, nullptr, {});
    Check(applied.Result == UPDATE_OK && Holds(applied.Entries, PURIFY, 1), "an upload adding Purify applies");
    Check(CheckApply(base, {}, nullptr, {}).Result == UPDATE_NO_DIFF, "an upload that changes nothing is NO_DIFF");
    applied = CheckApply(base, { { 999999, 1 } }, nullptr, {});
    Check(applied.Result == UPDATE_BAD_ENTRY && applied.Failed.EntryId == 999999, "an unknown entry is BAD_ENTRY");
    Check(CheckApply(base, { { PURIFY, 1 }, { PURIFY, 1 } }, nullptr, {}).Result == UPDATE_BAD_ENTRY,
        "an entry listed twice is BAD_ENTRY");

    Check(CheckApply(base, { { PATH_OF_STRENGTH, 1 } }, nullptr, {}).Result == UPDATE_OK,
        "a level 1 Hero can choose a path");
    Check(CheckApply(base, { { PATH_OF_STRENGTH, 1 }, { PATH_OF_AGILITY, 1 } }, nullptr, {}).Result ==
        UPDATE_BAD_ENTRY, "two paths at once are BAD_ENTRY: a choice group holds one entry");
    Build const strength(catalog, realm, 20, { { PATH_OF_STRENGTH, 1 } });
    applied = CheckApply(strength, { { PATH_OF_AGILITY, 1 } }, nullptr, {});
    Check(applied.Result == UPDATE_OK && Holds(applied.Entries, PATH_OF_AGILITY, 1) && !applied.Money &&
        !applied.Marks, "switching path replaces the old one and costs nothing");

    applied = CheckApply(base, { { BEAR_FORM, 1 }, { FERAL_SHAPESHIFT_MASTERY, 1 } }, nullptr, {});
    Check(applied.Result == UPDATE_OK &&
            PositionOf(applied.Entries, FERAL_SHAPESHIFT_MASTERY) < PositionOf(applied.Entries, BEAR_FORM),
        "an upload naming Bear Form before its mastery is reordered, not refused");

    std::vector<Entry> greedy;
    std::uint32_t spent = 0;
    for (std::uint32_t id : catalog.RowOrder)
    {
        Row const& row = catalog.Rows.at(id);
        if (!row.AECost || row.TECost || Build(catalog, realm, 12).ValidateLearn(id, nullptr) != LEARN_OK)
            continue;
        greedy.push_back({ id, 1 });
        spent += row.AECost;
        if (spent > 32)
            break;
    }
    applied = CheckApply(base, greedy, nullptr, {});
    Check(spent > 32 && applied.Result == UPDATE_NOT_TRAVERSIBLE && applied.Learn == LEARN_MISSING_AE,
        "an upload over the essence budget is NOT_TRAVERSIBLE with MISSING_AE");

    Build const trained(catalog, realm, 20, { { PURIFY, 1 } });
    applied = CheckApply(trained, {}, nullptr, { 0, 250 });
    Check(applied.Result == UPDATE_OK && applied.Marks == 250 && applied.Money == 0,
        "unlearning at level 20 spends 250 Marks of Ascension while they last");
    applied = CheckApply(trained, {}, nullptr, { 71 * 20, 0 });
    Check(applied.Result == UPDATE_OK && applied.Money == 71 * 20 && applied.Marks == 0,
        "without marks the level 20 unlearn costs 71 copper per level");
    Check(CheckApply(trained, {}, nullptr, { 71 * 20 - 1, 0 }).Result == UPDATE_BAD_UPDATE_COSTS,
        "an unlearn nobody can pay for is BAD_UPDATE_COSTS");
    applied = CheckApply(Build(catalog, realm, 10, { { PURIFY, 1 } }), {}, nullptr, {});
    Check(applied.Result == UPDATE_OK && !applied.Money && !applied.Marks, "unlearning is free up to level 10");

    Row const* bySpell = RowBySpell(catalog, realm, HERO_CLASS, PURIFY_SPELL);
    Check(bySpell && bySpell->EntryId == PURIFY, "a Hero Architect build spell finds the entry that teaches it");
    Check(!RowBySpell(catalog, realm, HERO_CLASS, 999999), "a spell no entry teaches finds nothing");
    std::uint32_t const bearSpell = bear->Spells[0];
    Row const* bearTeacher = RowBySpell(catalog, realm, HERO_CLASS, bearSpell);
    Check(bearTeacher && bearTeacher->EntryId <= BEAR_FORM && ClassAdmits(catalog, *bearTeacher, HERO_CLASS),
        "a spell several entries teach finds the lowest visible entry the Hero may learn, as the client does");
    std::vector<GuideSpell> const guide = { { PURIFY_SPELL, 12 }, { bearSpell, 10 }, { 999999, 1 } };
    std::vector<Entry> guided = GuideEntries(catalog, realm, 12, HERO_CLASS, guide, 0);
    Check(bearTeacher && Holds(guided, PURIFY, 1) && Holds(guided, bearTeacher->EntryId, 1),
        "a followed build at level 12 takes every spell of level 12 or below");
    Check(Holds(guided, PATH_OF_STRENGTH, 1), "a Strength build also takes the Path of Strength");
    Check(guided.size() == 3, "a spell no entry teaches adds nothing");
    guided = GuideEntries(catalog, realm, 11, HERO_CLASS, guide, 1);
    Check(!Holds(guided, PURIFY, 1) && guided.size() == 2 && Holds(guided, PATH_OF_AGILITY, 1),
        "at level 11 the level 12 spell waits, and an Agility build takes the Path of Agility");
    std::vector<Entry> const twice = GuideEntries(catalog, realm, 12, HERO_CLASS,
        { { PURIFY_SPELL, 1 }, { PURIFY_SPELL, 2 } }, 2);
    Check(twice.size() == 1 && Holds(twice, PURIFY, 2),
        "a spell listed twice adds a second rank, and a Stamina build takes no path");
    std::vector<Entry> fitted = FitGuide(catalog, realm, 9, HERO_CLASS, { { PURIFY, 1 }, { PATH_OF_STRENGTH, 1 } }, {});
    Check(fitted.size() == 1 && Holds(fitted, PATH_OF_STRENGTH, 1),
        "a followed entry the level does not allow yet waits, and the rest is still learned");
    fitted = FitGuide(catalog, realm, 12, HERO_CLASS, { { BEAR_FORM, 1 }, { FERAL_SHAPESHIFT_MASTERY, 1 } }, {});
    Check(Holds(fitted, FERAL_SHAPESHIFT_MASTERY, 1) && Holds(fitted, BEAR_FORM, 1) &&
            PositionOf(fitted, FERAL_SHAPESHIFT_MASTERY) < PositionOf(fitted, BEAR_FORM),
        "a followed entry listed before its requirement is learned after it");
    fitted = FitGuide(catalog, realm, 12, HERO_CLASS, { { PATH_OF_AGILITY, 1 } }, { { PATH_OF_STRENGTH, 1 } });
    Check(fitted.size() == 1 && Holds(fitted, PATH_OF_AGILITY, 1),
        "the followed build's path replaces the path the character kept");
    fitted = FitGuide(catalog, realm, 12, HERO_CLASS, { { PURIFY, 1 } }, { { PATH_OF_STRENGTH, 1 } });
    Check(fitted.size() == 2 && Holds(fitted, PATH_OF_STRENGTH, 1), "a kept entry the build does not replace stays");
    applied = CheckApply(base, GuideEntries(catalog, realm, 12, HERO_CLASS, { { PURIFY_SPELL, 12 } }, 0), nullptr, {});
    Check(applied.Result == UPDATE_OK && Holds(applied.Entries, PURIFY, 1) && Holds(applied.Entries, PATH_OF_STRENGTH, 1),
        "the followed build applies through the upload rules");

    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> const learnEffects = { LEARN_EFFECTS };
    LearnedSpells const learned = [&learnEffects](std::uint32_t spellId)
    {
        auto const itr = learnEffects.find(spellId);
        return itr == learnEffects.end() ? std::vector<std::uint32_t>() : itr->second;
    };
    Check(TaughtSpells(BLOOD_GORGED_RANK_5, learned) == std::vector<std::uint32_t>({ 50111, 61278 }),
        "a rank that only teaches spells grants the spells it teaches");
    Check(TaughtSpells(WILL_OF_THE_NECROPOLIS_RANK_3, learned) == std::vector<std::uint32_t>({ 52286 }),
        "a rank with a learn effect beside other effects grants only the taught spell");
    Check(TaughtSpells(PURIFY_SPELL, learned) == std::vector<std::uint32_t>({ PURIFY_SPELL }),
        "a rank with no learn effect grants itself");

    Realm reborn;
    reborn.Live = true;
    reborn.WarcraftReborn = true;
    Check(Build(catalog, reborn, 20, {}, WARRIOR).TEBudget() == 11 &&
            Build(catalog, reborn, 20, {}, WARRIOR).AEBudget() == 20,
        "a level 20 Warrior on Warcraft Reborn has 11 TE, its level minus 9, and 20 AE");
    Row const* heroicStrike = catalog.Find(IMPROVED_HEROIC_STRIKE);
    Check(heroicStrike && Visible(catalog, reborn, *heroicStrike),
        "a Reborn Warrior talent is offered on Warcraft Reborn");
    Check(!Visible(catalog, reborn, *purify), "a Hero entry is hidden on Warcraft Reborn");
    std::size_t stockTalents = 0;
    for (auto const& [id, row] : catalog.Rows)
    {
        auto const type = catalog.ClassTypes.find(row.ClassType);
        std::uint32_t const classId = type == catalog.ClassTypes.end() ? 0 : type->second.Class;
        bool const stockClass = classId >= WARRIOR && classId <= DRUID && classId != HERO_CLASS &&
            classId != DEATH_KNIGHT;
        stockTalents += (row.Type == ENTRY_TALENT || row.Type == ENTRY_TALENT_ABILITY) && stockClass &&
            Visible(catalog, reborn, row);
    }
    Check(stockTalents == 761, "Warcraft Reborn offers the nine stock classes 761 talent entries");

    return failures ? 1 : 0;
}
"""


PATH_SPELLS = (84864, 84865, 84866, 84867, 129243)
SPELL_DESCRIPTION = 170


def tooltip_spells(dbc):
    data = (dbc / "Spell.dbc").read_bytes()
    count, _, size, _ = struct.unpack_from("<4I", data, 4)
    strings = data[20 + count * size:]
    named = {}
    for row in range(count):
        spell_id = struct.unpack_from("<I", data, 20 + row * size)[0]
        if spell_id in PATH_SPELLS:
            offset = struct.unpack_from("<I", data, 20 + row * size + SPELL_DESCRIPTION * 4)[0]
            text = strings[offset:strings.index(bytes(1), offset)].decode("utf-8", "replace")
            named[spell_id] = sorted({int(value) for value in re.findall(r"@s:(\d+):", text)})
    return named


def companion_table():
    text = (ROOT / "src/server/coa/AscensionWildcard.cpp").read_text(encoding="utf-8")
    return {int(head): sorted(int(value) for value in spells.split(","))
            for head, spells in re.findall(r"\{ (\d+), \{ ([\d, ]+) \} \}", text)}


def check_path_passives(dbc):
    named, table = tooltip_spells(dbc), companion_table()
    ok = len(named) == len(PATH_SPELLS) and all(len(named[path]) == 2 and table.get(path) == named[path]
                                                 for path in PATH_SPELLS)
    print(f"{'PASS' if ok else 'FAIL'}: every path grants the two passives its tooltip names (@s:<spell>)")
    return ok


LEARN_SPELL_EFFECT = 36
SPELL_EFFECT, SPELL_TRIGGER = 71, 116


def learn_effects(directory):
    raw = (directory / "Spell.dbc").read_bytes()
    count, fields, size = struct.unpack_from("<III", raw, 4)
    rows = {row[0]: row for row in struct.iter_unpack(f"<{fields}I", raw[20:20 + count * size])}
    entries = []
    for spell in (61158, 50150, 1152):
        row = rows[spell]
        taught = [row[SPELL_TRIGGER + i] for i in range(3) if row[SPELL_EFFECT + i] == LEARN_SPELL_EFFECT]
        if taught:
            entries.append("{ %d, { %s } }" % (spell, ", ".join(map(str, taught))))
    source = (ROOT / "src/server/coa/AscensionFreepick.cpp").read_text(encoding="utf-8")
    assert source.count("RankSpells(row->Spells[") == 2, "free-pick grants and removals both expand rank spells"
    return ", ".join(entries)


def main():
    parser = argparse.ArgumentParser(description=CLI_DESCRIPTION)
    parser.add_argument("--dbc-dir", type=Path)
    args = parser.parse_args()
    args.dbc_dir = args.dbc_dir or dbc_dir()

    compiler = shutil.which(os.environ.get("CXX", "cl.exe" if os.name == "nt" else "c++"))
    assert compiler, "Enable a C++20 compiler (VS Developer PowerShell on Windows)."
    with tempfile.TemporaryDirectory(prefix="coa-freepick-rules-") as directory:
        out = Path(directory)
        for name, text in STUBS.items():
            (out / name).write_text(text, encoding="utf-8")
        (out / "main.cpp").write_text(MAIN.replace("LEARN_EFFECTS", learn_effects(args.dbc_dir.resolve())),
                                      encoding="utf-8")
        includes = [out, ROOT / "src/server/coa", ROOT / "src/server/shared/DataStores", ROOT / "src/common"]
        sources = [out / "main.cpp", ROOT / "src/server/coa/AscensionFreepickRules.cpp",
                   ROOT / "src/server/shared/DataStores/ClientDBC.cpp"]
        executable = out / ("rules.exe" if os.name == "nt" else "rules")
        if Path(compiler).stem.lower() == "cl":
            flags = ["/nologo", "/std:c++20", "/EHsc", "/utf-8", "/D_CRT_SECURE_NO_WARNINGS",
                     *["/I" + str(p) for p in includes], *map(str, sources), "/Fe" + str(executable)]
        else:
            flags = ["-std=c++20", "-Wall", "-Wextra", *["-I" + str(p) for p in includes], *map(str, sources),
                     "-o", str(executable)]
        build = subprocess.run([compiler, *flags], cwd=out, capture_output=True, text=True, errors="replace")
        if build.returncode:
            raise SystemExit("Free-pick rules harness did not compile:\n" + build.stdout + build.stderr)
        result = subprocess.run([str(executable), str(args.dbc_dir.resolve())], text=True)
        paths_ok = check_path_passives(args.dbc_dir.resolve())
        raise SystemExit(result.returncode or (0 if paths_ok else 1))


if __name__ == "__main__":
    main()
