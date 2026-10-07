/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

// wow-realms: the copy binding's pair map (ADR-0006 in the wow-realms repository). The startup
// line is one of wow-realms' check/self-reports.

#include "RealmsBinding.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <string>

namespace
{
    enum BindingFlags : uint32
    {
        BINDING_COPY                = 0x1,
        BINDING_CORRECTION_EXCLUDED = 0x2
    };

    std::vector<uint32> partners;
    std::vector<uint32> flags;
    Realms::BindingCounts bound;
    Realms::SpellInfoLookup boundSpellInfo;

    bool IsStockClass(uint8 classId)
    {
        return classId >= CLASS_WARRIOR && classId <= CLASS_DRUID && classId != CLASS_HERO;
    }

    uint32 CopyOfNamesake(uint32 id)
    {
        if (id >= partners.size() || (flags[id] & BINDING_COPY))
            return 0;
        return partners[id];
    }

    using FieldValues = std::vector<std::pair<std::string, uint64>>;

    FieldValues CorrectedFields(SpellInfo const& spell)
    {
        FieldValues fields = {
            { "Attributes", spell.Attributes }, { "AttributesEx", spell.AttributesEx },
            { "AttributesEx2", spell.AttributesEx2 }, { "AttributesEx3", spell.AttributesEx3 },
            { "AttributesEx4", spell.AttributesEx4 }, { "AttributesEx5", spell.AttributesEx5 },
            { "AttributesEx6", spell.AttributesEx6 }, { "AttributesEx7", spell.AttributesEx7 },
            { "Mechanic", spell.Mechanic }, { "Dispel", spell.Dispel }, { "Stances", spell.Stances },
            { "ProcFlags", spell.ProcFlags }, { "ProcChance", spell.ProcChance }, { "ProcCharges", spell.ProcCharges },
            { "DurationEntry", uint64(reinterpret_cast<uintptr_t>(spell.DurationEntry)) },
            { "RangeEntry", uint64(reinterpret_cast<uintptr_t>(spell.RangeEntry)) },
            { "SchoolMask", spell.SchoolMask },
            { "SpellFamilyFlags[0]", spell.SpellFamilyFlags[0] }, { "SpellFamilyFlags[1]", spell.SpellFamilyFlags[1] },
            { "SpellFamilyFlags[2]", spell.SpellFamilyFlags[2] },
            { "InterruptFlags", spell.InterruptFlags }, { "AuraInterruptFlags", spell.AuraInterruptFlags }
        };
        for (SpellEffectInfo const& effect : spell.GetEffects())
        {
            std::string const prefix = "Effects[" + std::to_string(effect.EffectIndex) + "].";
            fields.emplace_back(prefix + "Effect", effect.Effect);
            fields.emplace_back(prefix + "ApplyAuraName", effect.ApplyAuraName);
            fields.emplace_back(prefix + "BasePoints", uint32(effect.BasePoints));
            fields.emplace_back(prefix + "MiscValue", uint32(effect.MiscValue));
            fields.emplace_back(prefix + "MiscValueB", uint32(effect.MiscValueB));
            fields.emplace_back(prefix + "TriggerSpell", effect.TriggerSpell);
            fields.emplace_back(prefix + "TargetA", effect.TargetA.GetTarget());
            fields.emplace_back(prefix + "TargetB", effect.TargetB.GetTarget());
            fields.emplace_back(prefix + "RadiusEntry", uint64(reinterpret_cast<uintptr_t>(effect.RadiusEntry)));
            fields.emplace_back(prefix + "Amplitude", effect.Amplitude);
            for (uint8 part = 0; part < 3; ++part)
                fields.emplace_back(prefix + "SpellClassMask[" + std::to_string(part) + "]",
                    effect.SpellClassMask[part]);
        }
        return fields;
    }

    std::string ChangedFields(FieldValues const& before, FieldValues const& after)
    {
        std::string changed;
        for (std::size_t index = 0; index < before.size(); ++index)
        {
            if (before[index].second == after[index].second)
                continue;
            if (!changed.empty())
                changed += ", ";
            changed += before[index].first;
        }
        return changed;
    }

    // The client's Spell.dbc leaves some copies without a value their namesake has, where the namesake's
    // rows and scripts rely on it: no proc flags (the paladin seals); no proc chance (a spell_proc row may
    // hold the flags); no charges where it procs; an effect's spell-mod mask of 0 under the same aura;
    // family flags the namesake's modifiers match on; a trigger spell the store lacks
    // (docs/warcraft-reborn/audit.md in wow-realms). Each gap takes the namesake's value; a value Ascension
    // set on the copy is kept. Runs before the spell_proc rows load, which read the proc fields from here.
    void FillGaps(SpellInfo& copy, SpellInfo const& namesake, uint32 spellStoreSize,
        Realms::SpellInfoLookup const& spellInfo, Realms::BindingCounts& counts)
    {
        if (!copy.ProcFlags && namesake.ProcFlags)
        {
            copy.ProcFlags = namesake.ProcFlags;
            ++counts.ProcFlagsMirrored;
        }
        // A spell_proc row can supply the flags; its chance still comes from here.
        if (!copy.ProcChance && namesake.ProcChance)
        {
            copy.ProcChance = namesake.ProcChance;
            ++counts.ProcChancesMirrored;
        }
        if (copy.ProcFlags && !copy.ProcCharges && namesake.ProcCharges)
        {
            copy.ProcCharges = namesake.ProcCharges;
            ++counts.ProcChargesMirrored;
        }

        bool const sameFamily = copy.SpellFamilyName && copy.SpellFamilyName == namesake.SpellFamilyName;
        if (sameFamily && (namesake.SpellFamilyFlags & ~copy.SpellFamilyFlags))
        {
            copy.SpellFamilyFlags |= namesake.SpellFamilyFlags;
            ++counts.FamilyFlagsMirrored;
        }

        auto const exists = [&](uint32 id) { return id < spellStoreSize && spellInfo(id); };
        for (uint8 index = 0; index < MAX_SPELL_EFFECTS; ++index)
        {
            SpellEffectInfo const& theirs = namesake.GetEffect(SpellEffIndex(index));
            SpellEffectInfo& ours = copy._GetEffect(SpellEffIndex(index));
            if (sameFamily && !ours.SpellClassMask && theirs.SpellClassMask && ours.ApplyAuraName
                && ours.ApplyAuraName == theirs.ApplyAuraName)
            {
                ours.SpellClassMask = theirs.SpellClassMask;
                ++counts.ClassMasksMirrored;
            }
            if (ours.TriggerSpell && !exists(ours.TriggerSpell) && theirs.TriggerSpell && exists(theirs.TriggerSpell))
            {
                ours.TriggerSpell = theirs.TriggerSpell;
                ++counts.TriggersMirrored;
            }
        }
    }

    bool PairTablesExist()
    {
        QueryResult result = WorldDatabase.Query(
            "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = DATABASE() "
            "AND table_name IN ('custom_wcr_pair', 'custom_wcr_override', 'custom_wcr_mask', "
            "'custom_wcr_stock_proc')");
        return result && result->Fetch()[0].Get<uint64>() == 4;
    }
}

namespace Realms
{
    void LoadBinding()
    {
        ClearBinding();
        if (sConfigMgr->GetOption<std::string>("CoA.ClassModel", "coa") != "wcr")
            return;

        if (!PairTablesExist())
        {
            LOG_ERROR("coa", "Realms copy-binding: custom_wcr_pair, custom_wcr_override, custom_wcr_mask or "
                "custom_wcr_stock_proc is missing, "
                "no copies are bound");
            return;
        }

        std::vector<PairRow> pairs;
        if (QueryResult result = WorldDatabase.Query("SELECT `copy`, `namesake` FROM `custom_wcr_pair` ORDER BY `copy`"))
        {
            do
            {
                Field const* fields = result->Fetch();
                pairs.push_back({ fields[0].Get<uint32>(), fields[1].Get<uint32>() });
            } while (result->NextRow());
        }

        std::vector<uint32> exclusions;
        if (QueryResult result = WorldDatabase.Query(
            "SELECT `spell` FROM `custom_wcr_override` WHERE `kind` = 'no_correction'"))
        {
            do
            {
                exclusions.push_back(result->Fetch()[0].Get<uint32>());
            } while (result->NextRow());
        }

        std::vector<MaskRow> stockMasks;
        if (QueryResult result = WorldDatabase.Query(
            "SELECT `copy`, `effect_index`, `mask0`, `mask1`, `mask2` FROM `custom_wcr_mask` ORDER BY `copy`, `effect_index`"))
        {
            do
            {
                Field const* fields = result->Fetch();
                stockMasks.push_back({ fields[0].Get<uint32>(), fields[1].Get<uint8>(),
                    { fields[2].Get<uint32>(), fields[3].Get<uint32>(), fields[4].Get<uint32>() } });
            } while (result->NextRow());
        }

        std::vector<StockProcRow> stockProcs;
        if (QueryResult result = WorldDatabase.Query(
            "SELECT `copy`, `field`, `value` FROM `custom_wcr_stock_proc` ORDER BY `copy`, `field`"))
        {
            do
            {
                Field const* fields = result->Fetch();
                stockProcs.push_back({ fields[0].Get<uint32>(),
                    fields[1].Get<std::string>() == "ProcChance" ? StockProcField::ProcChance : StockProcField::ProcFlags,
                    fields[2].Get<uint32>() });
            } while (result->NextRow());
        }

        Bind(pairs, exclusions, sSpellMgr->GetSpellInfoStoreSize(),
            [](uint32 spellId) { return const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(spellId)); }, stockMasks,
            stockProcs);
    }

    BindingCounts Bind(std::vector<PairRow> const& pairs, std::vector<uint32> const& correctionExclusions,
        uint32 spellStoreSize, SpellInfoLookup const& spellInfo, std::vector<MaskRow> const& stockMasks,
        std::vector<StockProcRow> const& stockProcs)
    {
        ClearBinding();
        partners.assign(spellStoreSize, 0);
        flags.assign(spellStoreSize, 0);

        BindingCounts counts;
        for (PairRow const& pair : pairs)
        {
            SpellInfo* copy = pair.Copy < spellStoreSize ? spellInfo(pair.Copy) : nullptr;
            SpellInfo* namesake = pair.Namesake < spellStoreSize ? spellInfo(pair.Namesake) : nullptr;
            if (!copy || !namesake)
            {
                LOG_ERROR("coa", "Realms copy-binding: pair {} -> {} names a spell with no SpellInfo, skipped",
                    pair.Copy, pair.Namesake);
                continue;
            }

            // The first pair to bind a spell wins; the pair list is read in copy order.
            if (pair.Copy == pair.Namesake || partners[pair.Copy] || partners[pair.Namesake])
            {
                LOG_ERROR("coa", "Realms copy-binding: pair {} -> {} pairs a spell with itself or one already "
                    "bound, skipped", pair.Copy, pair.Namesake);
                continue;
            }

            partners[pair.Copy] = pair.Namesake;
            partners[pair.Namesake] = pair.Copy;
            flags[pair.Copy] |= BINDING_COPY;
            copy->RealmsNamesake = pair.Namesake;
            ++counts.Pairs;
            FillGaps(*copy, *namesake, spellStoreSize, spellInfo, counts);
        }

        for (MaskRow const& row : stockMasks)
        {
            if (row.Copy >= spellStoreSize || !(flags[row.Copy] & BINDING_COPY) || row.EffectIndex >= MAX_SPELL_EFFECTS)
            {
                LOG_ERROR("coa", "Realms copy-binding: 3.3.5a mask for {} effect {} names no bound copy effect, "
                    "skipped", row.Copy, row.EffectIndex);
                continue;
            }

            SpellEffectInfo& effect = spellInfo(row.Copy)->_GetEffect(SpellEffIndex(row.EffectIndex));
            flag96 const stock(row.Mask[0], row.Mask[1], row.Mask[2]);
            if (stock & ~effect.SpellClassMask)
            {
                effect.SpellClassMask |= stock;
                ++counts.StockMasksRestored;
            }
        }

        for (StockProcRow const& row : stockProcs)
        {
            if (row.Copy >= spellStoreSize || !(flags[row.Copy] & BINDING_COPY))
            {
                LOG_ERROR("coa", "Realms copy-binding: 3.3.5a proc field for {} names no bound copy, skipped", row.Copy);
                continue;
            }

            SpellInfo* copy = spellInfo(row.Copy);
            uint32& field = row.Field == StockProcField::ProcChance ? copy->ProcChance : copy->ProcFlags;
            if (field != row.Value)
            {
                field = row.Value;
                ++counts.StockProcFieldsRestored;
            }
        }

        for (uint32 copy : correctionExclusions)
        {
            if (copy < spellStoreSize && (flags[copy] & BINDING_COPY))
            {
                flags[copy] |= BINDING_CORRECTION_EXCLUDED;
                ++counts.CorrectionExclusions;
            }
        }

        if (!counts.Pairs)
        {
            ClearBinding();
            return counts;
        }

        bound = counts;
        boundSpellInfo = spellInfo;
        return counts;
    }

    void ClearBinding()
    {
        partners.clear();
        flags.clear();
        bound = {};
        boundSpellInfo = nullptr;
    }

    void MirrorCorrection(uint32 spellId, SpellFix fix, std::vector<SpellInfo*> const& corrected)
    {
        uint32 const copyId = CopyOfNamesake(spellId);
        if (!copyId || CorrectionExcluded(copyId))
            return;

        SpellInfo* copy = boundSpellInfo(copyId);
        if (std::find(corrected.begin(), corrected.end(), copy) != corrected.end())
            return;

        ++bound.CorrectionsMirrored;
        if (!sLog->ShouldLog("realms.corrections", LOG_LEVEL_INFO))
        {
            fix(copy);
            return;
        }

        FieldValues const before = CorrectedFields(*copy);
        fix(copy);
        std::string const changed = ChangedFields(before, CorrectedFields(*copy));
        if (!changed.empty())
            LOG_INFO("realms.corrections", "{} <- {}: {}", copyId, spellId, changed);
    }

    BindingCounts ReportBinding()
    {
        if (Enabled())
            LOG_INFO("coa", "Realms copy-binding: {} pairs, {} correction exclusions, {} corrections mirrored, "
                "gaps filled: {} proc flags, {} proc chances, {} proc charges, {} class masks, {} family flags, "
                "{} triggers; 3.3.5a restored: {} masks, {} proc fields", bound.Pairs, bound.CorrectionExclusions,
                bound.CorrectionsMirrored, bound.ProcFlagsMirrored, bound.ProcChancesMirrored,
                bound.ProcChargesMirrored, bound.ClassMasksMirrored, bound.FamilyFlagsMirrored,
                bound.TriggersMirrored, bound.StockMasksRestored, bound.StockProcFieldsRestored);
        return bound;
    }

    bool Enabled()
    {
        return !partners.empty();
    }

    uint32 Partner(uint32 id)
    {
        return id < partners.size() ? partners[id] : 0;
    }

    uint32 StockId(uint32 id)
    {
        return id < flags.size() && (flags[id] & BINDING_COPY) ? partners[id] : id;
    }

    uint32 ForCaster(Unit const* caster, uint32 id)
    {
        if (!caster || !CopyOfNamesake(id))
            return id;
        return ForPlayer(caster->GetCharmerOrOwnerPlayerOrPlayerItself(), id);
    }

    uint32 ForPlayer(Player const* player, uint32 id)
    {
        uint32 const copy = CopyOfNamesake(id);
        if (!copy || !player || !IsStockClass(player->getClass()))
            return id;
        return copy;
    }

    bool CorrectionExcluded(uint32 copy)
    {
        return copy < flags.size() && (flags[copy] & BINDING_CORRECTION_EXCLUDED);
    }
}
