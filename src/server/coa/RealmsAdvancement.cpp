/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

// wow-realms: advancement, free-pick serving the Warcraft Reborn realm's stock classes; the startup
// line is one of wow-realms' check/self-reports.

#include "RealmsAdvancement.h"
#include "AscensionFreepick.h"
#include "ClientDBC.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "RealmsBinding.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "Trainer.h"
#include "World.h"
#include <algorithm>
#include <atomic>
#include <unordered_map>

namespace Realms
{
namespace
{
constexpr std::uint32_t TREE_COUNT = 3;

std::unordered_map<uint8, TreeTabIds> ClassTreeTabs;
AscensionWarcraftReborn::Data RebornData;
AscensionFreepick::Realm RebornRealm;
bool RebornDataLoaded = false;
std::unordered_map<uint8, std::unordered_set<uint32>> ClassTrainerSpells;

bool StockPlayer(Player const* player)
{
    return AscensionFreepick::IsRebornCharacter(player);
}

bool StockClass(std::uint32_t classId)
{
    return classId >= CLASS_WARRIOR && classId <= CLASS_DRUID && classId != CLASS_HERO;
}

std::string WithoutSpaces(std::string name)
{
    std::erase(name, ' ');
    return name;
}

bool OverTEBudget(AscensionFreepick::Build const& build)
{
    return build.GlobalTE(0) > build.TEBudget();
}

bool TalentRow(AscensionFreepick::Row const& row)
{
    return row.Type == AscensionFreepick::ENTRY_TALENT || row.Type == AscensionFreepick::ENTRY_TALENT_ABILITY;
}

std::map<std::uint32_t, std::string> NamesOf(ClientDBC const& dbc)
{
    std::map<std::uint32_t, std::string> names;
    for (uint32 index = 0; index < dbc.GetRecordCount(); ++index)
    {
        ClientDBC::Record const record = dbc.GetRecord(index);
        names[record.GetUInt32(0)] = std::string(record.GetString(1));
    }
    return names;
}

void LoadStartingKits()
{
    RebornRealm = AscensionFreepick::ReadRealm();
    RebornDataLoaded = AscensionWarcraftReborn::LoadData(RebornData);
    if (!RebornDataLoaded)
    {
        LOG_ERROR("coa", "Realms advancement cannot forget to a starting kit: Reborn data did not load");
        return;
    }
    if (QueryResult result = WorldDatabase.Query("SELECT `class`, `spell` FROM `custom_wcr_trainer_source`"))
        do
        {
            Field const* fields = result->Fetch();
            ClassTrainerSpells[fields[0].Get<uint8>()].insert(fields[1].Get<uint32>());
        } while (result->NextRow());
}

uint32 StartLevel(Player const* player)
{
    return player->getClass() == CLASS_DEATH_KNIGHT ? sWorld->getIntConfig(CONFIG_START_HEROIC_PLAYER_LEVEL) :
        sWorld->getIntConfig(CONFIG_START_PLAYER_LEVEL);
}

std::vector<LineAbility> LineAbilitiesOf(std::uint32_t spellId)
{
    std::vector<LineAbility> abilities;
    SkillLineAbilityMapBounds const bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
        abilities.push_back({ itr->second->SkillLine, itr->second->AcquireMethod });
    return abilities;
}

void LoadTreeTabs(AscensionFreepick::Catalog const& catalog)
{
    ClientDBC tabTypes, talentTabs;
    if (!tabTypes.Load(GetClientDBCPath("CharacterAdvancementTabTypes.dbc"), 2) ||
        !talentTabs.Load(GetClientDBCPath("TalentTab.dbc"), 2))
    {
        LOG_ERROR("coa", "Realms advancement cannot map talent trees: its client DBCs did not load");
        return;
    }
    std::map<std::uint32_t, std::string> const tabTypeNames = NamesOf(tabTypes);
    std::map<std::uint32_t, std::string> const talentTabNames = NamesOf(talentTabs);

    std::map<uint8, std::map<std::uint32_t, std::string>> catalogTabs;
    for (auto const& [entryId, row] : catalog.Rows)
    {
        auto const type = catalog.ClassTypes.find(row.ClassType);
        if (!TalentRow(row) || type == catalog.ClassTypes.end() || !type->second.Stock ||
            !StockClass(type->second.Class))
            continue;
        auto const name = tabTypeNames.find(row.Tab);
        catalogTabs[uint8(type->second.Class)][row.Tab] = name == tabTypeNames.end() ? "" : name->second;
    }

    for (auto const& [classId, tabs] : catalogTabs)
    {
        uint32 const* pages = GetTalentTabPages(classId);
        std::array<std::string, TREE_COUNT> pageNames;
        for (std::uint32_t page = 0; page < TREE_COUNT; ++page)
            if (auto const name = talentTabNames.find(pages[page]); name != talentTabNames.end())
                pageNames[page] = name->second;
        TreeTabMatch const match = MatchTreeTabs(tabs, pageNames);
        if (!match.ByName)
            LOG_WARN("coa", "Realms advancement matched class {}'s talent trees {} {} {} partly by id, not by name",
                uint32(classId), match.Tabs[0], match.Tabs[1], match.Tabs[2]);
        ClassTreeTabs[classId] = match.Tabs;
    }
}
}

bool RefusesLegacyTalents(Player const* player)
{
    if (!StockPlayer(player))
        return false;
    LOG_DEBUG("network", "Refused a legacy talent opcode from {}: free-pick holds a stock class's talents",
        player->GetName());
    return true;
}

std::uint32_t StockCatalogRows(AscensionFreepick::Catalog const& catalog)
{
    std::uint32_t rows = 0;
    for (auto const& [entryId, row] : catalog.Rows)
    {
        auto const type = catalog.ClassTypes.find(row.ClassType);
        rows += type != catalog.ClassTypes.end() && type->second.Stock && !type->second.Hero &&
            !type->second.ConquestOfAzeroth;
    }
    return rows;
}

std::uint32_t BudgetedStockClasses(AscensionFreepick::Catalog const& catalog)
{
    std::uint32_t classes = 0;
    for (auto const& [classId, levels] : catalog.Budget)
        classes += StockClass(classId) && !levels.empty();
    return classes;
}

TreeTabMatch MatchTreeTabs(std::map<std::uint32_t, std::string> const& catalogTabs,
    std::array<std::string, 3> const& pageNames)
{
    TreeTabMatch match;
    std::vector<std::uint32_t> unmatched;
    for (auto const& [tab, name] : catalogTabs)
        unmatched.push_back(tab);
    std::array<bool, TREE_COUNT> found{};
    for (std::uint32_t page = 0; page < TREE_COUNT; ++page)
    {
        auto const tab = std::find_if(unmatched.begin(), unmatched.end(), [&](std::uint32_t id)
        {
            return !pageNames[page].empty() && WithoutSpaces(catalogTabs.at(id)) == WithoutSpaces(pageNames[page]);
        });
        if (tab == unmatched.end())
            continue;
        match.Tabs[page] = *tab;
        found[page] = true;
        unmatched.erase(tab);
    }
    auto next = unmatched.begin();
    for (std::uint32_t page = 0; page < TREE_COUNT; ++page)
    {
        if (found[page])
            continue;
        match.ByName = false;
        if (next != unmatched.end())
            match.Tabs[page] = *next++;
    }
    return match;
}

std::map<uint8, uint32> PointsPerTab(AscensionFreepick::Build const& build, TreeTabIds const& tabs)
{
    std::map<uint8, uint32> points{ { 0, 0 }, { 1, 0 }, { 2, 0 } };
    for (AscensionFreepick::Entry const& entry : build.Entries())
    {
        AscensionFreepick::Row const* row = build.Data().Find(entry.EntryId);
        if (!row || !TalentRow(*row))
            continue;
        for (uint8 tree = 0; tree < TREE_COUNT; ++tree)
            if (tabs[tree] && row->Tab == tabs[tree])
                points[tree] += row->TECost * entry.Rank;
    }
    return points;
}

std::uint32_t SpendBudget(AscensionFreepick::Build& build, TreeTabIds const& tabs, uint8 treeIndex,
    AscensionFreepick::UnitCheck const& unit)
{
    AscensionFreepick::Catalog const& catalog = build.Data();
    if (OverTEBudget(build))
        build.SetEntries({});
    std::uint32_t const before = build.GlobalTE(0);
    std::array<uint8, TREE_COUNT> order{ treeIndex, 0, 0 };
    for (uint8 tree = 0, slot = 1; tree < TREE_COUNT; ++tree)
        if (tree != treeIndex)
            order[slot++] = tree;

    for (uint8 tree : order)
    {
        std::vector<AscensionFreepick::Row const*> rows;
        for (std::uint32_t entryId : catalog.RowOrder)
        {
            AscensionFreepick::Row const& row = catalog.Rows.at(entryId);
            if (tabs[tree] && row.Tab == tabs[tree] && TalentRow(row) &&
                AscensionFreepick::ClassAdmits(catalog, row, build.Class()))
                rows.push_back(&row);
        }
        std::stable_sort(rows.begin(), rows.end(), [](auto const* left, auto const* right)
        {
            return left->RequiredLevel != right->RequiredLevel ? left->RequiredLevel < right->RequiredLevel :
                left->EntryId < right->EntryId;
        });
        for (bool progress = true; progress && build.RemainingTE();)
        {
            progress = false;
            for (AscensionFreepick::Row const* row : rows)
                while (build.RemainingTE() && build.ValidateLearn(row->EntryId, unit) == AscensionFreepick::LEARN_OK)
                {
                    build.AddRank(row->EntryId);
                    progress = true;
                }
        }
        if (!build.RemainingTE())
            break;
    }
    return build.GlobalTE(0) - before;
}

StartingKit KitFrom(AscensionWarcraftReborn::Data const& data, AscensionFreepick::Realm const& realm, uint8 classId,
    uint32 startLevel, uint32 level, std::unordered_set<uint32> trainerSpells)
{
    std::vector<std::uint32_t> const lines = AscensionWarcraftReborn::ClassSkillLines(data, realm, classId);
    std::vector<std::uint32_t> const starting =
        AscensionWarcraftReborn::StartingSpells(data, realm, classId, startLevel);
    std::vector<std::uint32_t> const automatic = AscensionWarcraftReborn::AutomaticSpells(data, realm, classId, level);
    StartingKit kit{ { lines.begin(), lines.end() }, { starting.begin(), starting.end() }, std::move(trainerSpells) };
    kit.CreateSpells.insert(automatic.begin(), automatic.end());
    return kit;
}

std::vector<std::uint32_t> SpellsToForget(std::vector<std::uint32_t> const& known, StartingKit const& kit,
    SkillLineLookup const& lineAbilities)
{
    std::vector<std::uint32_t> forgotten;
    for (std::uint32_t spellId : known)
    {
        bool onClassLine = false;
        bool learnedWithLine = kit.CreateSpells.contains(spellId);
        for (LineAbility const& ability : lineAbilities(spellId))
        {
            if (!kit.Lines.contains(ability.Line))
                continue;
            onClassLine = true;
            learnedWithLine = learnedWithLine || ability.AcquireMethod == SKILL_LINE_ABILITY_LEARNED_ON_SKILL_LEARN;
        }
        if ((onClassLine || kit.TrainerSpells.contains(spellId)) && !learnedWithLine)
            forgotten.push_back(spellId);
    }
    return forgotten;
}

TreeTabIds TreeTabs(uint8 classId)
{
    auto const tabs = ClassTreeTabs.find(classId);
    return tabs == ClassTreeTabs.end() ? TreeTabIds{} : tabs->second;
}

std::map<uint8, uint32> PointsPerTab(Player const* player)
{
    if (!StockPlayer(player))
        return { { 0, 0 }, { 1, 0 }, { 2, 0 } };
    return PointsPerTab(AscensionFreepick::PlayerBuild(player), TreeTabs(player->getClass()));
}

uint32 SpendTalentBudget(Player* player, uint8 treeIndex)
{
    if (!StockPlayer(player) || treeIndex >= TREE_COUNT)
        return 0;
    AscensionFreepick::Build build = AscensionFreepick::PlayerBuild(player);
    if (OverTEBudget(build))
    {
        LOG_INFO("coa", "Realms advancement replaced {}'s talent build of {} TE over a budget of {}",
            player->GetName(), build.GlobalTE(0), build.TEBudget());
        AscensionFreepick::ClearBuild(player);
        build = AscensionFreepick::PlayerBuild(player);
    }
    std::uint32_t const spent = SpendBudget(build, TreeTabs(player->getClass()), treeIndex, nullptr);
    if (!spent)
    {
        static std::atomic<std::uint32_t> logged{ 0 };
        std::uint32_t const bit = 1u << player->getClass();
        if (build.RemainingTE() && !(logged.fetch_or(bit) & bit))
            LOG_WARN("coa", "Realms advancement found no talent rank class {} can take with {} TE left",
                uint32(player->getClass()), build.RemainingTE());
        return 0;
    }

    std::vector<AscensionCoATalentState::KnownEntry> upload;
    for (AscensionFreepick::Entry const& entry : build.Entries())
        upload.push_back({ entry.EntryId, entry.Rank });
    AscensionFreepick::UploadResult const applied = AscensionFreepick::ApplyUpload(player, upload);
    return std::string_view(applied.Result) == "CA_UPDATE_ENTRIES_OK" ? spent : 0;
}

uint32 ForgetToStartingKit(Player* player, uint8 levelAfter)
{
    if (!StockPlayer(player) || !RebornDataLoaded)
        return 0;
    std::uint32_t forgotten = AscensionFreepick::ClearBuild(player);

    std::unordered_set<uint32> trainerSpells;
    if (auto const loaded = ClassTrainerSpells.find(player->getClass()); loaded != ClassTrainerSpells.end())
        trainerSpells = loaded->second;
    StartingKit const kit = KitFrom(RebornData, RebornRealm, player->getClass(), StartLevel(player), levelAfter,
        std::move(trainerSpells));

    std::vector<std::uint32_t> known;
    for (auto const& [spellId, spell] : player->GetSpellMap())
        if (spell->State != PLAYERSPELL_REMOVED)
            known.push_back(spellId);
    for (std::uint32_t spellId : SpellsToForget(known, kit, LineAbilitiesOf))
        if (player->HasSpell(spellId))
        {
            player->removeSpell(spellId, SPEC_MASK_ALL, false);
            ++forgotten;
        }
    LOG_INFO("coa", "Realms advancement forgot {} spells of {} to the starting kit", forgotten, player->GetName());
    return forgotten;
}

class RealmsAdvancementWorld final : public WorldScript
{
public:
    RealmsAdvancementWorld() : WorldScript("RealmsAdvancementWorld", { WORLDHOOK_ON_STARTUP }) { }

    void OnStartup() override
    {
        if (!Enabled())
            return;
        AscensionFreepick::Initialize();
        AscensionFreepick::Catalog const& catalog = AscensionFreepick::LoadedCatalog();
        LoadTreeTabs(catalog);
        LoadStartingKits();
        LOG_INFO("coa", "Realms advancement: {} stock catalog rows, {} classes budgeted", StockCatalogRows(catalog),
            BudgetedStockClasses(catalog));
        Trainer::SetClassTrainerFor(nullptr);
        LOG_INFO("coa", "Realms trainers: active");
    }
};
}

void AddSC_RealmsAdvancement()
{
    new Realms::RealmsAdvancementWorld();
}
