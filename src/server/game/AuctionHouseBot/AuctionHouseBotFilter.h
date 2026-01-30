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

#ifndef AUCTION_HOUSE_BOT_FILTER_H
#define AUCTION_HOUSE_BOT_FILTER_H

#include "Define.h"
#include <string>
#include <vector>
#include <unordered_set>

class ItemTemplate;

// Reasons why an item might be filtered
enum class AHBotFilterReason : uint8
{
    FILTER_NONE = 0,           // Not filtered, item is allowed
    FILTER_BLACKLISTED,        // In blacklist table
    FILTER_NAME_CONTAINS,      // Name contains forbidden text
    FILTER_QUALITY,            // Quality not allowed
    FILTER_ITEM_LEVEL,         // Item level out of range
    FILTER_REQ_LEVEL,          // Required level out of range
    FILTER_SKILL_RANK,         // Skill rank out of range
    FILTER_BINDING,            // Binding type not allowed
    FILTER_NO_PRICE,           // No buy/sell price
    FILTER_NOT_IN_LOOT,        // Not in loot tables (if required)
    FILTER_VENDOR_ONLY,        // Only available from vendor (if filtered)
    FILTER_CLASS_DISABLED,     // Item class disabled
    FILTER_SUBCLASS_DISABLED,  // Item subclass disabled
    FILTER_QUEST_ITEM,         // Quest-only item
    FILTER_RECIPE_PRODUCED,    // Filtered recipe-produced item
    FILTER_QUEST_REWARD,       // Filtered quest reward item

    MAX_FILTER_REASON
};

// Get human-readable filter reason
char const* GetFilterReasonName(AHBotFilterReason reason);

// Item filter configuration
struct AHBotFilterConfig
{
    // Name-based filters (case insensitive)
    std::vector<std::string> ForbiddenNameSubstrings;

    // Quality filters
    bool AllowGray = true;
    bool AllowWhite = true;
    bool AllowGreen = true;
    bool AllowBlue = true;
    bool AllowPurple = true;
    bool AllowOrange = false;
    bool AllowYellow = false;

    // Source filters
    bool AllowVendorItems = false;
    bool AllowLootItems = true;
    bool AllowMiscItems = false;
    bool FilterRecipeProducedItems = false;
    bool FilterQuestRewardItems = false;

    // Binding filters
    bool AllowBindNone = true;
    bool AllowBindPickup = false;
    bool AllowBindEquip = true;
    bool AllowBindUse = true;
    bool AllowBindQuest = false;

    // Allow items with zero price
    bool AllowZeroPriceConsumable = false;
    bool AllowZeroPriceContainer = false;
    bool AllowZeroPriceWeapon = false;
    bool AllowZeroPriceGem = false;
    bool AllowZeroPriceArmor = false;
    bool AllowZeroPriceReagent = false;
    bool AllowZeroPriceProjectile = false;
    bool AllowZeroPriceTradeGood = false;
    bool AllowZeroPriceRecipe = false;
    bool AllowZeroPriceQuiver = false;
    bool AllowZeroPriceQuest = false;
    bool AllowZeroPriceKey = false;
    bool AllowZeroPriceMisc = false;
    bool AllowZeroPriceGlyph = false;

    // Lockbox filter
    bool AllowLockboxes = false;

    // Global level/skill filters
    uint32 MinItemLevel = 0;
    uint32 MaxItemLevel = 0;  // 0 = no max
    uint32 MinReqLevel = 0;
    uint32 MaxReqLevel = 0;   // 0 = no max
    uint32 MinSkillRank = 0;
    uint32 MaxSkillRank = 0;  // 0 = no max

    // Class-specific filters
    uint32 MountMinReqLevel = 0;
    uint32 MountMaxReqLevel = 0;
    uint32 MountMinSkillRank = 0;
    uint32 MountMaxSkillRank = 0;

    uint32 GlyphMinReqLevel = 0;
    uint32 GlyphMaxReqLevel = 0;
    uint32 GlyphMinItemLevel = 0;
    uint32 GlyphMaxItemLevel = 0;

    uint32 TradeGoodMinItemLevel = 0;
    uint32 TradeGoodMaxItemLevel = 0;

    uint32 ContainerMinItemLevel = 0;
    uint32 ContainerMaxItemLevel = 0;

    // Use buy price instead of sell price for pricing
    bool UseBuyPriceForSeller = false;
};

// Item filter system for AHBot
class TC_GAME_API AuctionBotFilter
{
private:
    AuctionBotFilter() = default;
    ~AuctionBotFilter() = default;
    AuctionBotFilter(AuctionBotFilter const&) = delete;
    AuctionBotFilter& operator=(AuctionBotFilter const&) = delete;

public:
    static AuctionBotFilter* instance();

    // Initialize filters from config
    void Initialize();

    // Reload filters
    void Reload();

    // Check if item passes all filters
    // Returns FILTER_NONE if allowed, otherwise returns the reason for filtering
    AHBotFilterReason CheckItem(uint32 itemId) const;
    AHBotFilterReason CheckItem(ItemTemplate const* proto) const;

    // Check specific filter types
    bool PassesNameFilter(ItemTemplate const* proto) const;
    bool PassesQualityFilter(ItemTemplate const* proto) const;
    bool PassesBindingFilter(ItemTemplate const* proto) const;
    bool PassesPriceFilter(ItemTemplate const* proto) const;
    bool PassesLevelFilter(ItemTemplate const* proto) const;
    bool PassesSourceFilter(uint32 itemId) const;

    // Get current filter config (for GM commands)
    AHBotFilterConfig const& GetConfig() const { return _config; }

    // Add/remove name filter at runtime
    void AddForbiddenName(std::string const& substring);
    void RemoveForbiddenName(std::string const& substring);

private:
    void LoadConfigFromFile();
    void LoadForbiddenNames();

    bool CheckClassSpecificFilters(ItemTemplate const* proto) const;
    bool IsZeroPriceAllowed(ItemTemplate const* proto) const;

    AHBotFilterConfig _config;

    // Cached vendor and loot item sets for source filtering
    std::unordered_set<uint32> _vendorItems;
    std::unordered_set<uint32> _lootItems;
};

#define sAuctionBotFilter AuctionBotFilter::instance()

#endif // AUCTION_HOUSE_BOT_FILTER_H
