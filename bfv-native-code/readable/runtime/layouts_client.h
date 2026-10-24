/* Class layouts of client methods for `this->field` (tools/fields_l2.py):
 * the fields L2 methods access through `this`, at their offset and width. */
#ifndef BFV_LAYOUTS_CLIENT_H
#define BFV_LAYOUTS_CLIENT_H

/* dice::meme::ActionNode */
struct ActionNode {
    uint8_t _0[0xc];
    uint32_t Action;  /* 0xc */
} __attribute__((packed));

/* dice::ref2::world::Armor */
struct Armor {
    uint8_t _0[0x3c];
    uint32_t maxHitPoints;  /* 0x3c */
    uint8_t _40[0x94];
    uint32_t recentHead;  /* 0xd4 */
    uint32_t recentTail;  /* 0xd8 */
} __attribute__((packed));

/* dice::meme::BfGaugeNode */
struct BfGaugeNode {
    uint8_t _0[0xc];
    uint32_t Data;  /* 0xc */
} __attribute__((packed));

/* dice::meme::BfLocaleNode */
struct BfLocaleNode {
    uint8_t _0[0xc];
    uint32_t Locale;  /* 0xc */
} __attribute__((packed));

/* dice::meme::BfRectangle */
struct BfRectangle {
    uint8_t _0[0xc];
    uint32_t Thickness;  /* 0xc */
} __attribute__((packed));

/* dice::meme::BfVariablePictureFillNode2 */
struct BfVariablePictureFillNode2 {
    uint8_t _0[0x28];
    uint32_t Size;  /* 0x28 */
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
    uint8_t _0[0x6d0];
    uint32_t Game_ASEPort;  /* 0x6d0 */
} __attribute__((packed));

/* Cmd_Game_extrapolationTime */
struct Cmd_Game_extrapolationTime {
    uint8_t _0[0x48];
    uint32_t Game_extrapolationTime;  /* 0x48 */
} __attribute__((packed));

/* Cmd_Game_gameMode */
struct Cmd_Game_gameMode {
    uint8_t _0[0x234];
    uint32_t Game_gameMode;  /* 0x234 */
} __attribute__((packed));

/* Cmd_Game_gameSpyLANPort */
struct Cmd_Game_gameSpyLANPort {
    uint8_t _0[0x6c8];
    uint32_t Game_gameSpyLANPort;  /* 0x6c8 */
} __attribute__((packed));

/* Cmd_Game_gameSpyPort */
struct Cmd_Game_gameSpyPort {
    uint8_t _0[0x6cc];
    uint32_t Game_gameSpyPort;  /* 0x6cc */
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
    uint8_t _0[0x168];
    uint32_t Game_maxCollInterval;  /* 0x168 */
} __attribute__((packed));

/* Cmd_Game_serverAlliedTeamRatio */
struct Cmd_Game_serverAlliedTeamRatio {
    uint8_t _0[0x500];
    uint32_t Game_serverAlliedTeamRatio;  /* 0x500 */
} __attribute__((packed));

/* Cmd_Game_serverAxisTeamRatio */
struct Cmd_Game_serverAxisTeamRatio {
    uint8_t _0[0x504];
    uint32_t Game_serverAxisTeamRatio;  /* 0x504 */
} __attribute__((packed));

/* Cmd_Game_serverBandwidthChokeLimit */
struct Cmd_Game_serverBandwidthChokeLimit {
    uint8_t _0[0x564];
    uint32_t Game_serverBandwidthChokeLimit;  /* 0x564 */
} __attribute__((packed));

/* Cmd_Game_serverCoopAiSkill */
struct Cmd_Game_serverCoopAiSkill {
    uint8_t _0[0x508];
    uint32_t Game_serverCoopAiSkill;  /* 0x508 */
} __attribute__((packed));

/* Cmd_Game_serverCoopCpu */
struct Cmd_Game_serverCoopCpu {
    uint8_t _0[0x50c];
    uint32_t Game_serverCoopCpu;  /* 0x50c */
} __attribute__((packed));

/* Cmd_Game_serverDeathCameraType */
struct Cmd_Game_serverDeathCameraType {
    uint8_t _0[0x6c0];
    uint32_t Game_serverDeathCameraType;  /* 0x6c0 */
} __attribute__((packed));

/* Cmd_Game_serverGamePlayMode */
struct Cmd_Game_serverGamePlayMode {
    uint8_t _0[0x4cc];
    uint32_t Game_serverGamePlayMode;  /* 0x4cc */
} __attribute__((packed));

/* Cmd_Game_serverGameRoundStartDelay */
struct Cmd_Game_serverGameRoundStartDelay {
    uint8_t _0[0x4ec];
    uint32_t Game_serverGameRoundStartDelay;  /* 0x4ec */
} __attribute__((packed));

/* Cmd_Game_serverGameStartDelay */
struct Cmd_Game_serverGameStartDelay {
    uint8_t _0[0x4e8];
    uint32_t Game_serverGameStartDelay;  /* 0x4e8 */
} __attribute__((packed));

/* Cmd_Game_serverGameTime */
struct Cmd_Game_serverGameTime {
    uint8_t _0[0x4d0];
    uint32_t Game_serverGameTime;  /* 0x4d0 */
} __attribute__((packed));

/* Cmd_Game_serverMaxPlayers */
struct Cmd_Game_serverMaxPlayers {
    uint8_t _0[0x4d4];
    uint32_t Game_serverMaxPlayers;  /* 0x4d4 */
} __attribute__((packed));

/* Cmd_Game_serverNameTagDistance */
struct Cmd_Game_serverNameTagDistance {
    uint8_t _0[0x55c];
    uint32_t Game_serverNameTagDistance;  /* 0x55c */
} __attribute__((packed));

/* Cmd_Game_serverNameTagDistanceScope */
struct Cmd_Game_serverNameTagDistanceScope {
    uint8_t _0[0x560];
    uint32_t Game_serverNameTagDistanceScope;  /* 0x560 */
} __attribute__((packed));

/* Cmd_Game_serverNumReservedSlots */
struct Cmd_Game_serverNumReservedSlots {
    uint8_t _0[0x550];
    uint32_t Game_serverNumReservedSlots;  /* 0x550 */
} __attribute__((packed));

/* Cmd_Game_serverNumberOfRounds */
struct Cmd_Game_serverNumberOfRounds {
    uint8_t _0[0x4dc];
    uint32_t Game_serverNumberOfRounds;  /* 0x4dc */
} __attribute__((packed));

/* Cmd_Game_serverPort */
struct Cmd_Game_serverPort {
    uint8_t _0[0x6b8];
    uint32_t Game_serverPort;  /* 0x6b8 */
} __attribute__((packed));

/* Cmd_Game_serverScoreLimit */
struct Cmd_Game_serverScoreLimit {
    uint8_t _0[0x4d8];
    uint32_t Game_serverScoreLimit;  /* 0x4d8 */
} __attribute__((packed));

/* Cmd_Game_serverSoldierFriendlyFire */
struct Cmd_Game_serverSoldierFriendlyFire {
    uint8_t _0[0x4f0];
    uint32_t Game_serverSoldierFriendlyFire;  /* 0x4f0 */
} __attribute__((packed));

/* Cmd_Game_serverSoldierFriendlyFireOnSplash */
struct Cmd_Game_serverSoldierFriendlyFireOnSplash {
    uint8_t _0[0x578];
    uint32_t Game_serverSoldierFriendlyFireOnSplash;  /* 0x578 */
} __attribute__((packed));

/* Cmd_Game_serverSpawnDelay */
struct Cmd_Game_serverSpawnDelay {
    uint8_t _0[0x4e4];
    uint32_t Game_serverSpawnDelay;  /* 0x4e4 */
} __attribute__((packed));

/* Cmd_Game_serverSpawnTime */
struct Cmd_Game_serverSpawnTime {
    uint8_t _0[0x4e0];
    uint32_t Game_serverSpawnTime;  /* 0x4e0 */
} __attribute__((packed));

/* Cmd_Game_serverTicketRatio */
struct Cmd_Game_serverTicketRatio {
    uint8_t _0[0x4f8];
    uint32_t Game_serverTicketRatio;  /* 0x4f8 */
} __attribute__((packed));

/* Cmd_Game_serverVehicleFriendlyFire */
struct Cmd_Game_serverVehicleFriendlyFire {
    uint8_t _0[0x4f4];
    uint32_t Game_serverVehicleFriendlyFire;  /* 0x4f4 */
} __attribute__((packed));

/* Cmd_Game_serverVehicleFriendlyFireOnSplash */
struct Cmd_Game_serverVehicleFriendlyFireOnSplash {
    uint8_t _0[0x57c];
    uint32_t Game_serverVehicleFriendlyFireOnSplash;  /* 0x57c */
} __attribute__((packed));

/* Cmd_Game_setCampaignAiSkill */
struct Cmd_Game_setCampaignAiSkill {
    uint8_t _0[0x68];
    uint32_t Game_setCampaignAiSkill;  /* 0x68 */
} __attribute__((packed));

/* Cmd_Game_setCampaignBotRatio */
struct Cmd_Game_setCampaignBotRatio {
    uint8_t _0[0x6c];
    uint32_t Game_setCampaignBotRatio;  /* 0x6c */
} __attribute__((packed));

/* Cmd_Game_setCampaignDifficulty */
struct Cmd_Game_setCampaignDifficulty {
    uint8_t _0[0x7c];
    uint32_t Game_setCampaignDifficulty;  /* 0x7c */
} __attribute__((packed));

/* Cmd_Game_setCampaignNrOfLives */
struct Cmd_Game_setCampaignNrOfLives {
    uint8_t _0[0x74];
    uint32_t Game_setCampaignNrOfLives;  /* 0x74 */
} __attribute__((packed));

/* Cmd_Game_setCampaignScores */
struct Cmd_Game_setCampaignScores {
    uint8_t _0[0x434];
    uint32_t Game_setCampaignScores;  /* 0x434 */
} __attribute__((packed));

/* Cmd_Game_setMissionNr */
struct Cmd_Game_setMissionNr {
    uint8_t _0[0x458];
    uint32_t Game_setMissionNr;  /* 0x458 */
} __attribute__((packed));

/* Cmd_Game_setSkirmishAiSkill */
struct Cmd_Game_setSkirmishAiSkill {
    uint8_t _0[0x84];
    uint32_t Game_setSkirmishAiSkill;  /* 0x84 */
} __attribute__((packed));

/* Cmd_Game_setSkirmishBotRatio */
struct Cmd_Game_setSkirmishBotRatio {
    uint8_t _0[0x88];
    uint32_t Game_setSkirmishBotRatio;  /* 0x88 */
} __attribute__((packed));

/* Cmd_Game_setSkirmishDifficulty */
struct Cmd_Game_setSkirmishDifficulty {
    uint8_t _0[0x98];
    uint32_t Game_setSkirmishDifficulty;  /* 0x98 */
} __attribute__((packed));

/* Cmd_Game_setSkirmishNrOfLives */
struct Cmd_Game_setSkirmishNrOfLives {
    uint8_t _0[0x90];
    uint32_t Game_setSkirmishNrOfLives;  /* 0x90 */
} __attribute__((packed));

/* Cmd_Game_team */
struct Cmd_Game_team {
    uint8_t _0[0x150];
    uint32_t Game_team;  /* 0x150 */
} __attribute__((packed));

/* Cmd_Shadow_numOutlinesPerFrame */
struct Cmd_Shadow_numOutlinesPerFrame {
    uint8_t _0[0x58];
    uint32_t Shadow_numOutlinesPerFrame;  /* 0x58 */
} __attribute__((packed));

/* Cmd_setCloudFog */
struct Cmd_setCloudFog {
    uint8_t _0[0x20];
    uint32_t setCloudFog;  /* 0x20 */
} __attribute__((packed));

/* dice::ref2::world::ControlPoint */
struct ControlPoint {
    uint8_t _0[0x1d4];
    uint32_t ObjectTemplate_team;  /* 0x1d4 */
} __attribute__((packed));

/* dice::meme::EffectNode */
struct EffectNode {
    uint8_t _0[0xc];
    uint32_t Effect;  /* 0xc */
} __attribute__((packed));

/* dice::ref2::world::EmitterTemplate */
struct EmitterTemplate {
    uint8_t _0[0x1c8];
    uint32_t ObjectTemplate_startProbability;  /* 0x1c8 */
    uint8_t _1cc[0x4];
    uint32_t ObjectTemplate_lodDistance;  /* 0x1d0 */
} __attribute__((packed));

/* dice::meme::ExtendedFocusToPressedEffectNode */
struct ExtendedFocusToPressedEffectNode {
    uint8_t _0[0xc];
    uint32_t Effect;  /* 0xc */
} __attribute__((packed));

/* dice::meme::ExtendedNormalToFocusEffectNode */
struct ExtendedNormalToFocusEffectNode {
    uint8_t _0[0xc];
    uint32_t Effect;  /* 0xc */
} __attribute__((packed));

/* dice::ref2::world::FireArms */
struct FireArms {
    uint8_t _0[0x33c];
    uint32_t linkedArm;  /* 0x33c */
} __attribute__((packed));

/* dice::meme::FocusEffectNode */
struct FocusEffectNode {
    uint8_t _0[0xc];
    uint32_t Effect;  /* 0xc */
} __attribute__((packed));

/* dice::meme::FocusToPressedEffectNode */
struct FocusToPressedEffectNode {
    uint8_t _0[0xc];
    uint32_t Effect;  /* 0xc */
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

/* dice::meme::IndexCountData */
struct IndexCountData {
    uint8_t _0[0xc];
    uint32_t Data;  /* 0xc */
} __attribute__((packed));

/* dice::ref2::world::ItemTemplate */
struct ItemTemplate {
    uint8_t _0[0x1f0];
    uint8_t teamChange;  /* 0x1f0 */
} __attribute__((packed));

/* dice::ref2::world::LensFlareTemplate */
struct LensFlareTemplate {
    uint8_t _0[0x1cc];
    uint32_t ObjectTemplate_getNumFlares;  /* 0x1cc */
    uint8_t _1d0[0x4];
    uint32_t ObjectTemplate_getNumCoronas;  /* 0x1d4 */
} __attribute__((packed));

/* dice::ref2::MemoryPool */
struct MemoryPool {
    uint8_t _0[0x28];
    uint32_t usedCount;  /* 0x28 */
    uint32_t elementsPerBlock;  /* 0x2c */
} __attribute__((packed));

/* dice::meme::NormalToFocusEffectNode */
struct NormalToFocusEffectNode {
    uint8_t _0[0xc];
    uint32_t Effect;  /* 0xc */
} __attribute__((packed));

/* dice::meme::NotFocusEffectNode */
struct NotFocusEffectNode {
    uint8_t _0[0xc];
    uint32_t Effect;  /* 0xc */
} __attribute__((packed));

/* dice::ref2::io::OldConsole */
struct OldConsole {
    uint8_t _0[0x285];
    uint8_t echoErrors;  /* 0x285 */
    uint8_t echo;  /* 0x286 */
} __attribute__((packed));

/* dice::bf::menu::OverHeatRecover */
struct OverHeatRecover {
    uint8_t _0[0x10];
    uint32_t ShowRecover;  /* 0x10 */
} __attribute__((packed));

/* dice::ref2::world::PlayerManager */
struct PlayerManager {
    uint8_t _0[0x54];
    uint32_t localPlayer;  /* 0x54 */
} __attribute__((packed));

/* dice::bf::menu::Radio */
struct Radio {
    uint8_t _0[0xc];
    uint8_t ShowRadioIcons;  /* 0xc */
} __attribute__((packed));

/* dice::bf::menu::ReloadBar */
struct ReloadBar {
    uint8_t _0[0xc];
    uint32_t ReloadTimeSecondary;  /* 0xc */
} __attribute__((packed));

/* dice::meme::ShowEffectNode */
struct ShowEffectNode {
    uint8_t _0[0xc];
    uint32_t Effect;  /* 0xc */
} __attribute__((packed));

/* dice::bf::menu::Soldier */
struct Soldier {
    uint8_t _0[0x18];
    uint8_t ShowSoldierIcon;  /* 0x18 */
} __attribute__((packed));

/* dice::bf::menu::SpawnInterface */
struct SpawnInterface {
    uint8_t _0[0x61];
    uint8_t RedrawKit;  /* 0x61 */
} __attribute__((packed));

/* StateTimer */
struct StateTimer {
    uint8_t _0[0x18];
    uint32_t state;  /* 0x18 */
} __attribute__((packed));

/* dice::bf::menu::Submarine */
struct Submarine {
    uint8_t _0[0x2c];
    uint8_t ShowDepthHud;  /* 0x2c */
} __attribute__((packed));

/* dice::meme::TextNode */
struct TextNode {
    uint8_t _0[0xc];
    uint32_t Style;  /* 0xc */
    uint32_t String;  /* 0x10 */
} __attribute__((packed));

/* dice::ref2::rend::TextureManager */
struct TextureManager {
    uint8_t _0[0x55];
    uint8_t mipmaps;  /* 0x55 */
} __attribute__((packed));

/* dice::bf::menu::ToolTipManager */
struct ToolTipManager {
    uint8_t _0[0xc];
    uint32_t ToolTipIndex;  /* 0xc */
} __attribute__((packed));

/* dice::meme::TransitionalHideEffectNode */
struct TransitionalHideEffectNode {
    uint8_t _0[0xc];
    uint32_t Effect;  /* 0xc */
} __attribute__((packed));

/* dice::meme::TransitionalShowEffectNode */
struct TransitionalShowEffectNode {
    uint8_t _0[0xc];
    uint32_t Effect;  /* 0xc */
} __attribute__((packed));

/* dice::bf::menu::Weapon */
struct Weapon {
    uint8_t _0[0x12];
    uint8_t ShowWeaponIcon;  /* 0x12 */
} __attribute__((packed));

/* dice::WrapperBase */
struct WrapperBase {
    uint8_t _0[0x10];
    uint32_t children_first;  /* 0x10 */
    uint8_t _14[0x4];
    uint32_t children_end;  /* 0x18 */
} __attribute__((packed));

#endif
