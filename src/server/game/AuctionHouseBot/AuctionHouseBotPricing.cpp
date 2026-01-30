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

#include "AuctionHouseBotPricing.h"
#include "AuctionHouseBotData.h"
#include "Config.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Random.h"
#include <cmath>

AuctionBotPricing* AuctionBotPricing::instance()
{
    static AuctionBotPricing instance;
    return &instance;
}

void AuctionBotPricing::Initialize()
{
    TC_LOG_INFO("ahbot", "AHBot: Initializing pricing engine...");
    LoadConfigFromFile();
    TC_LOG_INFO("ahbot", "AHBot: Pricing engine initialized");
}

void AuctionBotPricing::Reload()
{
    TC_LOG_INFO("ahbot", "AHBot: Reloading pricing configuration...");
    LoadConfigFromFile();
}

void AuctionBotPricing::LoadConfigFromFile()
{
    // Load vendor floor
    _config.VendorFloorPercent = sConfigMgr->GetIntDefault("AuctionHouseBot.Pricing.VendorFloor", 125);

    // Load max buyout
    _config.MaxBuyoutPrice = sConfigMgr->GetIntDefault("AuctionHouseBot.Pricing.MaxBuyout", 1000000000);

    // Load price variation
    _config.PriceVariationMin = sConfigMgr->GetFloatDefault("AuctionHouseBot.Pricing.Variation.Min", -0.15f);
    _config.PriceVariationMax = sConfigMgr->GetFloatDefault("AuctionHouseBot.Pricing.Variation.Max", 0.25f);

    // Load bid price range from existing config
    _config.BidPriceMin = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BIDPRICE_MIN);
    _config.BidPriceMax = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BIDPRICE_MAX);

    // Load feature toggles
    _config.UseLogarithmicPricing = sConfigMgr->GetBoolDefault("AuctionHouseBot.Pricing.Logarithmic.Enabled", true);
    _config.UseDropTierPricing = sConfigMgr->GetBoolDefault("AuctionHouseBot.Pricing.DropTier.Enabled", true);

    // Load category minimum prices
    _config.MinPriceConsumable = sConfigMgr->GetIntDefault("AuctionHouseBot.Pricing.MinPrice.Consumable", 100);
    _config.MinPriceContainer = sConfigMgr->GetIntDefault("AuctionHouseBot.Pricing.MinPrice.Container", 1000);
    _config.MinPriceWeapon = sConfigMgr->GetIntDefault("AuctionHouseBot.Pricing.MinPrice.Weapon", 5000);
    _config.MinPriceGem = sConfigMgr->GetIntDefault("AuctionHouseBot.Pricing.MinPrice.Gem", 2000);
    _config.MinPriceArmor = sConfigMgr->GetIntDefault("AuctionHouseBot.Pricing.MinPrice.Armor", 5000);
    _config.MinPriceReagent = sConfigMgr->GetIntDefault("AuctionHouseBot.Pricing.MinPrice.Reagent", 10);
    _config.MinPriceProjectile = sConfigMgr->GetIntDefault("AuctionHouseBot.Pricing.MinPrice.Projectile", 1);
    _config.MinPriceTradeGood = sConfigMgr->GetIntDefault("AuctionHouseBot.Pricing.MinPrice.TradeGood", 50);
    _config.MinPriceRecipe = sConfigMgr->GetIntDefault("AuctionHouseBot.Pricing.MinPrice.Recipe", 500);
    _config.MinPriceQuiver = sConfigMgr->GetIntDefault("AuctionHouseBot.Pricing.MinPrice.Quiver", 500);
    _config.MinPriceQuest = sConfigMgr->GetIntDefault("AuctionHouseBot.Pricing.MinPrice.Quest", 100);
    _config.MinPriceKey = sConfigMgr->GetIntDefault("AuctionHouseBot.Pricing.MinPrice.Key", 100);
    _config.MinPriceMisc = sConfigMgr->GetIntDefault("AuctionHouseBot.Pricing.MinPrice.Misc", 100);
    _config.MinPriceGlyph = sConfigMgr->GetIntDefault("AuctionHouseBot.Pricing.MinPrice.Glyph", 500);

    // Load drop tier multipliers (use defaults if not specified)
    for (uint8 i = 0; i < MAX_DROP_RATE_TIERS; ++i)
    {
        std::string key = "AuctionHouseBot.Pricing.DropTier." + std::to_string(i);
        _config.DropTierMultiplier[i] = sConfigMgr->GetFloatDefault(key.c_str(), 0.0f);
    }

    // Load logarithmic multipliers
    _config.LogarithmicMultiplierPotion = sConfigMgr->GetFloatDefault("AuctionHouseBot.Pricing.Log.Potion", 0.8f);
    _config.LogarithmicMultiplierElixir = sConfigMgr->GetFloatDefault("AuctionHouseBot.Pricing.Log.Elixir", 0.9f);
    _config.LogarithmicMultiplierFlask = sConfigMgr->GetFloatDefault("AuctionHouseBot.Pricing.Log.Flask", 0.95f);
    _config.LogarithmicMultiplierScroll = sConfigMgr->GetFloatDefault("AuctionHouseBot.Pricing.Log.Scroll", 0.7f);
    _config.LogarithmicMultiplierFood = sConfigMgr->GetFloatDefault("AuctionHouseBot.Pricing.Log.Food", 0.75f);
    _config.LogarithmicMultiplierBandage = sConfigMgr->GetFloatDefault("AuctionHouseBot.Pricing.Log.Bandage", 0.7f);
    _config.LogarithmicMultiplierCloth = sConfigMgr->GetFloatDefault("AuctionHouseBot.Pricing.Log.Cloth", 0.85f);
    _config.LogarithmicMultiplierLeather = sConfigMgr->GetFloatDefault("AuctionHouseBot.Pricing.Log.Leather", 0.85f);
    _config.LogarithmicMultiplierMetal = sConfigMgr->GetFloatDefault("AuctionHouseBot.Pricing.Log.Metal", 0.85f);
    _config.LogarithmicMultiplierHerb = sConfigMgr->GetFloatDefault("AuctionHouseBot.Pricing.Log.Herb", 0.85f);
    _config.LogarithmicMultiplierElemental = sConfigMgr->GetFloatDefault("AuctionHouseBot.Pricing.Log.Elemental", 0.9f);
    _config.LogarithmicMultiplierEnchanting = sConfigMgr->GetFloatDefault("AuctionHouseBot.Pricing.Log.Enchanting", 0.9f);
    _config.LogarithmicMultiplierJewelcrafting = sConfigMgr->GetFloatDefault("AuctionHouseBot.Pricing.Log.Jewelcrafting", 0.9f);
    _config.LogarithmicMultiplierGem = sConfigMgr->GetFloatDefault("AuctionHouseBot.Pricing.Log.Gem", 0.92f);

    TC_LOG_DEBUG("ahbot", "AHBot: Pricing config - VendorFloor: {}%, MaxBuyout: {}, Variation: {}% to {}%",
        _config.VendorFloorPercent, _config.MaxBuyoutPrice,
        int32(_config.PriceVariationMin * 100), int32(_config.PriceVariationMax * 100));
}

AHBotPriceResult AuctionBotPricing::CalculatePrice(uint32 itemId, uint32 stackCount, AuctionHouseType houseType) const
{
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
    {
        AHBotPriceResult result;
        result.BuyoutPrice = 1;
        result.BidPrice = 1;
        return result;
    }

    return CalculatePrice(proto, stackCount, houseType);
}

AHBotPriceResult AuctionBotPricing::CalculatePrice(ItemTemplate const* proto, uint32 stackCount, AuctionHouseType houseType) const
{
    AHBotPriceResult result;
    result.TotalMultiplier = 1.0f;

    if (!proto || stackCount == 0)
    {
        result.BuyoutPrice = 1;
        result.BidPrice = 1;
        return result;
    }

    // 1. Get base price per item
    result.BasePrice = GetBasePrice(proto);

    // 2. Apply multipliers
    float classMultiplier = GetClassMultiplier(proto, houseType);
    float qualityMultiplier = GetQualityMultiplier(proto, houseType);
    float dropTierMultiplier = _config.UseDropTierPricing ? GetDropTierMultiplier(proto->ItemId) : 1.0f;
    float itemLevelMultiplier = GetItemLevelMultiplier(proto);

    result.TotalMultiplier = classMultiplier * qualityMultiplier * dropTierMultiplier * itemLevelMultiplier;

    // 3. Calculate per-item price with multipliers
    float perItemPrice = static_cast<float>(result.BasePrice) * result.TotalMultiplier;

    // 4. Calculate stack price
    float stackPrice;
    if (_config.UseLogarithmicPricing && ShouldUseLogarithmicPricing(proto))
    {
        // Logarithmic pricing: price doesn't scale linearly with stack size
        float logMultiplier = GetLogarithmicMultiplier(proto, stackCount);
        stackPrice = perItemPrice * stackCount * logMultiplier;
    }
    else
    {
        // Linear pricing
        stackPrice = perItemPrice * stackCount;
    }

    // 5. Apply price variation
    stackPrice = ApplyPriceVariation(stackPrice);

    // 6. Apply vendor floor
    if (proto->SellPrice > 0)
    {
        uint32 vendorFloor = (proto->SellPrice * stackCount * _config.VendorFloorPercent) / 100;
        if (stackPrice < vendorFloor)
        {
            stackPrice = static_cast<float>(vendorFloor);
            result.WasFloored = true;
        }
    }

    // 7. Apply maximum cap
    if (stackPrice > static_cast<float>(_config.MaxBuyoutPrice))
    {
        stackPrice = static_cast<float>(_config.MaxBuyoutPrice);
        result.WasCapped = true;
    }

    // 8. Ensure minimum price of 1 copper
    result.BuyoutPrice = std::max(1u, static_cast<uint32>(stackPrice + 0.5f));

    // 9. Calculate bid price
    float bidPercent = frand(_config.BidPriceMin, _config.BidPriceMax);
    result.BidPrice = std::max(1u, static_cast<uint32>(result.BuyoutPrice * bidPercent));

    return result;
}

uint32 AuctionBotPricing::GetBasePrice(ItemTemplate const* proto) const
{
    if (!proto)
        return 1;

    // Try to use vendor buy price first
    if (proto->BuyPrice > 0)
        return proto->BuyPrice;

    // Fall back to sell price with markup
    if (proto->SellPrice > 0)
        return proto->SellPrice * 4;  // Typical buy/sell ratio

    // Use category minimum
    return GetCategoryMinimumPrice(proto);
}

uint32 AuctionBotPricing::GetCategoryMinimumPrice(ItemTemplate const* proto) const
{
    switch (proto->Class)
    {
        case ITEM_CLASS_CONSUMABLE:     return _config.MinPriceConsumable;
        case ITEM_CLASS_CONTAINER:      return _config.MinPriceContainer;
        case ITEM_CLASS_WEAPON:         return _config.MinPriceWeapon;
        case ITEM_CLASS_GEM:            return _config.MinPriceGem;
        case ITEM_CLASS_ARMOR:          return _config.MinPriceArmor;
        case ITEM_CLASS_REAGENT:        return _config.MinPriceReagent;
        case ITEM_CLASS_PROJECTILE:     return _config.MinPriceProjectile;
        case ITEM_CLASS_TRADE_GOODS:    return _config.MinPriceTradeGood;
        case ITEM_CLASS_RECIPE:         return _config.MinPriceRecipe;
        case ITEM_CLASS_QUIVER:         return _config.MinPriceQuiver;
        case ITEM_CLASS_QUEST:          return _config.MinPriceQuest;
        case ITEM_CLASS_KEY:            return _config.MinPriceKey;
        case ITEM_CLASS_MISCELLANEOUS:  return _config.MinPriceMisc;
        case ITEM_CLASS_GLYPH:          return _config.MinPriceGlyph;
        default:                        return 100;  // 1 silver default
    }
}

float AuctionBotPricing::GetClassMultiplier(ItemTemplate const* proto, AuctionHouseType houseType) const
{
    // Get class price ratio from main config
    uint32 classRatio = 100;
    switch (proto->Class)
    {
        case ITEM_CLASS_CONSUMABLE:     classRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_CONSUMABLE_PRICE_RATIO); break;
        case ITEM_CLASS_CONTAINER:      classRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_CONTAINER_PRICE_RATIO); break;
        case ITEM_CLASS_WEAPON:         classRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_WEAPON_PRICE_RATIO); break;
        case ITEM_CLASS_GEM:            classRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_GEM_PRICE_RATIO); break;
        case ITEM_CLASS_ARMOR:          classRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_ARMOR_PRICE_RATIO); break;
        case ITEM_CLASS_REAGENT:        classRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_REAGENT_PRICE_RATIO); break;
        case ITEM_CLASS_PROJECTILE:     classRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_PROJECTILE_PRICE_RATIO); break;
        case ITEM_CLASS_TRADE_GOODS:    classRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_TRADEGOOD_PRICE_RATIO); break;
        case ITEM_CLASS_RECIPE:         classRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_RECIPE_PRICE_RATIO); break;
        case ITEM_CLASS_QUIVER:         classRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_QUIVER_PRICE_RATIO); break;
        case ITEM_CLASS_QUEST:          classRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_QUEST_PRICE_RATIO); break;
        case ITEM_CLASS_KEY:            classRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_KEY_PRICE_RATIO); break;
        case ITEM_CLASS_MISCELLANEOUS:  classRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_MISC_PRICE_RATIO); break;
        case ITEM_CLASS_GLYPH:          classRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_CLASS_GLYPH_PRICE_RATIO); break;
        default: break;
    }

    // Get house price ratio
    uint32 houseRatio = sAuctionBotConfig->GetConfigPriceRatio(houseType);

    return (static_cast<float>(classRatio) / 100.0f) * (static_cast<float>(houseRatio) / 100.0f);
}

float AuctionBotPricing::GetQualityMultiplier(ItemTemplate const* proto, AuctionHouseType /*houseType*/) const
{
    // Get quality price ratio from main config
    uint32 qualityRatio = 100;
    switch (proto->Quality)
    {
        case ITEM_QUALITY_POOR:      qualityRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_ITEM_GRAY_PRICE_RATIO); break;
        case ITEM_QUALITY_NORMAL:    qualityRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_ITEM_WHITE_PRICE_RATIO); break;
        case ITEM_QUALITY_UNCOMMON:  qualityRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_ITEM_GREEN_PRICE_RATIO); break;
        case ITEM_QUALITY_RARE:      qualityRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_ITEM_BLUE_PRICE_RATIO); break;
        case ITEM_QUALITY_EPIC:      qualityRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_ITEM_PURPLE_PRICE_RATIO); break;
        case ITEM_QUALITY_LEGENDARY: qualityRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_ITEM_ORANGE_PRICE_RATIO); break;
        case ITEM_QUALITY_ARTIFACT:  qualityRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_ITEM_YELLOW_PRICE_RATIO); break;
        default: break;
    }

    return static_cast<float>(qualityRatio) / 100.0f;
}

float AuctionBotPricing::GetDropTierMultiplier(uint32 itemId) const
{
    AuctionBotItemInfo const* info = sAuctionBotData->GetItemInfo(itemId);
    if (!info)
        return 1.0f;

    uint8 tierIndex = static_cast<uint8>(info->DropTier);

    // Check for custom multiplier
    if (_config.DropTierMultiplier[tierIndex] > 0.0f)
        return _config.DropTierMultiplier[tierIndex];

    // Use default multiplier
    return GetDropTierPriceMultiplier(info->DropTier);
}

float AuctionBotPricing::GetItemLevelMultiplier(ItemTemplate const* proto) const
{
    // Scale price based on item level for equipment
    if (proto->Class == ITEM_CLASS_WEAPON || proto->Class == ITEM_CLASS_ARMOR)
    {
        // Items scale exponentially with level
        // Low level items (1-20): ~1.0x
        // Mid level items (40-60): ~1.5-2.0x
        // High level items (80+): ~3.0-5.0x
        float levelFactor = static_cast<float>(proto->ItemLevel) / 50.0f;
        return 0.5f + (levelFactor * levelFactor * 0.5f);
    }

    return 1.0f;
}

bool AuctionBotPricing::ShouldUseLogarithmicPricing(ItemTemplate const* proto) const
{
    // Consumables that stack
    if (proto->Class == ITEM_CLASS_CONSUMABLE && proto->GetMaxStackSize() > 1)
        return true;

    // Trade goods
    if (proto->Class == ITEM_CLASS_TRADE_GOODS && proto->GetMaxStackSize() > 1)
        return true;

    // Gems (stackable ones)
    if (proto->Class == ITEM_CLASS_GEM && proto->GetMaxStackSize() > 1)
        return true;

    // Reagents
    if (proto->Class == ITEM_CLASS_REAGENT)
        return true;

    // Projectiles
    if (proto->Class == ITEM_CLASS_PROJECTILE)
        return true;

    return false;
}

float AuctionBotPricing::GetLogarithmicMultiplier(ItemTemplate const* proto, uint32 stackCount) const
{
    if (stackCount <= 1)
        return 1.0f;

    // Get base log factor based on item type
    float logFactor = 0.85f;  // Default

    if (proto->Class == ITEM_CLASS_CONSUMABLE)
    {
        switch (proto->SubClass)
        {
            case ITEM_SUBCLASS_CONSUMABLE:
            case ITEM_SUBCLASS_POTION:
                logFactor = _config.LogarithmicMultiplierPotion;
                break;
            case ITEM_SUBCLASS_ELIXIR:
                logFactor = _config.LogarithmicMultiplierElixir;
                break;
            case ITEM_SUBCLASS_FLASK:
                logFactor = _config.LogarithmicMultiplierFlask;
                break;
            case ITEM_SUBCLASS_SCROLL:
                logFactor = _config.LogarithmicMultiplierScroll;
                break;
            case ITEM_SUBCLASS_FOOD:
            case ITEM_SUBCLASS_FOOD_DRINK:
                logFactor = _config.LogarithmicMultiplierFood;
                break;
            case ITEM_SUBCLASS_BANDAGE:
                logFactor = _config.LogarithmicMultiplierBandage;
                break;
            default:
                break;
        }
    }
    else if (proto->Class == ITEM_CLASS_TRADE_GOODS)
    {
        switch (proto->SubClass)
        {
            case ITEM_SUBCLASS_CLOTH:
                logFactor = _config.LogarithmicMultiplierCloth;
                break;
            case ITEM_SUBCLASS_LEATHER:
                logFactor = _config.LogarithmicMultiplierLeather;
                break;
            case ITEM_SUBCLASS_METAL_STONE:
                logFactor = _config.LogarithmicMultiplierMetal;
                break;
            case ITEM_SUBCLASS_HERB:
                logFactor = _config.LogarithmicMultiplierHerb;
                break;
            case ITEM_SUBCLASS_ELEMENTAL:
                logFactor = _config.LogarithmicMultiplierElemental;
                break;
            case ITEM_SUBCLASS_ENCHANTING:
                logFactor = _config.LogarithmicMultiplierEnchanting;
                break;
            case ITEM_SUBCLASS_JEWELCRAFTING:
                logFactor = _config.LogarithmicMultiplierJewelcrafting;
                break;
            default:
                break;
        }
    }
    else if (proto->Class == ITEM_CLASS_GEM)
    {
        logFactor = _config.LogarithmicMultiplierGem;
    }

    // Calculate logarithmic discount
    // Formula: price_per_item * stack * log_factor^(stack-1)
    // This means buying in bulk gives a discount
    return std::pow(logFactor, static_cast<float>(stackCount - 1) / static_cast<float>(proto->GetMaxStackSize()));
}

float AuctionBotPricing::ApplyPriceVariation(float basePrice) const
{
    float variation = frand(_config.PriceVariationMin, _config.PriceVariationMax);
    return basePrice * (1.0f + variation);
}

void AuctionBotPricing::SetVendorFloorPercent(uint32 percent)
{
    _config.VendorFloorPercent = percent;
    TC_LOG_INFO("ahbot", "AHBot: Vendor floor set to {}%", percent);
}

void AuctionBotPricing::SetMaxBuyoutPrice(uint32 price)
{
    _config.MaxBuyoutPrice = price;
    TC_LOG_INFO("ahbot", "AHBot: Max buyout price set to {}", price);
}

void AuctionBotPricing::SetPriceVariationRange(float min, float max)
{
    _config.PriceVariationMin = min;
    _config.PriceVariationMax = max;
    TC_LOG_INFO("ahbot", "AHBot: Price variation range set to {}% to {}%",
        int32(min * 100), int32(max * 100));
}
