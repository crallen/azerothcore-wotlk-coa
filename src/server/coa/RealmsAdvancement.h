/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

#ifndef REALMS_ADVANCEMENT_H
#define REALMS_ADVANCEMENT_H

#include "AscensionFreepickRules.h"
#include "Define.h"
#include "RealmsLegacyTalents.h"
#include <array>
#include <map>
#include <string>

namespace Realms
{
using TreeTabIds = std::array<std::uint32_t, 3>;

struct TreeTabMatch
{
    TreeTabIds Tabs{};
    bool ByName = true;
};

std::uint32_t StockCatalogRows(AscensionFreepick::Catalog const& catalog);
std::uint32_t BudgetedStockClasses(AscensionFreepick::Catalog const& catalog);

TreeTabMatch MatchTreeTabs(std::map<std::uint32_t, std::string> const& catalogTabs,
    std::array<std::string, 3> const& pageNames);
std::map<uint8, uint32> PointsPerTab(AscensionFreepick::Build const& build, TreeTabIds const& tabs);
// Spends the build's remaining TE in treeIndex's tree, then the other two, and returns the TE added. A build
// already over its TE budget (stored at a higher level or under older data) is emptied first, so the whole
// budget is spent on a fresh build.
std::uint32_t SpendBudget(AscensionFreepick::Build& build, TreeTabIds const& tabs, uint8 treeIndex,
    AscensionFreepick::UnitCheck const& unit);

TreeTabIds TreeTabs(uint8 classId);
std::map<uint8, uint32> PointsPerTab(Player const* player);
// SpendBudget on the player's stored build, applied as an upload; an over-budget stored build is cleared through
// ClearBuild (its spells removed) before the fresh build is uploaded. Returns the TE spent, 0 when no rank fit or
// the upload was refused.
uint32 SpendTalentBudget(Player* player, uint8 treeIndex);
}

#endif
