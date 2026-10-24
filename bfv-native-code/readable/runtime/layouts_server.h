/* Class layouts of server methods for `this->field` (tools/fields_l2.py):
 * the fields L2 methods access through `this`, at their offset and width. */
#ifndef BFV_LAYOUTS_SERVER_H
#define BFV_LAYOUTS_SERVER_H

/* dice::bf::ai::AISettings */
struct AISettings {
    uint8_t _0[0x2c];
    uint32_t nSides;  /* 0x2c */
} __attribute__((packed));

/* dice::ref2::world::Armor */
struct Armor {
    uint8_t _0[0x3c];
    uint32_t maxHitPoints;  /* 0x3c */
    uint8_t _40[0x94];
    uint32_t recentHead;  /* 0xd4 */
    uint32_t recentTail;  /* 0xd8 */
    uint8_t _dc[0x34];
    uint8_t destroyed;  /* 0x110 */
} __attribute__((packed));

/* BitStream */
struct BitStream {
    uint32_t data;  /* 0x0 */
    uint8_t _4[0x10];
    uint32_t readBitPos;  /* 0x14 */
    uint8_t _18[0x4];
    uint32_t endBit;  /* 0x1c */
    uint8_t failed;  /* 0x20 */
    uint8_t _21[0x3];
    uint32_t error;  /* 0x24 */
} __attribute__((packed));

/* dice::bf::ai::BotManager */
struct BotManager {
    uint8_t _0[0x148];
    uint8_t showBotSense;  /* 0x148 */
} __attribute__((packed));

/* Cmd_Game_ASEPort */
struct Cmd_Game_ASEPort {
    uint8_t _0[0x65c];
    uint32_t Game_ASEPort;  /* 0x65c */
} __attribute__((packed));

/* Cmd_Game_extrapolationTime */
struct Cmd_Game_extrapolationTime {
    uint8_t _0[0x48];
    uint32_t Game_extrapolationTime;  /* 0x48 */
} __attribute__((packed));

/* Cmd_Game_gameMode */
struct Cmd_Game_gameMode {
    uint8_t _0[0x1c4];
    uint32_t Game_gameMode;  /* 0x1c4 */
} __attribute__((packed));

/* Cmd_Game_gameSpyLANPort */
struct Cmd_Game_gameSpyLANPort {
    uint8_t _0[0x654];
    uint32_t Game_gameSpyLANPort;  /* 0x654 */
} __attribute__((packed));

/* Cmd_Game_gameSpyPort */
struct Cmd_Game_gameSpyPort {
    uint8_t _0[0x658];
    uint32_t Game_gameSpyPort;  /* 0x658 */
} __attribute__((packed));

/* Cmd_Game_goToInterface */
struct Cmd_Game_goToInterface {
    uint8_t _0[0x8c];
    uint32_t Game_goToInterface;  /* 0x8c */
} __attribute__((packed));

/* Cmd_Game_interpolationTime */
struct Cmd_Game_interpolationTime {
    uint8_t _0[0x44];
    uint32_t Game_interpolationTime;  /* 0x44 */
} __attribute__((packed));

/* Cmd_Game_maxCollInterval */
struct Cmd_Game_maxCollInterval {
    uint8_t _0[0xf8];
    uint32_t Game_maxCollInterval;  /* 0xf8 */
} __attribute__((packed));

/* Cmd_Game_serverAlliedTeamRatio */
struct Cmd_Game_serverAlliedTeamRatio {
    uint8_t _0[0x48c];
    uint32_t Game_serverAlliedTeamRatio;  /* 0x48c */
} __attribute__((packed));

/* Cmd_Game_serverAxisTeamRatio */
struct Cmd_Game_serverAxisTeamRatio {
    uint8_t _0[0x490];
    uint32_t Game_serverAxisTeamRatio;  /* 0x490 */
} __attribute__((packed));

/* Cmd_Game_serverBandwidthChokeLimit */
struct Cmd_Game_serverBandwidthChokeLimit {
    uint8_t _0[0x4f0];
    uint32_t Game_serverBandwidthChokeLimit;  /* 0x4f0 */
} __attribute__((packed));

/* Cmd_Game_serverCoopAiSkill */
struct Cmd_Game_serverCoopAiSkill {
    uint8_t _0[0x494];
    uint32_t Game_serverCoopAiSkill;  /* 0x494 */
} __attribute__((packed));

/* Cmd_Game_serverCoopCpu */
struct Cmd_Game_serverCoopCpu {
    uint8_t _0[0x498];
    uint32_t Game_serverCoopCpu;  /* 0x498 */
} __attribute__((packed));

/* Cmd_Game_serverDeathCameraType */
struct Cmd_Game_serverDeathCameraType {
    uint8_t _0[0x64c];
    uint32_t Game_serverDeathCameraType;  /* 0x64c */
} __attribute__((packed));

/* Cmd_Game_serverGamePlayMode */
struct Cmd_Game_serverGamePlayMode {
    uint8_t _0[0x458];
    uint32_t Game_serverGamePlayMode;  /* 0x458 */
} __attribute__((packed));

/* Cmd_Game_serverGameRoundStartDelay */
struct Cmd_Game_serverGameRoundStartDelay {
    uint8_t _0[0x478];
    uint32_t Game_serverGameRoundStartDelay;  /* 0x478 */
} __attribute__((packed));

/* Cmd_Game_serverGameStartDelay */
struct Cmd_Game_serverGameStartDelay {
    uint8_t _0[0x474];
    uint32_t Game_serverGameStartDelay;  /* 0x474 */
} __attribute__((packed));

/* Cmd_Game_serverGameTime */
struct Cmd_Game_serverGameTime {
    uint8_t _0[0x45c];
    uint32_t Game_serverGameTime;  /* 0x45c */
} __attribute__((packed));

/* Cmd_Game_serverMaxPlayers */
struct Cmd_Game_serverMaxPlayers {
    uint8_t _0[0x460];
    uint32_t Game_serverMaxPlayers;  /* 0x460 */
} __attribute__((packed));

/* Cmd_Game_serverNameTagDistance */
struct Cmd_Game_serverNameTagDistance {
    uint8_t _0[0x4e8];
    uint32_t Game_serverNameTagDistance;  /* 0x4e8 */
} __attribute__((packed));

/* Cmd_Game_serverNameTagDistanceScope */
struct Cmd_Game_serverNameTagDistanceScope {
    uint8_t _0[0x4ec];
    uint32_t Game_serverNameTagDistanceScope;  /* 0x4ec */
} __attribute__((packed));

/* Cmd_Game_serverNumReservedSlots */
struct Cmd_Game_serverNumReservedSlots {
    uint8_t _0[0x4dc];
    uint32_t Game_serverNumReservedSlots;  /* 0x4dc */
} __attribute__((packed));

/* Cmd_Game_serverNumberOfRounds */
struct Cmd_Game_serverNumberOfRounds {
    uint8_t _0[0x468];
    uint32_t Game_serverNumberOfRounds;  /* 0x468 */
} __attribute__((packed));

/* Cmd_Game_serverPort */
struct Cmd_Game_serverPort {
    uint8_t _0[0x644];
    uint32_t Game_serverPort;  /* 0x644 */
} __attribute__((packed));

/* Cmd_Game_serverScoreLimit */
struct Cmd_Game_serverScoreLimit {
    uint8_t _0[0x464];
    uint32_t Game_serverScoreLimit;  /* 0x464 */
} __attribute__((packed));

/* Cmd_Game_serverSoldierFriendlyFire */
struct Cmd_Game_serverSoldierFriendlyFire {
    uint8_t _0[0x47c];
    uint32_t Game_serverSoldierFriendlyFire;  /* 0x47c */
} __attribute__((packed));

/* Cmd_Game_serverSoldierFriendlyFireOnSplash */
struct Cmd_Game_serverSoldierFriendlyFireOnSplash {
    uint8_t _0[0x504];
    uint32_t Game_serverSoldierFriendlyFireOnSplash;  /* 0x504 */
} __attribute__((packed));

/* Cmd_Game_serverSpawnDelay */
struct Cmd_Game_serverSpawnDelay {
    uint8_t _0[0x470];
    uint32_t Game_serverSpawnDelay;  /* 0x470 */
} __attribute__((packed));

/* Cmd_Game_serverSpawnTime */
struct Cmd_Game_serverSpawnTime {
    uint8_t _0[0x46c];
    uint32_t Game_serverSpawnTime;  /* 0x46c */
} __attribute__((packed));

/* Cmd_Game_serverTicketRatio */
struct Cmd_Game_serverTicketRatio {
    uint8_t _0[0x484];
    uint32_t Game_serverTicketRatio;  /* 0x484 */
} __attribute__((packed));

/* Cmd_Game_serverVehicleFriendlyFire */
struct Cmd_Game_serverVehicleFriendlyFire {
    uint8_t _0[0x480];
    uint32_t Game_serverVehicleFriendlyFire;  /* 0x480 */
} __attribute__((packed));

/* Cmd_Game_serverVehicleFriendlyFireOnSplash */
struct Cmd_Game_serverVehicleFriendlyFireOnSplash {
    uint8_t _0[0x508];
    uint32_t Game_serverVehicleFriendlyFireOnSplash;  /* 0x508 */
} __attribute__((packed));

/* Cmd_Game_setCampaignScores */
struct Cmd_Game_setCampaignScores {
    uint8_t _0[0x3c0];
    uint32_t Game_setCampaignScores;  /* 0x3c0 */
} __attribute__((packed));

/* Cmd_Game_setMissionNr */
struct Cmd_Game_setMissionNr {
    uint8_t _0[0x3e4];
    uint32_t Game_setMissionNr;  /* 0x3e4 */
} __attribute__((packed));

/* dice::ref2::world::ControlPoint */
struct ControlPoint {
    uint8_t _0[0x1cc];
    uint32_t ObjectTemplate_team;  /* 0x1cc */
} __attribute__((packed));

/* dice::ref2::world::EmitterTemplate */
struct EmitterTemplate {
    uint8_t _0[0x19c];
    uint32_t ObjectTemplate_startProbability;  /* 0x19c */
    uint8_t _1a0[0x4];
    uint32_t ObjectTemplate_lodDistance;  /* 0x1a4 */
} __attribute__((packed));

/* dice::ref2::io::FileChangeManager */
struct FileChangeManager {
    uint8_t _0[0x44];
    uint8_t enable;  /* 0x44 */
} __attribute__((packed));

/* dice::ref2::world::FireArms */
struct FireArms {
    uint8_t _0[0x33c];
    uint32_t linkedArm;  /* 0x33c */
} __attribute__((packed));

/* dice::bf::Game */
struct Game {
    uint8_t _0[0xec];
    uint8_t aiLevel;  /* 0xec */
    uint8_t _ed[0x2c];
    uint8_t useActiveCombatArea;  /* 0x119 */
    uint8_t _11a[0x7a];
    uint8_t randomNames;  /* 0x194 */
} __attribute__((packed));

/* dice::bf::GameServer */
struct GameServer {
    uint8_t _0[0x2a0];
    uint32_t godMode;  /* 0x2a0 */
} __attribute__((packed));

/* dice::ref2::world::ItemTemplate */
struct ItemTemplate {
    uint8_t _0[0x1c4];
    uint8_t teamChange;  /* 0x1c4 */
} __attribute__((packed));

/* dice::ref2::world::LensFlareTemplate */
struct LensFlareTemplate {
    uint8_t _0[0x1a0];
    uint32_t ObjectTemplate_getNumFlares;  /* 0x1a0 */
    uint8_t _1a4[0x4];
    uint32_t ObjectTemplate_getNumCoronas;  /* 0x1a8 */
} __attribute__((packed));

/* dice::ref2::MemoryPool */
struct MemoryPool {
    uint8_t _0[0x28];
    uint32_t usedCount;  /* 0x28 */
    uint32_t elementsPerBlock;  /* 0x2c */
} __attribute__((packed));

/* dice::ref2::world::Object */
struct Object {
    uint8_t _0[0x54];
    uint32_t localPlayer;  /* 0x54 */
} __attribute__((packed));

/* dice::ref2::io::OldConsole */
struct OldConsole {
    uint8_t _0[0x285];
    uint8_t echoErrors;  /* 0x285 */
    uint8_t echo;  /* 0x286 */
} __attribute__((packed));

/* dice::ref2::world::PlayerManager */
struct PlayerManager {
    uint8_t _0[0x54];
    uint32_t localPlayer;  /* 0x54 */
} __attribute__((packed));

/* StateTimer */
struct StateTimer {
    uint8_t _0[0x18];
    uint32_t state;  /* 0x18 */
} __attribute__((packed));

/* dice::WrapperBase */
struct WrapperBase {
    uint8_t _0[0x10];
    uint32_t children_first;  /* 0x10 */
    uint8_t _14[0x4];
    uint32_t children_end;  /* 0x18 */
} __attribute__((packed));

#endif
