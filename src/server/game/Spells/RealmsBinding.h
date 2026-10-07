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
        // Gaps in a copy's client data filled from its namesake (FillGaps).
        uint32 ProcFlagsMirrored = 0;
        uint32 ProcChancesMirrored = 0;
        uint32 ProcChargesMirrored = 0;
        uint32 ClassMasksMirrored = 0;
        uint32 FamilyFlagsMirrored = 0;
        uint32 TriggersMirrored = 0;
        // 3.3.5a spell-mod masks restored on copies whose namesake lost them too (custom_wcr_mask).
        uint32 StockMasksRestored = 0;
        // 3.3.5a proc flags and chances set on copies whose own contradict their tooltip (custom_wcr_stock_proc).
        uint32 StockProcFieldsRestored = 0;
    };

    // A spell-mod mask 3.3.5a's namesake has on one of the copy's effects, which today's Spell.dbc lacks on
    // both sides; Bind ORs it into the copy's.
    struct MaskRow
    {
        uint32 Copy;
        uint8 EffectIndex;
        uint32 Mask[3];
    };

    // A proc field of 3.3.5a's namesake that Bind sets on the copy.
    enum class StockProcField : uint8
    {
        ProcFlags,
        ProcChance
    };

    struct StockProcRow
    {
        uint32 Copy;
        StockProcField Field;
        uint32 Value;
    };

    using SpellInfoLookup = std::function<SpellInfo*(uint32 spellId)>;
    using SpellFix = void (*)(SpellInfo* spellInfo);

    void LoadBinding();
    BindingCounts Bind(std::vector<PairRow> const& pairs, std::vector<uint32> const& correctionExclusions,
        uint32 spellStoreSize, SpellInfoLookup const& spellInfo, std::vector<MaskRow> const& stockMasks = {},
        std::vector<StockProcRow> const& stockProcs = {});
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
