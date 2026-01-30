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

#include "AuctionHouseBot.h"
#include "AccountMgr.h"
#include "AuctionHouseBotBuyer.h"
#include "AuctionHouseBotData.h"
#include "AuctionHouseBotFilter.h"
#include "AuctionHouseBotPricing.h"
#include "AuctionHouseBotSeller.h"
#include "AuctionHouseMgr.h"
#include "Config.h"
#include "Containers.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Item.h"
#include "Log.h"
#include "World.h"

AuctionBotConfig* AuctionBotConfig::instance()
{
    static AuctionBotConfig instance;
    return &instance;
}

bool AuctionBotConfig::Initialize()
{
    GetConfigFromFile();

    if (!GetConfig(CONFIG_AHBOT_BUYER_ENABLED) && !GetConfig(CONFIG_AHBOT_SELLER_ENABLED))
    {
        TC_LOG_INFO("ahbot", "AHBOT is Disabled.");
        return false;
    }

    if (GetConfig(CONFIG_AHBOT_ALLIANCE_ITEM_AMOUNT_RATIO) == 0 && GetConfig(CONFIG_AHBOT_HORDE_ITEM_AMOUNT_RATIO) == 0 && GetConfig(CONFIG_AHBOT_NEUTRAL_ITEM_AMOUNT_RATIO) == 0 &&
        !GetConfig(CONFIG_AHBOT_BUYER_ALLIANCE_ENABLED) && !GetConfig(CONFIG_AHBOT_BUYER_HORDE_ENABLED) && !GetConfig(CONFIG_AHBOT_BUYER_NEUTRAL_ENABLED))
    {
        TC_LOG_INFO("ahbot", "All feature of AuctionHouseBot are disabled!");
        return false;
    }

    if (GetConfig(CONFIG_AHBOT_ALLIANCE_ITEM_AMOUNT_RATIO) == 0 && GetConfig(CONFIG_AHBOT_HORDE_ITEM_AMOUNT_RATIO) == 0 && GetConfig(CONFIG_AHBOT_NEUTRAL_ITEM_AMOUNT_RATIO) == 0)
        TC_LOG_INFO("ahbot", "AuctionHouseBot SELLER is disabled!");

    if (!GetConfig(CONFIG_AHBOT_BUYER_ALLIANCE_ENABLED) && !GetConfig(CONFIG_AHBOT_BUYER_HORDE_ENABLED) && !GetConfig(CONFIG_AHBOT_BUYER_NEUTRAL_ENABLED))
        TC_LOG_INFO("ahbot", "AuctionHouseBot BUYER is disabled!");

    if (sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_AUCTION))
    {
        TC_LOG_INFO("ahbot", "AllowTwoSide.Interaction.Auction is enabled, AuctionHouseBot faction-specific settings might not work as expected!");
        if (GetConfig(CONFIG_AHBOT_ALLIANCE_ITEM_AMOUNT_RATIO) != 0 || GetConfig(CONFIG_AHBOT_HORDE_ITEM_AMOUNT_RATIO) != 0
            || GetConfig(CONFIG_AHBOT_BUYER_ALLIANCE_ENABLED) || GetConfig(CONFIG_AHBOT_BUYER_HORDE_ENABLED))
            TC_LOG_WARN("ahbot", "AllowTwoSide.Interaction.Auction is enabled, AuctionHouseBot should be enabled only for Neutral faction!");
    }

    _itemsPerCycleBoost = GetConfig(CONFIG_AHBOT_ITEMS_PER_CYCLE_BOOST);
    _itemsPerCycleNormal = GetConfig(CONFIG_AHBOT_ITEMS_PER_CYCLE_NORMAL);

    if (uint32 ahBotAccId = GetConfig(CONFIG_AHBOT_ACCOUNT_ID))
    {
        // check character count
        if (AccountMgr::GetCharactersCount(GetConfig(CONFIG_AHBOT_ACCOUNT_ID)))
        {
            // find account guids associated with ahbot account
            uint32 count = 0;
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARS_BY_ACCOUNT_ID);
            stmt->setUInt32(0, ahBotAccId);
            if (PreparedQueryResult result = CharacterDatabase.Query(stmt))
            {
                do
                {
                    Field* fields = result->Fetch();
                    _AHBotCharacters.push_back(fields[0].GetUInt32());
                    ++count;
                } while (result->NextRow());
            }

            TC_LOG_DEBUG("ahbot", "AuctionHouseBot found {} characters", count);
        }
        else
            TC_LOG_WARN("ahbot", "AuctionHouseBot Account ID {} has no associated characters.", ahBotAccId);
    }

    // Initialize new subsystems
    sAuctionBotData->Initialize();
    sAuctionBotFilter->Initialize();
    sAuctionBotPricing->Initialize();

    return true;
}

void AuctionBotConfig::SetConfig(AuctionBotConfigUInt32Values index, char const* fieldname, uint32 defvalue)
{
    SetConfig(index, sConfigMgr->GetIntDefault(fieldname, defvalue));

    if (int32(GetConfig(index)) < 0)
    {
        TC_LOG_ERROR("ahbot", "AHBot: {} ({}) can't be negative. Using {} instead.", fieldname, int32(GetConfig(index)), defvalue);
        SetConfig(index, defvalue);
    }
}

void AuctionBotConfig::SetConfigMax(AuctionBotConfigUInt32Values index, char const* fieldname, uint32 defvalue, uint32 maxvalue)
{
    SetConfig(index, sConfigMgr->GetIntDefault(fieldname, defvalue));

    if (GetConfig(index) > maxvalue)
    {
        TC_LOG_ERROR("ahbot", "AHBot: {} ({}) must be in range 0...{}. Using {} instead.", fieldname, GetConfig(index), maxvalue, maxvalue);
        SetConfig(index, maxvalue);
    }
}

void AuctionBotConfig::SetConfigMinMax(AuctionBotConfigUInt32Values index, char const* fieldname, uint32 defvalue, uint32 minvalue, uint32 maxvalue)
{
    SetConfig(index, sConfigMgr->GetIntDefault(fieldname, defvalue));

    if (GetConfig(index) > maxvalue)
    {
        TC_LOG_ERROR("ahbot", "AHBot: {} ({}) must be in range {}...{}. Using {} instead.", fieldname, GetConfig(index), minvalue, maxvalue, maxvalue);
        SetConfig(index, maxvalue);
    }

    if (GetConfig(index) < minvalue)
    {
        TC_LOG_ERROR("ahbot", "AHBot: {} ({}) must be in range {}...{}. Using {} instead.", fieldname, GetConfig(index), minvalue, maxvalue, minvalue);
        SetConfig(index, minvalue);
    }
}

void AuctionBotConfig::SetConfig(AuctionBotConfigBoolValues index, char const* fieldname, bool defvalue)
{
    SetConfig(index, sConfigMgr->GetBoolDefault(fieldname, defvalue));
}

void AuctionBotConfig::SetConfig(AuctionBotConfigFloatValues index, char const* fieldname, float defvalue)
{
    SetConfig(index, sConfigMgr->GetFloatDefault(fieldname, defvalue));
}

//Get AuctionHousebot configuration file
void AuctionBotConfig::GetConfigFromFile()
{
    SetConfig(CONFIG_AHBOT_ACCOUNT_ID, "AuctionHouseBot.Account", 0);

    SetConfigMax(CONFIG_AHBOT_ALLIANCE_ITEM_AMOUNT_RATIO, "AuctionHouseBot.Alliance.Items.Amount.Ratio", 100, 10000);
    SetConfigMax(CONFIG_AHBOT_HORDE_ITEM_AMOUNT_RATIO, "AuctionHouseBot.Horde.Items.Amount.Ratio", 100, 10000);
    SetConfigMax(CONFIG_AHBOT_NEUTRAL_ITEM_AMOUNT_RATIO, "AuctionHouseBot.Neutral.Items.Amount.Ratio", 100, 10000);

    SetAHBotIncludes(sConfigMgr->GetStringDefault("AuctionHouseBot.forceIncludeItems", ""));
    SetAHBotExcludes(sConfigMgr->GetStringDefault("AuctionHouseBot.forceExcludeItems", ""));

    SetConfig(CONFIG_AHBOT_BUYER_ALLIANCE_ENABLED, "AuctionHouseBot.Buyer.Alliance.Enabled", false);
    SetConfig(CONFIG_AHBOT_BUYER_HORDE_ENABLED, "AuctionHouseBot.Buyer.Horde.Enabled", false);
    SetConfig(CONFIG_AHBOT_BUYER_NEUTRAL_ENABLED, "AuctionHouseBot.Buyer.Neutral.Enabled", false);

    SetConfig(CONFIG_AHBOT_BUYER_CHANCE_FACTOR, "AuctionHouseBot.Buyer.ChanceFactor", 2.0f);

    SetConfig(CONFIG_AHBOT_ITEMS_VENDOR, "AuctionHouseBot.Items.Vendor", false);
    SetConfig(CONFIG_AHBOT_ITEMS_LOOT, "AuctionHouseBot.Items.Loot", true);
    SetConfig(CONFIG_AHBOT_ITEMS_MISC, "AuctionHouseBot.Items.Misc", false);

    SetConfig(CONFIG_AHBOT_BIND_NO, "AuctionHouseBot.Bind.No", true);
    SetConfig(CONFIG_AHBOT_BIND_PICKUP, "AuctionHouseBot.Bind.Pickup", false);
    SetConfig(CONFIG_AHBOT_BIND_EQUIP, "AuctionHouseBot.Bind.Equip", true);
    SetConfig(CONFIG_AHBOT_BIND_USE, "AuctionHouseBot.Bind.Use", true);
    SetConfig(CONFIG_AHBOT_BIND_QUEST, "AuctionHouseBot.Bind.Quest", false);
    SetConfig(CONFIG_AHBOT_LOCKBOX_ENABLED, "AuctionHouseBot.LockBox.Enabled", false);

    SetConfig(CONFIG_AHBOT_BUYPRICE_SELLER, "AuctionHouseBot.BuyPrice.Seller", false);

    SetConfig(CONFIG_AHBOT_ITEMS_PER_CYCLE_BOOST, "AuctionHouseBot.ItemsPerCycle.Boost", 1000);
    SetConfig(CONFIG_AHBOT_ITEMS_PER_CYCLE_NORMAL, "AuctionHouseBot.ItemsPerCycle.Normal", 20);

    SetConfig(CONFIG_AHBOT_ITEM_MIN_ITEM_LEVEL, "AuctionHouseBot.Items.ItemLevel.Min", 0);
    SetConfig(CONFIG_AHBOT_ITEM_MAX_ITEM_LEVEL, "AuctionHouseBot.Items.ItemLevel.Max", 0);
    SetConfig(CONFIG_AHBOT_ITEM_MIN_REQ_LEVEL, "AuctionHouseBot.Items.ReqLevel.Min", 0);
    SetConfig(CONFIG_AHBOT_ITEM_MAX_REQ_LEVEL, "AuctionHouseBot.Items.ReqLevel.Max", 0);
    SetConfig(CONFIG_AHBOT_ITEM_MIN_SKILL_RANK, "AuctionHouseBot.Items.ReqSkill.Min", 0);
    SetConfig(CONFIG_AHBOT_ITEM_MAX_SKILL_RANK, "AuctionHouseBot.Items.ReqSkill.Max", 0);

    SetConfig(CONFIG_AHBOT_ITEM_GRAY_AMOUNT, "AuctionHouseBot.Items.Amount.Gray", 0);
    SetConfig(CONFIG_AHBOT_ITEM_WHITE_AMOUNT, "AuctionHouseBot.Items.Amount.White", 2000);
    SetConfig(CONFIG_AHBOT_ITEM_GREEN_AMOUNT, "AuctionHouseBot.Items.Amount.Green", 2500);
    SetConfig(CONFIG_AHBOT_ITEM_BLUE_AMOUNT, "AuctionHouseBot.Items.Amount.Blue", 1500);
    SetConfig(CONFIG_AHBOT_ITEM_PURPLE_AMOUNT, "AuctionHouseBot.Items.Amount.Purple", 500);
    SetConfig(CONFIG_AHBOT_ITEM_ORANGE_AMOUNT, "AuctionHouseBot.Items.Amount.Orange", 0);
    SetConfig(CONFIG_AHBOT_ITEM_YELLOW_AMOUNT, "AuctionHouseBot.Items.Amount.Yellow", 0);

    SetConfigMax(CONFIG_AHBOT_CLASS_CONSUMABLE_PRIORITY, "AuctionHouseBot.Class.Consumable", 6, 10);
    SetConfigMax(CONFIG_AHBOT_CLASS_CONTAINER_PRIORITY, "AuctionHouseBot.Class.Container", 4, 10);
    SetConfigMax(CONFIG_AHBOT_CLASS_WEAPON_PRIORITY, "AuctionHouseBot.Class.Weapon", 8, 10);
    SetConfigMax(CONFIG_AHBOT_CLASS_GEM_PRIORITY, "AuctionHouseBot.Class.Gem", 3, 10);
    SetConfigMax(CONFIG_AHBOT_CLASS_ARMOR_PRIORITY, "AuctionHouseBot.Class.Armor", 8, 10);
    SetConfigMax(CONFIG_AHBOT_CLASS_REAGENT_PRIORITY, "AuctionHouseBot.Class.Reagent", 1, 10);
    SetConfigMax(CONFIG_AHBOT_CLASS_PROJECTILE_PRIORITY, "AuctionHouseBot.Class.Projectile", 2, 10);
    SetConfigMax(CONFIG_AHBOT_CLASS_TRADEGOOD_PRIORITY, "AuctionHouseBot.Class.TradeGood", 10, 10);
    SetConfigMax(CONFIG_AHBOT_CLASS_GENERIC_PRIORITY, "AuctionHouseBot.Class.Generic", 1, 10);
    SetConfigMax(CONFIG_AHBOT_CLASS_RECIPE_PRIORITY, "AuctionHouseBot.Class.Recipe", 6, 10);
    SetConfigMax(CONFIG_AHBOT_CLASS_QUIVER_PRIORITY, "AuctionHouseBot.Class.Quiver", 1, 10);
    SetConfigMax(CONFIG_AHBOT_CLASS_QUEST_PRIORITY, "AuctionHouseBot.Class.Quest", 1, 10);
    SetConfigMax(CONFIG_AHBOT_CLASS_KEY_PRIORITY, "AuctionHouseBot.Class.Key", 1, 10);
    SetConfigMax(CONFIG_AHBOT_CLASS_MISC_PRIORITY, "AuctionHouseBot.Class.Misc", 5, 10);
    SetConfigMax(CONFIG_AHBOT_CLASS_GLYPH_PRIORITY, "AuctionHouseBot.Class.Glyph", 3, 10);

    SetConfig(CONFIG_AHBOT_ALLIANCE_PRICE_RATIO, "AuctionHouseBot.Alliance.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_HORDE_PRICE_RATIO, "AuctionHouseBot.Horde.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_NEUTRAL_PRICE_RATIO, "AuctionHouseBot.Neutral.Price.Ratio", 100);

    SetConfig(CONFIG_AHBOT_ITEM_GRAY_PRICE_RATIO, "AuctionHouseBot.Items.Gray.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_ITEM_WHITE_PRICE_RATIO, "AuctionHouseBot.Items.White.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_ITEM_GREEN_PRICE_RATIO, "AuctionHouseBot.Items.Green.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_ITEM_BLUE_PRICE_RATIO, "AuctionHouseBot.Items.Blue.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_ITEM_PURPLE_PRICE_RATIO, "AuctionHouseBot.Items.Purple.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_ITEM_ORANGE_PRICE_RATIO, "AuctionHouseBot.Items.Orange.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_ITEM_YELLOW_PRICE_RATIO, "AuctionHouseBot.Items.Yellow.Price.Ratio", 100);

    SetConfig(CONFIG_AHBOT_CLASS_CONSUMABLE_PRICE_RATIO, "AuctionHouseBot.Class.Consumable.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_CLASS_CONTAINER_PRICE_RATIO, "AuctionHouseBot.Class.Container.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_CLASS_WEAPON_PRICE_RATIO, "AuctionHouseBot.Class.Weapon.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_CLASS_GEM_PRICE_RATIO, "AuctionHouseBot.Class.Gem.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_CLASS_ARMOR_PRICE_RATIO, "AuctionHouseBot.Class.Armor.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_CLASS_REAGENT_PRICE_RATIO, "AuctionHouseBot.Class.Reagent.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_CLASS_PROJECTILE_PRICE_RATIO, "AuctionHouseBot.Class.Projectile.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_CLASS_TRADEGOOD_PRICE_RATIO, "AuctionHouseBot.Class.TradeGood.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_CLASS_GENERIC_PRICE_RATIO, "AuctionHouseBot.Class.Generic.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_CLASS_RECIPE_PRICE_RATIO, "AuctionHouseBot.Class.Recipe.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_CLASS_QUIVER_PRICE_RATIO, "AuctionHouseBot.Class.Quiver.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_CLASS_QUEST_PRICE_RATIO, "AuctionHouseBot.Class.Quest.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_CLASS_KEY_PRICE_RATIO, "AuctionHouseBot.Class.Key.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_CLASS_MISC_PRICE_RATIO, "AuctionHouseBot.Class.Misc.Price.Ratio", 100);
    SetConfig(CONFIG_AHBOT_CLASS_GLYPH_PRICE_RATIO, "AuctionHouseBot.Class.Glyph.Price.Ratio", 100);

    SetConfig(CONFIG_AHBOT_CLASS_CONSUMABLE_ALLOW_ZERO, "AuctionHouseBot.Class.Consumable.Allow.Zero", false);
    SetConfig(CONFIG_AHBOT_CLASS_CONTAINER_ALLOW_ZERO, "AuctionHouseBot.Class.Container.Allow.Zero", false);
    SetConfig(CONFIG_AHBOT_CLASS_WEAPON_ALLOW_ZERO, "AuctionHouseBot.Class.Weapon.Allow.Zero", false);
    SetConfig(CONFIG_AHBOT_CLASS_GEM_ALLOW_ZERO, "AuctionHouseBot.Class.Gem.Allow.Zero", false);
    SetConfig(CONFIG_AHBOT_CLASS_ARMOR_ALLOW_ZERO, "AuctionHouseBot.Class.Armor.Allow.Zero", false);
    SetConfig(CONFIG_AHBOT_CLASS_REAGENT_ALLOW_ZERO, "AuctionHouseBot.Class.Reagent.Allow.Zero", false);
    SetConfig(CONFIG_AHBOT_CLASS_PROJECTILE_ALLOW_ZERO, "AuctionHouseBot.Class.Projectile.Allow.Zero", false);
    SetConfig(CONFIG_AHBOT_CLASS_TRADEGOOD_ALLOW_ZERO, "AuctionHouseBot.Class.TradeGood.Allow.Zero", false);
    SetConfig(CONFIG_AHBOT_CLASS_RECIPE_ALLOW_ZERO, "AuctionHouseBot.Class.Recipe.Allow.Zero", false);
    SetConfig(CONFIG_AHBOT_CLASS_QUIVER_ALLOW_ZERO, "AuctionHouseBot.Class.Quiver.Allow.Zero", false);
    SetConfig(CONFIG_AHBOT_CLASS_QUEST_ALLOW_ZERO, "AuctionHouseBot.Class.Quest.Allow.Zero", false);
    SetConfig(CONFIG_AHBOT_CLASS_KEY_ALLOW_ZERO, "AuctionHouseBot.Class.Key.Allow.Zero", false);
    SetConfig(CONFIG_AHBOT_CLASS_MISC_ALLOW_ZERO, "AuctionHouseBot.Class.Misc.Allow.Zero", false);
    SetConfig(CONFIG_AHBOT_CLASS_GLYPH_ALLOW_ZERO, "AuctionHouseBot.Class.Glyph.Allow.Zero", false);

    SetConfig(CONFIG_AHBOT_MINTIME, "AuctionHouseBot.MinTime", 1);
    SetConfig(CONFIG_AHBOT_MAXTIME, "AuctionHouseBot.MaxTime", 72);

    SetConfigMinMax(CONFIG_AHBOT_BUYER_RECHECK_INTERVAL, "AuctionHouseBot.Buyer.Recheck.Interval", 20, 1, DAY / MINUTE);
    SetConfig(CONFIG_AHBOT_BUYER_BASEPRICE_GRAY, "AuctionHouseBot.Buyer.Baseprice.Gray", 3504);
    SetConfig(CONFIG_AHBOT_BUYER_BASEPRICE_WHITE, "AuctionHouseBot.Buyer.Baseprice.White", 5429);
    SetConfig(CONFIG_AHBOT_BUYER_BASEPRICE_GREEN, "AuctionHouseBot.Buyer.Baseprice.Green", 21752);
    SetConfig(CONFIG_AHBOT_BUYER_BASEPRICE_BLUE, "AuctionHouseBot.Buyer.Baseprice.Blue", 36463);
    SetConfig(CONFIG_AHBOT_BUYER_BASEPRICE_PURPLE, "AuctionHouseBot.Buyer.Baseprice.Purple", 87124);
    SetConfig(CONFIG_AHBOT_BUYER_BASEPRICE_ORANGE, "AuctionHouseBot.Buyer.Baseprice.Orange", 214347);
    SetConfig(CONFIG_AHBOT_BUYER_BASEPRICE_YELLOW, "AuctionHouseBot.Buyer.Baseprice.Yellow", 407406);
    SetConfig(CONFIG_AHBOT_BUYER_CHANCEMULTIPLIER_GRAY, "AuctionHouseBot.Buyer.ChanceMultiplier.Gray", 100);
    SetConfig(CONFIG_AHBOT_BUYER_CHANCEMULTIPLIER_WHITE, "AuctionHouseBot.Buyer.ChanceMultiplier.White", 100);
    SetConfig(CONFIG_AHBOT_BUYER_CHANCEMULTIPLIER_GREEN, "AuctionHouseBot.Buyer.ChanceMultiplier.Green", 100);
    SetConfig(CONFIG_AHBOT_BUYER_CHANCEMULTIPLIER_BLUE, "AuctionHouseBot.Buyer.ChanceMultiplier.Blue", 100);
    SetConfig(CONFIG_AHBOT_BUYER_CHANCEMULTIPLIER_PURPLE, "AuctionHouseBot.Buyer.ChanceMultiplier.Purple", 100);
    SetConfig(CONFIG_AHBOT_BUYER_CHANCEMULTIPLIER_ORANGE, "AuctionHouseBot.Buyer.ChanceMultiplier.Orange", 100);
    SetConfig(CONFIG_AHBOT_BUYER_CHANCEMULTIPLIER_YELLOW, "AuctionHouseBot.Buyer.ChanceMultiplier.Yellow", 100);

    SetConfig(CONFIG_AHBOT_SELLER_ENABLED, "AuctionHouseBot.Seller.Enabled", false);
    SetConfig(CONFIG_AHBOT_BUYER_ENABLED, "AuctionHouseBot.Buyer.Enabled", false);

    SetConfig(CONFIG_AHBOT_CLASS_MISC_MOUNT_MIN_REQ_LEVEL, "AuctionHouseBot.Class.Misc.Mount.ReqLevel.Min", 0);
    SetConfig(CONFIG_AHBOT_CLASS_MISC_MOUNT_MAX_REQ_LEVEL, "AuctionHouseBot.Class.Misc.Mount.ReqLevel.Max", 0);
    SetConfig(CONFIG_AHBOT_CLASS_MISC_MOUNT_MIN_SKILL_RANK, "AuctionHouseBot.Class.Misc.Mount.ReqSkill.Min", 0);
    SetConfig(CONFIG_AHBOT_CLASS_MISC_MOUNT_MAX_SKILL_RANK, "AuctionHouseBot.Class.Misc.Mount.ReqSkill.Max", 0);
    SetConfig(CONFIG_AHBOT_CLASS_GLYPH_MIN_REQ_LEVEL, "AuctionHouseBot.Class.Glyph.ReqLevel.Min", 0);
    SetConfig(CONFIG_AHBOT_CLASS_GLYPH_MAX_REQ_LEVEL, "AuctionHouseBot.Class.Glyph.ReqLevel.Max", 0);
    SetConfig(CONFIG_AHBOT_CLASS_GLYPH_MIN_ITEM_LEVEL, "AuctionHouseBot.Class.Glyph.ItemLevel.Min", 0);
    SetConfig(CONFIG_AHBOT_CLASS_GLYPH_MAX_ITEM_LEVEL, "AuctionHouseBot.Class.Glyph.ItemLevel.Max", 0);
    SetConfig(CONFIG_AHBOT_CLASS_TRADEGOOD_MIN_ITEM_LEVEL, "AuctionHouseBot.Class.TradeGood.ItemLevel.Min", 0);
    SetConfig(CONFIG_AHBOT_CLASS_TRADEGOOD_MAX_ITEM_LEVEL, "AuctionHouseBot.Class.TradeGood.ItemLevel.Max", 0);
    SetConfig(CONFIG_AHBOT_CLASS_CONTAINER_MIN_ITEM_LEVEL, "AuctionHouseBot.Class.Container.ItemLevel.Min", 0);
    SetConfig(CONFIG_AHBOT_CLASS_CONTAINER_MAX_ITEM_LEVEL, "AuctionHouseBot.Class.Container.ItemLevel.Max", 0);

    SetConfig(CONFIG_AHBOT_CLASS_RANDOMSTACKRATIO_CONSUMABLE, "AuctionHouseBot.Class.RandomStackRatio.Consumable", 20);
    SetConfig(CONFIG_AHBOT_CLASS_RANDOMSTACKRATIO_CONTAINER, "AuctionHouseBot.Class.RandomStackRatio.Container", 0);
    SetConfig(CONFIG_AHBOT_CLASS_RANDOMSTACKRATIO_WEAPON, "AuctionHouseBot.Class.RandomStackRatio.Weapon", 0);
    SetConfig(CONFIG_AHBOT_CLASS_RANDOMSTACKRATIO_GEM, "AuctionHouseBot.Class.RandomStackRatio.Gem", 20);
    SetConfig(CONFIG_AHBOT_CLASS_RANDOMSTACKRATIO_ARMOR, "AuctionHouseBot.Class.RandomStackRatio.Armor", 0);
    SetConfig(CONFIG_AHBOT_CLASS_RANDOMSTACKRATIO_REAGENT, "AuctionHouseBot.Class.RandomStackRatio.Reagent", 100);
    SetConfig(CONFIG_AHBOT_CLASS_RANDOMSTACKRATIO_PROJECTILE, "AuctionHouseBot.Class.RandomStackRatio.Projectile", 100);
    SetConfig(CONFIG_AHBOT_CLASS_RANDOMSTACKRATIO_TRADEGOOD, "AuctionHouseBot.Class.RandomStackRatio.TradeGood", 50);
    SetConfig(CONFIG_AHBOT_CLASS_RANDOMSTACKRATIO_GENERIC, "AuctionHouseBot.Class.RandomStackRatio.Generic", 100);
    SetConfig(CONFIG_AHBOT_CLASS_RANDOMSTACKRATIO_RECIPE, "AuctionHouseBot.Class.RandomStackRatio.Recipe", 0);
    SetConfig(CONFIG_AHBOT_CLASS_RANDOMSTACKRATIO_QUIVER, "AuctionHouseBot.Class.RandomStackRatio.Quiver", 0);
    SetConfig(CONFIG_AHBOT_CLASS_RANDOMSTACKRATIO_QUEST, "AuctionHouseBot.Class.RandomStackRatio.Quest", 100);
    SetConfig(CONFIG_AHBOT_CLASS_RANDOMSTACKRATIO_KEY, "AuctionHouseBot.Class.RandomStackRatio.Key", 100);
    SetConfig(CONFIG_AHBOT_CLASS_RANDOMSTACKRATIO_MISC, "AuctionHouseBot.Class.RandomStackRatio.Misc", 100);
    SetConfig(CONFIG_AHBOT_CLASS_RANDOMSTACKRATIO_GLYPH, "AuctionHouseBot.Class.RandomStackRatio.Glyph", 0);

    SetConfig(CONFIG_AHBOT_BIDPRICE_MIN, "AuctionHouseBot.BidPrice.Min", 0.6f);
    SetConfig(CONFIG_AHBOT_BIDPRICE_MAX, "AuctionHouseBot.BidPrice.Max", 0.9f);

    // === NEW: Drop Tier configs ===
    SetConfig(CONFIG_AHBOT_TIER_WEIGHT_50_PERCENT, "AuctionHouseBot.Tier.Weight.50Percent", 100);
    SetConfig(CONFIG_AHBOT_TIER_WEIGHT_10_PERCENT, "AuctionHouseBot.Tier.Weight.10Percent", 80);
    SetConfig(CONFIG_AHBOT_TIER_WEIGHT_5_PERCENT, "AuctionHouseBot.Tier.Weight.5Percent", 60);
    SetConfig(CONFIG_AHBOT_TIER_WEIGHT_2_PERCENT, "AuctionHouseBot.Tier.Weight.2Percent", 40);
    SetConfig(CONFIG_AHBOT_TIER_WEIGHT_1_PERCENT, "AuctionHouseBot.Tier.Weight.1Percent", 30);
    SetConfig(CONFIG_AHBOT_TIER_WEIGHT_0_5_PERCENT, "AuctionHouseBot.Tier.Weight.05Percent", 20);
    SetConfig(CONFIG_AHBOT_TIER_WEIGHT_0_2_PERCENT, "AuctionHouseBot.Tier.Weight.02Percent", 15);
    SetConfig(CONFIG_AHBOT_TIER_WEIGHT_0_1_PERCENT, "AuctionHouseBot.Tier.Weight.01Percent", 10);
    SetConfig(CONFIG_AHBOT_TIER_WEIGHT_0_05_PERCENT, "AuctionHouseBot.Tier.Weight.005Percent", 7);
    SetConfig(CONFIG_AHBOT_TIER_WEIGHT_0_02_PERCENT, "AuctionHouseBot.Tier.Weight.002Percent", 5);
    SetConfig(CONFIG_AHBOT_TIER_WEIGHT_0_01_PERCENT, "AuctionHouseBot.Tier.Weight.001Percent", 3);
    SetConfig(CONFIG_AHBOT_TIER_WEIGHT_0_005_PERCENT, "AuctionHouseBot.Tier.Weight.0005Percent", 1);
    SetConfig(CONFIG_AHBOT_TIER_WEIGHT_NO_DROP, "AuctionHouseBot.Tier.Weight.NoDrop", 50);

    // Drop tier price multipliers
    SetConfig(CONFIG_AHBOT_TIER_PRICE_50_PERCENT, "AuctionHouseBot.Tier.Price.50Percent", 0.5f);
    SetConfig(CONFIG_AHBOT_TIER_PRICE_10_PERCENT, "AuctionHouseBot.Tier.Price.10Percent", 0.75f);
    SetConfig(CONFIG_AHBOT_TIER_PRICE_5_PERCENT, "AuctionHouseBot.Tier.Price.5Percent", 1.0f);
    SetConfig(CONFIG_AHBOT_TIER_PRICE_2_PERCENT, "AuctionHouseBot.Tier.Price.2Percent", 1.25f);
    SetConfig(CONFIG_AHBOT_TIER_PRICE_1_PERCENT, "AuctionHouseBot.Tier.Price.1Percent", 1.5f);
    SetConfig(CONFIG_AHBOT_TIER_PRICE_0_5_PERCENT, "AuctionHouseBot.Tier.Price.05Percent", 2.0f);
    SetConfig(CONFIG_AHBOT_TIER_PRICE_0_2_PERCENT, "AuctionHouseBot.Tier.Price.02Percent", 2.5f);
    SetConfig(CONFIG_AHBOT_TIER_PRICE_0_1_PERCENT, "AuctionHouseBot.Tier.Price.01Percent", 3.0f);
    SetConfig(CONFIG_AHBOT_TIER_PRICE_0_05_PERCENT, "AuctionHouseBot.Tier.Price.005Percent", 4.0f);
    SetConfig(CONFIG_AHBOT_TIER_PRICE_0_02_PERCENT, "AuctionHouseBot.Tier.Price.002Percent", 5.0f);
    SetConfig(CONFIG_AHBOT_TIER_PRICE_0_01_PERCENT, "AuctionHouseBot.Tier.Price.001Percent", 7.5f);
    SetConfig(CONFIG_AHBOT_TIER_PRICE_0_005_PERCENT, "AuctionHouseBot.Tier.Price.0005Percent", 10.0f);
    SetConfig(CONFIG_AHBOT_TIER_PRICE_NO_DROP, "AuctionHouseBot.Tier.Price.NoDrop", 1.0f);

    // === NEW: Pricing configs ===
    SetConfig(CONFIG_AHBOT_PRICING_VENDOR_FLOOR_PERCENT, "AuctionHouseBot.Pricing.VendorFloor", 125);
    SetConfig(CONFIG_AHBOT_PRICING_MAX_BUYOUT, "AuctionHouseBot.Pricing.MaxBuyout", 1000000000);

    SetConfig(CONFIG_AHBOT_PRICING_VARIATION_MIN, "AuctionHouseBot.Pricing.Variation.Min", -0.15f);
    SetConfig(CONFIG_AHBOT_PRICING_VARIATION_MAX, "AuctionHouseBot.Pricing.Variation.Max", 0.25f);

    // Category minimum prices
    SetConfig(CONFIG_AHBOT_PRICING_MIN_CONSUMABLE, "AuctionHouseBot.Pricing.MinPrice.Consumable", 100);
    SetConfig(CONFIG_AHBOT_PRICING_MIN_CONTAINER, "AuctionHouseBot.Pricing.MinPrice.Container", 1000);
    SetConfig(CONFIG_AHBOT_PRICING_MIN_WEAPON, "AuctionHouseBot.Pricing.MinPrice.Weapon", 5000);
    SetConfig(CONFIG_AHBOT_PRICING_MIN_GEM, "AuctionHouseBot.Pricing.MinPrice.Gem", 2000);
    SetConfig(CONFIG_AHBOT_PRICING_MIN_ARMOR, "AuctionHouseBot.Pricing.MinPrice.Armor", 5000);
    SetConfig(CONFIG_AHBOT_PRICING_MIN_REAGENT, "AuctionHouseBot.Pricing.MinPrice.Reagent", 10);
    SetConfig(CONFIG_AHBOT_PRICING_MIN_PROJECTILE, "AuctionHouseBot.Pricing.MinPrice.Projectile", 1);
    SetConfig(CONFIG_AHBOT_PRICING_MIN_TRADEGOOD, "AuctionHouseBot.Pricing.MinPrice.TradeGood", 50);
    SetConfig(CONFIG_AHBOT_PRICING_MIN_RECIPE, "AuctionHouseBot.Pricing.MinPrice.Recipe", 500);
    SetConfig(CONFIG_AHBOT_PRICING_MIN_QUIVER, "AuctionHouseBot.Pricing.MinPrice.Quiver", 500);
    SetConfig(CONFIG_AHBOT_PRICING_MIN_QUEST, "AuctionHouseBot.Pricing.MinPrice.Quest", 100);
    SetConfig(CONFIG_AHBOT_PRICING_MIN_KEY, "AuctionHouseBot.Pricing.MinPrice.Key", 100);
    SetConfig(CONFIG_AHBOT_PRICING_MIN_MISC, "AuctionHouseBot.Pricing.MinPrice.Misc", 100);
    SetConfig(CONFIG_AHBOT_PRICING_MIN_GLYPH, "AuctionHouseBot.Pricing.MinPrice.Glyph", 500);

    // === NEW: Feature toggles ===
    SetConfig(CONFIG_AHBOT_PRICING_USE_DROP_TIER, "AuctionHouseBot.Pricing.UseDropTier", true);
    SetConfig(CONFIG_AHBOT_PRICING_USE_LOGARITHMIC, "AuctionHouseBot.Pricing.UseLogarithmic", true);
    SetConfig(CONFIG_AHBOT_FILTER_RECIPE_PRODUCED, "AuctionHouseBot.Filter.RecipeProduced", false);
    SetConfig(CONFIG_AHBOT_FILTER_QUEST_REWARD, "AuctionHouseBot.Filter.QuestReward", false);
    SetConfig(CONFIG_AHBOT_USE_WEIGHTED_SELECTION, "AuctionHouseBot.UseWeightedSelection", true);

    // === NEW: Logarithmic pricing multipliers ===
    SetConfig(CONFIG_AHBOT_LOG_MULTIPLIER_POTION, "AuctionHouseBot.Pricing.Log.Potion", 0.8f);
    SetConfig(CONFIG_AHBOT_LOG_MULTIPLIER_ELIXIR, "AuctionHouseBot.Pricing.Log.Elixir", 0.9f);
    SetConfig(CONFIG_AHBOT_LOG_MULTIPLIER_FLASK, "AuctionHouseBot.Pricing.Log.Flask", 0.95f);
    SetConfig(CONFIG_AHBOT_LOG_MULTIPLIER_SCROLL, "AuctionHouseBot.Pricing.Log.Scroll", 0.7f);
    SetConfig(CONFIG_AHBOT_LOG_MULTIPLIER_FOOD, "AuctionHouseBot.Pricing.Log.Food", 0.75f);
    SetConfig(CONFIG_AHBOT_LOG_MULTIPLIER_BANDAGE, "AuctionHouseBot.Pricing.Log.Bandage", 0.7f);
    SetConfig(CONFIG_AHBOT_LOG_MULTIPLIER_CLOTH, "AuctionHouseBot.Pricing.Log.Cloth", 0.85f);
    SetConfig(CONFIG_AHBOT_LOG_MULTIPLIER_LEATHER, "AuctionHouseBot.Pricing.Log.Leather", 0.85f);
    SetConfig(CONFIG_AHBOT_LOG_MULTIPLIER_METAL, "AuctionHouseBot.Pricing.Log.Metal", 0.85f);
    SetConfig(CONFIG_AHBOT_LOG_MULTIPLIER_HERB, "AuctionHouseBot.Pricing.Log.Herb", 0.85f);
    SetConfig(CONFIG_AHBOT_LOG_MULTIPLIER_ELEMENTAL, "AuctionHouseBot.Pricing.Log.Elemental", 0.9f);
    SetConfig(CONFIG_AHBOT_LOG_MULTIPLIER_ENCHANTING, "AuctionHouseBot.Pricing.Log.Enchanting", 0.9f);
    SetConfig(CONFIG_AHBOT_LOG_MULTIPLIER_JEWELCRAFTING, "AuctionHouseBot.Pricing.Log.Jewelcrafting", 0.9f);
    SetConfig(CONFIG_AHBOT_LOG_MULTIPLIER_GEM, "AuctionHouseBot.Pricing.Log.Gem", 0.92f);

    // === NEW: Subclass priorities (Trade Goods) ===
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_TRADEGOOD_CLOTH_PRIORITY, "AuctionHouseBot.Subclass.TradeGood.Cloth", 8, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_TRADEGOOD_LEATHER_PRIORITY, "AuctionHouseBot.Subclass.TradeGood.Leather", 7, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_TRADEGOOD_METAL_PRIORITY, "AuctionHouseBot.Subclass.TradeGood.Metal", 8, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_TRADEGOOD_MEAT_PRIORITY, "AuctionHouseBot.Subclass.TradeGood.Meat", 5, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_TRADEGOOD_HERB_PRIORITY, "AuctionHouseBot.Subclass.TradeGood.Herb", 9, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_TRADEGOOD_ELEMENTAL_PRIORITY, "AuctionHouseBot.Subclass.TradeGood.Elemental", 6, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_TRADEGOOD_ENCHANTING_PRIORITY, "AuctionHouseBot.Subclass.TradeGood.Enchanting", 7, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_TRADEGOOD_JEWELCRAFTING_PRIORITY, "AuctionHouseBot.Subclass.TradeGood.Jewelcrafting", 6, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_TRADEGOOD_PARTS_PRIORITY, "AuctionHouseBot.Subclass.TradeGood.Parts", 4, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_TRADEGOOD_DEVICES_PRIORITY, "AuctionHouseBot.Subclass.TradeGood.Devices", 3, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_TRADEGOOD_EXPLOSIVES_PRIORITY, "AuctionHouseBot.Subclass.TradeGood.Explosives", 4, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_TRADEGOOD_MATERIALS_PRIORITY, "AuctionHouseBot.Subclass.TradeGood.Materials", 5, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_TRADEGOOD_OTHER_PRIORITY, "AuctionHouseBot.Subclass.TradeGood.Other", 3, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_TRADEGOOD_ARMOR_ENCHANT_PRIORITY, "AuctionHouseBot.Subclass.TradeGood.ArmorEnchant", 5, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_TRADEGOOD_WEAPON_ENCHANT_PRIORITY, "AuctionHouseBot.Subclass.TradeGood.WeaponEnchant", 5, 10);

    // === NEW: Subclass priorities (Consumables) ===
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_CONSUMABLE_POTION_PRIORITY, "AuctionHouseBot.Subclass.Consumable.Potion", 8, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_CONSUMABLE_ELIXIR_PRIORITY, "AuctionHouseBot.Subclass.Consumable.Elixir", 7, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_CONSUMABLE_FLASK_PRIORITY, "AuctionHouseBot.Subclass.Consumable.Flask", 9, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_CONSUMABLE_SCROLL_PRIORITY, "AuctionHouseBot.Subclass.Consumable.Scroll", 5, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_CONSUMABLE_FOOD_PRIORITY, "AuctionHouseBot.Subclass.Consumable.Food", 6, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_CONSUMABLE_BANDAGE_PRIORITY, "AuctionHouseBot.Subclass.Consumable.Bandage", 4, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_CONSUMABLE_ITEM_ENHANCEMENT_PRIORITY, "AuctionHouseBot.Subclass.Consumable.ItemEnhancement", 6, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_CONSUMABLE_OTHER_PRIORITY, "AuctionHouseBot.Subclass.Consumable.Other", 3, 10);

    // === NEW: Subclass priorities (Gems) ===
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_GEM_RED_PRIORITY, "AuctionHouseBot.Subclass.Gem.Red", 8, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_GEM_BLUE_PRIORITY, "AuctionHouseBot.Subclass.Gem.Blue", 7, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_GEM_YELLOW_PRIORITY, "AuctionHouseBot.Subclass.Gem.Yellow", 7, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_GEM_PURPLE_PRIORITY, "AuctionHouseBot.Subclass.Gem.Purple", 6, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_GEM_GREEN_PRIORITY, "AuctionHouseBot.Subclass.Gem.Green", 6, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_GEM_ORANGE_PRIORITY, "AuctionHouseBot.Subclass.Gem.Orange", 6, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_GEM_META_PRIORITY, "AuctionHouseBot.Subclass.Gem.Meta", 9, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_GEM_SIMPLE_PRIORITY, "AuctionHouseBot.Subclass.Gem.Simple", 3, 10);
    SetConfigMax(CONFIG_AHBOT_SUBCLASS_GEM_PRISMATIC_PRIORITY, "AuctionHouseBot.Subclass.Gem.Prismatic", 5, 10);
}

char const* AuctionBotConfig::GetHouseTypeName(AuctionHouseType houseType)
{
    static char const* names[MAX_AUCTION_HOUSE_TYPE] = { "Neutral", "Alliance", "Horde" };
    return names[houseType];
}

// Picks a random character from the list of AHBot chars
uint32 AuctionBotConfig::GetRandChar() const
{
    if (_AHBotCharacters.empty())
        return 0;

    return Trinity::Containers::SelectRandomContainerElement(_AHBotCharacters);
}

// Picks a random AHBot character, but excludes a specific one. This is used
// to have another character than the auction owner place bids
uint32 AuctionBotConfig::GetRandCharExclude(uint32 exclude) const
{
    if (_AHBotCharacters.empty())
        return 0;

    std::vector<uint32> filteredCharacters;
    filteredCharacters.reserve(_AHBotCharacters.size() - 1);

    for (uint32 charId : _AHBotCharacters)
        if (charId != exclude)
            filteredCharacters.push_back(charId);

    if (filteredCharacters.empty())
        return 0;

    return Trinity::Containers::SelectRandomContainerElement(filteredCharacters);
}

bool AuctionBotConfig::IsBotChar(uint32 characterID) const
{
    return !characterID || std::find(_AHBotCharacters.begin(), _AHBotCharacters.end(), characterID) != _AHBotCharacters.end();
}

uint32 AuctionBotConfig::GetConfigItemAmountRatio(AuctionHouseType houseType) const
{
    switch (houseType)
    {
        case AUCTION_HOUSE_ALLIANCE:
            return GetConfig(CONFIG_AHBOT_ALLIANCE_ITEM_AMOUNT_RATIO);
        case AUCTION_HOUSE_HORDE:
            return GetConfig(CONFIG_AHBOT_HORDE_ITEM_AMOUNT_RATIO);
        default:
            return GetConfig(CONFIG_AHBOT_NEUTRAL_ITEM_AMOUNT_RATIO);
    }
}

uint32 AuctionBotConfig::GetConfigPriceRatio(AuctionHouseType houseType) const
{
    switch (houseType)
    {
        case AUCTION_HOUSE_ALLIANCE:
            return GetConfig(CONFIG_AHBOT_ALLIANCE_PRICE_RATIO);
        case AUCTION_HOUSE_HORDE:
            return GetConfig(CONFIG_AHBOT_HORDE_PRICE_RATIO);
        default:
            return GetConfig(CONFIG_AHBOT_NEUTRAL_PRICE_RATIO);
    }
}

bool AuctionBotConfig::GetConfigBuyerEnabled(AuctionHouseType houseType) const
{
    switch (houseType)
    {
        case AUCTION_HOUSE_ALLIANCE:
            return GetConfig(CONFIG_AHBOT_BUYER_ALLIANCE_ENABLED);
        case AUCTION_HOUSE_HORDE:
            return GetConfig(CONFIG_AHBOT_BUYER_HORDE_ENABLED);
        default:
            return GetConfig(CONFIG_AHBOT_BUYER_NEUTRAL_ENABLED);
    }
}

uint32 AuctionBotConfig::GetConfigItemQualityAmount(AuctionQuality quality) const
{
    switch (quality)
    {
        case AUCTION_QUALITY_GRAY:
            return GetConfig(CONFIG_AHBOT_ITEM_GRAY_AMOUNT);
        case AUCTION_QUALITY_WHITE:
            return GetConfig(CONFIG_AHBOT_ITEM_WHITE_AMOUNT);
        case AUCTION_QUALITY_GREEN:
            return GetConfig(CONFIG_AHBOT_ITEM_GREEN_AMOUNT);
        case AUCTION_QUALITY_BLUE:
            return GetConfig(CONFIG_AHBOT_ITEM_BLUE_AMOUNT);
        case AUCTION_QUALITY_PURPLE:
            return GetConfig(CONFIG_AHBOT_ITEM_PURPLE_AMOUNT);
        case AUCTION_QUALITY_ORANGE:
            return GetConfig(CONFIG_AHBOT_ITEM_ORANGE_AMOUNT);
        default:
            return GetConfig(CONFIG_AHBOT_ITEM_YELLOW_AMOUNT);
    }
}

uint32 AuctionBotConfig::GetDropTierListWeight(uint8 tier) const
{
    if (tier >= 13)
        return 50;

    AuctionBotConfigUInt32Values configIndex = static_cast<AuctionBotConfigUInt32Values>(CONFIG_AHBOT_TIER_WEIGHT_50_PERCENT + tier);
    return GetConfig(configIndex);
}

float AuctionBotConfig::GetDropTierPriceMultiplier(uint8 tier) const
{
    if (tier >= 13)
        return 1.0f;

    AuctionBotConfigFloatValues configIndex = static_cast<AuctionBotConfigFloatValues>(CONFIG_AHBOT_TIER_PRICE_50_PERCENT + tier);
    return GetConfig(configIndex);
}

AuctionHouseBot::AuctionHouseBot(): _buyer(nullptr), _seller(nullptr), _operationSelector(0)
{
}

AuctionHouseBot::~AuctionHouseBot()
{
    delete _buyer;
    delete _seller;
}

void AuctionHouseBot::InitializeAgents()
{
    if (sAuctionBotConfig->GetConfig(CONFIG_AHBOT_SELLER_ENABLED))
    {
        delete _seller;

        _seller = new AuctionBotSeller();
        if (!_seller->Initialize())
        {
            delete _seller;
            _seller = nullptr;
        }
    }

    if (sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_ENABLED))
    {
        delete _buyer;

        _buyer = new AuctionBotBuyer();
        if (!_buyer->Initialize())
        {
            delete _buyer;
            _buyer = nullptr;
        }
    }
}

void AuctionHouseBot::Initialize()
{
    if (sAuctionBotConfig->Initialize())
        InitializeAgents();
}

void AuctionHouseBot::SetItemsRatio(uint32 al, uint32 ho, uint32 ne)
{
    if (_seller)
        _seller->SetItemsRatio(al, ho, ne);
}

void AuctionHouseBot::SetItemsRatioForHouse(AuctionHouseType house, uint32 val)
{
    if (_seller)
        _seller->SetItemsRatioForHouse(house, val);
}

void AuctionHouseBot::SetItemsAmount(std::array<uint32, MAX_AUCTION_QUALITY> const& amounts)
{
    if (_seller)
        _seller->SetItemsAmount(amounts);
}

void AuctionHouseBot::SetItemsAmountForQuality(AuctionQuality quality, uint32 val)
{
    if (_seller)
        _seller->SetItemsAmountForQuality(quality, val);
}

void AuctionHouseBot::ReloadAllConfig()
{
    sAuctionBotConfig->Reload();
    sAuctionBotData->Reload();
    sAuctionBotFilter->Reload();
    sAuctionBotPricing->Reload();
    InitializeAgents();
}

void AuctionHouseBot::PrepareStatusInfos(std::unordered_map<AuctionHouseType, AuctionHouseBotStatusInfoPerType>& statusInfo)
{
    for (AuctionHouseType ahType : EnumUtils::Iterate<AuctionHouseType>())
    {
        statusInfo[ahType].ItemsCount = 0;

        for (AuctionQuality quality : EnumUtils::Iterate<AuctionQuality>())
            statusInfo[ahType].QualityInfo[quality] = 0;

        AuctionHouseObject* auctionHouse = sAuctionMgr->GetAuctionsMap(ahType);
        for (AuctionHouseObject::AuctionEntryMap::const_iterator itr = auctionHouse->GetAuctionsBegin(); itr != auctionHouse->GetAuctionsEnd(); ++itr)
        {
            AuctionEntry* auctionEntry = itr->second;
            if (Item* item = sAuctionMgr->GetAItem(auctionEntry->itemGUIDLow))
            {
                ItemTemplate const* prototype = item->GetTemplate();
                if (!auctionEntry->owner || sAuctionBotConfig->IsBotChar(auctionEntry->owner)) // Add only ahbot items
                {
                    if (prototype->Quality < MAX_AUCTION_QUALITY)
                        ++statusInfo[ahType].QualityInfo[AuctionQuality(prototype->Quality)];

                    ++statusInfo[ahType].ItemsCount;
                }
            }
        }
    }
}

void AuctionHouseBot::PrepareExtendedStatusInfos(std::unordered_map<AuctionHouseType, AuctionHouseBotExtendedStatusInfo>& statusInfo)
{
    for (AuctionHouseType ahType : EnumUtils::Iterate<AuctionHouseType>())
    {
        statusInfo[ahType].ItemsCount = 0;

        for (AuctionQuality quality : EnumUtils::Iterate<AuctionQuality>())
            statusInfo[ahType].QualityInfo[quality] = 0;

        for (uint8 tier = 0; tier < 13; ++tier)
            statusInfo[ahType].TierInfo[tier] = 0;

        AuctionHouseObject* auctionHouse = sAuctionMgr->GetAuctionsMap(ahType);
        for (AuctionHouseObject::AuctionEntryMap::const_iterator itr = auctionHouse->GetAuctionsBegin(); itr != auctionHouse->GetAuctionsEnd(); ++itr)
        {
            AuctionEntry* auctionEntry = itr->second;
            if (Item* item = sAuctionMgr->GetAItem(auctionEntry->itemGUIDLow))
            {
                ItemTemplate const* prototype = item->GetTemplate();
                if (!auctionEntry->owner || sAuctionBotConfig->IsBotChar(auctionEntry->owner))
                {
                    if (prototype->Quality < MAX_AUCTION_QUALITY)
                        ++statusInfo[ahType].QualityInfo[AuctionQuality(prototype->Quality)];

                    // Get drop tier info
                    if (AuctionBotItemInfo const* info = sAuctionBotData->GetItemInfo(prototype->ItemId))
                        ++statusInfo[ahType].TierInfo[static_cast<uint8>(info->DropTier)];

                    ++statusInfo[ahType].ItemsCount;
                }
            }
        }
    }
}

void AuctionHouseBot::ForceUpdateCycle()
{
    TC_LOG_INFO("ahbot", "AHBot: Forcing update cycle...");
    Update();
}

void AuctionHouseBot::EmptyAuctions(AuctionHouseType houseType)
{
    TC_LOG_INFO("ahbot", "AHBot: Emptying {} auction house...", AuctionBotConfig::GetHouseTypeName(houseType));

    AuctionHouseObject* auctionHouse = sAuctionMgr->GetAuctionsMap(houseType);
    for (AuctionHouseObject::AuctionEntryMap::const_iterator itr = auctionHouse->GetAuctionsBegin(); itr != auctionHouse->GetAuctionsEnd(); ++itr)
        if (!itr->second->owner || sAuctionBotConfig->IsBotChar(itr->second->owner))
            itr->second->expire_time = GameTime::GetGameTime();
}

void AuctionHouseBot::Rebuild(bool all)
{
    for (uint32 i = 0; i < MAX_AUCTION_HOUSE_TYPE; ++i)
    {
        AuctionHouseObject* auctionHouse = sAuctionMgr->GetAuctionsMap(AuctionHouseType(i));
        for (AuctionHouseObject::AuctionEntryMap::const_iterator itr = auctionHouse->GetAuctionsBegin(); itr != auctionHouse->GetAuctionsEnd(); ++itr)
            if (!itr->second->owner || sAuctionBotConfig->IsBotChar(itr->second->owner)) // ahbot auction
                if (all || itr->second->bid == 0)           // expire now auction if no bid or forced
                    itr->second->expire_time = GameTime::GetGameTime();
    }
}

AuctionHouseBot* AuctionHouseBot::instance()
{
    static AuctionHouseBot instance;
    return &instance;
}

void AuctionHouseBot::Update()
{
    // nothing do...
    if (!_buyer && !_seller)
        return;

    // scan all possible update cases until first success
    for (uint32 count = 0; count < 2 * MAX_AUCTION_HOUSE_TYPE; ++count)
    {
        bool successStep = false;

        if (_operationSelector < MAX_AUCTION_HOUSE_TYPE)
        {
            if (_seller)
                successStep = _seller->Update(AuctionHouseType(_operationSelector));
        }
        else
        {
            if (_buyer)
                successStep = _buyer->Update(AuctionHouseType(_operationSelector - MAX_AUCTION_HOUSE_TYPE));
        }

        ++_operationSelector;
        if (_operationSelector >= 2 * MAX_AUCTION_HOUSE_TYPE)
            _operationSelector = 0;

        // one success update per call
        if (successStep)
            break;
    }
}
