/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "AuctionHouseBotData.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"

DropRateTier GetDropTierFromChance(float chance)
{
    if (chance >= 50.0f)  return DropRateTier::TIER_50_PERCENT;
    if (chance >= 10.0f)  return DropRateTier::TIER_10_PERCENT;
    if (chance >= 5.0f)   return DropRateTier::TIER_5_PERCENT;
    if (chance >= 2.0f)   return DropRateTier::TIER_2_PERCENT;
    if (chance >= 1.0f)   return DropRateTier::TIER_1_PERCENT;
    if (chance >= 0.5f)   return DropRateTier::TIER_0_5_PERCENT;
    if (chance >= 0.2f)   return DropRateTier::TIER_0_2_PERCENT;
    if (chance >= 0.1f)   return DropRateTier::TIER_0_1_PERCENT;
    if (chance >= 0.05f)  return DropRateTier::TIER_0_05_PERCENT;
    if (chance >= 0.02f)  return DropRateTier::TIER_0_02_PERCENT;
    if (chance >= 0.01f)  return DropRateTier::TIER_0_01_PERCENT;
    if (chance > 0.0f)    return DropRateTier::TIER_0_005_PERCENT;
    return DropRateTier::TIER_NO_DROP;
}

char const* GetDropTierName(DropRateTier tier)
{
    static char const* names[MAX_DROP_RATE_TIERS] = {
        "50%+",
        "10-50%",
        "5-10%",
        "2-5%",
        "1-2%",
        "0.5-1%",
        "0.2-0.5%",
        "0.1-0.2%",
        "0.05-0.1%",
        "0.02-0.05%",
        "0.01-0.02%",
        "<0.01%",
        "No Drop"
    };

    if (static_cast<uint8>(tier) >= MAX_DROP_RATE_TIERS)
        return "Unknown";

    return names[static_cast<uint8>(tier)];
}

float GetDropTierPriceMultiplier(DropRateTier tier)
{
    // Price multipliers increase as drop rate decreases (rarer = more expensive)
    static float multipliers[MAX_DROP_RATE_TIERS] = {
        0.5f,   // TIER_50_PERCENT - very common
        0.75f,  // TIER_10_PERCENT
        1.0f,   // TIER_5_PERCENT - baseline
        1.25f,  // TIER_2_PERCENT
        1.5f,   // TIER_1_PERCENT
        2.0f,   // TIER_0_5_PERCENT
        2.5f,   // TIER_0_2_PERCENT
        3.0f,   // TIER_0_1_PERCENT
        4.0f,   // TIER_0_05_PERCENT
        5.0f,   // TIER_0_02_PERCENT
        7.5f,   // TIER_0_01_PERCENT
        10.0f,  // TIER_0_005_PERCENT - extremely rare
        1.0f    // TIER_NO_DROP - crafted/vendor/quest items use baseline
    };

    if (static_cast<uint8>(tier) >= MAX_DROP_RATE_TIERS)
        return 1.0f;

    return multipliers[static_cast<uint8>(tier)];
}

uint32 GetDropTierListWeight(DropRateTier tier)
{
    // List weights - more common items appear more frequently
    static uint32 weights[MAX_DROP_RATE_TIERS] = {
        100,  // TIER_50_PERCENT - very common, list often
        80,   // TIER_10_PERCENT
        60,   // TIER_5_PERCENT
        40,   // TIER_2_PERCENT
        30,   // TIER_1_PERCENT
        20,   // TIER_0_5_PERCENT
        15,   // TIER_0_2_PERCENT
        10,   // TIER_0_1_PERCENT
        7,    // TIER_0_05_PERCENT
        5,    // TIER_0_02_PERCENT
        3,    // TIER_0_01_PERCENT
        1,    // TIER_0_005_PERCENT - extremely rare, list rarely
        50    // TIER_NO_DROP - crafted/vendor items at medium frequency
    };

    if (static_cast<uint8>(tier) >= MAX_DROP_RATE_TIERS)
        return 50;

    return weights[static_cast<uint8>(tier)];
}

AuctionBotDataMgr* AuctionBotDataMgr::instance()
{
    static AuctionBotDataMgr instance;
    return &instance;
}

void AuctionBotDataMgr::Initialize()
{
    TC_LOG_INFO("ahbot", "AHBot: Loading item data cache...");

    ClearCaches();

    LoadDropRates();
    LoadRecipeProducedItems();
    LoadQuestRewardItems();
    LoadVendorItems();
    LoadBlacklist();

    // Log statistics
    uint32 tierCounts[MAX_DROP_RATE_TIERS] = {};
    for (auto const& pair : _itemInfoCache)
        ++tierCounts[static_cast<uint8>(pair.second.DropTier)];

    TC_LOG_INFO("ahbot", "AHBot: Loaded {} items into data cache", _itemInfoCache.size());
    TC_LOG_INFO("ahbot", "AHBot: Drop tier distribution:");
    for (uint8 i = 0; i < MAX_DROP_RATE_TIERS; ++i)
        if (tierCounts[i] > 0)
            TC_LOG_INFO("ahbot", "AHBot:   {}: {} items", GetDropTierName(static_cast<DropRateTier>(i)), tierCounts[i]);

    TC_LOG_INFO("ahbot", "AHBot: Recipe-produced items: {}", _recipeProducedItems.size());
    TC_LOG_INFO("ahbot", "AHBot: Quest reward items: {}", _questRewardItems.size());
    TC_LOG_INFO("ahbot", "AHBot: Blacklisted items: {}", _blacklistedItems.size());
}

void AuctionBotDataMgr::Reload()
{
    TC_LOG_INFO("ahbot", "AHBot: Reloading item data cache...");
    Initialize();
}

void AuctionBotDataMgr::ClearCaches()
{
    _itemInfoCache.clear();
    _recipeProducedItems.clear();
    _questRewardItems.clear();
    _blacklistedItems.clear();
}

AuctionBotItemInfo& AuctionBotDataMgr::GetOrCreateItemInfo(uint32 itemId)
{
    auto itr = _itemInfoCache.find(itemId);
    if (itr != _itemInfoCache.end())
        return itr->second;

    AuctionBotItemInfo info;
    info.ItemId = itemId;

    // Load base prices from item template
    if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId))
    {
        info.SellPrice = proto->SellPrice;
        info.BuyPrice = proto->BuyPrice;
    }

    return _itemInfoCache.emplace(itemId, info).first->second;
}

AuctionBotItemInfo const* AuctionBotDataMgr::GetItemInfo(uint32 itemId) const
{
    auto itr = _itemInfoCache.find(itemId);
    if (itr != _itemInfoCache.end())
        return &itr->second;
    return nullptr;
}

bool AuctionBotDataMgr::IsItemBlacklisted(uint32 itemId) const
{
    return _blacklistedItems.count(itemId) > 0;
}

void AuctionBotDataMgr::AddToBlacklist(uint32 itemId, uint8 reason)
{
    _blacklistedItems.insert(itemId);

    // Also persist to database
    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_INS_AHBOT_BLACKLIST);
    stmt->setUInt32(0, itemId);
    stmt->setUInt8(1, reason);
    WorldDatabase.Execute(stmt);
}

void AuctionBotDataMgr::RemoveFromBlacklist(uint32 itemId)
{
    _blacklistedItems.erase(itemId);

    // Also remove from database
    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_AHBOT_BLACKLIST);
    stmt->setUInt32(0, itemId);
    WorldDatabase.Execute(stmt);
}

bool AuctionBotDataMgr::IsRecipeProducedItem(uint32 itemId) const
{
    return _recipeProducedItems.count(itemId) > 0;
}

bool AuctionBotDataMgr::IsQuestRewardItem(uint32 itemId) const
{
    return _questRewardItems.count(itemId) > 0;
}

uint32 AuctionBotDataMgr::GetItemCountForTier(DropRateTier tier) const
{
    uint32 count = 0;
    for (auto const& pair : _itemInfoCache)
        if (pair.second.DropTier == tier)
            ++count;
    return count;
}

void AuctionBotDataMgr::LoadDropRates()
{
    TC_LOG_INFO("ahbot", "AHBot: Loading drop rates from loot tables...");

    // Query all loot tables and get maximum drop chance for each item
    QueryResult result = WorldDatabase.Query(
        "SELECT item, MAX(Chance) as max_chance FROM ("
        "  SELECT item, Chance FROM creature_loot_template WHERE Reference = 0 "
        "  UNION ALL SELECT item, Chance FROM gameobject_loot_template WHERE Reference = 0 "
        "  UNION ALL SELECT item, Chance FROM fishing_loot_template WHERE Reference = 0 "
        "  UNION ALL SELECT item, Chance FROM item_loot_template WHERE Reference = 0 "
        "  UNION ALL SELECT item, Chance FROM skinning_loot_template WHERE Reference = 0 "
        "  UNION ALL SELECT item, Chance FROM pickpocketing_loot_template WHERE Reference = 0 "
        "  UNION ALL SELECT item, Chance FROM prospecting_loot_template WHERE Reference = 0 "
        "  UNION ALL SELECT item, Chance FROM milling_loot_template WHERE Reference = 0 "
        "  UNION ALL SELECT item, Chance FROM disenchant_loot_template WHERE Reference = 0 "
        ") combined GROUP BY item"
    );

    if (!result)
    {
        TC_LOG_WARN("ahbot", "AHBot: No drop rate data found in loot tables");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 itemId = fields[0].GetUInt32();
        float maxChance = fields[1].GetFloat();

        AuctionBotItemInfo& info = GetOrCreateItemInfo(itemId);
        info.MaxDropChance = maxChance;
        info.DropTier = GetDropTierFromChance(maxChance);
        ++count;

    } while (result->NextRow());

    TC_LOG_INFO("ahbot", "AHBot: Loaded drop rates for {} items", count);
}

void AuctionBotDataMgr::LoadRecipeProducedItems()
{
    TC_LOG_INFO("ahbot", "AHBot: Loading recipe-produced items...");

    // Items produced by crafting recipes (class 9 = Recipe)
    QueryResult result = WorldDatabase.Query(
        "SELECT DISTINCT slt.item FROM spell_loot_template slt "
        "INNER JOIN item_template it ON it.spellid_1 = slt.Entry "
        "WHERE it.class = 9 AND slt.Reference = 0"
    );

    if (!result)
    {
        TC_LOG_DEBUG("ahbot", "AHBot: No recipe-produced items found");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 itemId = fields[0].GetUInt32();
        _recipeProducedItems.insert(itemId);

        AuctionBotItemInfo& info = GetOrCreateItemInfo(itemId);
        info.IsRecipeProduced = true;
        ++count;

    } while (result->NextRow());

    TC_LOG_INFO("ahbot", "AHBot: Loaded {} recipe-produced items", count);
}

void AuctionBotDataMgr::LoadQuestRewardItems()
{
    TC_LOG_INFO("ahbot", "AHBot: Loading quest reward items...");

    // Items rewarded from quests
    QueryResult result = WorldDatabase.Query(
        "SELECT RewardChoiceItemId1 FROM quest_template WHERE RewardChoiceItemId1 != 0 "
        "UNION SELECT RewardChoiceItemId2 FROM quest_template WHERE RewardChoiceItemId2 != 0 "
        "UNION SELECT RewardChoiceItemId3 FROM quest_template WHERE RewardChoiceItemId3 != 0 "
        "UNION SELECT RewardChoiceItemId4 FROM quest_template WHERE RewardChoiceItemId4 != 0 "
        "UNION SELECT RewardChoiceItemId5 FROM quest_template WHERE RewardChoiceItemId5 != 0 "
        "UNION SELECT RewardChoiceItemId6 FROM quest_template WHERE RewardChoiceItemId6 != 0 "
        "UNION SELECT RewardItemId1 FROM quest_template WHERE RewardItemId1 != 0 "
        "UNION SELECT RewardItemId2 FROM quest_template WHERE RewardItemId2 != 0 "
        "UNION SELECT RewardItemId3 FROM quest_template WHERE RewardItemId3 != 0 "
        "UNION SELECT RewardItemId4 FROM quest_template WHERE RewardItemId4 != 0"
    );

    if (!result)
    {
        TC_LOG_DEBUG("ahbot", "AHBot: No quest reward items found");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 itemId = fields[0].GetUInt32();
        _questRewardItems.insert(itemId);

        AuctionBotItemInfo& info = GetOrCreateItemInfo(itemId);
        info.IsQuestReward = true;
        ++count;

    } while (result->NextRow());

    TC_LOG_INFO("ahbot", "AHBot: Loaded {} quest reward items", count);
}

void AuctionBotDataMgr::LoadVendorItems()
{
    TC_LOG_INFO("ahbot", "AHBot: Loading vendor item prices...");

    // Load items sold by vendors and their prices
    QueryResult result = WorldDatabase.Query(
        "SELECT DISTINCT nv.item, nv.ExtendedCost "
        "FROM npc_vendor nv "
        "WHERE nv.item != 0"
    );

    if (!result)
    {
        TC_LOG_DEBUG("ahbot", "AHBot: No vendor items found");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 itemId = fields[0].GetUInt32();

        AuctionBotItemInfo& info = GetOrCreateItemInfo(itemId);
        info.IsVendorItem = true;

        // Get the buy price from item template
        if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId))
            info.VendorPrice = proto->BuyPrice;

        ++count;

    } while (result->NextRow());

    TC_LOG_INFO("ahbot", "AHBot: Loaded {} vendor items", count);
}

void AuctionBotDataMgr::LoadBlacklist()
{
    TC_LOG_INFO("ahbot", "AHBot: Loading item blacklist...");

    QueryResult result = WorldDatabase.Query("SELECT item FROM auctionhousebot_blacklist");

    if (!result)
    {
        TC_LOG_DEBUG("ahbot", "AHBot: No blacklisted items found");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 itemId = fields[0].GetUInt32();
        _blacklistedItems.insert(itemId);
        ++count;

    } while (result->NextRow());

    TC_LOG_INFO("ahbot", "AHBot: Loaded {} blacklisted items", count);
}
