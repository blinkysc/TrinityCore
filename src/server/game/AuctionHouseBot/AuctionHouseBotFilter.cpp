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

#include "AuctionHouseBotFilter.h"
#include "AuctionHouseBotData.h"
#include "AuctionHouseBot.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include <algorithm>
#include <cctype>

char const* GetFilterReasonName(AHBotFilterReason reason)
{
    static char const* names[] = {
        "None",
        "Blacklisted",
        "Name Contains Forbidden Text",
        "Quality Not Allowed",
        "Item Level Out of Range",
        "Required Level Out of Range",
        "Skill Rank Out of Range",
        "Binding Type Not Allowed",
        "No Price",
        "Not In Loot Tables",
        "Vendor Only",
        "Item Class Disabled",
        "Item Subclass Disabled",
        "Quest Item",
        "Recipe Produced",
        "Quest Reward"
    };

    if (static_cast<uint8>(reason) >= static_cast<uint8>(AHBotFilterReason::MAX_FILTER_REASON))
        return "Unknown";

    return names[static_cast<uint8>(reason)];
}

AuctionBotFilter* AuctionBotFilter::instance()
{
    static AuctionBotFilter instance;
    return &instance;
}

void AuctionBotFilter::Initialize()
{
    TC_LOG_INFO("ahbot", "AHBot: Initializing item filters...");

    LoadConfigFromFile();
    LoadForbiddenNames();

    // Load vendor items
    _vendorItems.clear();
    CreatureTemplateContainer const& creatures = sObjectMgr->GetCreatureTemplates();
    for (auto const& creatureTemplatePair : creatures)
        if (VendorItemData const* data = sObjectMgr->GetNpcVendorItemList(creatureTemplatePair.first))
            for (VendorItem const& vendorItem : data->m_items)
                _vendorItems.insert(vendorItem.item);

    TC_LOG_DEBUG("ahbot", "AHBot: Loaded {} vendor items for filtering", _vendorItems.size());

    // Load loot items
    _lootItems.clear();
    QueryResult result = WorldDatabase.Query(
        "SELECT `item` FROM `creature_loot_template` WHERE `Reference` = 0 UNION "
        "SELECT `item` FROM `disenchant_loot_template` WHERE `Reference` = 0 UNION "
        "SELECT `item` FROM `fishing_loot_template` WHERE `Reference` = 0 UNION "
        "SELECT `item` FROM `gameobject_loot_template` WHERE `Reference` = 0 UNION "
        "SELECT `item` FROM `item_loot_template` WHERE `Reference` = 0 UNION "
        "SELECT `item` FROM `milling_loot_template` WHERE `Reference` = 0 UNION "
        "SELECT `item` FROM `pickpocketing_loot_template` WHERE `Reference` = 0 UNION "
        "SELECT `item` FROM `prospecting_loot_template` WHERE `Reference` = 0 UNION "
        "SELECT `item` FROM `reference_loot_template` WHERE `Reference` = 0 UNION "
        "SELECT `item` FROM `skinning_loot_template` WHERE `Reference` = 0 UNION "
        "SELECT `item` FROM `spell_loot_template` WHERE `Reference` = 0");

    if (result)
    {
        do
        {
            _lootItems.insert(result->Fetch()[0].GetUInt32());
        } while (result->NextRow());
    }

    TC_LOG_DEBUG("ahbot", "AHBot: Loaded {} loot items for filtering", _lootItems.size());
    TC_LOG_INFO("ahbot", "AHBot: Item filters initialized");
}

void AuctionBotFilter::Reload()
{
    TC_LOG_INFO("ahbot", "AHBot: Reloading item filters...");
    Initialize();
}

void AuctionBotFilter::LoadConfigFromFile()
{
    // Load quality filters
    _config.AllowGray = true;    // Controlled by amount settings
    _config.AllowWhite = true;
    _config.AllowGreen = true;
    _config.AllowBlue = true;
    _config.AllowPurple = true;
    _config.AllowOrange = true;
    _config.AllowYellow = true;

    // Load source filters from main AHBot config
    _config.AllowVendorItems = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_ITEMS_VENDOR);
    _config.AllowLootItems = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_ITEMS_LOOT);
    _config.AllowMiscItems = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_ITEMS_MISC);

    // Load binding filters
    _config.AllowBindNone = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BIND_NO);
    _config.AllowBindPickup = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BIND_PICKUP);
    _config.AllowBindEquip = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BIND_EQUIP);
    _config.AllowBindUse = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BIND_USE);
    _config.AllowBindQuest = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BIND_QUEST);

    // Load zero price filters
    _config.AllowZeroPriceConsumable = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_CONSUMABLE_ALLOW_ZERO);
    _config.AllowZeroPriceContainer = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_CONTAINER_ALLOW_ZERO);
    _config.AllowZeroPriceWeapon = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_WEAPON_ALLOW_ZERO);
    _config.AllowZeroPriceGem = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_GEM_ALLOW_ZERO);
    _config.AllowZeroPriceArmor = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_ARMOR_ALLOW_ZERO);
    _config.AllowZeroPriceReagent = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_REAGENT_ALLOW_ZERO);
    _config.AllowZeroPriceProjectile = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_PROJECTILE_ALLOW_ZERO);
    _config.AllowZeroPriceTradeGood = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_TRADEGOOD_ALLOW_ZERO);
    _config.AllowZeroPriceRecipe = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_RECIPE_ALLOW_ZERO);
    _config.AllowZeroPriceQuiver = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_QUIVER_ALLOW_ZERO);
    _config.AllowZeroPriceQuest = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_QUEST_ALLOW_ZERO);
    _config.AllowZeroPriceKey = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_KEY_ALLOW_ZERO);
    _config.AllowZeroPriceMisc = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_MISC_ALLOW_ZERO);
    _config.AllowZeroPriceGlyph = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_GLYPH_ALLOW_ZERO);

    // Load lockbox filter
    _config.AllowLockboxes = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_LOCKBOX_ENABLED);

    // Load global level filters
    _config.MinItemLevel = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_ITEM_MIN_ITEM_LEVEL);
    _config.MaxItemLevel = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_ITEM_MAX_ITEM_LEVEL);
    _config.MinReqLevel = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_ITEM_MIN_REQ_LEVEL);
    _config.MaxReqLevel = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_ITEM_MAX_REQ_LEVEL);
    _config.MinSkillRank = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_ITEM_MIN_SKILL_RANK);
    _config.MaxSkillRank = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_ITEM_MAX_SKILL_RANK);

    // Load class-specific filters
    _config.MountMinReqLevel = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_MISC_MOUNT_MIN_REQ_LEVEL);
    _config.MountMaxReqLevel = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_MISC_MOUNT_MAX_REQ_LEVEL);
    _config.MountMinSkillRank = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_MISC_MOUNT_MIN_SKILL_RANK);
    _config.MountMaxSkillRank = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_MISC_MOUNT_MAX_SKILL_RANK);

    _config.GlyphMinReqLevel = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_GLYPH_MIN_REQ_LEVEL);
    _config.GlyphMaxReqLevel = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_GLYPH_MAX_REQ_LEVEL);
    _config.GlyphMinItemLevel = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_GLYPH_MIN_ITEM_LEVEL);
    _config.GlyphMaxItemLevel = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_GLYPH_MAX_ITEM_LEVEL);

    _config.TradeGoodMinItemLevel = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_TRADEGOOD_MIN_ITEM_LEVEL);
    _config.TradeGoodMaxItemLevel = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_TRADEGOOD_MAX_ITEM_LEVEL);

    _config.ContainerMinItemLevel = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_CONTAINER_MIN_ITEM_LEVEL);
    _config.ContainerMaxItemLevel = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_CONTAINER_MAX_ITEM_LEVEL);

    // Load price mode
    _config.UseBuyPriceForSeller = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYPRICE_SELLER);

    // Load recipe/quest filters (new options)
    _config.FilterRecipeProducedItems = sConfigMgr->GetBoolDefault("AuctionHouseBot.Filter.RecipeProduced", false);
    _config.FilterQuestRewardItems = sConfigMgr->GetBoolDefault("AuctionHouseBot.Filter.QuestReward", false);
}

void AuctionBotFilter::LoadForbiddenNames()
{
    _config.ForbiddenNameSubstrings.clear();

    // Default forbidden name substrings
    std::string defaultForbidden = sConfigMgr->GetStringDefault("AuctionHouseBot.Filter.ForbiddenNames", "OLD,Test,Deprecated,DEBUG,UNUSED,Monster -,QA,PH");

    // Parse comma-separated list
    std::stringstream ss(defaultForbidden);
    std::string item;
    while (std::getline(ss, item, ','))
    {
        // Trim whitespace
        size_t start = item.find_first_not_of(" \t");
        size_t end = item.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos)
        {
            std::string trimmed = item.substr(start, end - start + 1);
            if (!trimmed.empty())
                _config.ForbiddenNameSubstrings.push_back(trimmed);
        }
    }

    TC_LOG_DEBUG("ahbot", "AHBot: Loaded {} forbidden name patterns", _config.ForbiddenNameSubstrings.size());
}

AHBotFilterReason AuctionBotFilter::CheckItem(uint32 itemId) const
{
    // Check blacklist first
    if (sAuctionBotData->IsItemBlacklisted(itemId))
        return AHBotFilterReason::FILTER_BLACKLISTED;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
        return AHBotFilterReason::FILTER_BLACKLISTED;  // Invalid item

    return CheckItem(proto);
}

AHBotFilterReason AuctionBotFilter::CheckItem(ItemTemplate const* proto) const
{
    if (!proto)
        return AHBotFilterReason::FILTER_BLACKLISTED;

    // Check blacklist
    if (sAuctionBotData->IsItemBlacklisted(proto->ItemId))
        return AHBotFilterReason::FILTER_BLACKLISTED;

    // Check name filter
    if (!PassesNameFilter(proto))
        return AHBotFilterReason::FILTER_NAME_CONTAINS;

    // Check quality filter
    if (!PassesQualityFilter(proto))
        return AHBotFilterReason::FILTER_QUALITY;

    // Check binding filter
    if (!PassesBindingFilter(proto))
        return AHBotFilterReason::FILTER_BINDING;

    // Check price filter
    if (!PassesPriceFilter(proto))
        return AHBotFilterReason::FILTER_NO_PRICE;

    // Check level filters
    if (!PassesLevelFilter(proto))
        return AHBotFilterReason::FILTER_ITEM_LEVEL;

    // Check class-specific filters
    if (!CheckClassSpecificFilters(proto))
        return AHBotFilterReason::FILTER_SUBCLASS_DISABLED;

    // Check source filters
    if (!PassesSourceFilter(proto->ItemId))
        return AHBotFilterReason::FILTER_NOT_IN_LOOT;

    // Check recipe-produced filter
    if (_config.FilterRecipeProducedItems && sAuctionBotData->IsRecipeProducedItem(proto->ItemId))
        return AHBotFilterReason::FILTER_RECIPE_PRODUCED;

    // Check quest reward filter
    if (_config.FilterQuestRewardItems && sAuctionBotData->IsQuestRewardItem(proto->ItemId))
        return AHBotFilterReason::FILTER_QUEST_REWARD;

    return AHBotFilterReason::FILTER_NONE;
}

bool AuctionBotFilter::PassesNameFilter(ItemTemplate const* proto) const
{
    if (_config.ForbiddenNameSubstrings.empty())
        return true;

    std::string name = proto->Name1;

    // Convert to lowercase for case-insensitive comparison
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);

    for (std::string const& forbidden : _config.ForbiddenNameSubstrings)
    {
        std::string forbiddenLower = forbidden;
        std::transform(forbiddenLower.begin(), forbiddenLower.end(), forbiddenLower.begin(), ::tolower);

        if (name.find(forbiddenLower) != std::string::npos)
            return false;
    }

    return true;
}

bool AuctionBotFilter::PassesQualityFilter(ItemTemplate const* proto) const
{
    switch (proto->Quality)
    {
        case ITEM_QUALITY_POOR:      return _config.AllowGray;
        case ITEM_QUALITY_NORMAL:    return _config.AllowWhite;
        case ITEM_QUALITY_UNCOMMON:  return _config.AllowGreen;
        case ITEM_QUALITY_RARE:      return _config.AllowBlue;
        case ITEM_QUALITY_EPIC:      return _config.AllowPurple;
        case ITEM_QUALITY_LEGENDARY: return _config.AllowOrange;
        case ITEM_QUALITY_ARTIFACT:  return _config.AllowYellow;
        default:                     return false;
    }
}

bool AuctionBotFilter::PassesBindingFilter(ItemTemplate const* proto) const
{
    switch (proto->Bonding)
    {
        case NO_BIND:            return _config.AllowBindNone;
        case BIND_WHEN_PICKED_UP: return _config.AllowBindPickup;
        case BIND_WHEN_EQUIPED:  return _config.AllowBindEquip;
        case BIND_WHEN_USE:      return _config.AllowBindUse;
        case BIND_QUEST_ITEM:    return _config.AllowBindQuest;
        default:                 return false;
    }
}

bool AuctionBotFilter::PassesPriceFilter(ItemTemplate const* proto) const
{
    if (IsZeroPriceAllowed(proto))
        return true;

    if (_config.UseBuyPriceForSeller)
        return proto->SellPrice > 0;
    else
        return proto->BuyPrice > 0;
}

bool AuctionBotFilter::IsZeroPriceAllowed(ItemTemplate const* proto) const
{
    switch (proto->Class)
    {
        case ITEM_CLASS_CONSUMABLE:     return _config.AllowZeroPriceConsumable;
        case ITEM_CLASS_CONTAINER:      return _config.AllowZeroPriceContainer;
        case ITEM_CLASS_WEAPON:         return _config.AllowZeroPriceWeapon;
        case ITEM_CLASS_GEM:            return _config.AllowZeroPriceGem;
        case ITEM_CLASS_ARMOR:          return _config.AllowZeroPriceArmor;
        case ITEM_CLASS_REAGENT:        return _config.AllowZeroPriceReagent;
        case ITEM_CLASS_PROJECTILE:     return _config.AllowZeroPriceProjectile;
        case ITEM_CLASS_TRADE_GOODS:    return _config.AllowZeroPriceTradeGood;
        case ITEM_CLASS_RECIPE:         return _config.AllowZeroPriceRecipe;
        case ITEM_CLASS_QUIVER:         return _config.AllowZeroPriceQuiver;
        case ITEM_CLASS_QUEST:          return _config.AllowZeroPriceQuest;
        case ITEM_CLASS_KEY:            return _config.AllowZeroPriceKey;
        case ITEM_CLASS_MISCELLANEOUS:  return _config.AllowZeroPriceMisc;
        case ITEM_CLASS_GLYPH:          return _config.AllowZeroPriceGlyph;
        default:                        return false;
    }
}

bool AuctionBotFilter::PassesLevelFilter(ItemTemplate const* proto) const
{
    // Check item level
    if (_config.MinItemLevel > 0 && proto->ItemLevel < _config.MinItemLevel)
        return false;
    if (_config.MaxItemLevel > 0 && proto->ItemLevel > _config.MaxItemLevel)
        return false;

    // Check required level
    if (_config.MinReqLevel > 0 && proto->RequiredLevel < _config.MinReqLevel)
        return false;
    if (_config.MaxReqLevel > 0 && proto->RequiredLevel > _config.MaxReqLevel)
        return false;

    // Check skill rank
    if (_config.MinSkillRank > 0 && proto->RequiredSkillRank < _config.MinSkillRank)
        return false;
    if (_config.MaxSkillRank > 0 && proto->RequiredSkillRank > _config.MaxSkillRank)
        return false;

    return true;
}

bool AuctionBotFilter::CheckClassSpecificFilters(ItemTemplate const* proto) const
{
    switch (proto->Class)
    {
        case ITEM_CLASS_MISCELLANEOUS:
            // Mount-specific filters
            if (proto->SubClass == ITEM_SUBCLASS_JUNK_MOUNT)
            {
                if (_config.MountMinReqLevel > 0 && proto->RequiredLevel < _config.MountMinReqLevel)
                    return false;
                if (_config.MountMaxReqLevel > 0 && proto->RequiredLevel > _config.MountMaxReqLevel)
                    return false;
                if (_config.MountMinSkillRank > 0 && proto->RequiredSkillRank < _config.MountMinSkillRank)
                    return false;
                if (_config.MountMaxSkillRank > 0 && proto->RequiredSkillRank > _config.MountMaxSkillRank)
                    return false;
            }

            // Lockbox filter
            if (proto->HasFlag(ITEM_FLAG_HAS_LOOT))
            {
                if (!proto->LockID)
                    return false;  // Skip non-locked lootable items
                if (!_config.AllowLockboxes)
                    return false;
            }
            break;

        case ITEM_CLASS_GLYPH:
            if (_config.GlyphMinReqLevel > 0 && proto->RequiredLevel < _config.GlyphMinReqLevel)
                return false;
            if (_config.GlyphMaxReqLevel > 0 && proto->RequiredLevel > _config.GlyphMaxReqLevel)
                return false;
            if (_config.GlyphMinItemLevel > 0 && proto->ItemLevel < _config.GlyphMinItemLevel)
                return false;
            if (_config.GlyphMaxItemLevel > 0 && proto->ItemLevel > _config.GlyphMaxItemLevel)
                return false;
            break;

        case ITEM_CLASS_TRADE_GOODS:
            if (_config.TradeGoodMinItemLevel > 0 && proto->ItemLevel < _config.TradeGoodMinItemLevel)
                return false;
            if (_config.TradeGoodMaxItemLevel > 0 && proto->ItemLevel > _config.TradeGoodMaxItemLevel)
                return false;
            break;

        case ITEM_CLASS_CONTAINER:
        case ITEM_CLASS_QUIVER:
            if (_config.ContainerMinItemLevel > 0 && proto->ItemLevel < _config.ContainerMinItemLevel)
                return false;
            if (_config.ContainerMaxItemLevel > 0 && proto->ItemLevel > _config.ContainerMaxItemLevel)
                return false;
            break;
    }

    return true;
}

bool AuctionBotFilter::PassesSourceFilter(uint32 itemId) const
{
    bool isVendorItem = _vendorItems.count(itemId) > 0;
    bool isLootItem = _lootItems.count(itemId) > 0;

    // If vendor items are not allowed, filter them out
    if (!_config.AllowVendorItems && isVendorItem && !isLootItem)
        return false;

    // If loot items are not allowed, filter them out
    if (!_config.AllowLootItems && isLootItem && !isVendorItem)
        return false;

    // If misc items are not allowed, filter out items that are neither vendor nor loot
    if (!_config.AllowMiscItems && !isVendorItem && !isLootItem)
        return false;

    return true;
}

void AuctionBotFilter::AddForbiddenName(std::string const& substring)
{
    // Check if already exists
    for (std::string const& existing : _config.ForbiddenNameSubstrings)
    {
        std::string existingLower = existing;
        std::string substringLower = substring;
        std::transform(existingLower.begin(), existingLower.end(), existingLower.begin(), ::tolower);
        std::transform(substringLower.begin(), substringLower.end(), substringLower.begin(), ::tolower);
        if (existingLower == substringLower)
            return;  // Already exists
    }

    _config.ForbiddenNameSubstrings.push_back(substring);
    TC_LOG_INFO("ahbot", "AHBot: Added forbidden name pattern: {}", substring);
}

void AuctionBotFilter::RemoveForbiddenName(std::string const& substring)
{
    std::string substringLower = substring;
    std::transform(substringLower.begin(), substringLower.end(), substringLower.begin(), ::tolower);

    auto it = std::remove_if(_config.ForbiddenNameSubstrings.begin(), _config.ForbiddenNameSubstrings.end(),
        [&substringLower](std::string const& s)
        {
            std::string sLower = s;
            std::transform(sLower.begin(), sLower.end(), sLower.begin(), ::tolower);
            return sLower == substringLower;
        });

    if (it != _config.ForbiddenNameSubstrings.end())
    {
        _config.ForbiddenNameSubstrings.erase(it, _config.ForbiddenNameSubstrings.end());
        TC_LOG_INFO("ahbot", "AHBot: Removed forbidden name pattern: {}", substring);
    }
}
