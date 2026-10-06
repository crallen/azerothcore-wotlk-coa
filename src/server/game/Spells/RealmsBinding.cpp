/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

// wow-realms: the copy binding's pair map (ADR-0006 in the wow-realms repository). The startup
// line is one of wow-realms' check/self-reports.

#include "RealmsBinding.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

namespace
{
    enum BindingFlags : uint32
    {
        BINDING_COPY                = 0x1,
        BINDING_CORRECTION_EXCLUDED = 0x2
    };

    std::vector<uint32> partners;
    std::vector<uint32> flags;

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

    bool HasCreatureTemplate(uint32 entry)
    {
        return sObjectMgr->GetCreatureTemplate(entry) != nullptr;
    }

    bool PairTablesExist()
    {
        QueryResult result = WorldDatabase.Query(
            "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = DATABASE() "
            "AND table_name IN ('custom_wcr_pair', 'custom_wcr_override')");
        return result && result->Fetch()[0].Get<uint64>() == 2;
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
            LOG_ERROR("coa", "Realms copy-binding: custom_wcr_pair or custom_wcr_override is missing, "
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

        BindingCounts const counts = Bind(pairs, exclusions, sSpellMgr->GetSpellInfoStoreSize(),
            [](uint32 spellId) { return const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(spellId)); });
        LOG_INFO("coa", "Realms copy-binding: {} pairs, {} correction exclusions", counts.Pairs,
            counts.CorrectionExclusions);
    }

    BindingCounts Bind(std::vector<PairRow> const& pairs, std::vector<uint32> const& correctionExclusions,
        uint32 spellStoreSize, SpellInfoLookup const& spellInfo)
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
            ClearBinding();
        return counts;
    }

    void ClearBinding()
    {
        partners.clear();
        flags.clear();
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

    uint32 CreatureEntry(uint32 entry)
    {
        return CreatureEntry(entry, HasCreatureTemplate);
    }

    uint32 CreatureEntry(uint32 entry, TemplateExists templateExists)
    {
        if (!Enabled() || entry < CopyIdOffset || templateExists(entry) || !templateExists(entry - CopyIdOffset))
            return entry;
        return entry - CopyIdOffset;
    }

    bool CorrectionExcluded(uint32 copy)
    {
        return copy < flags.size() && (flags[copy] & BINDING_CORRECTION_EXCLUDED);
    }
}
