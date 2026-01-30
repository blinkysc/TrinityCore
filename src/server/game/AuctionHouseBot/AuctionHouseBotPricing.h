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

#ifndef AUCTION_HOUSE_BOT_PRICING_H
#define AUCTION_HOUSE_BOT_PRICING_H

#include "Define.h"
#include "AuctionHouseBot.h"
#include "AuctionHouseBotData.h"

class ItemTemplate;

// Pricing configuration for AHBot
struct AHBotPricingConfig
{
    // Vendor floor - minimum markup over vendor sell price (percentage, 125 = 25% markup)
    uint32 VendorFloorPercent = 125;

    // Maximum buyout price in copper (100k gold = 1,000,000,000 copper)
    uint32 MaxBuyoutPrice = 1000000000;

    // Price variation range (percentage of base price)
    float PriceVariationMin = -0.15f;  // -15%
    float PriceVariationMax = 0.25f;   // +25%

    // Bid price range (percentage of buyout)
    float BidPriceMin = 0.6f;
    float BidPriceMax = 0.9f;

    // Use logarithmic pricing for stackable consumables
    bool UseLogarithmicPricing = true;

    // Enable drop tier pricing multipliers
    bool UseDropTierPricing = true;

    // Category minimum prices (in copper) - floor prices for items without vendor value
    uint32 MinPriceConsumable = 100;    // 1 silver
    uint32 MinPriceContainer = 1000;    // 10 silver
    uint32 MinPriceWeapon = 5000;       // 50 silver
    uint32 MinPriceGem = 2000;          // 20 silver
    uint32 MinPriceArmor = 5000;        // 50 silver
    uint32 MinPriceReagent = 10;        // 10 copper
    uint32 MinPriceProjectile = 1;      // 1 copper
    uint32 MinPriceTradeGood = 50;      // 50 copper
    uint32 MinPriceRecipe = 500;        // 5 silver
    uint32 MinPriceQuiver = 500;        // 5 silver
    uint32 MinPriceQuest = 100;         // 1 silver
    uint32 MinPriceKey = 100;           // 1 silver
    uint32 MinPriceMisc = 100;          // 1 silver
    uint32 MinPriceGlyph = 500;         // 5 silver

    // Drop tier price multipliers (override defaults if non-zero)
    float DropTierMultiplier[MAX_DROP_RATE_TIERS] = {};

    // Subclass-specific multipliers for logarithmic pricing
    float LogarithmicMultiplierPotion = 0.8f;
    float LogarithmicMultiplierElixir = 0.9f;
    float LogarithmicMultiplierFlask = 0.95f;
    float LogarithmicMultiplierScroll = 0.7f;
    float LogarithmicMultiplierFood = 0.75f;
    float LogarithmicMultiplierBandage = 0.7f;
    float LogarithmicMultiplierCloth = 0.85f;
    float LogarithmicMultiplierLeather = 0.85f;
    float LogarithmicMultiplierMetal = 0.85f;
    float LogarithmicMultiplierHerb = 0.85f;
    float LogarithmicMultiplierElemental = 0.9f;
    float LogarithmicMultiplierEnchanting = 0.9f;
    float LogarithmicMultiplierJewelcrafting = 0.9f;
    float LogarithmicMultiplierGem = 0.92f;
};

// Result of price calculation
struct AHBotPriceResult
{
    uint32 BuyoutPrice = 0;     // Final buyout price in copper
    uint32 BidPrice = 0;        // Starting bid price in copper
    uint32 BasePrice = 0;       // Base price before multipliers
    float TotalMultiplier = 1.0f; // Combined multiplier applied
    bool WasCapped = false;     // True if price was capped at maximum
    bool WasFloored = false;    // True if price was raised to minimum
};

// Pricing engine for AHBot
class TC_GAME_API AuctionBotPricing
{
private:
    AuctionBotPricing() = default;
    ~AuctionBotPricing() = default;
    AuctionBotPricing(AuctionBotPricing const&) = delete;
    AuctionBotPricing& operator=(AuctionBotPricing const&) = delete;

public:
    static AuctionBotPricing* instance();

    // Initialize pricing configuration
    void Initialize();

    // Reload configuration
    void Reload();

    // Calculate price for an item
    AHBotPriceResult CalculatePrice(uint32 itemId, uint32 stackCount, AuctionHouseType houseType) const;
    AHBotPriceResult CalculatePrice(ItemTemplate const* proto, uint32 stackCount, AuctionHouseType houseType) const;

    // Get individual price components for debugging
    uint32 GetBasePrice(ItemTemplate const* proto) const;
    float GetClassMultiplier(ItemTemplate const* proto, AuctionHouseType houseType) const;
    float GetQualityMultiplier(ItemTemplate const* proto, AuctionHouseType houseType) const;
    float GetDropTierMultiplier(uint32 itemId) const;
    float GetItemLevelMultiplier(ItemTemplate const* proto) const;

    // Get current config
    AHBotPricingConfig const& GetConfig() const { return _config; }

    // Set config values at runtime
    void SetVendorFloorPercent(uint32 percent);
    void SetMaxBuyoutPrice(uint32 price);
    void SetPriceVariationRange(float min, float max);

private:
    void LoadConfigFromFile();

    // Price calculation helpers
    uint32 GetCategoryMinimumPrice(ItemTemplate const* proto) const;
    float GetLogarithmicMultiplier(ItemTemplate const* proto, uint32 stackCount) const;
    float ApplyPriceVariation(float basePrice) const;

    // Check if item should use logarithmic pricing
    bool ShouldUseLogarithmicPricing(ItemTemplate const* proto) const;

    AHBotPricingConfig _config;
};

#define sAuctionBotPricing AuctionBotPricing::instance()

#endif // AUCTION_HOUSE_BOT_PRICING_H
