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

#include "AuctionHouseBotBuyer.h"
#include "AuctionHouseBotPricing.h"
#include "GameTime.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "Random.h"

AuctionBotBuyer::AuctionBotBuyer() : _checkInterval(20 * MINUTE)
{
    // Define faction for our main data class.
    for (int i = 0; i < MAX_AUCTION_HOUSE_TYPE; ++i)
        _houseConfig[i].Initialize(AuctionHouseType(i));
}

AuctionBotBuyer::~AuctionBotBuyer()
{
}

bool AuctionBotBuyer::Initialize()
{
    LoadConfig();

    bool activeHouse = false;
    for (int i = 0; i < MAX_AUCTION_HOUSE_TYPE; ++i)
    {
        if (_houseConfig[i].BuyerEnabled)
        {
            activeHouse = true;
            break;
        }
    }

    if (!activeHouse)
        return false;

    // load Check interval
    _checkInterval = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_RECHECK_INTERVAL) * MINUTE;
    TC_LOG_DEBUG("ahbot", "AHBot buyer interval is {} minutes", _checkInterval / MINUTE);

    // Load advanced pricing setting
    _useAdvancedPricing = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_USE_ADVANCED_PRICING) != 0;
    if (_useAdvancedPricing)
        TC_LOG_INFO("ahbot", "AHBot buyer using advanced pricing evaluation");

    return true;
}

void AuctionBotBuyer::LoadConfig()
{
    for (int i = 0; i < MAX_AUCTION_HOUSE_TYPE; ++i)
    {
        _houseConfig[i].BuyerEnabled = sAuctionBotConfig->GetConfigBuyerEnabled(AuctionHouseType(i));
        if (_houseConfig[i].BuyerEnabled)
            LoadBuyerValues(_houseConfig[i]);
    }
}

void AuctionBotBuyer::LoadBuyerValues(BuyerConfiguration& config)
{
    // Load advanced pricing settings
    config.UseAdvancedPricing = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_USE_ADVANCED_PRICING) != 0;
    config.MaxPriceRatio = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_MAX_PRICE_RATIO) / 100.0f;
    config.GoodDealThreshold = sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_GOOD_DEAL_THRESHOLD) / 100.0f;

    if (config.MaxPriceRatio < 0.1f)
        config.MaxPriceRatio = 1.5f;
    if (config.GoodDealThreshold < 0.1f)
        config.GoodDealThreshold = 0.8f;
}

// Makes an AHbot buyer cycle for AH type if necessary
bool AuctionBotBuyer::Update(AuctionHouseType houseType)
{
    if (!sAuctionBotConfig->GetConfigBuyerEnabled(houseType))
        return false;

    TC_LOG_DEBUG("ahbot", "AHBot: {} buying ...", AuctionBotConfig::GetHouseTypeName(houseType));

    BuyerConfiguration& config = _houseConfig[houseType];
    uint32 eligibleItems = GetItemInformation(config);
    if (eligibleItems)
    {
        // Prepare list of items to bid or buy - remove old items
        PrepareListOfEntry(config);
        // Process buying and bidding items
        BuyAndBidItems(config);
    }

    return true;
}

// Collects information about item counts and minimum prices to SameItemInfo and updates EligibleItems - a list with new items eligible for bot to buy and bid
// Returns count of items in AH that were eligible for being bought or bidded on by ahbot buyer (EligibleItems size)
uint32 AuctionBotBuyer::GetItemInformation(BuyerConfiguration& config)
{
    config.SameItemInfo.clear();
    time_t now = GameTime::GetGameTime();
    uint32 count = 0;

    AuctionHouseObject* house = sAuctionMgr->GetAuctionsMap(config.GetHouseType());
    for (AuctionHouseObject::AuctionEntryMap::const_iterator itr = house->GetAuctionsBegin(); itr != house->GetAuctionsEnd(); ++itr)
    {
        AuctionEntry* entry = itr->second;

        if (!entry->owner || sAuctionBotConfig->IsBotChar(entry->owner))
            continue; // Skip auctions owned by AHBot

        Item* item = sAuctionMgr->GetAItem(entry->itemGUIDLow);
        if (!item)
            continue;

        BuyerItemInfo& itemInfo = config.SameItemInfo[item->GetEntry()];

        // Update item entry's count and total bid prices
        // This can be used later to determine the prices and chances to bid
        uint32 itemBidPrice = entry->startbid / item->GetCount();
        itemInfo.TotalBidPrice = itemInfo.TotalBidPrice + itemBidPrice;
        itemInfo.BidItemCount++;

        // Set minimum bid price
        if (!itemInfo.MinBidPrice)
            itemInfo.MinBidPrice = itemBidPrice;
        else
            itemBidPrice = std::min(itemInfo.MinBidPrice, itemBidPrice);

        // Set minimum buyout price if item has buyout
        if (entry->buyout)
        {
            // Update item entry's count and total buyout prices
            // This can be used later to determine the prices and chances to buyout
            uint32 itemBuyPrice = entry->buyout / item->GetCount();
            itemInfo.TotalBuyPrice = itemInfo.TotalBuyPrice + itemBuyPrice;
            itemInfo.BuyItemCount++;

            if (!itemInfo.MinBuyPrice)
                itemInfo.MinBuyPrice = itemBuyPrice;
            else
                itemInfo.MinBuyPrice = std::min(itemInfo.MinBuyPrice, itemBuyPrice);
        }

        // Add/update EligibleItems if:
        // * no bid
        // * bid from player
        if (!entry->bid || entry->bidder)
        {
            config.EligibleItems[entry->Id].LastExist = now;
            config.EligibleItems[entry->Id].AuctionId = entry->Id;
            ++count;
        }
    }

    TC_LOG_DEBUG("ahbot", "AHBot: {} items added to buyable/biddable vector for ah type: {}", count, config.GetHouseType());
    TC_LOG_DEBUG("ahbot", "AHBot: SameItemInfo size = {}", (uint32)config.SameItemInfo.size());
    return count;
}

// ahInfo can be NULL
bool AuctionBotBuyer::RollBuyChance(BuyerItemInfo const* ahInfo, Item const* item, AuctionEntry const* auction, uint32 /*bidPrice*/)
{
    if (!auction->buyout)
        return false;

    float itemBuyPrice = float(auction->buyout / item->GetCount());
    float itemPrice = float(item->GetTemplate()->SellPrice ? item->GetTemplate()->SellPrice : GetVendorPrice(item->GetTemplate()->Quality));
    // The AH cut needs to be added to the price, but we dont want a 100% chance to buy if the price is exactly AH default
    itemPrice *= 1.4f;

    // This value is between 0 and 100 and is used directly as the chance to buy or bid
    // Value equal or above 100 means 100% chance and value below 0 means 0% chance
    float chance = std::min(100.f, std::pow(100.f, 1.f + (1.f - itemBuyPrice / itemPrice) / sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_CHANCE_FACTOR)));

    // If a player has bidded on item, have fifth of normal chance
    if (auction->bidder)
        chance = chance / 5.f;

    if (ahInfo)
    {
        float avgBuyPrice = ahInfo->TotalBuyPrice / float(ahInfo->BuyItemCount);

        TC_LOG_DEBUG("ahbot", "AHBot: buyout average: {:.1f} items with buyout: {}", avgBuyPrice, ahInfo->BuyItemCount);

        // If there are more than 5 items on AH of this entry, try weigh in the average buyout price
        if (ahInfo->BuyItemCount > 5)
            chance *= 1.f / std::sqrt(itemBuyPrice / avgBuyPrice);
    }

    // Add config weigh in for quality
    chance *= GetChanceMultiplier(item->GetTemplate()->Quality) / 100.0f;

    float rand = frand(0.f, 100.f);
    bool win = rand <= chance;
    TC_LOG_DEBUG("ahbot", "AHBot: {} BUY! chance = {:.2f}, price = {}, buyprice = {}.", win ? "WIN" : "LOSE", chance, uint32(itemPrice), uint32(itemBuyPrice));
    return win;
}

// ahInfo can be NULL
bool AuctionBotBuyer::RollBidChance(BuyerItemInfo const* ahInfo, Item const* item, AuctionEntry const* auction, uint32 bidPrice)
{
    float itemBidPrice = float(bidPrice / item->GetCount());
    float itemPrice = float(item->GetTemplate()->SellPrice ? item->GetTemplate()->SellPrice : GetVendorPrice(item->GetTemplate()->Quality));
    // The AH cut needs to be added to the price, but we dont want a 100% chance to buy if the price is exactly AH default
    itemPrice *= 1.4f;

    // This value is between 0 and 100 and is used directly as the chance to buy or bid
    // Value equal or above 100 means 100% chance and value below 0 means 0% chance
    float chance = std::min(100.f, std::pow(100.f, 1.f + (1.f - itemBidPrice / itemPrice) / sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_CHANCE_FACTOR)));

    if (ahInfo)
    {
        float avgBidPrice = ahInfo->TotalBidPrice / float(ahInfo->BidItemCount);

        TC_LOG_DEBUG("ahbot", "AHBot: Bid average: {:.1f} biddable item count: {}", avgBidPrice, ahInfo->BidItemCount);

        // If there are more than 5 items on AH of this entry, try weigh in the average bid price
        if (ahInfo->BidItemCount >= 5)
            chance *= 1.f / std::sqrt(itemBidPrice / avgBidPrice);
    }

    // If a player has bidded on item, have fifth of normal chance
    if (auction->bidder && !sAuctionBotConfig->IsBotChar(auction->bidder))
        chance = chance / 5.f;

    // Add config weigh in for quality
    chance *= GetChanceMultiplier(item->GetTemplate()->Quality) / 100.0f;

    float rand = frand(0.f, 100.f);
    bool win = rand <= chance;
    TC_LOG_DEBUG("ahbot", "AHBot: {} BID! chance = {:.2f}, price = {}, bidprice = {}.", win ? "WIN" : "LOSE", chance, uint32(itemPrice), uint32(itemBidPrice));
    return win;
}

// Removes items from EligibleItems that we shouldnt buy or bid on
// The last existed time on them should be older than now
void AuctionBotBuyer::PrepareListOfEntry(BuyerConfiguration& config)
{
    // now - 5 seconds to leave out all old entries but keep the ones just updated a moment ago
    time_t now = GameTime::GetGameTime() - 5;

    for (CheckEntryMap::iterator itr = config.EligibleItems.begin(); itr != config.EligibleItems.end();)
    {
        if (itr->second.LastExist < now)
            config.EligibleItems.erase(itr++);
        else
            ++itr;
    }

    TC_LOG_DEBUG("ahbot", "AHBot: EligibleItems size = {}", (uint32)config.EligibleItems.size());
}

// Tries to bid and buy items based on their prices and chances set in configs
void AuctionBotBuyer::BuyAndBidItems(BuyerConfiguration& config)
{
    time_t now = GameTime::GetGameTime();
    AuctionHouseObject* auctionHouse = sAuctionMgr->GetAuctionsMap(config.GetHouseType());
    CheckEntryMap& items = config.EligibleItems;

    // Max amount of items to buy or bid
    uint32 cycles = sAuctionBotConfig->GetItemPerCycleNormal();
    if (items.size() > sAuctionBotConfig->GetItemPerCycleBoost())
    {
        // set more cycles if there is a huge influx of items
        cycles = sAuctionBotConfig->GetItemPerCycleBoost();
        TC_LOG_DEBUG("ahbot", "AHBot: Boost value used for Buyer! (if this happens often adjust both ItemsPerCycle in worldserver.conf)");
    }

    // Process items eligible to be bidded or bought
    CheckEntryMap::iterator itr = items.begin();
    while (cycles && itr != items.end())
    {
        AuctionEntry* auction = auctionHouse->GetAuction(itr->second.AuctionId);
        if (!auction)
        {
            TC_LOG_DEBUG("ahbot", "AHBot: Entry {} doesn't exists, perhaps bought already?", itr->second.AuctionId);
            items.erase(itr++);
            continue;
        }

        // Check if the item has been checked once before
        // If it has been checked and it was recently, skip it
        if (itr->second.LastChecked && (now - itr->second.LastChecked) <= _checkInterval)
        {
            TC_LOG_DEBUG("ahbot", "AHBot: In time interval wait for entry {}!", auction->Id);
            ++itr;
            continue;
        }

        Item* item = sAuctionMgr->GetAItem(auction->itemGUIDLow);
        if (!item)
        {
            // auction item not accessible, possible auction in payment pending mode
            items.erase(itr++);
            continue;
        }

        // price to bid if bidding
        uint32 bidPrice;
        if (auction->bid >= auction->startbid)
        {
            // get bid price to outbid previous bidder
            bidPrice = auction->bid + auction->GetAuctionOutBid();
        }
        else
        {
            // no previous bidders - use starting bid
            bidPrice = auction->startbid;
        }

        bool successBuy = false;
        bool successBid = false;

        // Use advanced pricing if enabled
        if (_useAdvancedPricing && config.UseAdvancedPricing)
        {
            BuyerItemPriceInfo priceInfo = EvaluateItemPrice(item, auction, config.GetHouseType());

            // Skip if price is too high
            if (priceInfo.IsBadDeal && priceInfo.PriceRatio > config.MaxPriceRatio)
            {
                TC_LOG_DEBUG("ahbot", "AHBot: Skipping entry {} - price ratio {:.2f} exceeds max {:.2f}",
                    auction->Id, priceInfo.PriceRatio, config.MaxPriceRatio);
                itr->second.LastChecked = now;
                ++itr;
                continue;
            }

            TC_LOG_DEBUG("ahbot", "AHBot: Rolling for AHentry {} (advanced pricing, ratio {:.2f}):", auction->Id, priceInfo.PriceRatio);

            successBuy = RollBuyChanceAdvanced(priceInfo, auction);
            successBid = RollBidChanceAdvanced(priceInfo, auction, bidPrice);
        }
        else
        {
            BuyerItemInfo const* ahInfo = nullptr;
            BuyerItemInfoMap::const_iterator sameItemItr = config.SameItemInfo.find(item->GetEntry());
            if (sameItemItr != config.SameItemInfo.end())
                ahInfo = &sameItemItr->second;

            TC_LOG_DEBUG("ahbot", "AHBot: Rolling for AHentry {}:", auction->Id);

            // Roll buy and bid chances
            successBuy = RollBuyChance(ahInfo, item, auction, bidPrice);
            successBid = RollBidChance(ahInfo, item, auction, bidPrice);
        }

        // If roll bidding succesfully and bid price is above buyout -> buyout
        // If roll for buying was successful but not for bid, buyout directly
        // If roll bidding was also successful, buy the entry with 20% chance
        // - Better bid than buy since the item is bought by bot if no player bids after
        // Otherwise bid if roll for bid was successful
        if ((auction->buyout && successBid && bidPrice >= auction->buyout) ||
            (successBuy && (!successBid || urand(1, 5) == 1)))
            BuyEntry(auction, auctionHouse); // buyout
        else if (successBid)
            PlaceBidToEntry(auction, bidPrice); // bid

        itr->second.LastChecked = now;
        --cycles;
        ++itr;
    }

    // Clear not needed entries
    config.SameItemInfo.clear();
}

uint32 AuctionBotBuyer::GetVendorPrice(uint32 quality)
{
    switch (quality)
    {
        case ITEM_QUALITY_POOR:
            return sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_BASEPRICE_GRAY);
        case ITEM_QUALITY_NORMAL:
            return sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_BASEPRICE_WHITE);
        case ITEM_QUALITY_UNCOMMON:
            return sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_BASEPRICE_GREEN);
        case ITEM_QUALITY_RARE:
            return sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_BASEPRICE_BLUE);
        case ITEM_QUALITY_EPIC:
            return sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_BASEPRICE_PURPLE);
        case ITEM_QUALITY_LEGENDARY:
            return sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_BASEPRICE_ORANGE);
        case ITEM_QUALITY_ARTIFACT:
            return sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_BASEPRICE_YELLOW);
        default:
            return 1 * SILVER;
    }
}

uint32 AuctionBotBuyer::GetChanceMultiplier(uint32 quality)
{
    switch (quality)
    {
        case ITEM_QUALITY_POOR:
            return sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_CHANCEMULTIPLIER_GRAY);
        case ITEM_QUALITY_NORMAL:
            return sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_CHANCEMULTIPLIER_WHITE);
        case ITEM_QUALITY_UNCOMMON:
            return sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_CHANCEMULTIPLIER_GREEN);
        case ITEM_QUALITY_RARE:
            return sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_CHANCEMULTIPLIER_BLUE);
        case ITEM_QUALITY_EPIC:
            return sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_CHANCEMULTIPLIER_PURPLE);
        case ITEM_QUALITY_LEGENDARY:
            return sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_CHANCEMULTIPLIER_ORANGE);
        case ITEM_QUALITY_ARTIFACT:
            return sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_CHANCEMULTIPLIER_YELLOW);
        default:
            return 100;
    }
}

// Buys the auction and does necessary actions to complete the buyout
void AuctionBotBuyer::BuyEntry(AuctionEntry* auction, AuctionHouseObject* auctionHouse)
{
    TC_LOG_DEBUG("ahbot", "AHBot: Entry {} bought at {:.2f}g", auction->Id, float(auction->buyout) / float(GOLD));

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    // Send mail to previous bidder if any
    if (auction->bidder && !sAuctionBotConfig->IsBotChar(auction->bidder))
        sAuctionMgr->SendAuctionOutbiddedMail(auction, auction->buyout, nullptr, trans);

    // Set bot as bidder and set new bid amount
    auction->bidder = sAuctionBotConfig->GetRandCharExclude(auction->owner);
    auction->bid = auction->buyout;

    // Mails must be under transaction control too to prevent data loss
    sAuctionMgr->SendAuctionSalePendingMail(auction, trans);
    sAuctionMgr->SendAuctionSuccessfulMail(auction, trans);
    sAuctionMgr->SendAuctionWonMail(auction, trans);

    // Delete auction from DB
    auction->DeleteFromDB(trans);

    // Remove auction item and auction from memory
    sAuctionMgr->RemoveAItem(auction->itemGUIDLow);
    auctionHouse->RemoveAuction(auction);

    // Run SQLs
    CharacterDatabase.CommitTransaction(trans);

    // Update statistics
    ++_totalPurchases;
    _totalGoldSpent += auction->buyout;
}

// Bids on the auction and does the necessary actions for bidding
void AuctionBotBuyer::PlaceBidToEntry(AuctionEntry* auction, uint32 bidPrice)
{
    TC_LOG_DEBUG("ahbot", "AHBot: Bid placed to entry {}, {:.2f}g", auction->Id, float(bidPrice) / float(GOLD));

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    // Send mail to previous bidder if any
    if (auction->bidder && !sAuctionBotConfig->IsBotChar(auction->bidder))
        sAuctionMgr->SendAuctionOutbiddedMail(auction, bidPrice, nullptr, trans);

    // Set bot as bidder and set new bid amount
    auction->bidder = sAuctionBotConfig->GetRandCharExclude(auction->owner);
    auction->bid = bidPrice;
    auction->Flags = AuctionEntryFlag(auction->Flags & ~AUCTION_ENTRY_FLAG_GM_LOG_BUYER);

    // Update auction to DB
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_AUCTION_BID);
    stmt->setUInt32(0, auction->bidder);
    stmt->setUInt32(1, auction->bid);
    stmt->setUInt8(2, auction->Flags);
    stmt->setUInt32(3, auction->Id);
    trans->Append(stmt);

    // Run SQLs
    CharacterDatabase.CommitTransaction(trans);

    // Update statistics
    ++_totalBids;
}

// New methods for advanced pricing

void AuctionBotBuyer::ResetStatistics()
{
    _totalPurchases = 0;
    _totalBids = 0;
    _totalGoldSpent = 0;
}

BuyerItemPriceInfo AuctionBotBuyer::EvaluateItemPrice(Item const* item, AuctionEntry const* auction, AuctionHouseType houseType) const
{
    BuyerItemPriceInfo result;
    result.ItemId = item->GetEntry();
    result.StackCount = item->GetCount();
    result.ActualBuyoutPrice = auction->buyout;

    // Get fair price from pricing engine
    AHBotPriceResult priceResult = sAuctionBotPricing->CalculatePrice(item->GetTemplate(), item->GetCount(), houseType);
    result.FairBuyoutPrice = priceResult.BuyoutPrice;

    // Get drop tier
    result.DropTier = sAuctionBotData->GetItemDropTier(item->GetEntry());

    // Calculate price ratio
    if (result.FairBuyoutPrice > 0 && auction->buyout > 0)
    {
        result.PriceRatio = float(auction->buyout) / float(result.FairBuyoutPrice);
        result.IsGoodDeal = result.PriceRatio < 0.8f;
        result.IsBadDeal = result.PriceRatio > 1.2f;
    }
    else
    {
        result.PriceRatio = 1.0f;
        result.IsGoodDeal = false;
        result.IsBadDeal = false;
    }

    return result;
}

bool AuctionBotBuyer::RollBuyChanceAdvanced(BuyerItemPriceInfo const& priceInfo, AuctionEntry const* auction)
{
    if (!auction->buyout)
        return false;

    // Base chance based on price ratio
    // Good deals have higher chance, bad deals have lower chance
    float baseChance;
    if (priceInfo.PriceRatio <= 0.5f)
        baseChance = 90.0f;  // Very good deal
    else if (priceInfo.PriceRatio <= 0.8f)
        baseChance = 70.0f;  // Good deal
    else if (priceInfo.PriceRatio <= 1.0f)
        baseChance = 50.0f;  // Fair price
    else if (priceInfo.PriceRatio <= 1.2f)
        baseChance = 25.0f;  // Slightly overpriced
    else if (priceInfo.PriceRatio <= 1.5f)
        baseChance = 10.0f;  // Overpriced
    else
        baseChance = 2.0f;   // Very overpriced

    // Apply drop tier modifier - more likely to buy rare items
    float tierModifier = GetDropTierBuyChanceModifier(priceInfo.DropTier);
    float chance = baseChance * tierModifier;

    // If a player has bidded on item, reduce chance
    if (auction->bidder)
        chance *= 0.2f;

    // Cap chance at 95%
    chance = std::min(chance, 95.0f);

    float roll = frand(0.f, 100.f);
    bool win = roll <= chance;

    TC_LOG_DEBUG("ahbot", "AHBot: {} BUY (advanced)! chance = {:.2f}, priceRatio = {:.2f}, tier = {}",
        win ? "WIN" : "LOSE", chance, priceInfo.PriceRatio, static_cast<uint8>(priceInfo.DropTier));

    return win;
}

bool AuctionBotBuyer::RollBidChanceAdvanced(BuyerItemPriceInfo const& priceInfo, AuctionEntry const* auction, uint32 bidPrice)
{
    // Calculate bid price ratio against fair price
    float bidPriceRatio = 1.0f;
    if (priceInfo.FairBuyoutPrice > 0)
        bidPriceRatio = float(bidPrice) / float(priceInfo.FairBuyoutPrice);

    // Base chance based on bid price ratio
    float baseChance;
    if (bidPriceRatio <= 0.3f)
        baseChance = 80.0f;  // Very low bid
    else if (bidPriceRatio <= 0.5f)
        baseChance = 60.0f;  // Low bid
    else if (bidPriceRatio <= 0.7f)
        baseChance = 40.0f;  // Moderate bid
    else if (bidPriceRatio <= 0.9f)
        baseChance = 25.0f;  // Fair bid
    else
        baseChance = 10.0f;  // High bid

    // Apply drop tier modifier
    float tierModifier = GetDropTierBuyChanceModifier(priceInfo.DropTier);
    float chance = baseChance * tierModifier;

    // If a player has bidded on item, reduce chance significantly
    if (auction->bidder && !sAuctionBotConfig->IsBotChar(auction->bidder))
        chance *= 0.2f;

    // Cap chance at 90%
    chance = std::min(chance, 90.0f);

    float roll = frand(0.f, 100.f);
    bool win = roll <= chance;

    TC_LOG_DEBUG("ahbot", "AHBot: {} BID (advanced)! chance = {:.2f}, bidRatio = {:.2f}, tier = {}",
        win ? "WIN" : "LOSE", chance, bidPriceRatio, static_cast<uint8>(priceInfo.DropTier));

    return win;
}

float AuctionBotBuyer::GetDropTierBuyChanceModifier(DropRateTier tier) const
{
    // Rarer items get higher buy chance modifier
    switch (tier)
    {
        case DropRateTier::TIER_50_PERCENT:
        case DropRateTier::TIER_10_PERCENT:
            return 0.8f;  // Common items - lower priority
        case DropRateTier::TIER_5_PERCENT:
        case DropRateTier::TIER_2_PERCENT:
            return 1.0f;  // Normal items
        case DropRateTier::TIER_1_PERCENT:
        case DropRateTier::TIER_0_5_PERCENT:
            return 1.2f;  // Uncommon items
        case DropRateTier::TIER_0_2_PERCENT:
        case DropRateTier::TIER_0_1_PERCENT:
            return 1.5f;  // Rare items
        case DropRateTier::TIER_0_05_PERCENT:
        case DropRateTier::TIER_0_02_PERCENT:
            return 1.8f;  // Very rare items
        case DropRateTier::TIER_0_01_PERCENT:
        case DropRateTier::TIER_0_005_PERCENT:
            return 2.0f;  // Extremely rare items
        case DropRateTier::TIER_NO_DROP:
        default:
            return 1.0f;  // Unknown rarity
    }
}
