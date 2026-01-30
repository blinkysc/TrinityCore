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

#ifndef AUCTION_HOUSE_BOT_DATA_H
#define AUCTION_HOUSE_BOT_DATA_H

#include "Define.h"
#include <unordered_map>
#include <unordered_set>

// Drop rate tiers based on loot table chances
// Used to determine item rarity for pricing and listing weights
enum class DropRateTier : uint8
{
    TIER_50_PERCENT   = 0,   // >= 50% drop rate (very common)
    TIER_10_PERCENT   = 1,   // >= 10% drop rate
    TIER_5_PERCENT    = 2,   // >= 5% drop rate
    TIER_2_PERCENT    = 3,   // >= 2% drop rate
    TIER_1_PERCENT    = 4,   // >= 1% drop rate
    TIER_0_5_PERCENT  = 5,   // >= 0.5% drop rate
    TIER_0_2_PERCENT  = 6,   // >= 0.2% drop rate
    TIER_0_1_PERCENT  = 7,   // >= 0.1% drop rate
    TIER_0_05_PERCENT = 8,   // >= 0.05% drop rate
    TIER_0_02_PERCENT = 9,   // >= 0.02% drop rate
    TIER_0_01_PERCENT = 10,  // >= 0.01% drop rate
    TIER_0_005_PERCENT = 11, // < 0.01% drop rate (extremely rare)
    TIER_NO_DROP      = 12,  // Not found in loot tables (crafted, vendor, quest, etc.)

    MAX_DROP_TIER
};

#define MAX_DROP_RATE_TIERS 13

// Get tier from raw drop rate percentage
DropRateTier GetDropTierFromChance(float chance);

// Get tier name for display
char const* GetDropTierName(DropRateTier tier);

// Get default price multiplier for tier
float GetDropTierPriceMultiplier(DropRateTier tier);

// Get default list weight for tier
uint32 GetDropTierListWeight(DropRateTier tier);

// Extended item information for AHBot
struct AuctionBotItemInfo
{
    uint32 ItemId = 0;
    DropRateTier DropTier = DropRateTier::TIER_NO_DROP;
    float MaxDropChance = 0.0f;          // Highest drop chance from any source
    bool IsRecipeProduced = false;       // Created by a recipe (crafted)
    bool IsQuestReward = false;          // Reward from a quest
    bool IsVendorItem = false;           // Sold by a vendor
    uint32 VendorPrice = 0;              // Price to buy from vendor (if any)
    uint32 SellPrice = 0;                // Sell price to vendor
    uint32 BuyPrice = 0;                 // Buy price from item template
};

// Data cache for AHBot item metadata
class TC_GAME_API AuctionBotDataMgr
{
private:
    AuctionBotDataMgr() = default;
    ~AuctionBotDataMgr() = default;
    AuctionBotDataMgr(AuctionBotDataMgr const&) = delete;
    AuctionBotDataMgr& operator=(AuctionBotDataMgr const&) = delete;

public:
    static AuctionBotDataMgr* instance();

    // Initialize all data caches - call after DB is ready
    void Initialize();

    // Reload caches
    void Reload();

    // Get item info (returns nullptr if item not tracked)
    AuctionBotItemInfo const* GetItemInfo(uint32 itemId) const;

    // Check if item is in blacklist
    bool IsItemBlacklisted(uint32 itemId) const;

    // Add/remove from runtime blacklist
    void AddToBlacklist(uint32 itemId, uint8 reason = 0);
    void RemoveFromBlacklist(uint32 itemId);

    // Check item flags
    bool IsRecipeProducedItem(uint32 itemId) const;
    bool IsQuestRewardItem(uint32 itemId) const;

    // Get tier statistics for debugging
    uint32 GetItemCountForTier(DropRateTier tier) const;
    uint32 GetTotalTrackedItems() const { return static_cast<uint32>(_itemInfoCache.size()); }

private:
    // Load methods
    void LoadDropRates();
    void LoadRecipeProducedItems();
    void LoadQuestRewardItems();
    void LoadVendorItems();
    void LoadBlacklist();

    // Clear all caches
    void ClearCaches();

    // Get or create item info entry
    AuctionBotItemInfo& GetOrCreateItemInfo(uint32 itemId);

    // Item info cache - keyed by item ID
    std::unordered_map<uint32, AuctionBotItemInfo> _itemInfoCache;

    // Sets for quick lookup
    std::unordered_set<uint32> _recipeProducedItems;
    std::unordered_set<uint32> _questRewardItems;
    std::unordered_set<uint32> _blacklistedItems;
};

#define sAuctionBotData AuctionBotDataMgr::instance()

#endif // AUCTION_HOUSE_BOT_DATA_H
