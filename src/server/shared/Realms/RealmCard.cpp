/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "RealmCard.h"
#include "Config.h"
#include "StringFormat.h"

// Pages 1 and 2 hold six cards each and page 3 twelve; page 4 is the scrolling list.
RealmCardSlot GetRealmCardSlot(std::size_t order)
{
    if (order < 6)
        return { 1, uint32(order + 1) };
    if (order < 12)
        return { 2, uint32(order - 5) };
    if (order < 24)
        return { 3, uint32(order - 11) };
    return { 4, uint32(order - 23) };
}

// RealmCards.<id>.* keys are absent from the config file unless a realm overrides its card, so a missing one is
// not logged.
RealmCardStyle GetRealmCardStyle(uint32 realmId, RealmCardStyle const& globalStyle)
{
    std::string const prefix = Acore::StringFormat("RealmCards.{}.", realmId);
    return { sConfigMgr->GetOption<uint32>(prefix + "Expansion", globalStyle.Expansion, false),
        sConfigMgr->GetOption<uint32>(prefix + "GameMode", globalStyle.GameMode, false),
        sConfigMgr->GetOption<std::string>(prefix + "Image", globalStyle.Image, false) };
}

std::string BuildRealmCardName(std::string const& realmName, RealmCardStyle const& style, RealmCardSlot slot)
{
    return Acore::StringFormat("{}!{}!{}!{}!1!{}!{}!0", realmName, style.Expansion, style.GameMode, style.Image,
        uint32(slot.Page), slot.Index);
}
