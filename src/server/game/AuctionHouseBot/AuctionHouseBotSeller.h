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

#ifndef AUCTION_HOUSE_BOT_SELLER_H
#define AUCTION_HOUSE_BOT_SELLER_H

#include "Define.h"
#include "ItemTemplate.h"
#include "AuctionHouseBot.h"
#include "AuctionHouseBotData.h"
#include <array>
#include <unordered_map>
#include <unordered_set>

struct ItemToSell
{
    uint32 Color;
    uint32 Itemclass;
};

typedef std::vector<ItemToSell> ItemsToSellArray;
typedef std::vector<std::vector<uint32>> AllItemsArray;

struct SellerItemInfo
{
    uint32 AmountOfItems = 0;
    uint32 MissItems = 0;
};

struct SellerItemClassSharedInfo
{
    uint32 PriceRatio = 0;
    uint32 RandomStackRatio = 100;
};

struct SellerItemQualitySharedInfo
{
    uint32 AmountOfItems = 0;
    uint32 PriceRatio = 0;
};

// Extended seller item pool entry with weight information
struct SellerPoolItem
{
    uint32 ItemId = 0;
    uint32 ListWeight = 100;        // Weight for weighted random selection
    DropRateTier DropTier = DropRateTier::TIER_NO_DROP;
};

class SellerConfiguration
{
public:
    SellerConfiguration() : LastMissedItem(0), _houseType(AUCTION_HOUSE_NEUTRAL), _minTime(1), _maxTime(72), _itemInfo(), _itemSharedQualityInfo(), _itemSharedClassInfo() { }

    void Initialize(AuctionHouseType houseType)
    {
        _houseType = houseType;
    }

    AuctionHouseType GetHouseType() const { return _houseType; }

    uint32 LastMissedItem;

    void SetMinTime(uint32 value)
    {
        _minTime = value;
    }
    uint32 GetMinTime() const
    {
        return std::min(1u, std::min(_minTime, _maxTime));
    }

    void SetMaxTime(uint32 value) { _maxTime = value; }
    uint32 GetMaxTime() const { return _maxTime; }

    // Data access classified by item class and item quality
    void SetItemsAmountPerClass(AuctionQuality quality, ItemClass itemClass, uint32 amount) { _itemInfo[quality][itemClass].AmountOfItems = amount; }
    uint32 GetItemsAmountPerClass(AuctionQuality quality, ItemClass itemClass) const { return _itemInfo[quality][itemClass].AmountOfItems; }

    void SetMissedItemsPerClass(AuctionQuality quality, ItemClass itemClass, uint32 found)
    {
        if (_itemInfo[quality][itemClass].AmountOfItems > found)
            _itemInfo[quality][itemClass].MissItems = _itemInfo[quality][itemClass].AmountOfItems - found;
        else
            _itemInfo[quality][itemClass].MissItems = 0;
    }
    uint32 GetMissedItemsPerClass(AuctionQuality quality, ItemClass itemClass) const { return _itemInfo[quality][itemClass].MissItems; }

    // Data for every quality of item
    void SetItemsAmountPerQuality(AuctionQuality quality, uint32 cnt) { _itemSharedQualityInfo[quality].AmountOfItems = cnt; }
    uint32 GetItemsAmountPerQuality(AuctionQuality quality) const { return _itemSharedQualityInfo[quality].AmountOfItems; }

    void SetPriceRatioPerQuality(AuctionQuality quality, uint32 value) { _itemSharedQualityInfo[quality].PriceRatio = value; }
    uint32 GetPriceRatioPerQuality(AuctionQuality quality) const { return _itemSharedQualityInfo[quality].PriceRatio; }

    // data for every class of item
    void SetPriceRatioPerClass(ItemClass itemClass, uint32 value) { _itemSharedClassInfo[itemClass].PriceRatio = value; }
    uint32 GetPriceRatioPerClass(ItemClass itemClass) const { return _itemSharedClassInfo[itemClass].PriceRatio; }

    void SetRandomStackRatioPerClass(ItemClass itemClass, uint32 value) { _itemSharedClassInfo[itemClass].RandomStackRatio = value; }
    uint32 GetRandomStackRatioPerClass(ItemClass itemClass) const { return _itemSharedClassInfo[itemClass].RandomStackRatio; }

private:
    AuctionHouseType _houseType;
    uint32 _minTime;
    uint32 _maxTime;

    SellerItemInfo _itemInfo[MAX_AUCTION_QUALITY][MAX_ITEM_CLASS];

    SellerItemQualitySharedInfo _itemSharedQualityInfo[MAX_ITEM_QUALITY];
    SellerItemClassSharedInfo _itemSharedClassInfo[MAX_ITEM_CLASS];
};

// This class handle all Selling method
// (holder of AHB_Seller_Config data for each auction house type)
class TC_GAME_API AuctionBotSeller : public AuctionBotAgent
{
public:
    typedef std::vector<uint32> ItemPool;
    typedef std::vector<SellerPoolItem> WeightedItemPool;

    AuctionBotSeller();
    ~AuctionBotSeller();

    bool Initialize() override;
    bool Update(AuctionHouseType houseType) override;

    void AddNewAuctions(SellerConfiguration& config);
    void SetItemsRatio(uint32 al, uint32 ho, uint32 ne);
    void SetItemsRatioForHouse(AuctionHouseType house, uint32 val);
    void SetItemsAmount(std::array<uint32, MAX_AUCTION_QUALITY> const& amounts);
    void SetItemsAmountForQuality(AuctionQuality quality, uint32 val);
    void LoadConfig();

    // New: Get pool statistics
    uint32 GetItemPoolSize(uint8 quality) const;
    uint32 GetTotalPoolSize() const;

private:
    SellerConfiguration _houseConfig[MAX_AUCTION_HOUSE_TYPE];

    // Original item pools (for backward compatibility)
    ItemPool _itemPool[MAX_AUCTION_QUALITY][MAX_ITEM_CLASS];

    // New: Weighted item pools for improved selection
    WeightedItemPool _weightedPool[MAX_AUCTION_QUALITY];
    std::unordered_map<uint8, uint32> _totalWeightPerQuality;

    // Force include/exclude lists
    std::unordered_set<uint32> _forceIncludeItems;
    std::unordered_set<uint32> _forceExcludeItems;

    void LoadSellerValues(SellerConfiguration& config);
    uint32 SetStat(SellerConfiguration& config);
    bool GetItemsToSell(SellerConfiguration& config, ItemsToSellArray& itemsToSellArray, AllItemsArray const& addedItem);
    void SetPricesOfItem(ItemTemplate const* itemProto, SellerConfiguration& config, uint32& buyp, uint32& bidp, uint32 stackcnt);
    uint32 GetStackSizeForItem(ItemTemplate const* itemProto, SellerConfiguration& config) const;
    void LoadItemsQuantity(SellerConfiguration& config);
    static uint32 GetBuyModifier(ItemTemplate const* prototype);
    static uint32 GetSellModifier(ItemTemplate const* itemProto);

    // New: Enhanced item selection
    void LoadForceIncludeExclude();
    void BuildWeightedPools();
    uint32 SelectWeightedItem(uint8 quality);
    uint32 GetItemListWeight(uint32 itemId, ItemTemplate const* proto) const;
    bool IsItemAllowedForSale(ItemTemplate const* proto) const;

    // New: Use advanced pricing
    void SetPricesOfItemAdvanced(ItemTemplate const* itemProto, AuctionHouseType houseType, uint32 stackCount, uint32& buyoutPrice, uint32& bidPrice);

    bool _useWeightedSelection = false;
    bool _useAdvancedPricing = false;
};

#endif
