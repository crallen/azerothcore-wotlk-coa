/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

#ifndef REALMS_ADVANCEMENT_H
#define REALMS_ADVANCEMENT_H

#include "AscensionFreepickRules.h"
#include "RealmsLegacyTalents.h"

namespace Realms
{
std::uint32_t StockCatalogRows(AscensionFreepick::Catalog const& catalog);
std::uint32_t BudgetedStockClasses(AscensionFreepick::Catalog const& catalog);
}

#endif
