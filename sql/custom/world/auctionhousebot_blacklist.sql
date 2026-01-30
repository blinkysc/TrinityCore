-- AuctionHouseBot blacklist table
-- This table stores items that should be excluded from the AHBot item pool

DROP TABLE IF EXISTS `auctionhousebot_blacklist`;
CREATE TABLE `auctionhousebot_blacklist` (
    `item` INT UNSIGNED NOT NULL COMMENT 'Item entry from item_template',
    `reason` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Reason code: 0=Manual, 1=Filter, 2=Exploit, 3=Balance',
    `comment` VARCHAR(255) DEFAULT NULL COMMENT 'Optional reason description',
    `added_date` TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT 'When this item was blacklisted',
    PRIMARY KEY (`item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='AHBot item blacklist';

-- Pre-populate with some commonly problematic items
-- (These are examples - adjust based on your server's needs)

-- Items that could cause economic issues
-- INSERT INTO `auctionhousebot_blacklist` (`item`, `reason`, `comment`) VALUES
-- (xxxxx, 3, 'Example: Item causes economy imbalance');

-- Items from GM or test content
-- INSERT INTO `auctionhousebot_blacklist` (`item`, `reason`, `comment`) VALUES
-- (xxxxx, 2, 'GM/Test item');
