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

/// \addtogroup world The World
/// @{
/// \file

#ifndef __WORLD_H
#define __WORLD_H

#include "Common.h"
#include "AsyncCallbackProcessor.h"
#include "DatabaseEnvFwd.h"
#include "LockedQueue.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "Timer.h"

#include <atomic>
#include <list>
#include <map>
#include <unordered_map>

class Player;
class WorldPacket;
class WorldSession;
class WorldSocket;
struct Realm;

// ServerMessages.dbc
enum ServerMessageType
{
    SERVER_MSG_SHUTDOWN_TIME          = 1,
    SERVER_MSG_RESTART_TIME           = 2,
    SERVER_MSG_STRING                 = 3,
    SERVER_MSG_SHUTDOWN_CANCELLED     = 4,
    SERVER_MSG_RESTART_CANCELLED      = 5
};

enum ShutdownMask : uint32
{
    SHUTDOWN_MASK_RESTART = 1,
    SHUTDOWN_MASK_IDLE    = 2,
    SHUTDOWN_MASK_FORCE   = 4
};

enum ShutdownExitCode : uint32
{
    SHUTDOWN_EXIT_CODE = 0,
    ERROR_EXIT_CODE    = 1,
    RESTART_EXIT_CODE  = 2
};

/// Timers for different object refresh rates
enum WorldTimers
{
    WUPDATE_AUCTIONS,
    WUPDATE_AUCTIONS_PENDING,
    WUPDATE_UPTIME,
    WUPDATE_CORPSES,
    WUPDATE_EVENTS,
    WUPDATE_CLEANDB,
    WUPDATE_AUTOBROADCAST,
    WUPDATE_MAILBOXQUEUE,
    WUPDATE_DELETECHARS,
    WUPDATE_AHBOT,
    WUPDATE_PINGDB,
    WUPDATE_CHECK_FILECHANGES,
    WUPDATE_WHO_LIST,
    WUPDATE_CHANNEL_SAVE,
    WUPDATE_COUNT
};

/// Configuration elements
enum WorldBoolConfigs : uint32
{
    CONFIG_DURABILITY_LOSS_IN_PVP = 0,
    CONFIG_ADDON_CHANNEL,
    CONFIG_CLEAN_CHARACTER_DB,
    CONFIG_GRID_UNLOAD,
    CONFIG_STATS_SAVE_ONLY_ON_LOGOUT,
    CONFIG_ALLOW_TWO_SIDE_INTERACTION_CALENDAR,
    CONFIG_ALLOW_TWO_SIDE_INTERACTION_CHANNEL,
    CONFIG_ALLOW_TWO_SIDE_INTERACTION_GROUP,
    CONFIG_ALLOW_TWO_SIDE_INTERACTION_GUILD,
    CONFIG_ALLOW_TWO_SIDE_INTERACTION_AUCTION,
    CONFIG_ALLOW_TWO_SIDE_TRADE,
    CONFIG_ALL_TAXI_PATHS,
    CONFIG_INSTANT_TAXI,
    CONFIG_INSTANCE_IGNORE_LEVEL,
    CONFIG_INSTANCE_IGNORE_RAID,
    CONFIG_CAST_UNSTUCK,
    CONFIG_ALLOW_GM_GROUP,
    CONFIG_GM_LOWER_SECURITY,
    CONFIG_SKILL_PROSPECTING,
    CONFIG_SKILL_MILLING,
    CONFIG_WEATHER,
    CONFIG_ALWAYS_MAX_SKILL_FOR_LEVEL,
    CONFIG_QUEST_IGNORE_RAID,
    CONFIG_CHAT_PARTY_RAID_WARNINGS,
    CONFIG_DETECT_POS_COLLISION,
    CONFIG_RESTRICTED_LFG_CHANNEL,
    CONFIG_CHAT_FAKE_MESSAGE_PREVENTING,
    CONFIG_DEATH_CORPSE_RECLAIM_DELAY_PVP,
    CONFIG_DEATH_CORPSE_RECLAIM_DELAY_PVE,
    CONFIG_DEATH_BONES_WORLD,
    CONFIG_DEATH_BONES_BG_OR_ARENA,
    CONFIG_DIE_COMMAND_MODE,
    CONFIG_DECLINED_NAMES_USED,
    CONFIG_BATTLEGROUND_CAST_DESERTER,
    CONFIG_BATTLEGROUND_QUEUE_ANNOUNCER_ENABLE,
    CONFIG_BATTLEGROUND_QUEUE_ANNOUNCER_PLAYERONLY,
    CONFIG_BATTLEGROUND_STORE_STATISTICS_ENABLE,
    CONFIG_BATTLEGROUND_TRACK_DESERTERS,
    CONFIG_BG_XP_FOR_KILL,
    CONFIG_ARENA_AUTO_DISTRIBUTE_POINTS,
    CONFIG_ARENA_QUEUE_ANNOUNCER_ENABLE,
    CONFIG_ARENA_SEASON_IN_PROGRESS,
    CONFIG_ARENA_LOG_EXTENDED_INFO,
    CONFIG_OFFHAND_CHECK_AT_SPELL_UNLEARN,
    CONFIG_VMAP_INDOOR_CHECK,
    CONFIG_START_ALL_SPELLS,
    CONFIG_START_ALL_EXPLORED,
    CONFIG_START_ALL_REP,
    CONFIG_ALWAYS_MAXSKILL,
    CONFIG_PVP_TOKEN_ENABLE,
    CONFIG_NO_RESET_TALENT_COST,
    CONFIG_SHOW_KICK_IN_WORLD,
    CONFIG_SHOW_MUTE_IN_WORLD,
    CONFIG_SHOW_BAN_IN_WORLD,
    CONFIG_AUTOBROADCAST,
    CONFIG_ALLOW_TICKETS,
    CONFIG_DELETE_CHARACTER_TICKET_TRACE,
    CONFIG_DBC_ENFORCE_ITEM_ATTRIBUTES,
    CONFIG_PRESERVE_CUSTOM_CHANNELS,
    CONFIG_PDUMP_NO_PATHS,
    CONFIG_PDUMP_NO_OVERWRITE,
    CONFIG_QUEST_IGNORE_AUTO_ACCEPT,
    CONFIG_QUEST_IGNORE_AUTO_COMPLETE,
    CONFIG_QUEST_ENABLE_QUEST_TRACKER,
    CONFIG_WARDEN_ENABLED,
    CONFIG_ENABLE_MMAPS,
    CONFIG_WINTERGRASP_ENABLE,
    CONFIG_EVENT_ANNOUNCE,
    CONFIG_STATS_LIMITS_ENABLE,
    CONFIG_INSTANCES_RESET_ANNOUNCE,
    CONFIG_IP_BASED_ACTION_LOGGING,
    CONFIG_ALLOW_TRACK_BOTH_RESOURCES,
    CONFIG_CALCULATE_CREATURE_ZONE_AREA_DATA,
    CONFIG_CALCULATE_GAMEOBJECT_ZONE_AREA_DATA,
    CONFIG_RESET_DUEL_COOLDOWNS,
    CONFIG_RESET_DUEL_HEALTH_MANA,
    CONFIG_BASEMAP_LOAD_GRIDS,
    CONFIG_INSTANCEMAP_LOAD_GRIDS,
    CONFIG_HOTSWAP_ENABLED,
    CONFIG_HOTSWAP_RECOMPILER_ENABLED,
    CONFIG_HOTSWAP_EARLY_TERMINATION_ENABLED,
    CONFIG_HOTSWAP_BUILD_FILE_RECREATION_ENABLED,
    CONFIG_HOTSWAP_INSTALL_ENABLED,
    CONFIG_HOTSWAP_PREFIX_CORRECTION_ENABLED,
    CONFIG_PREVENT_RENAME_CUSTOMIZATION,
    CONFIG_CACHE_DATA_QUERIES,
    CONFIG_CHECK_GOBJECT_LOS,
    CONFIG_RESPAWN_DYNAMIC_ESCORTNPC,
    CONFIG_REGEN_HP_CANNOT_REACH_TARGET_IN_RAID,
    CONFIG_ALLOW_LOGGING_IP_ADDRESSES_IN_DATABASE,
    BOOL_CONFIG_VALUE_COUNT
};

enum WorldFloatConfigs : uint32
{
    CONFIG_GROUP_XP_DISTANCE = 0,
    CONFIG_MAX_RECRUIT_A_FRIEND_DISTANCE,
    CONFIG_SIGHT_MONSTER,
    CONFIG_LISTEN_RANGE_SAY,
    CONFIG_LISTEN_RANGE_TEXTEMOTE,
    CONFIG_LISTEN_RANGE_YELL,
    CONFIG_CREATURE_FAMILY_FLEE_ASSISTANCE_RADIUS,
    CONFIG_CREATURE_FAMILY_ASSISTANCE_RADIUS,
    CONFIG_THREAT_RADIUS,
    CONFIG_CHANCE_OF_GM_SURVEY,
    CONFIG_STATS_LIMITS_DODGE,
    CONFIG_STATS_LIMITS_PARRY,
    CONFIG_STATS_LIMITS_BLOCK,
    CONFIG_STATS_LIMITS_CRIT,
    CONFIG_ARENA_WIN_RATING_MODIFIER_1,
    CONFIG_ARENA_WIN_RATING_MODIFIER_2,
    CONFIG_ARENA_LOSE_RATING_MODIFIER,
    CONFIG_ARENA_MATCHMAKER_RATING_MODIFIER,
    CONFIG_RESPAWN_DYNAMICRATE_CREATURE,
    CONFIG_RESPAWN_DYNAMICRATE_GAMEOBJECT,
    FLOAT_CONFIG_VALUE_COUNT
};

enum WorldIntConfigs : uint32
{
    CONFIG_COMPRESSION = 0,
    CONFIG_INTERVAL_SAVE,
    CONFIG_INTERVAL_GRIDCLEAN,
    CONFIG_INTERVAL_MAPUPDATE,
    CONFIG_INTERVAL_CHANGEWEATHER,
    CONFIG_INTERVAL_DISCONNECT_TOLERANCE,
    CONFIG_PORT_WORLD,
    CONFIG_SOCKET_TIMEOUTTIME,
    CONFIG_SESSION_ADD_DELAY,
    CONFIG_GAME_TYPE,
    CONFIG_REALM_ZONE,
    CONFIG_STRICT_PLAYER_NAMES,
    CONFIG_STRICT_CHARTER_NAMES,
    CONFIG_STRICT_PET_NAMES,
    CONFIG_MIN_PLAYER_NAME,
    CONFIG_MIN_CHARTER_NAME,
    CONFIG_MIN_PET_NAME,
    CONFIG_CHARACTER_CREATING_DISABLED,
    CONFIG_CHARACTER_CREATING_DISABLED_RACEMASK,
    CONFIG_CHARACTER_CREATING_DISABLED_CLASSMASK,
    CONFIG_CHARACTERS_PER_ACCOUNT,
    CONFIG_CHARACTERS_PER_REALM,
    CONFIG_DEATH_KNIGHTS_PER_REALM,
    CONFIG_CHARACTER_CREATING_MIN_LEVEL_FOR_DEATH_KNIGHT,
    CONFIG_SKIP_CINEMATICS,
    CONFIG_MAX_PLAYER_LEVEL,
    CONFIG_MIN_DUALSPEC_LEVEL,
    CONFIG_START_PLAYER_LEVEL,
    CONFIG_START_DEATH_KNIGHT_PLAYER_LEVEL,
    CONFIG_START_PLAYER_MONEY,
    CONFIG_MAX_HONOR_POINTS,
    CONFIG_START_HONOR_POINTS,
    CONFIG_MAX_ARENA_POINTS,
    CONFIG_START_ARENA_POINTS,
    CONFIG_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL,
    CONFIG_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL_DIFFERENCE,
    CONFIG_INSTANCE_RESET_TIME_HOUR,
    CONFIG_INSTANCE_UNLOAD_DELAY,
    CONFIG_DAILY_QUEST_RESET_TIME_HOUR,
    CONFIG_WEEKLY_QUEST_RESET_TIME_WDAY,
    CONFIG_MAX_PRIMARY_TRADE_SKILL,
    CONFIG_MIN_PETITION_SIGNS,
    CONFIG_MIN_QUEST_SCALED_XP_RATIO,
    CONFIG_MIN_CREATURE_SCALED_XP_RATIO,
    CONFIG_MIN_DISCOVERED_SCALED_XP_RATIO,
    CONFIG_GM_LOGIN_STATE,
    CONFIG_GM_VISIBLE_STATE,
    CONFIG_GM_ACCEPT_TICKETS,
    CONFIG_GM_CHAT,
    CONFIG_GM_WHISPERING_TO,
    CONFIG_GM_FREEZE_DURATION,
    CONFIG_GM_LEVEL_IN_GM_LIST,
    CONFIG_GM_LEVEL_IN_WHO_LIST,
    CONFIG_START_GM_LEVEL,
    CONFIG_FORCE_SHUTDOWN_THRESHOLD,
    CONFIG_GROUP_VISIBILITY,
    CONFIG_MAIL_DELIVERY_DELAY,
    CONFIG_CLEAN_OLD_MAIL_TIME,
    CONFIG_UPTIME_UPDATE,
    CONFIG_SKILL_CHANCE_ORANGE,
    CONFIG_SKILL_CHANCE_YELLOW,
    CONFIG_SKILL_CHANCE_GREEN,
    CONFIG_SKILL_CHANCE_GREY,
    CONFIG_SKILL_CHANCE_MINING_STEPS,
    CONFIG_SKILL_CHANCE_SKINNING_STEPS,
    CONFIG_SKILL_GAIN_CRAFTING,
    CONFIG_SKILL_GAIN_DEFENSE,
    CONFIG_SKILL_GAIN_GATHERING,
    CONFIG_SKILL_GAIN_WEAPON,
    CONFIG_MAX_OVERSPEED_PINGS,
    CONFIG_EXPANSION,
    CONFIG_CHATFLOOD_MESSAGE_COUNT,
    CONFIG_CHATFLOOD_MESSAGE_DELAY,
    CONFIG_CHATFLOOD_ADDON_MESSAGE_COUNT,
    CONFIG_CHATFLOOD_ADDON_MESSAGE_DELAY,
    CONFIG_CHATFLOOD_MUTE_TIME,
    CONFIG_CREATURE_FAMILY_ASSISTANCE_DELAY,
    CONFIG_CREATURE_FAMILY_FLEE_DELAY,
    CONFIG_WORLD_BOSS_LEVEL_DIFF,
    CONFIG_QUEST_LOW_LEVEL_HIDE_DIFF,
    CONFIG_QUEST_HIGH_LEVEL_HIDE_DIFF,
    CONFIG_CHAT_STRICT_LINK_CHECKING_SEVERITY,
    CONFIG_CHAT_STRICT_LINK_CHECKING_KICK,
    CONFIG_CHAT_CHANNEL_LEVEL_REQ,
    CONFIG_CHAT_WHISPER_LEVEL_REQ,
    CONFIG_CHAT_EMOTE_LEVEL_REQ,
    CONFIG_CHAT_SAY_LEVEL_REQ,
    CONFIG_CHAT_YELL_LEVEL_REQ,
    CONFIG_PARTY_LEVEL_REQ,
    CONFIG_TRADE_LEVEL_REQ,
    CONFIG_TICKET_LEVEL_REQ,
    CONFIG_AUCTION_LEVEL_REQ,
    CONFIG_MAIL_LEVEL_REQ,
    CONFIG_CORPSE_DECAY_NORMAL,
    CONFIG_CORPSE_DECAY_RARE,
    CONFIG_CORPSE_DECAY_ELITE,
    CONFIG_CORPSE_DECAY_RAREELITE,
    CONFIG_CORPSE_DECAY_WORLDBOSS,
    CONFIG_DEATH_SICKNESS_LEVEL,
    CONFIG_INSTANT_LOGOUT,
    CONFIG_DISABLE_BREATHING,
    CONFIG_BATTLEGROUND_INVITATION_TYPE,
    CONFIG_BATTLEGROUND_PREMATURE_FINISH_TIMER,
    CONFIG_BATTLEGROUND_PREMADE_GROUP_WAIT_FOR_MATCH,
    CONFIG_BATTLEGROUND_REPORT_AFK,
    CONFIG_ARENA_MAX_RATING_DIFFERENCE,
    CONFIG_ARENA_RATING_DISCARD_TIMER,
    CONFIG_ARENA_PREV_OPPONENTS_DISCARD_TIMER,
    CONFIG_ARENA_RATED_UPDATE_TIMER,
    CONFIG_ARENA_AUTO_DISTRIBUTE_INTERVAL_DAYS,
    CONFIG_ARENA_SEASON_ID,
    CONFIG_ARENA_START_RATING,
    CONFIG_ARENA_START_PERSONAL_RATING,
    CONFIG_ARENA_START_MATCHMAKER_RATING,
    CONFIG_MAX_WHO,
    CONFIG_HONOR_AFTER_DUEL,
    CONFIG_PVP_TOKEN_MAP_TYPE,
    CONFIG_PVP_TOKEN_ID,
    CONFIG_PVP_TOKEN_COUNT,
    CONFIG_ENABLE_SINFO_LOGIN,
    CONFIG_PLAYER_ALLOW_COMMANDS,
    CONFIG_NUMTHREADS,
    CONFIG_LOGDB_CLEARINTERVAL,
    CONFIG_LOGDB_CLEARTIME,
    CONFIG_CLIENTCACHE_VERSION,
    CONFIG_GUILD_EVENT_LOG_COUNT,
    CONFIG_GUILD_BANK_EVENT_LOG_COUNT,
    CONFIG_MIN_LEVEL_STAT_SAVE,
    CONFIG_RANDOM_BG_RESET_HOUR,
    CONFIG_CALENDAR_DELETE_OLD_EVENTS_HOUR,
    CONFIG_GUILD_RESET_HOUR,
    CONFIG_CHARDELETE_KEEP_DAYS,
    CONFIG_CHARDELETE_METHOD,
    CONFIG_CHARDELETE_MIN_LEVEL,
    CONFIG_CHARDELETE_DEATH_KNIGHT_MIN_LEVEL,
    CONFIG_AUTOBROADCAST_CENTER,
    CONFIG_AUTOBROADCAST_INTERVAL,
    CONFIG_MAX_RESULTS_LOOKUP_COMMANDS,
    CONFIG_DB_PING_INTERVAL,
    CONFIG_PRESERVE_CUSTOM_CHANNEL_DURATION,
    CONFIG_PRESERVE_CUSTOM_CHANNEL_INTERVAL,
    CONFIG_PERSISTENT_CHARACTER_CLEAN_FLAGS,
    CONFIG_LFG_OPTIONSMASK,
    CONFIG_MAX_INSTANCES_PER_HOUR,
    CONFIG_XP_BOOST_DAYMASK,
    CONFIG_WARDEN_CLIENT_RESPONSE_DELAY,
    CONFIG_WARDEN_CLIENT_CHECK_HOLDOFF,
    CONFIG_WARDEN_CLIENT_FAIL_ACTION,
    CONFIG_WARDEN_CLIENT_BAN_DURATION,
    CONFIG_WARDEN_NUM_INJECT_CHECKS,
    CONFIG_WARDEN_NUM_LUA_CHECKS,
    CONFIG_WARDEN_NUM_CLIENT_MOD_CHECKS,
    CONFIG_WINTERGRASP_PLR_MAX,
    CONFIG_WINTERGRASP_PLR_MIN,
    CONFIG_WINTERGRASP_PLR_MIN_LVL,
    CONFIG_WINTERGRASP_BATTLETIME,
    CONFIG_WINTERGRASP_NOBATTLETIME,
    CONFIG_WINTERGRASP_RESTART_AFTER_CRASH,
    CONFIG_PACKET_SPOOF_POLICY,
    CONFIG_PACKET_SPOOF_BANMODE,
    CONFIG_PACKET_SPOOF_BANDURATION,
    CONFIG_ACC_PASSCHANGESEC,
    CONFIG_BG_REWARD_WINNER_HONOR_FIRST,
    CONFIG_BG_REWARD_WINNER_ARENA_FIRST,
    CONFIG_BG_REWARD_WINNER_HONOR_LAST,
    CONFIG_BG_REWARD_WINNER_ARENA_LAST,
    CONFIG_BG_REWARD_LOSER_HONOR_FIRST,
    CONFIG_BG_REWARD_LOSER_HONOR_LAST,
    CONFIG_BIRTHDAY_TIME,
    CONFIG_CREATURE_PICKPOCKET_REFILL,
    CONFIG_CREATURE_STOP_FOR_PLAYER,
    CONFIG_AHBOT_UPDATE_INTERVAL,
    CONFIG_CHARTER_COST_GUILD,
    CONFIG_CHARTER_COST_ARENA_2v2,
    CONFIG_CHARTER_COST_ARENA_3v3,
    CONFIG_CHARTER_COST_ARENA_5v5,
    CONFIG_NO_GRAY_AGGRO_ABOVE,
    CONFIG_NO_GRAY_AGGRO_BELOW,
    CONFIG_AUCTION_GETALL_DELAY,
    CONFIG_AUCTION_SEARCH_DELAY,
    CONFIG_TALENTS_INSPECTING,
    CONFIG_RESPAWN_MINCHECKINTERVALMS,
    CONFIG_RESPAWN_DYNAMICMODE,
    CONFIG_RESPAWN_GUIDWARNLEVEL,
    CONFIG_RESPAWN_GUIDALERTLEVEL,
    CONFIG_RESPAWN_RESTARTQUIETTIME,
    CONFIG_RESPAWN_DYNAMICMINIMUM_CREATURE,
    CONFIG_RESPAWN_DYNAMICMINIMUM_GAMEOBJECT,
    CONFIG_RESPAWN_GUIDWARNING_FREQUENCY,
    CONFIG_SOCKET_TIMEOUTTIME_ACTIVE,
    CONFIG_PENDING_MOVE_CHANGES_TIMEOUT,
    INT_CONFIG_VALUE_COUNT
};

/// Server rates
enum Rates
{
    RATE_HEALTH = 0,
    RATE_POWER_MANA,
    RATE_POWER_RAGE_INCOME,
    RATE_POWER_RAGE_LOSS,
    RATE_POWER_RUNICPOWER_INCOME,
    RATE_POWER_RUNICPOWER_LOSS,
    RATE_POWER_FOCUS,
    RATE_POWER_ENERGY,
    RATE_SKILL_DISCOVERY,
    RATE_DROP_ITEM_POOR,
    RATE_DROP_ITEM_NORMAL,
    RATE_DROP_ITEM_UNCOMMON,
    RATE_DROP_ITEM_RARE,
    RATE_DROP_ITEM_EPIC,
    RATE_DROP_ITEM_LEGENDARY,
    RATE_DROP_ITEM_ARTIFACT,
    RATE_DROP_ITEM_REFERENCED,
    RATE_DROP_ITEM_REFERENCED_AMOUNT,
    RATE_DROP_MONEY,
    RATE_XP_KILL,
    RATE_XP_BG_KILL,
    RATE_XP_QUEST,
    RATE_XP_EXPLORE,
    RATE_REPAIRCOST,
    RATE_REPUTATION_GAIN,
    RATE_REPUTATION_LOWLEVEL_KILL,
    RATE_REPUTATION_LOWLEVEL_QUEST,
    RATE_REPUTATION_RECRUIT_A_FRIEND_BONUS,
    RATE_CREATURE_NORMAL_HP,
    RATE_CREATURE_ELITE_ELITE_HP,
    RATE_CREATURE_ELITE_RAREELITE_HP,
    RATE_CREATURE_ELITE_WORLDBOSS_HP,
    RATE_CREATURE_ELITE_RARE_HP,
    RATE_CREATURE_NORMAL_DAMAGE,
    RATE_CREATURE_ELITE_ELITE_DAMAGE,
    RATE_CREATURE_ELITE_RAREELITE_DAMAGE,
    RATE_CREATURE_ELITE_WORLDBOSS_DAMAGE,
    RATE_CREATURE_ELITE_RARE_DAMAGE,
    RATE_CREATURE_NORMAL_SPELLDAMAGE,
    RATE_CREATURE_ELITE_ELITE_SPELLDAMAGE,
    RATE_CREATURE_ELITE_RAREELITE_SPELLDAMAGE,
    RATE_CREATURE_ELITE_WORLDBOSS_SPELLDAMAGE,
    RATE_CREATURE_ELITE_RARE_SPELLDAMAGE,
    RATE_CREATURE_AGGRO,
    RATE_REST_INGAME,
    RATE_REST_OFFLINE_IN_TAVERN_OR_CITY,
    RATE_REST_OFFLINE_IN_WILDERNESS,
    RATE_DAMAGE_FALL,
    RATE_AUCTION_TIME,
    RATE_AUCTION_DEPOSIT,
    RATE_AUCTION_CUT,
    RATE_HONOR,
    RATE_ARENA_POINTS,
    RATE_TALENT,
    RATE_CORPSE_DECAY_LOOTED,
    RATE_INSTANCE_RESET_TIME,
    RATE_DURABILITY_LOSS_ON_DEATH,
    RATE_DURABILITY_LOSS_DAMAGE,
    RATE_DURABILITY_LOSS_PARRY,
    RATE_DURABILITY_LOSS_ABSORB,
    RATE_DURABILITY_LOSS_BLOCK,
    RATE_MOVESPEED,
    RATE_XP_BOOST,
    RATE_MONEY_QUEST,
    RATE_MONEY_MAX_LEVEL_QUEST,
    MAX_RATES
};

/// Can be used in SMSG_AUTH_RESPONSE packet
enum BillingPlanFlags
{
    SESSION_NONE            = 0x00,
    SESSION_UNUSED          = 0x01,
    SESSION_RECURRING_BILL  = 0x02,
    SESSION_FREE_TRIAL      = 0x04,
    SESSION_IGR             = 0x08,
    SESSION_USAGE           = 0x10,
    SESSION_TIME_MIXTURE    = 0x20,
    SESSION_RESTRICTED      = 0x40,
    SESSION_ENABLE_CAIS     = 0x80
};

enum RealmZone
{
    REALM_ZONE_UNKNOWN       = 0,                           // any language
    REALM_ZONE_DEVELOPMENT   = 1,                           // any language
    REALM_ZONE_UNITED_STATES = 2,                           // extended-Latin
    REALM_ZONE_OCEANIC       = 3,                           // extended-Latin
    REALM_ZONE_LATIN_AMERICA = 4,                           // extended-Latin
    REALM_ZONE_TOURNAMENT_5  = 5,                           // basic-Latin at create, any at login
    REALM_ZONE_KOREA         = 6,                           // East-Asian
    REALM_ZONE_TOURNAMENT_7  = 7,                           // basic-Latin at create, any at login
    REALM_ZONE_ENGLISH       = 8,                           // extended-Latin
    REALM_ZONE_GERMAN        = 9,                           // extended-Latin
    REALM_ZONE_FRENCH        = 10,                          // extended-Latin
    REALM_ZONE_SPANISH       = 11,                          // extended-Latin
    REALM_ZONE_RUSSIAN       = 12,                          // Cyrillic
    REALM_ZONE_TOURNAMENT_13 = 13,                          // basic-Latin at create, any at login
    REALM_ZONE_TAIWAN        = 14,                          // East-Asian
    REALM_ZONE_TOURNAMENT_15 = 15,                          // basic-Latin at create, any at login
    REALM_ZONE_CHINA         = 16,                          // East-Asian
    REALM_ZONE_CN1           = 17,                          // basic-Latin at create, any at login
    REALM_ZONE_CN2           = 18,                          // basic-Latin at create, any at login
    REALM_ZONE_CN3           = 19,                          // basic-Latin at create, any at login
    REALM_ZONE_CN4           = 20,                          // basic-Latin at create, any at login
    REALM_ZONE_CN5           = 21,                          // basic-Latin at create, any at login
    REALM_ZONE_CN6           = 22,                          // basic-Latin at create, any at login
    REALM_ZONE_CN7           = 23,                          // basic-Latin at create, any at login
    REALM_ZONE_CN8           = 24,                          // basic-Latin at create, any at login
    REALM_ZONE_TOURNAMENT_25 = 25,                          // basic-Latin at create, any at login
    REALM_ZONE_TEST_SERVER   = 26,                          // any language
    REALM_ZONE_TOURNAMENT_27 = 27,                          // basic-Latin at create, any at login
    REALM_ZONE_QA_SERVER     = 28,                          // any language
    REALM_ZONE_CN9           = 29,                          // basic-Latin at create, any at login
    REALM_ZONE_TEST_SERVER_2 = 30,                          // any language
    REALM_ZONE_CN10          = 31,                          // basic-Latin at create, any at login
    REALM_ZONE_CTC           = 32,
    REALM_ZONE_CNC           = 33,
    REALM_ZONE_CN1_4         = 34,                          // basic-Latin at create, any at login
    REALM_ZONE_CN2_6_9       = 35,                          // basic-Latin at create, any at login
    REALM_ZONE_CN3_7         = 36,                          // basic-Latin at create, any at login
    REALM_ZONE_CN5_8         = 37                           // basic-Latin at create, any at login
};

/// Storage class for commands issued for delayed execution
struct TC_GAME_API CliCommandHolder
{
    using Print = void(*)(void*, std::string_view);
    using CommandFinished = void(*)(void*, bool success);

    void* m_callbackArg;
    char* m_command;
    Print m_print;
    CommandFinished m_commandFinished;

    CliCommandHolder(void* callbackArg, char const* command, Print zprint, CommandFinished commandFinished);
    ~CliCommandHolder();

private:
    CliCommandHolder(CliCommandHolder const& right) = delete;
    CliCommandHolder& operator=(CliCommandHolder const& right) = delete;
};

typedef std::unordered_map<uint32, WorldSession*> SessionMap;

struct CharacterInfo
{
    std::string Name;
    uint32 AccountId;
    uint8 Class;
    uint8 Race;
    uint8 Sex;
    uint8 Level;
    ObjectGuid::LowType GuildId;
    uint32 ArenaTeamId[3];
};

/// The World
class TC_GAME_API World
{
    public:
        static World* instance();

        static std::atomic<uint32> m_worldLoopCounter;

        WorldSession* FindSession(uint32 id) const;
        void AddSession(WorldSession* s);
        void SendAutoBroadcast();
        bool RemoveSession(uint32 id);
        /// Get the number of current active sessions
        void UpdateMaxSessionCounters();
        SessionMap const& GetAllSessions() const { return _sessions; }
        uint32 GetActiveAndQueuedSessionCount() const { return _sessions.size(); }
        uint32 GetActiveSessionCount() const { return _sessions.size() - _queuedPlayer.size(); }
        uint32 GetQueuedSessionCount() const { return _queuedPlayer.size(); }
        /// Get the maximum number of parallel sessions on the server since last reboot
        uint32 GetMaxQueuedSessionCount() const { return _maxQueuedSessionCount; }
        uint32 GetMaxActiveSessionCount() const { return _maxActiveSessionCount; }
        /// Get number of players
        inline uint32 GetPlayerCount() const { return _playerCount; }
        inline uint32 GetMaxPlayerCount() const { return _maxPlayerCount; }
        /// Increase/Decrease number of players
        inline void IncreasePlayerCount()
        {
            _playerCount++;
            _maxPlayerCount = std::max(_maxPlayerCount, _playerCount);
        }
        inline void DecreasePlayerCount() { _playerCount--; }

        Player* FindPlayerInZone(uint32 zone);

        /// Deny clients?
        bool IsClosed() const;

        /// Close world
        void SetClosed(bool val);

        /// Security level limitations
        AccountTypes GetPlayerSecurityLimit() const { return _allowedSecurityLevel; }
        void SetPlayerSecurityLimit(AccountTypes sec);
        void LoadDBAllowedSecurityLevel();

        /// Active session server limit
        void SetPlayerAmountLimit(uint32 limit) { _playerLimit = limit; }
        uint32 GetPlayerAmountLimit() const { return _playerLimit; }

        //player Queue
        typedef std::list<WorldSession*> Queue;
        void AddQueuedPlayer(WorldSession*);
        bool RemoveQueuedPlayer(WorldSession* session);
        int32 GetQueuePos(WorldSession*);
        bool HasRecentlyDisconnected(WorldSession*);

        /// @todo Actions on m_allowMovement still to be implemented
        /// Is movement allowed?
        bool GetAllowMovement() const { return _allowMovement; }
        /// Allow/Disallow object movements
        void SetAllowMovement(bool allow) { _allowMovement = allow; }

        /// Set the string for new characters (first login)
        void SetNewCharString(std::string const& str) { _newCharString = str; }
        /// Get the string for new characters (first login)
        std::string const& GetNewCharString() const { return _newCharString; }

        LocaleConstant GetDefaultDbcLocale() const { return _defaultDBCLocale; }

        /// Get the path where data (dbc, maps) are stored on disk
        std::string const& GetDataPath() const { return _dataPath; }

        /// Next daily quests and random bg reset time
        time_t GetNextDailyQuestsResetTime() const { return _nextDailyQuestReset; }
        time_t GetNextWeeklyQuestsResetTime() const { return _nextWeeklyQuestReset; }
        time_t GetNextRandomBGResetTime() const { return _nextRandomBGReset; }

        /// Get the maximum skill level a player can reach
        uint16 GetConfigMaxSkillValue() const
        {
            uint16 lvl = uint16(getIntConfig(CONFIG_MAX_PLAYER_LEVEL));
            return lvl > 60 ? 300 + ((lvl - 60) * 75) / 10 : lvl * 5;
        }

        bool SetInitialWorldSettings();
        void LoadConfigSettings(bool reload = false);

        void SendWorldText(uint32 string_id, ...);
        void SendGlobalText(char const* text, WorldSession* self);
        void SendGMText(uint32 string_id, ...);
        void SendServerMessage(ServerMessageType messageID, std::string stringParam = "", Player* player = nullptr);
        void SendGlobalMessage(WorldPacket const* packet, WorldSession* self = nullptr, uint32 team = 0);
        void SendGlobalGMMessage(WorldPacket const* packet, WorldSession* self = nullptr, uint32 team = 0);

        /// Are we in the middle of a shutdown?
        bool IsShuttingDown() const { return _shutdownTimer > 0; }
        uint32 GetShutDownTimeLeft() const { return _shutdownTimer; }
        void ShutdownServ(uint32 time, uint32 options, uint8 exitcode, const std::string& reason = std::string());
        uint32 ShutdownCancel();
        void ShutdownMsg(bool show = false, Player* player = nullptr, const std::string& reason = std::string());
        static uint8 GetExitCode() { return _exitCode; }
        static void StopNow(uint8 exitcode) { _stopEvent = true; _exitCode = exitcode; }
        static bool IsStopped() { return _stopEvent; }

        void Update(uint32 diff);

        void UpdateSessions(uint32 diff);
        /// Set a server rate (see #Rates)
        void SetRate(Rates rate, float value) { _rateValues[rate]=value; }
        /// Get a server rate (see #Rates)
        float GetRate(Rates rate) const { return _rateValues[rate]; }

        /// Set a server configuration element (see #WorldConfigs)
        void setBoolConfig(WorldBoolConfigs index, bool value)
        {
            if (index < BOOL_CONFIG_VALUE_COUNT)
                _boolConfigs[index] = value;
        }

        /// Get a server configuration element (see #WorldConfigs)
        bool getBoolConfig(WorldBoolConfigs index) const
        {
            return index < BOOL_CONFIG_VALUE_COUNT ? _boolConfigs[index] : 0;
        }

        /// Set a server configuration element (see #WorldConfigs)
        void setFloatConfig(WorldFloatConfigs index, float value)
        {
            if (index < FLOAT_CONFIG_VALUE_COUNT)
                _floatConfigs[index] = value;
        }

        /// Get a server configuration element (see #WorldConfigs)
        float getFloatConfig(WorldFloatConfigs index) const
        {
            return index < FLOAT_CONFIG_VALUE_COUNT ? _floatConfigs[index] : 0;
        }

        /// Set a server configuration element (see #WorldConfigs)
        void setIntConfig(WorldIntConfigs index, uint32 value)
        {
            if (index < INT_CONFIG_VALUE_COUNT)
                _intConfigs[index] = value;
        }

        /// Get a server configuration element (see #WorldConfigs)
        uint32 getIntConfig(WorldIntConfigs index) const
        {
            return index < INT_CONFIG_VALUE_COUNT ? _intConfigs[index] : 0;
        }

        void SetWorldState(uint32 index, uint64 value);
        uint64 GetWorldState(uint32 index) const;
        void LoadWorldStates();

        /// Are we on a "Player versus Player" server?
        bool IsPvPRealm() const;
        bool IsFFAPvPRealm() const;

        void KickAll();
        void KickAllLess(AccountTypes sec);
        BanReturn BanAccount(BanMode mode, std::string const& nameOrIP, std::string const& duration, std::string const& reason, std::string const& author);
        BanReturn BanAccount(BanMode mode, std::string const& nameOrIP, uint32 duration_secs, std::string const& reason, std::string const& author);
        bool RemoveBanAccount(BanMode mode, std::string const& nameOrIP);
        BanReturn BanCharacter(std::string const& name, std::string const& duration, std::string const& reason, std::string const& author);
        bool RemoveBanCharacter(std::string const& name);

        // for max speed access
        static float GetMaxVisibleDistanceOnContinents()    { return _maxVisibleDistanceOnContinents; }
        static float GetMaxVisibleDistanceInInstances()     { return _maxVisibleDistanceInInstances;  }
        static float GetMaxVisibleDistanceInBG()            { return _maxVisibleDistanceInBG;         }
        static float GetMaxVisibleDistanceInArenas()        { return _maxVisibleDistanceInArenas;     }

        static int32 GetVisibilityNotifyPeriodOnContinents(){ return _visibility_notify_periodOnContinents; }
        static int32 GetVisibilityNotifyPeriodInInstances() { return _visibility_notify_periodInInstances;  }
        static int32 GetVisibilityNotifyPeriodInBG()        { return _visibility_notify_periodInBG;         }
        static int32 GetVisibilityNotifyPeriodInArenas()    { return _visibility_notify_periodInArenas;     }

        void ProcessCliCommands();
        void QueueCliCommand(CliCommandHolder* commandHolder) { _cliCmdQueue.add(commandHolder); }

        void ForceGameEventUpdate();

        void UpdateRealmCharCount(uint32 accid);

        LocaleConstant GetAvailableDbcLocale(LocaleConstant locale) const { if (_availableDBCLocaleMask & (1 << locale)) return locale; else return _defaultDBCLocale; }

        // used World DB version
        void LoadDBVersion();
        char const* GetDBVersion() const { return _DBVersion.c_str(); }

        void LoadAutobroadcasts();

        void UpdateAreaDependentAuras();

        uint32 GetCleaningFlags() const { return _cleaningFlags; }
        void SetCleaningFlags(uint32 flags) { _cleaningFlags = flags; }
        void ResetEventSeasonalQuests(uint16 event_id);

        void ReloadRBAC();

        void RemoveOldCorpses();
        void TriggerGuidWarning();
        void TriggerGuidAlert();
        bool IsGuidWarning() { return _guidWarn; }
        bool IsGuidAlert() { return _guidAlert; }

    protected:
        void _UpdateGameTime();

        // callback for UpdateRealmCharacters
        void _UpdateRealmCharCount(PreparedQueryResult resultCharCount);

        void InitQuestResetTimes();
        void CheckQuestResetTimes();
        void ResetDailyQuests();
        void ResetWeeklyQuests();
        void ResetMonthlyQuests();

        void InitRandomBGResetTime();
        void InitCalendarOldEventsDeletionTime();
        void InitGuildResetTime();
        void ResetRandomBG();
        void CalendarDeleteOldEvents();
        void ResetGuildCap();
    private:
        World();
        ~World();

        static std::atomic<bool> _stopEvent;
        static uint8 _exitCode;
        uint32 _shutdownTimer;
        uint32 _shutdownMask;

        uint32 _cleaningFlags;

        bool _isClosed;

        IntervalTimer _timers[WUPDATE_COUNT];
        time_t _mailTimer;
        time_t _mailTimerExpires;

        SessionMap _sessions;
        typedef std::unordered_map<uint32, time_t> DisconnectMap;
        DisconnectMap _disconnects;
        uint32 _maxActiveSessionCount;
        uint32 _maxQueuedSessionCount;
        uint32 _playerCount;
        uint32 _maxPlayerCount;

        std::string _newCharString;

        float _rateValues[MAX_RATES];
        uint32 _intConfigs[INT_CONFIG_VALUE_COUNT];
        bool _boolConfigs[BOOL_CONFIG_VALUE_COUNT];
        float _floatConfigs[FLOAT_CONFIG_VALUE_COUNT];
        typedef std::map<uint32, uint64> WorldStatesMap;
        WorldStatesMap _worldstates;
        uint32 _playerLimit;
        AccountTypes _allowedSecurityLevel;
        LocaleConstant _defaultDBCLocale;                     // from config for one from loaded DBC locales
        uint32 _availableDBCLocaleMask;                       // by loaded DBC
        void DetectDBCLang();
        bool _allowMovement;
        std::string _dataPath;

        // for max speed access
        static float _maxVisibleDistanceOnContinents;
        static float _maxVisibleDistanceInInstances;
        static float _maxVisibleDistanceInBG;
        static float _maxVisibleDistanceInArenas;

        static int32 _visibility_notify_periodOnContinents;
        static int32 _visibility_notify_periodInInstances;
        static int32 _visibility_notify_periodInBG;
        static int32 _visibility_notify_periodInArenas;

        // CLI command holder to be thread safe
        LockedQueue<CliCommandHolder*> _cliCmdQueue;

        // next daily quests and random bg reset time
        time_t _nextDailyQuestReset;
        time_t _nextWeeklyQuestReset;
        time_t _nextMonthlyQuestReset;
        time_t _nextRandomBGReset;
        time_t _nextCalendarOldEventsDeletionTime;
        time_t _nextGuildReset;

        //Player Queue
        Queue _queuedPlayer;

        // sessions that are added async
        void AddSession_(WorldSession* s);
        LockedQueue<WorldSession*> addSessQueue;

        // used versions
        std::string _DBVersion;

        typedef std::map<uint8, std::string> AutobroadcastsMap;
        AutobroadcastsMap _autobroadcasts;

        typedef std::map<uint8, uint8> AutobroadcastsWeightMap;
        AutobroadcastsWeightMap _autobroadcastsWeights;

        void ProcessQueryCallbacks();

        void SendGuidWarning();
        void DoGuidWarningRestart();
        void DoGuidAlertRestart();
        QueryCallbackProcessor _queryProcessor;

        std::string _guidWarningMsg;
        std::string _alertRestartReason;

        std::mutex _guidAlertLock;

        bool _guidWarn;
        bool _guidAlert;
        uint32 _warnDiff;
        time_t _warnShutdownTime;

    friend class debug_commandscript;
};

TC_GAME_API extern Realm realm;

#define sWorld World::instance()

#endif
/// @}
