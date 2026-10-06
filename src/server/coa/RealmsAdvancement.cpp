/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

// wow-realms: advancement, free-pick serving the Warcraft Reborn realm's stock classes; the startup
// line is one of wow-realms' check/self-reports.

#include "RealmsAdvancement.h"
#include "AscensionFreepick.h"
#include "Log.h"
#include "Player.h"
#include "RealmsBinding.h"
#include "ScriptMgr.h"

namespace Realms
{
namespace
{
bool StockPlayer(Player const* player)
{
    return AscensionFreepick::IsRebornCharacter(player);
}

bool StockClass(std::uint32_t classId)
{
    return classId >= CLASS_WARRIOR && classId <= CLASS_DRUID && classId != CLASS_HERO;
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
        LOG_INFO("coa", "Realms advancement: {} stock catalog rows, {} classes budgeted", StockCatalogRows(catalog),
            BudgetedStockClasses(catalog));
    }
};
}

void AddSC_RealmsAdvancement()
{
    new Realms::RealmsAdvancementWorld();
}
