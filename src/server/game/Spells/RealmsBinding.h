/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

// wow-realms: the Warcraft Reborn pair list (wcr_world.custom_wcr_pair) binds each spell copy to
// its stock namesake. It is loaded only when CoA.ClassModel = "wcr"; elsewhere the map is empty and
// every query returns its input.

#ifndef REALMS_BINDING_H
#define REALMS_BINDING_H

#include "Define.h"
#include <functional>
#include <vector>

class Player;
class SpellInfo;
class Unit;

namespace Realms
{
    struct PairRow
    {
        uint32 Copy;
        uint32 Namesake;
    };

    struct BindingCounts
    {
        uint32 Pairs = 0;
        uint32 CorrectionExclusions = 0;
        uint32 CorrectionsMirrored = 0;
        uint32 ProcFlagsMirrored = 0;
    };

    using SpellInfoLookup = std::function<SpellInfo*(uint32 spellId)>;
    using SpellFix = void (*)(SpellInfo* spellInfo);

    void LoadBinding();
    BindingCounts Bind(std::vector<PairRow> const& pairs, std::vector<uint32> const& correctionExclusions,
        uint32 spellStoreSize, SpellInfoLookup const& spellInfo);
    void ClearBinding();
    void MirrorCorrection(uint32 spellId, SpellFix fix, std::vector<SpellInfo*> const& corrected);
    BindingCounts ReportBinding();

    bool Enabled();
    uint32 Partner(uint32 id);
    // SpellInfo::StockId() for an id with no SpellInfo at hand: a copy's namesake, else the id.
    uint32 StockId(uint32 id);
    uint32 ForCaster(Unit const* caster, uint32 id);
    uint32 ForPlayer(Player const* player, uint32 id);
    bool CorrectionExcluded(uint32 copy);
}

#endif
