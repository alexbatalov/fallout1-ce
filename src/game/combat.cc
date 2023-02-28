#include "game/combat.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "game/actions.h"
#include "game/anim.h"
#include "game/art.h"
#include "game/combatai.h"
#include "game/critter.h"
#include "game/display.h"
#include "game/elevator.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gmouse.h"
#include "game/gsound.h"
#include "game/intface.h"
#include "game/item.h"
#include "game/loadsave.h"
#include "game/map.h"
#include "game/object.h"
#include "game/perk.h"
#include "game/pipboy.h"
#include "game/proto.h"
#include "game/queue.h"
#include "game/roll.h"
#include "game/scripts.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/tile.h"
#include "game/trait.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/db/db.h"
#include "plib/gnw/button.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"

namespace fallout {

#define CALLED_SHOT_WINDOW_X 108
#define CALLED_SHOT_WINDOW_Y 20
#define CALLED_SHOT_WINDOW_WIDTH 424
#define CALLED_SHOT_WINDOW_HEIGHT 309

static void combat_begin(Object* a1);
static void combat_begin_extra(Object* a1);
static void combat_over();
static void combat_add_noncoms();
static int compare_faster(const void* a1, const void* a2);
static void combat_sequence_init(Object* a1, Object* a2);
static void combat_sequence();
static int combat_input();
static int combat_turn(Object* a1, bool a2);
static bool combat_should_end();
static bool check_ranged_miss(Attack* attack);
static int shoot_along_path(Attack* attack, int a2, int a3, int anim);
static int compute_spray(Attack* attack, int accuracy, int* roundsHitMainTargetPtr, int* roundsSpentPtr, int anim);
static int compute_attack(Attack* attack);
static int attack_crit_success(Attack* a1);
static int attack_crit_failure(Attack* attack);
static void do_random_cripple(int* flagsPtr);
static int determine_to_hit_func(Object* attacker, Object* defender, int hitLocation, int hitMode, int check_range);
static void compute_damage(Attack* attack, int ammoQuantity, int bonusDamageMultiplier);
static void check_for_death(Object* a1, int a2, int* a3);
static void set_new_results(Object* a1, int a2);
static void damage_object(Object* obj, int damage, bool animated, bool a4);
static void combat_display_hit(char* dest, size_t size, Object* critter_obj, int damage);
static void combat_display_flags(char* a1, int flags, Object* a3);
static void combat_standup(Object* a1);
static void print_tohit(unsigned char* dest, int dest_pitch, int a3);
static char* combat_get_loc_name(Object* critter, int hitLocation);
static void draw_loc_off(int a1, int a2);
static void draw_loc_on(int a1, int a2);
static void draw_loc(int eventCode, int color);
static int get_called_shot_location(Object* critter, int* hitLocation, int hitMode);

// TODO: Remove.
//
// 0x500B50
static char _a_1[] = ".";

// 0x4FEC80
static int combat_turn_running = 0;

// 0x4FEC84
unsigned int combat_state = COMBAT_STATE_0x02;

// 0x4FEC88
STRUCT_664980* gcsd = NULL;

// 0x4FEC8C
bool combat_call_display = false;

// Accuracy modifiers for hit locations.
//
// 0x4FEC90
static int hit_location_penalty[HIT_LOCATION_COUNT] = {
    -40,
    -30,
    -30,
    0,
    -20,
    -20,
    -60,
    -30,
    0,
};

// Critical hit tables for every kill type.
//
// 0x510978
static CriticalHitDescription crit_succ_eff[KILL_TYPE_COUNT][HIT_LOCATION_COUNT][CRTICIAL_EFFECT_COUNT] = {
    // KILL_TYPE_MAN
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5002, 5003 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 5002, 5003 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 5004, 5003 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, STAT_LUCK, 0, DAM_BLIND, 5005, 5006 },
            { 6, DAM_DEAD, -1, 0, 0, 5007, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5008, 5000 },
            { 3, DAM_LOSE_TURN, -1, 0, 0, 5009, 5000 },
            { 4, 0, STAT_ENDURANCE, -3, DAM_CRIP_ARM_LEFT, 5010, 5011 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5012, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5012, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5013, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5008, 5000 },
            { 3, DAM_LOSE_TURN, -1, 0, 0, 5009, 5000 },
            { 4, 0, STAT_ENDURANCE, -3, DAM_CRIP_ARM_RIGHT, 5014, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5015, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5015, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5013, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5016, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5017, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5019, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5019, 5000 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5020, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5021, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5023, 5000 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5023, 5024 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_RIGHT, 5023, 5024 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5025, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5025, 5026 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5026, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5023, 5000 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5023, 5024 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_LEFT, 5023, 5024 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5025, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5025, 5026 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5026, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, STAT_LUCK, 4, DAM_BLIND, 5027, 5028 },
            { 4, DAM_BYPASS, STAT_LUCK, 3, DAM_BLIND, 5029, 5028 },
            { 6, DAM_BYPASS, STAT_LUCK, 2, DAM_BLIND, 5029, 5028 },
            { 6, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5030, 5000 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5031, 5000 },
            { 8, DAM_DEAD, -1, 0, 0, 5032, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5033, 5000 },
            { 3, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN, 5034, 5035 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 5035, 5036 },
            { 3, DAM_KNOCKED_OUT, -1, 0, 0, 5036, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5035, 5036 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5037, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5016, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5017, 5000 },
            { 4, 0, -1, 0, 0, 5018, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5019, 5000 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5020, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5021, 5000 },
        },
    },
    // KILL_TYPE_WOMAN
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5101, 5100 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5102, 5103 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 5102, 5103 },
            { 6, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 5104, 5103 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, STAT_LUCK, 0, DAM_BLIND, 5105, 5106 },
            { 6, DAM_DEAD, -1, 0, 0, 5107, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5108, 5100 },
            { 3, DAM_LOSE_TURN, -1, 0, 0, 5109, 5100 },
            { 4, 0, STAT_ENDURANCE, -2, DAM_CRIP_ARM_LEFT, 5110, 5111 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_CRIP_ARM_LEFT, 5110, 5111 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5112, 5100 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5113, 5100 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5108, 5100 },
            { 3, DAM_LOSE_TURN, -1, 0, 0, 5109, 5100 },
            { 4, 0, STAT_ENDURANCE, -2, DAM_CRIP_ARM_RIGHT, 5114, 5100 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_CRIP_ARM_RIGHT, 5114, 5100 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5115, 5100 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5113, 5100 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5116, 5100 },
            { 3, DAM_BYPASS, -1, 0, 0, 5117, 5100 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5119, 5100 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5119, 5100 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5120, 5100 },
            { 6, DAM_DEAD, -1, 0, 0, 5121, 5100 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5123, 5100 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5123, 5124 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_RIGHT, 5123, 5124 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5125, 5100 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5125, 5126 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5126, 5100 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5123, 5100 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5123, 5124 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_LEFT, 5123, 5124 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5125, 5100 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5125, 5126 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5126, 5100 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, STAT_LUCK, 4, DAM_BLIND, 5127, 5128 },
            { 4, DAM_BYPASS, STAT_LUCK, 3, DAM_BLIND, 5129, 5128 },
            { 6, DAM_BYPASS, STAT_LUCK, 2, DAM_BLIND, 5129, 5128 },
            { 6, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5130, 5100 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5131, 5100 },
            { 8, DAM_DEAD, -1, 0, 0, 5132, 5100 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5133, 5100 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 5133, 5134 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5134, 5135 },
            { 3, DAM_KNOCKED_OUT, -1, 0, 0, 5135, 5100 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5134, 5135 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5135, 5100 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5116, 5100 },
            { 3, DAM_BYPASS, -1, 0, 0, 5117, 5100 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5119, 5100 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5119, 5100 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5120, 5100 },
            { 6, DAM_DEAD, -1, 0, 0, 5121, 5100 },
        },
    },
    // KILL_TYPE_CHILD
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5200, 5201 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, -2, DAM_KNOCKED_OUT, 5202, 5203 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, -2, DAM_KNOCKED_OUT, 5202, 5203 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5203, 5000 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5203, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5204, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5205, 5000 },
            { 4, DAM_LOSE_TURN, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 5206, 5207 },
            { 4, DAM_LOSE_TURN, STAT_ENDURANCE, -2, DAM_CRIP_ARM_LEFT, 5206, 5207 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5208, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5208, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5208, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5209, 5000 },
            { 4, DAM_LOSE_TURN, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 5206, 5207 },
            { 4, DAM_LOSE_TURN, STAT_ENDURANCE, -2, DAM_CRIP_ARM_RIGHT, 5206, 5207 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5208, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5208, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5208, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5210, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5211, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5212, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5212, 5000 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5213, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5214, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 5215, 5000 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, DAM_CRIP_ARM_RIGHT | DAM_BLIND | DAM_ON_FIRE | DAM_EXPLODE, 5000, 0 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, DAM_CRIP_ARM_RIGHT | DAM_BLIND | DAM_ON_FIRE | DAM_EXPLODE, 5000, 0 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5217, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5217, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5217, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 5215, 5000 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, DAM_CRIP_ARM_RIGHT | DAM_BLIND | DAM_ON_FIRE | DAM_EXPLODE, 5000, 0 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, DAM_CRIP_ARM_RIGHT | DAM_BLIND | DAM_ON_FIRE | DAM_EXPLODE, 5000, 0 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5217, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5217, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5217, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, STAT_LUCK, 5, DAM_BLIND, 5218, 5219 },
            { 4, DAM_BYPASS, STAT_LUCK, 2, DAM_BLIND, 5220, 5221 },
            { 6, DAM_BYPASS, STAT_LUCK, -1, DAM_BLIND, 5220, 5221 },
            { 6, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5222, 5000 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5223, 5000 },
            { 8, DAM_DEAD, -1, 0, 0, 5224, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5225, 5000 },
            { 3, 0, -1, 0, 0, 5225, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5226, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5226, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5226, 5000 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 5226, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5210, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5211, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5211, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5212, 5000 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5213, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5214, 5000 },
        },
    },
    // KILL_TYPE_SUPER_MUTANT
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5300, 5000 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, -1, DAM_KNOCKED_DOWN, 5301, 5302 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -4, DAM_KNOCKED_DOWN, 5301, 5302 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 5302, 5303 },
            { 6, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5302, 5303 },
            { 6, DAM_DEAD, -1, 0, 0, 5304, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5300, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_LOSE_TURN, 5300, 5306 },
            { 4, DAM_BYPASS | DAM_LOSE_TURN, STAT_ENDURANCE, -1, DAM_CRIP_ARM_LEFT, 5307, 5308 },
            { 4, DAM_BYPASS | DAM_LOSE_TURN, STAT_ENDURANCE, -3, DAM_CRIP_ARM_LEFT, 5307, 5308 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5308, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5308, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5300, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_LOSE_TURN, 5300, 5006 },
            { 4, DAM_BYPASS | DAM_LOSE_TURN, STAT_ENDURANCE, -1, DAM_CRIP_ARM_RIGHT, 5307, 5309 },
            { 4, DAM_BYPASS | DAM_LOSE_TURN, STAT_ENDURANCE, -3, DAM_CRIP_ARM_RIGHT, 5307, 5309 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5309, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5309, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5300, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5301, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5302, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5302, 5000 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5310, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5311, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 5300, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5300, 5312 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_RIGHT, 5312, 5313 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5313, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5314, 5000 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5315, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 5300, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5300, 5312 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_LEFT, 5312, 5313 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, 0, 5313, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5314, 5000 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5315, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 5300, 5000 },
            { 4, DAM_BYPASS, STAT_LUCK, 5, DAM_BLIND, 5316, 5317 },
            { 6, DAM_BYPASS, STAT_LUCK, 3, DAM_BLIND, 5316, 5317 },
            { 6, DAM_BYPASS | DAM_LOSE_TURN, STAT_LUCK, 0, DAM_BLIND, 5318, 5319 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5320, 5000 },
            { 8, DAM_DEAD, -1, 0, 0, 5321, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5300, 5000 },
            { 3, 0, STAT_LUCK, 0, DAM_BYPASS, 5300, 5017 },
            { 3, DAM_BYPASS, STAT_ENDURANCE, -2, DAM_KNOCKED_DOWN, 5301, 5302 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5312, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5302, 5303 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5303, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5300, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5301, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5302, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5302, 5000 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5310, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5311, 5000 },
        },
    },
    // KILL_TYPE_GHOUL
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5400, 5003 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -1, DAM_KNOCKED_OUT, 5400, 5003 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, -2, DAM_KNOCKED_OUT, 5004, 5005 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, STAT_STRENGTH, 0, 0, 5005, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5401, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5016, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_DROP | DAM_LOSE_TURN, 5001, 5402 },
            { 4, DAM_DROP | DAM_LOSE_TURN, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 5402, 5012 },
            { 4, DAM_BYPASS | DAM_DROP | DAM_LOSE_TURN, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 5403, 5404 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS | DAM_DROP, -1, 0, 0, 5404, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS | DAM_DROP, -1, 0, 0, 5404, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5016, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_DROP | DAM_LOSE_TURN, 5001, 5402 },
            { 4, DAM_DROP | DAM_LOSE_TURN, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 5402, 5015 },
            { 4, DAM_BYPASS | DAM_DROP | DAM_LOSE_TURN, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 5403, 5404 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS | DAM_DROP, -1, 0, 0, 5404, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS | DAM_DROP, -1, 0, 0, 5404, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5017, 5000 },
            { 3, 0, -1, 0, 0, 5018, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5004, 5000 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5003, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5007, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 5023 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5023, 5024 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5024, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5024, 5026 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5026, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 5023 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5023, 5024 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, 0, 5024, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5024, 5026 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5026, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, STAT_LUCK, 3, DAM_BLIND, 5001, 5405 },
            { 4, DAM_BYPASS, STAT_LUCK, 0, DAM_BLIND, 5406, 5407 },
            { 6, DAM_BYPASS, STAT_LUCK, -3, DAM_BLIND, 5406, 5407 },
            { 6, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5030, 5000 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5031, 5000 },
            { 8, DAM_DEAD, -1, 0, 0, 5408, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_LUCK, 0, DAM_BYPASS, 5001, 5033 },
            { 3, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 5033, 5035 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5004, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5035, 5036 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5036, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5017, 5000 },
            { 3, 0, -1, 0, 0, 5018, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5004, 5000 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5003, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5007, 5000 },
        },
    },
    // KILL_TYPE_BRAHMIN
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 5, 0, STAT_ENDURANCE, 2, DAM_KNOCKED_DOWN, 5016, 5500 },
            { 5, 0, STAT_ENDURANCE, -1, DAM_KNOCKED_DOWN, 5016, 5500 },
            { 6, DAM_KNOCKED_OUT, STAT_STRENGTH, 0, 0, 5501, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5502, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5016, 5503 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5016, 5503 },
            { 4, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5503, 5000 },
            { 4, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5503, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5016, 5503 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5016, 5503 },
            { 4, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5503, 5000 },
            { 4, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5503, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5504, 5000 },
            { 3, 0, -1, 0, 0, 5504, 5000 },
            { 4, 0, -1, 0, 0, 5504, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5505, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5505, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5506, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5016, 5503 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5016, 5503 },
            { 4, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5503, 5000 },
            { 4, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5503, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5016, 5503 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5016, 5503 },
            { 4, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5503, 5000 },
            { 4, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5503, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, STAT_LUCK, 0, DAM_BLIND, 5029, 5507 },
            { 6, DAM_BYPASS, STAT_LUCK, -3, DAM_BLIND, 5029, 5507 },
            { 6, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5508, 5000 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5509, 5000 },
            { 8, DAM_DEAD, -1, 0, 0, 5510, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5511, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5511, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5512, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5512, 5000 },
            { 6, DAM_BYPASS, -1, 0, 0, 5513, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5504, 5000 },
            { 3, 0, -1, 0, 0, 5504, 5000 },
            { 4, 0, -1, 0, 0, 5504, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5505, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5505, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5506, 5000 },
        },
    },
    // KILL_TYPE_RADSCORPION
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 3, DAM_KNOCKED_DOWN, 5001, 5600 },
            { 5, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 5001, 5600 },
            { 5, 0, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN, 5001, 5600 },
            { 6, DAM_KNOCKED_DOWN, -1, 0, 0, 5600, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5601, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 5016, 5602 },
            { 4, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5602, 5000 },
            { 4, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5602, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_ENDURANCE, 2, DAM_CRIP_ARM_RIGHT, 5016, 5603 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 5016, 5603 },
            { 4, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 5603, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5604, 5000 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5605, 5000 },
            { 4, DAM_BYPASS, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5605, 5606 },
            { 4, DAM_DEAD, -1, 0, 0, 5607, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, 2, 0, 5001, 5600 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5600, 5608 },
            { 4, DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5609, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5608, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5608, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, 2, 0, 5001, 5600 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5600, 5008 },
            { 4, DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5609, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5608, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5608, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_AGILITY, 3, DAM_BLIND, 5001, 5610 },
            { 6, 0, STAT_AGILITY, 0, DAM_BLIND, 5016, 5610 },
            { 6, 0, STAT_AGILITY, -3, DAM_BLIND, 5016, 5610 },
            { 8, 0, STAT_AGILITY, -3, DAM_BLIND, 5611, 5612 },
            { 8, DAM_DEAD, -1, 0, 0, 5613, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5614, 5000 },
            { 3, 0, -1, 0, 0, 5614, 5000 },
            { 4, 0, -1, 0, 0, 5614, 5000 },
            { 4, DAM_KNOCKED_OUT, -1, 0, 0, 5615, 5000 },
            { 4, DAM_KNOCKED_OUT, -1, 0, 0, 5615, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5616, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5604, 5000 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5605, 5000 },
            { 4, DAM_BYPASS, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5605, 5606 },
            { 4, DAM_DEAD, -1, 0, 0, 5607, 5000 },
        },
    },
    // KILL_TYPE_RAT
    {
        // HIT_LOCATION_HEAD
        {
            { 4, DAM_BYPASS, -1, 0, 0, 5700, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5700, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5701, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5701, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5701, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5701, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5703, 5000 },
            { 3, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5703, 5000 },
            { 3, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5703, 5000 },
            { 4, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5703, 5000 },
            { 4, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5703, 5000 },
            { 4, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5703, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 5705, 5000 },
            { 3, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 5705, 5000 },
            { 3, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 5705, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 5705, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 5705, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 5705, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5706, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5707, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5707, 5000 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 5707, 5000 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 5707, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5708, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5709, 5000 },
            { 3, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5709, 5000 },
            { 3, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5709, 5000 },
            { 4, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5709, 5000 },
            { 4, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5709, 5000 },
            { 4, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5709, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5710, 5000 },
            { 3, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5710, 5000 },
            { 3, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5710, 5000 },
            { 4, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5710, 5000 },
            { 4, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5710, 5000 },
            { 4, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5710, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, DAM_BYPASS, -1, 0, 0, 5711, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5712, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5712, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5712, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5712, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5712, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5711, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5711, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5712, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5712, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5706, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5707, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5707, 5000 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 5707, 5000 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 5707, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5708, 5000 },
        },
    },
    // KILL_TYPE_FLOATER
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 5, 0, STAT_AGILITY, -3, DAM_KNOCKED_DOWN, 5016, 5800 },
            { 5, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5800, 5801 },
            { 6, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 5800, 5801 },
            { 6, DAM_DEAD, -1, 0, 0, 5802, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_LOSE_TURN, 5001, 5803 },
            { 4, 0, STAT_ENDURANCE, -2, DAM_LOSE_TURN, 5001, 5803 },
            { 3, DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5804, 5000 },
            { 4, DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5804, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5805, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_LOSE_TURN, 5001, 5803 },
            { 4, 0, STAT_ENDURANCE, -2, DAM_LOSE_TURN, 5001, 5803 },
            { 3, DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5804, 5000 },
            { 4, DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5804, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5805, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 3, 0, STAT_AGILITY, -2, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 5800, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5804, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5805, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, 1, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 4, 0, STAT_AGILITY, -1, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -1, DAM_CRIP_LEG_LEFT | DAM_CRIP_LEG_RIGHT, 5800, 5806 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, -3, DAM_CRIP_LEG_LEFT | DAM_CRIP_LEG_RIGHT, 5804, 5806 },
            { 6, DAM_DEAD | DAM_ON_FIRE, -1, 0, 0, 5807, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_LOSE_TURN, -1, 0, 0, 5803, 5000 },
            { 4, DAM_LOSE_TURN, -1, 0, 0, 5803, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_CRIP_ARM_RIGHT | DAM_LOSE_TURN, -1, 0, 0, 5808, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_CRIP_ARM_RIGHT | DAM_LOSE_TURN, -1, 0, 0, 5808, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5809, 5000 },
            { 5, 0, STAT_ENDURANCE, 0, DAM_BLIND, 5016, 5810 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_BLIND, 5809, 5810 },
            { 6, DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5810, 5000 },
            { 6, DAM_KNOCKED_DOWN | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5801, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 3, 0, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 3, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5800, 5000 },
            { 3, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5800, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 3, 0, STAT_AGILITY, -2, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 5800, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5804, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5805, 5000 },
        },
    },
    // KILL_TYPE_CENTAUR
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, 5016, 5900 },
            { 5, 0, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, 5016, 5900 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, 5901, 5900 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, 5901, 5900 },
            { 6, DAM_DEAD, -1, 0, 0, 5902, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_LOSE_TURN, 5016, 5903 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 5016, 5904 },
            { 4, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5904, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5905, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_LOSE_TURN, 5016, 5903 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 5016, 5904 },
            { 4, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 5904, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5905, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5901, 5000 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, 2, 0, 5901, 5900 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5900, 5000 },
            { 5, DAM_DEAD, -1, 0, 0, 5902, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5900, 5000 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5900, 5906 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5906, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5906, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_LOSE_TURN, -1, 0, 0, 5907, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5900, 5000 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5900, 5906 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, 0, 5906, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, 0, 5906, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_LOSE_TURN, -1, 0, 0, 5907, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 1, DAM_BLIND, 5001, 5908 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -1, DAM_BLIND, 5901, 5908 },
            { 6, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5909, 5000 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5910, 5000 },
            { 8, DAM_DEAD, -1, 0, 0, 5911, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 2, 0, -1, 0, 0, 5912, 5000 },
            { 2, 0, -1, 0, 0, 5912, 5000 },
            { 2, 0, -1, 0, 0, 5912, 5000 },
            { 2, 0, -1, 0, 0, 5912, 5000 },
            { 2, 0, -1, 0, 0, 5912, 5000 },
            { 2, 0, -1, 0, 0, 5912, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5901, 5000 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, 2, 0, 5901, 5900 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5900, 5000 },
            { 5, DAM_DEAD, -1, 0, 0, 5902, 5000 },
        },
    },
    // KILL_TYPE_ROBOT
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 6000, 5000 },
            { 4, 0, -1, 0, 0, 6000, 5000 },
            { 5, 0, -1, 0, 0, 6000, 5000 },
            { 5, DAM_KNOCKED_DOWN, -1, 0, 0, 6001, 5000 },
            { 6, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6002, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6003, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 6000, 6004 },
            { 3, 0, STAT_ENDURANCE, -3, DAM_CRIP_ARM_LEFT, 6000, 6004 },
            { 4, DAM_CRIP_ARM_LEFT, -1, 0, 0, 6004, 5000 },
            { 4, DAM_CRIP_ARM_LEFT, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 6004, 6005 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 6000, 6004 },
            { 3, 0, STAT_ENDURANCE, -3, DAM_CRIP_ARM_RIGHT, 6000, 6004 },
            { 4, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 6004, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 6004, 6005 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 6006, 5000 },
            { 4, 0, -1, 0, 0, 6007, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 6008, 5000 },
            { 6, DAM_BYPASS, -1, 0, 0, 6009, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6010, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 4, 0, -1, 0, 0, 6007, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 6000, 6004 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_CRIP_LEG_RIGHT, 6007, 6004 },
            { 4, DAM_CRIP_LEG_RIGHT, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 6004, 6011 },
            { 4, DAM_CRIP_LEG_RIGHT, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, 6004, 6012 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 4, 0, -1, 0, 0, 6007, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 6000, 6004 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_CRIP_LEG_LEFT, 6007, 6004 },
            { 4, DAM_CRIP_LEG_LEFT, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 6004, 6011 },
            { 4, DAM_CRIP_LEG_LEFT, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, 6004, 6012 },
        },
        // HIT_LOCATION_EYES
        {
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_BLIND, 6000, 6013 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_BLIND, 6000, 6013 },
            { 3, 0, STAT_ENDURANCE, -4, DAM_BLIND, 6000, 6013 },
            { 3, 0, STAT_ENDURANCE, -6, DAM_BLIND, 6000, 6013 },
            { 3, DAM_BLIND, -1, 0, 0, 6013, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, 0, STAT_ENDURANCE, -1, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, 6000, 6002 },
            { 3, 0, STAT_ENDURANCE, -4, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, 6000, 6002 },
            { 3, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, STAT_ENDURANCE, 0, 0, 6002, 6003 },
            { 3, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, STAT_ENDURANCE, -4, 0, 6002, 6003 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 6006, 5000 },
            { 4, 0, -1, 0, 0, 6007, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 6008, 5000 },
            { 6, DAM_BYPASS, -1, 0, 0, 6009, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6010, 5000 },
        },
    },
    // KILL_TYPE_DOG
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 5016, 6100 },
            { 4, 0, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN, 5016, 6100 },
            { 4, 0, STAT_ENDURANCE, -6, DAM_CRIP_ARM_LEFT | DAM_CRIP_ARM_RIGHT, 5016, 6101 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 6100, 6102 },
            { 4, DAM_DEAD, -1, 0, 0, 6103, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, -1, DAM_CRIP_LEG_LEFT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, -3, DAM_CRIP_LEG_LEFT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, -5, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, 5001, 6105 },
            { 3, DAM_CRIP_LEG_LEFT, STAT_AGILITY, -1, DAM_KNOCKED_DOWN, 6104, 6105 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, 0, 6105, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, -1, DAM_CRIP_LEG_RIGHT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, -3, DAM_CRIP_LEG_RIGHT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, -5, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, 5001, 6105 },
            { 3, DAM_CRIP_LEG_RIGHT, STAT_AGILITY, -1, DAM_KNOCKED_DOWN, 6104, 6105 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, 0, 6105, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, -1, DAM_KNOCKED_DOWN, 5001, 6100 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_AGILITY, -3, DAM_KNOCKED_DOWN, 5016, 6100 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 6100, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6103, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, STAT_ENDURANCE, 1, DAM_CRIP_LEG_RIGHT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_CRIP_LEG_RIGHT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, -4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, 5001, 6105 },
            { 3, DAM_CRIP_LEG_RIGHT, STAT_AGILITY, -1, DAM_KNOCKED_DOWN, 6104, 6105 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, 0, 6105, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, STAT_ENDURANCE, 1, DAM_CRIP_LEG_LEFT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_CRIP_LEG_LEFT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, -4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, 5001, 6105 },
            { 3, DAM_CRIP_LEG_LEFT, STAT_AGILITY, -1, DAM_KNOCKED_DOWN, 6104, 6105 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, 0, 6105, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5018, 5000 },
            { 6, DAM_BYPASS, -1, 0, 0, 5018, 5000 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, 3, DAM_BLIND, 5018, 6106 },
            { 8, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_BLIND, 5018, 6106 },
            { 8, DAM_DEAD, -1, 0, 0, 6107, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, -2, DAM_KNOCKED_DOWN, 5001, 6100 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_AGILITY, -5, DAM_KNOCKED_DOWN, 5016, 6100 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 6100, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6103, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, -1, DAM_KNOCKED_DOWN, 5001, 6100 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_AGILITY, -3, DAM_KNOCKED_DOWN, 5016, 6100 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 6100, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6103, 5000 },
        },
    },
    // KILL_TYPE_MANTIS
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 5001, 6200 },
            { 5, 0, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN, 5016, 6200 },
            { 5, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -1, DAM_KNOCKED_OUT, 6200, 6201 },
            { 6, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 6200, 6201 },
            { 6, DAM_DEAD, -1, 0, 0, 6202, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 5001, 6203 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 5001, 6203 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_CRIP_ARM_LEFT, 5001, 6203 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_CRIP_ARM_LEFT, 5016, 6203 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_CRIP_ARM_LEFT, 5016, 6203 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_LOSE_TURN, -1, 0, 0, 6204, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 5001, 6203 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 5001, 6203 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_CRIP_ARM_RIGHT, 5001, 6203 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_CRIP_ARM_RIGHT, 5016, 6203 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_CRIP_ARM_RIGHT, 5016, 6203 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_LOSE_TURN, -1, 0, 0, 6204, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 1000, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_BYPASS, 5001, 6205 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_BYPASS, 5001, 6205 },
            { 4, 0, STAT_ENDURANCE, -2, DAM_BYPASS, 5016, 6205 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_BYPASS, 5016, 6205 },
            { 6, DAM_DEAD, -1, 0, 0, 6206, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 6201 },
            { 3, 0, STAT_AGILITY, -2, DAM_KNOCKED_DOWN, 5001, 6201 },
            { 4, 0, STAT_AGILITY, -4, DAM_KNOCKED_DOWN, 5001, 6201 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 6201, 6203 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_RIGHT, 6201, 6203 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, 0, 6207, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 6201 },
            { 3, 0, STAT_AGILITY, -3, DAM_KNOCKED_DOWN, 5001, 6201 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -2, DAM_CRIP_LEG_LEFT, 6201, 6208 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -2, DAM_CRIP_LEG_LEFT, 6201, 6208 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -5, DAM_CRIP_LEG_LEFT, 6201, 6208 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, 0, 6208, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_LOSE_TURN, 6205, 6209 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_LOSE_TURN, 6205, 6209 },
            { 6, DAM_BYPASS | DAM_LOSE_TURN, STAT_ENDURANCE, -3, DAM_BLIND, 6209, 6210 },
            { 8, DAM_KNOCKED_DOWN | DAM_BYPASS | DAM_LOSE_TURN, STAT_ENDURANCE, -3, DAM_BLIND, 6209, 6210 },
            { 8, DAM_DEAD, -1, 0, 0, 6202, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 6205, 5000 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6209, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 1000, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_BYPASS, 5001, 6205 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_BYPASS, 5001, 6205 },
            { 4, 0, STAT_ENDURANCE, -2, DAM_BYPASS, 5016, 6205 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_BYPASS, 5016, 6205 },
            { 6, DAM_DEAD, -1, 0, 0, 6206, 5000 },
        },
    },
    // KILL_TYPE_DEATH_CLAW
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 5016, 5023 },
            { 5, 0, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN, 5016, 5023 },
            { 5, 0, STAT_ENDURANCE, -5, DAM_KNOCKED_DOWN, 5016, 5023 },
            { 6, 0, STAT_ENDURANCE, -4, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, 5016, 5004 },
            { 6, 0, STAT_ENDURANCE, -5, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, 5016, 5004 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 5001, 5011 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_CRIP_ARM_LEFT, 5001, 5011 },
            { 3, 0, STAT_ENDURANCE, -4, DAM_CRIP_ARM_LEFT, 5001, 5011 },
            { 3, 0, STAT_ENDURANCE, -6, DAM_CRIP_ARM_LEFT, 5001, 5011 },
            { 3, 0, STAT_ENDURANCE, -8, DAM_CRIP_ARM_LEFT, 5001, 5011 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 5001, 5014 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_CRIP_ARM_RIGHT, 5001, 5014 },
            { 3, 0, STAT_ENDURANCE, -4, DAM_CRIP_ARM_RIGHT, 5001, 5014 },
            { 3, 0, STAT_ENDURANCE, -6, DAM_CRIP_ARM_RIGHT, 5001, 5014 },
            { 3, 0, STAT_ENDURANCE, -8, DAM_CRIP_ARM_RIGHT, 5001, 5014 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, -1, DAM_BYPASS, 5001, 6300 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_ENDURANCE, -1, DAM_BYPASS, 5016, 6300 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5004, 5000 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5005, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 5004 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5001, 5004 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -2, DAM_CRIP_LEG_RIGHT, 5001, 5004 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -4, DAM_CRIP_LEG_RIGHT, 5016, 5022 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -5, DAM_CRIP_LEG_RIGHT, 5023, 5024 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -6, DAM_CRIP_LEG_RIGHT, 5023, 5024 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 5004 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5001, 5004 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -2, DAM_CRIP_LEG_RIGHT, 5001, 5004 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -4, DAM_CRIP_LEG_RIGHT, 5016, 5022 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -5, DAM_CRIP_LEG_RIGHT, 5023, 5024 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -6, DAM_CRIP_LEG_RIGHT, 5023, 5024 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, -3, DAM_LOSE_TURN, 5001, 6301 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -6, DAM_LOSE_TURN, 6300, 6301 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -2, DAM_BLIND, 6301, 6302 },
            { 8, DAM_BLIND | DAM_BYPASS, -1, 0, 0, 6302, 5000 },
            { 8, DAM_BLIND | DAM_BYPASS, -1, 0, 0, 6302, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 5, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5016, 5004 },
            { 5, 0, STAT_AGILITY, -3, DAM_KNOCKED_DOWN, 5016, 5004 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, -1, DAM_BYPASS, 5001, 6300 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_ENDURANCE, -1, DAM_BYPASS, 5016, 6300 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5004, 5000 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5005, 5000 },
        },
    },
    // KILL_TYPE_PLANT
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 6405, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 6400, 5000 },
            { 5, 0, -1, 0, 0, 6401, 5000 },
            { 5, DAM_BYPASS, -1, 0, 0, 6402, 5000 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_LOSE_TURN, 6402, 6403 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -6, DAM_LOSE_TURN, 6402, 6403 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 6400, 5000 },
            { 4, 0, -1, 0, 0, 6401, 5000 },
            { 4, 0, -1, 0, 0, 6401, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 6402, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 6405, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 6400, 5000 },
            { 5, 0, -1, 0, 0, 6401, 5000 },
            { 5, DAM_BYPASS, -1, 0, 0, 6402, 5000 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -4, DAM_BLIND, 6402, 6406 },
            { 6, DAM_BLIND | DAM_BYPASS, -1, 0, 0, 6406, 6404 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 6402, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 6402, 5000 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_LOSE_TURN, 6402, 6403 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -6, DAM_LOSE_TURN, 6402, 6403 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 6400, 5000 },
            { 4, 0, -1, 0, 0, 6401, 5000 },
            { 4, 0, -1, 0, 0, 6401, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 6402, 5000 },
        },
    },
};

// Player's criticals effects.
//
// 0x5179B0
static CriticalHitDescription pc_crit_succ_eff[HIT_LOCATION_COUNT][CRTICIAL_EFFECT_COUNT] = {
    {
        { 3, 0, -1, 0, 0, 6500, 5000 },
        { 3, DAM_BYPASS, STAT_ENDURANCE, 3, DAM_KNOCKED_DOWN, 6501, 6503 },
        { 3, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 6501, 6503 },
        { 3, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, 2, DAM_KNOCKED_OUT, 6503, 6502 },
        { 3, DAM_KNOCKED_OUT | DAM_BYPASS, STAT_LUCK, 2, DAM_BLIND, 6502, 6504 },
        { 6, DAM_BYPASS, STAT_ENDURANCE, -2, DAM_DEAD, 6501, 6505 },
    },
    {
        { 2, 0, -1, 0, 0, 6506, 5000 },
        { 2, DAM_LOSE_TURN, -1, 0, 0, 6507, 5000 },
        { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 6508, 6509 },
        { 3, DAM_BYPASS, -1, 0, 0, 6501, 5000 },
        { 3, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 6510, 5000 },
        { 3, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 6510, 5000 },
    },
    {
        { 2, 0, -1, 0, 0, 6506, 5000 },
        { 2, DAM_LOSE_TURN, -1, 0, 0, 6507, 5000 },
        { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 6508, 6509 },
        { 3, DAM_BYPASS, -1, 0, 0, 6501, 5000 },
        { 3, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 6511, 5000 },
        { 3, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 6511, 5000 },
    },
    {
        { 3, 0, -1, 0, 0, 6512, 5000 },
        { 3, 0, -1, 0, 0, 6512, 5000 },
        { 3, DAM_BYPASS, -1, 0, 0, 6508, 5000 },
        { 3, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6503, 5000 },
        { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6503, 5000 },
        { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_LUCK, 2, DAM_DEAD, 6503, 6513 },
    },
    {
        { 3, 0, -1, 0, 0, 6512, 5000 },
        { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 6514, 5000 },
        { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 6514, 6515 },
        { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 6516, 5000 },
        { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 6516, 5000 },
        { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 6517, 5000 },
    },
    {
        { 3, 0, -1, 0, 0, 6512, 5000 },
        { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 6514, 5000 },
        { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 6514, 6515 },
        { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 6516, 5000 },
        { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 6516, 5000 },
        { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 6517, 5000 },
    },
    {
        { 3, 0, -1, 0, 0, 6518, 5000 },
        { 3, 0, STAT_LUCK, 3, DAM_BLIND, 6518, 6519 },
        { 3, DAM_BYPASS, STAT_LUCK, 3, DAM_BLIND, 6501, 6519 },
        { 4, DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 6520, 5000 },
        { 4, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 6521, 5000 },
        { 6, DAM_DEAD, -1, 0, 0, 6522, 5000 },
    },
    {
        { 3, 0, -1, 0, 0, 6523, 5000 },
        { 3, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 6523, 6524 },
        { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 6524, 5000 },
        { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 4, DAM_KNOCKED_OUT, 6524, 6525 },
        { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 2, DAM_KNOCKED_OUT, 6524, 6525 },
        { 4, DAM_KNOCKED_OUT, -1, 0, 0, 6526, 5000 },
    },
    {
        { 3, 0, -1, 0, 0, 6512, 5000 },
        { 3, 0, -1, 0, 0, 6512, 5000 },
        { 3, DAM_BYPASS, -1, 0, 0, 6508, 5000 },
        { 3, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6503, 5000 },
        { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6503, 5000 },
        { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_LUCK, 2, DAM_DEAD, 6503, 6513 },
    },
};

// 0x504B38
static int combat_end_due_to_load = 0;

// 0x517F9C
static bool combat_cleanup_enabled = false;

// Provides effects caused by failing weapons.
//
// 0x504B3C
int cf_table[WEAPON_CRITICAL_FAILURE_TYPE_COUNT][WEAPON_CRITICAL_FAILURE_EFFECT_COUNT] = {
    { 0, DAM_LOSE_TURN, DAM_LOSE_TURN, DAM_HURT_SELF | DAM_KNOCKED_DOWN, DAM_CRIP_RANDOM },
    { 0, DAM_LOSE_TURN, DAM_DROP, DAM_RANDOM_HIT, DAM_HIT_SELF },
    { 0, DAM_LOSE_AMMO, DAM_DROP, DAM_RANDOM_HIT, DAM_DESTROY },
    { DAM_LOSE_TURN, DAM_LOSE_TURN | DAM_LOSE_AMMO, DAM_DROP | DAM_LOSE_TURN, DAM_RANDOM_HIT, DAM_EXPLODE | DAM_LOSE_TURN },
    { DAM_DUD, DAM_DROP, DAM_DROP | DAM_HURT_SELF, DAM_RANDOM_HIT, DAM_EXPLODE },
    { DAM_LOSE_TURN, DAM_DUD, DAM_DESTROY, DAM_RANDOM_HIT, DAM_EXPLODE | DAM_LOSE_TURN | DAM_KNOCKED_DOWN },
    { 0, DAM_LOSE_TURN, DAM_RANDOM_HIT, DAM_DESTROY, DAM_EXPLODE | DAM_LOSE_TURN | DAM_ON_FIRE },
};

// 0x504BC8
static int call_ty[4] = {
    122,
    188,
    252,
    316,
};

// 0x504BD8
static int hit_loc_left[4] = {
    HIT_LOCATION_HEAD,
    HIT_LOCATION_EYES,
    HIT_LOCATION_RIGHT_ARM,
    HIT_LOCATION_RIGHT_LEG,
};

// 0x504BE8
static int hit_loc_right[4] = {
    HIT_LOCATION_TORSO,
    HIT_LOCATION_GROIN,
    HIT_LOCATION_LEFT_ARM,
    HIT_LOCATION_LEFT_LEG,
};

// 0x56BBB0
static Attack main_ctd;

// combat.msg
//
// 0x56BC68
MessageList combat_message_file;

// 0x56BC70
static Object* call_target;

// 0x56BC74
static int call_win;

// 0x56BC78
static int combat_elev;

// 0x56BC88
static int list_total;

// 0x56BC7C
static Object* combat_ending_guy;

// 0x56BC98
static int list_noncom;

// 0x56BC80
Object* combat_turn_obj;

// 0x56BC84
static int combat_highlight;

// 0x56BC8C
static Object** combat_list;

// 0x56BC90
static int list_com;

// Experience received for killing critters during current combat.
//
// 0x56BC94
int combat_exps;

// 0x56BC9C
int combat_free_move;

// 0x41F810
int combat_init()
{
    char path[COMPAT_MAX_PATH];

    combat_turn_running = 0;
    combat_list = NULL;
    list_com = 0;
    list_noncom = 0;
    list_total = 0;
    gcsd = NULL;
    combat_call_display = 0;
    combat_state = COMBAT_STATE_0x02;
    obj_dude->data.critter.combat.ap = stat_level(obj_dude, STAT_MAXIMUM_ACTION_POINTS);
    combat_free_move = 0;
    combat_ending_guy = NULL;
    combat_end_due_to_load = 0;

    combat_cleanup_enabled = 0;

    if (!message_init(&combat_message_file)) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s%s", msg_path, "combat.msg");

    if (!message_load(&combat_message_file, path)) {
        return -1;
    }

    return 0;
}

// 0x41F8E8
void combat_reset()
{
    combat_turn_running = 0;
    combat_list = NULL;
    list_com = 0;
    list_noncom = 0;
    list_total = 0;
    gcsd = NULL;
    combat_call_display = 0;
    combat_state = COMBAT_STATE_0x02;
    obj_dude->data.critter.combat.ap = stat_level(obj_dude, STAT_MAXIMUM_ACTION_POINTS);
    combat_free_move = 0;
    combat_ending_guy = NULL;
}

// 0x41F950
void combat_exit()
{
    message_exit(&combat_message_file);
}

// 0x41F960
int find_cid(int start, int cid, Object** critterList, int critterListLength)
{
    int index;

    for (index = start; index < critterListLength; index++) {
        if (critterList[index]->cid == cid) {
            break;
        }
    }

    return index;
}

// 0x41F988
int combat_load(DB_FILE* stream)
{
    Object* obj;
    int cid;
    int i;
    int j;

    if (db_freadUInt32(stream, &combat_state) == -1) return -1;

    if (!isInCombat()) {
        obj = obj_find_first();
        while (obj != NULL) {
            if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
                if (obj->data.critter.combat.whoHitMeCid == -1) {
                    obj->data.critter.combat.whoHitMe = NULL;
                }
            }
            obj = obj_find_next();
        }
        return 0;
    }

    if (db_freadInt32(stream, &combat_turn_running) == -1) return -1;
    if (db_freadInt32(stream, &combat_free_move) == -1) return -1;
    if (db_freadInt32(stream, &combat_exps) == -1) return -1;
    if (db_freadInt32(stream, &list_com) == -1) return -1;
    if (db_freadInt32(stream, &list_noncom) == -1) return -1;
    if (db_freadInt32(stream, &list_total) == -1) return -1;

    if (obj_create_list(-1, map_elevation, OBJ_TYPE_CRITTER, &combat_list) != list_total) {
        obj_delete_list(combat_list);
        return -1;
    }

    if (db_freadInt32(stream, &cid) == -1) return -1;

    obj_dude->cid = cid;

    for (i = 0; i < list_total; i++) {
        if (combat_list[i]->data.critter.combat.whoHitMeCid == -1) {
            combat_list[i]->data.critter.combat.whoHitMe = NULL;
        } else {
            // NOTE: Uninline.
            j = find_cid(0, combat_list[i]->data.critter.combat.whoHitMeCid, combat_list, list_total);
            if (j == list_total) {
                combat_list[i]->data.critter.combat.whoHitMe = NULL;
            } else {
                combat_list[i]->data.critter.combat.whoHitMe = combat_list[j];
            }
        }
    }

    for (i = 0; i < list_total; i++) {
        if (db_freadInt32(stream, &cid) == -1) return -1;

        // NOTE: Uninline.
        j = find_cid(i, cid, combat_list, list_total);

        if (j == list_total) {
            return -1;
        }

        obj = combat_list[i];
        combat_list[i] = combat_list[j];
        combat_list[j] = obj;
    }

    for (i = 0; i < list_total; i++) {
        combat_list[i]->cid = i;
    }

    combat_begin_extra(obj_dude);

    return 0;
}

// 0x41FC54
int combat_save(DB_FILE* stream)
{
    int index;

    if (db_fwriteUInt32(stream, combat_state) == -1) return -1;

    if (!isInCombat()) return 0;

    if (db_fwriteInt32(stream, combat_turn_running) == -1) return -1;
    if (db_fwriteInt32(stream, combat_free_move) == -1) return -1;
    if (db_fwriteInt32(stream, combat_exps) == -1) return -1;
    if (db_fwriteInt32(stream, list_com) == -1) return -1;
    if (db_fwriteInt32(stream, list_noncom) == -1) return -1;
    if (db_fwriteInt32(stream, list_total) == -1) return -1;
    if (db_fwriteInt32(stream, obj_dude->cid) == -1) return -1;

    for (index = 0; index < list_total; index++) {
        if (db_fwriteInt32(stream, combat_list[index]->cid) == -1) return -1;
    }

    return 0;
}

// 0x41FD48
Object* combat_whose_turn()
{
    if (isInCombat()) {
        return combat_turn_obj;
    } else {
        return NULL;
    }
}

// 0x41FD5C
void combat_data_init(Object* obj)
{
    obj->data.critter.combat.damageLastTurn = 0;
    obj->data.critter.combat.results = 0;
}

// 0x41FD70
static void combat_begin(Object* a1)
{
    int index;

    anim_stop();
    remove_bk_process(dude_fidget);
    combat_elev = map_elevation;

    if (!isInCombat()) {
        combat_exps = 0;
        combat_list = NULL;
        list_total = obj_create_list(-1, combat_elev, OBJ_TYPE_CRITTER, &combat_list);
        list_noncom = list_total;
        list_com = 0;

        for (index = 0; index < list_total; index++) {
            Object* critter = combat_list[index];
            CritterCombatData* combatData = &(critter->data.critter.combat);
            combatData->maneuver &= CRITTER_MANEUVER_ENGAGING;
            combatData->damageLastTurn = 0;
            combatData->whoHitMe = NULL;
            combatData->ap = 0;
            critter->cid = index;
        }

        combat_state |= COMBAT_STATE_0x01;

        tile_refresh_display();
        game_ui_disable(0);
        gmouse_set_cursor(MOUSE_CURSOR_WAIT_WATCH);
        combat_ending_guy = NULL;
        combat_begin_extra(a1);
        intface_end_window_open(true);
        gmouse_enable_scrolling();
    }
}

// 0x41FE6C
static void combat_begin_extra(Object* a1)
{
    int index;
    int outline_type;

    for (index = 0; index < list_total; index++) {
        outline_type = OUTLINE_TYPE_HOSTILE;
        if (perk_level(PERK_FRIENDLY_FOE)) {
            if (combat_list[index]->data.critter.combat.team == obj_dude->data.critter.combat.team) {
                outline_type = OUTLINE_TYPE_FRIENDLY;
            }
        }

        obj_outline_object(combat_list[index], outline_type, NULL);
        obj_turn_off_outline(combat_list[index], NULL);
    }

    combat_ctd_init(&main_ctd, a1, NULL, HIT_MODE_PUNCH, HIT_LOCATION_TORSO);

    combat_turn_obj = a1;

    combat_ai_begin(list_total, combat_list);

    combat_highlight = 2;
    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TARGET_HIGHLIGHT_KEY, &combat_highlight);
}

// 0x41FF48
static void combat_over()
{
    int index;

    add_bk_process(dude_fidget);

    for (index = 0; index < list_noncom + list_com; index++) {
        Object* critter = combat_list[index];
        critter->data.critter.combat.damageLastTurn = 0;
        critter->data.critter.combat.maneuver = CRITTER_MANEUVER_NONE;
    }

    for (index = 0; index < list_total; index++) {
        Object* critter = combat_list[index];
        critter->data.critter.combat.ap = 0;
        obj_remove_outline(critter, NULL);
        critter->data.critter.combat.whoHitMe = NULL;
    }

    tile_refresh_display();

    intface_update_items(true);

    obj_dude->data.critter.combat.ap = stat_level(obj_dude, STAT_MAXIMUM_ACTION_POINTS);

    intface_update_move_points(0);

    if (game_user_wants_to_quit == 0) {
        combat_give_exps(combat_exps);
    }

    combat_exps = 0;

    combat_state &= ~COMBAT_STATE_0x01;
    combat_state |= COMBAT_STATE_0x02;

    if (list_total != 0) {
        obj_delete_list(combat_list);
    }

    list_total = 0;

    combat_ai_over();
    game_ui_enable();
    gmouse_3d_set_mode(GAME_MOUSE_MODE_MOVE);
    intface_update_ac(true);

    if (critter_is_prone(obj_dude)) {
        if (!critter_is_dead(obj_dude)) {
            if (combat_ending_guy == NULL) {
                queue_remove_this(obj_dude, EVENT_TYPE_KNOCKOUT);
                critter_wake_up(obj_dude, NULL);
            }
        }
    }
}

// 0x4200A8
void combat_over_from_load()
{
    combat_over();
    combat_state = 0;
    combat_end_due_to_load = 1;
}

// Give exp for destroying critter.
//
// 0x4200C8
void combat_give_exps(int exp_points)
{
    MessageListItem format;
    MessageListItem prefix;
    int current_hp;
    int max_hp;
    char text[132];

    if (exp_points <= 0) {
        return;
    }

    if (critter_is_dead(obj_dude)) {
        return;
    }

    stat_pc_add_experience(exp_points);

    format.num = 621; // %s you earn %d exp. points.
    if (!message_search(&proto_main_msg_file, &format)) {
        return;
    }

    prefix.num = roll_random(0, 3) + 622; // generate prefix for message

    current_hp = stat_level(obj_dude, STAT_CURRENT_HIT_POINTS);
    max_hp = stat_level(obj_dude, STAT_MAXIMUM_HIT_POINTS);
    if (current_hp == max_hp && roll_random(0, 100) > 65) {
        prefix.num = 626; // Best possible prefix: For destroying your enemies without taking a scratch,
    }

    if (!message_search(&proto_main_msg_file, &prefix)) {
        return;
    }

    snprintf(text, sizeof(text), format.text, prefix.text, exp_points);
    display_print(text);
}

// 0x4201A0
static void combat_add_noncoms()
{
    int index;

    for (index = list_com; index < list_com + list_noncom; index++) {
        Object* obj = combat_list[index];
        if (combatai_want_to_join(obj)) {
            obj->data.critter.combat.maneuver = CRITTER_MANEUVER_NONE;

            Object** objectPtr1 = &(combat_list[index]);
            Object** objectPtr2 = &(combat_list[list_com]);
            Object* t = *objectPtr1;
            *objectPtr1 = *objectPtr2;
            *objectPtr2 = t;

            list_com += 1;
            list_noncom -= 1;

            if (obj != obj_dude) {
                combat_turn(obj, false);
            }
        }
    }
}

// 0x420234
int combat_in_range(Object* critter)
{
    int perception;
    int index;

    perception = stat_level(critter, STAT_PERCEPTION);

    for (index = 0; index < list_com; index++) {
        if (obj_dist(combat_list[index], critter) <= perception) {
            return 1;
        }
    }

    return 0;
}

// Compares critters by sequence.
//
// 0x420288
static int compare_faster(const void* a1, const void* a2)
{
    Object* v1 = *(Object**)a1;
    Object* v2 = *(Object**)a2;

    int sequence1 = stat_level(v1, STAT_SEQUENCE);
    int sequence2 = stat_level(v2, STAT_SEQUENCE);
    if (sequence1 > sequence2) {
        return -1;
    } else if (sequence1 < sequence2) {
        return 1;
    }

    int luck1 = stat_level(v1, STAT_LUCK);
    int luck2 = stat_level(v2, STAT_LUCK);
    if (luck1 > luck2) {
        return -1;
    } else if (luck1 < luck2) {
        return 1;
    }

    return 0;
}

// 0x4202FC
static void combat_sequence_init(Object* a1, Object* a2)
{
    int next = 0;
    if (a1 != NULL) {
        for (int index = 0; index < list_total; index++) {
            Object* obj = combat_list[index];
            if (obj == a1) {
                Object* temp = combat_list[next];
                combat_list[index] = temp;
                combat_list[next] = obj;
                next += 1;
                break;
            }
        }
    }

    if (a2 != NULL) {
        for (int index = 0; index < list_total; index++) {
            Object* obj = combat_list[index];
            if (obj == a2) {
                Object* temp = combat_list[next];
                combat_list[index] = temp;
                combat_list[next] = obj;
                next += 1;
                break;
            }
        }
    }

    if (a1 != obj_dude && a2 != obj_dude) {
        for (int index = 0; index < list_total; index++) {
            Object* obj = combat_list[index];
            if (obj == obj_dude) {
                Object* temp = combat_list[next];
                combat_list[index] = temp;
                combat_list[next] = obj;
                next += 1;
                break;
            }
        }
    }

    list_com = next;
    list_noncom -= next;

    if (a1 != NULL) {
        critter_set_who_hit_me(a1, a2);
    }

    if (a2 != NULL) {
        critter_set_who_hit_me(a2, a1);
    }
}

// 0x420440
static void combat_sequence()
{
    combat_add_noncoms();

    int count = list_com;

    for (int index = 0; index < count; index++) {
        Object* critter = combat_list[index];
        if ((critter->data.critter.combat.results & DAM_DEAD) != 0) {
            combat_list[index] = combat_list[count - 1];
            combat_list[count - 1] = critter;

            combat_list[count - 1] = combat_list[list_noncom + count - 1];
            combat_list[list_noncom + count - 1] = critter;

            index -= 1;
            count -= 1;
        }
    }

    for (int index = 0; index < count; index++) {
        Object* critter = combat_list[index];
        if (critter != obj_dude) {
            if ((critter->data.critter.combat.results & DAM_KNOCKED_OUT) != 0
                || critter->data.critter.combat.maneuver == CRITTER_MANEUVER_DISENGAGING) {
                critter->data.critter.combat.maneuver &= ~CRITTER_MANEUVER_ENGAGING;
                list_noncom += 1;

                combat_list[index] = combat_list[count - 1];
                combat_list[count - 1] = critter;

                count -= 1;
                index -= 1;
            }
        }
    }

    if (count != 0) {
        list_com = count;
        qsort(combat_list, count, sizeof(*combat_list), compare_faster);
        count = list_com;
    }

    list_com = count;

    inc_game_time_in_seconds(5);
}

// 0x420554
void combat_end()
{
    if (combat_elev == obj_dude->elevation) {
        MessageListItem messageListItem;
        int dudeTeam = obj_dude->data.critter.combat.team;

        for (int index = 0; index < list_com; index++) {
            Object* critter = combat_list[index];
            if (critter != obj_dude) {
                int critterTeam = critter->data.critter.combat.team;
                Object* critterWhoHitMe = critter->data.critter.combat.whoHitMe;
                if (critterTeam != dudeTeam || (critterWhoHitMe != NULL && critterWhoHitMe->data.critter.combat.team == critterTeam)) {
                    if (!combatai_want_to_stop(critter)) {
                        messageListItem.num = 103;
                        if (message_search(&combat_message_file, &messageListItem)) {
                            display_print(messageListItem.text);
                        }
                        return;
                    }
                }
            }
        }

        for (int index = list_com; index < list_com + list_noncom; index++) {
            Object* critter = combat_list[index];
            if (critter != obj_dude) {
                int critterTeam = critter->data.critter.combat.team;
                Object* critterWhoHitMe = critter->data.critter.combat.whoHitMe;
                if (critterTeam != dudeTeam || (critterWhoHitMe != NULL && critterWhoHitMe->data.critter.combat.team == critterTeam)) {
                    if (combatai_want_to_join(critter)) {
                        messageListItem.num = 103;
                        if (message_search(&combat_message_file, &messageListItem)) {
                            display_print(messageListItem.text);
                        }
                        return;
                    }
                }
            }
        }
    }

    combat_state |= COMBAT_STATE_0x08;
}

// 0x420698
void combat_turn_run()
{
    while (combat_turn_running > 0) {
        sharedFpsLimiter.mark();

        process_bk();

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// 0x4206B0
static int combat_input()
{
    int input;

    while ((combat_state & COMBAT_STATE_0x02) != 0) {
        sharedFpsLimiter.mark();

        if ((combat_state & COMBAT_STATE_0x08) != 0) {
            break;
        }

        if ((obj_dude->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD | DAM_LOSE_TURN)) != 0) {
            break;
        }

        if (obj_dude->data.critter.combat.ap <= 0) {
            break;
        }

        if (game_user_wants_to_quit != 0) {
            break;
        }

        if (combat_end_due_to_load != 0) {
            break;
        }

        input = get_input();

        if (input == KEY_SPACE) {
            break;
        }

        if (input == KEY_RETURN) {
            combat_end();
        } else {
            scripts_check_state_in_combat();
            game_handle_input(input, true);
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    int old_user_wants_to_quit = game_user_wants_to_quit;
    if (game_user_wants_to_quit == 1) {
        game_user_wants_to_quit = 0;
    }

    if ((combat_state & COMBAT_STATE_0x08) != 0) {
        combat_state &= ~COMBAT_STATE_0x08;
        return -1;
    }

    if (game_user_wants_to_quit != 0 || old_user_wants_to_quit != 0 || combat_end_due_to_load != 0) {
        return -1;
    }

    scripts_check_state_in_combat();

    return 0;
}

// 0x420790
void combat_end_turn()
{
    combat_state &= ~COMBAT_STATE_0x02;
}

// 0x420798
static int combat_turn(Object* a1, bool a2)
{
    int action_points;
    bool script_override = false;
    Script* script;

    combat_turn_obj = a1;

    combat_ctd_init(&main_ctd, a1, NULL, HIT_MODE_PUNCH, HIT_LOCATION_TORSO);

    if ((a1->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD | DAM_LOSE_TURN)) != 0) {
        a1->data.critter.combat.results &= ~DAM_LOSE_TURN;
    } else {
        if (!a2) {
            action_points = stat_level(a1, STAT_MAXIMUM_ACTION_POINTS);
            if (gcsd != NULL) {
                action_points += gcsd->actionPointsBonus;
            }
            a1->data.critter.combat.ap = action_points;
        }

        if (a1 == obj_dude) {
            kb_clear();
            intface_update_ac(true);
            combat_free_move = 2 * perk_level(PERK_BONUS_MOVE);
            intface_update_move_points(obj_dude->data.critter.combat.ap);
        } else {
            soundUpdate();
        }

        if (a1->sid != -1) {
            scr_set_objs(a1->sid, NULL, NULL);
            scr_set_ext_param(a1->sid, 4);
            exec_script_proc(a1->sid, SCRIPT_PROC_COMBAT);

            if (scr_ptr(a1->sid, &script) != -1) {
                script_override = script->scriptOverrides;
            }

            if (game_user_wants_to_quit == 1) {
                return -1;
            }
        }

        if (!script_override) {
            if (!a2) {
                if (critter_is_prone(a1)) {
                    combat_standup(a1);
                }
            }

            if (a1 == obj_dude) {
                game_ui_enable();
                gmouse_3d_refresh();

                if (gcsd != NULL) {
                    combat_attack_this(gcsd->defender);
                }

                if (!a2) {
                    combat_state |= 0x02;
                }

                intface_end_buttons_enable();

                if (combat_highlight != 0) {
                    combat_outline_on();
                }

                if (combat_input() == -1) {
                    game_ui_disable(1);
                    gmouse_set_cursor(MOUSE_CURSOR_WAIT_WATCH);
                    a1->data.critter.combat.damageLastTurn = 0;
                    intface_end_buttons_disable();
                    combat_outline_off();
                    intface_update_move_points(-1);
                    intface_update_ac(true);
                    combat_free_move = 0;
                    return -1;
                }
            } else {
                Rect rect;
                if (obj_turn_on_outline(a1, &rect) == 0) {
                    tile_refresh_rect(&rect, a1->elevation);
                }

                combat_ai(a1, gcsd != NULL ? gcsd->defender : NULL);
            }
        }

        combat_turn_run();

        if (a1 == obj_dude) {
            game_ui_disable(1);
            gmouse_set_cursor(MOUSE_CURSOR_WAIT_WATCH);
            intface_end_buttons_disable();
            combat_outline_off();
            intface_update_move_points(-1);
            combat_turn_obj = NULL;
            intface_update_ac(true);
            combat_turn_obj = obj_dude;
        } else {
            Rect rect;
            if (obj_turn_off_outline(a1, &rect) == 0) {
                tile_refresh_rect(&rect, a1->elevation);
            }
        }
    }

    a1->data.critter.combat.damageLastTurn = 0;

    if ((obj_dude->data.critter.combat.results & DAM_DEAD) != 0) {
        return -1;
    }

    if (a1 == obj_dude && combat_elev != obj_dude->elevation) {
        return -1;
    }

    combat_free_move = 0;

    return 0;
}

// 0x420A54
static bool combat_should_end()
{
    if (list_com <= 1) {
        return true;
    }

    int index;
    for (index = 0; index < list_com; index++) {
        if (combat_list[index] == obj_dude) {
            break;
        }
    }

    if (index == list_com) {
        return true;
    }

    int team = obj_dude->data.critter.combat.team;

    for (index = 0; index < list_com; index++) {
        Object* critter = combat_list[index];
        if (critter->data.critter.combat.team != team) {
            break;
        }

        Object* critterWhoHitMe = critter->data.critter.combat.whoHitMe;
        if (critterWhoHitMe != NULL && critterWhoHitMe->data.critter.combat.team == team) {
            break;
        }
    }

    if (index == list_com) {
        return true;
    }

    return false;
}

// 0x420B20
void combat(STRUCT_664980* attack)
{
    if (attack == NULL
        || (attack->attacker == NULL || attack->attacker->elevation == map_elevation)
        || (attack->defender == NULL || attack->defender->elevation == map_elevation)) {
        int v3 = combat_state & 0x01;

        combat_begin(NULL);

        int v6;

        if (v3 != 0) {
            if (combat_turn(obj_dude, true) == -1) {
                v6 = -1;
            } else {
                int index;
                for (index = 0; index < list_com; index++) {
                    if (combat_list[index] == obj_dude) {
                        break;
                    }
                }
                v6 = index + 1;
            }
            gcsd = NULL;
        } else {
            if (attack != NULL) {
                combat_sequence_init(attack->attacker, attack->defender);
            } else {
                combat_sequence_init(NULL, NULL);
            }

            gcsd = attack;
            v6 = 0;
        }

        do {
            if (v6 == -1) {
                break;
            }

            for (; v6 < list_com; v6++) {
                if (combat_turn(combat_list[v6], false) == -1) {
                    break;
                }

                if (combat_ending_guy != NULL) {
                    break;
                }

                gcsd = NULL;
            }

            if (v6 < list_com) {
                break;
            }

            combat_sequence();
            v6 = 0;
        } while (!combat_should_end());

        if (combat_end_due_to_load) {
            game_ui_enable();
            gmouse_3d_set_mode(GAME_MOUSE_MODE_MOVE);
        } else {
            gmouse_disable_scrolling();
            intface_end_window_close(true);
            gmouse_enable_scrolling();
            combat_over();
            scr_exec_map_update_scripts();
        }

        combat_end_due_to_load = 0;

        if (game_user_wants_to_quit == 1) {
            game_user_wants_to_quit = 0;
        }
    }
}

// 0x420C84
void combat_ctd_init(Attack* attack, Object* attacker, Object* defender, int hitMode, int hitLocation)
{
    attack->attacker = attacker;
    attack->hitMode = hitMode;
    attack->weapon = item_hit_with(attacker, hitMode);
    attack->attackHitLocation = HIT_LOCATION_TORSO;
    attack->attackerDamage = 0;
    attack->attackerFlags = 0;
    attack->ammoQuantity = 0;
    attack->criticalMessageId = -1;
    attack->defender = defender;
    attack->tile = defender != NULL ? defender->tile : -1;
    attack->defenderHitLocation = hitLocation;
    attack->defenderDamage = 0;
    attack->defenderFlags = 0;
    attack->defenderKnockback = 0;
    attack->extrasLength = 0;
    attack->oops = defender;
}

// 0x420CFC
int combat_attack(Object* attacker, Object* defender, int hitMode, int hitLocation)
{
    bool aiming;
    int actionPoints;

    if (hitMode == HIT_MODE_PUNCH && roll_random(1, 4) == 1) {
        int fid = art_id(OBJ_TYPE_CRITTER, attacker->fid & 0xFFF, ANIM_KICK_LEG, (attacker->fid & 0xF000) >> 12, (attacker->fid & 0x70000000) >> 28);
        if (art_exists(fid)) {
            hitMode = HIT_MODE_KICK;
        }
    }

    combat_ctd_init(&main_ctd, attacker, defender, hitMode, hitLocation);
    debug_printf("computing attack...\n");

    if (compute_attack(&main_ctd) == -1) {
        return -1;
    }

    if (gcsd != NULL) {
        main_ctd.defenderDamage += gcsd->damageBonus;

        if (main_ctd.defenderDamage < gcsd->minDamage) {
            main_ctd.defenderDamage = gcsd->minDamage;
        }

        if (main_ctd.defenderDamage > gcsd->maxDamage) {
            main_ctd.defenderDamage = gcsd->maxDamage;
        }

        if (gcsd->field_1C) {
            // FIXME: looks like a bug, two different fields are used to set
            // one field.
            main_ctd.defenderFlags = gcsd->field_20;
            main_ctd.defenderFlags = gcsd->field_24;
        }
    }

    if (main_ctd.defenderHitLocation == HIT_LOCATION_TORSO || main_ctd.defenderHitLocation == HIT_LOCATION_UNCALLED) {
        if (attacker == obj_dude) {
            intface_get_attack(&hitMode, &aiming);
        } else {
            aiming = false;
        }
    } else {
        aiming = true;
    }

    actionPoints = item_w_mp_cost(attacker, main_ctd.hitMode, aiming);
    debug_printf("sequencing attack...\n");

    if (action_attack(&main_ctd) == -1) {
        return -1;
    }

    if (actionPoints > attacker->data.critter.combat.ap) {
        attacker->data.critter.combat.ap = 0;
    } else {
        attacker->data.critter.combat.ap -= actionPoints;
    }

    if (attacker == obj_dude) {
        intface_update_move_points(attacker->data.critter.combat.ap);
        critter_set_who_hit_me(attacker, defender);
    }

    combat_call_display = 1;
    combat_cleanup_enabled = 1;
    debug_printf("running attack...\n");

    return 0;
}

// 0x420EB0
int combat_bullet_start(const Object* a1, const Object* a2)
{
    int rotation = tile_dir(a1->tile, a2->tile);
    return tile_num_in_direction(a1->tile, rotation, 1);
}

// 0x420ED4
static bool check_ranged_miss(Attack* attack)
{
    int range = item_w_range(attack->attacker, attack->hitMode);
    int to = tile_num_beyond(attack->attacker->tile, attack->defender->tile, range);

    int roll = ROLL_FAILURE;
    Object* critter = attack->attacker;
    if (critter != NULL) {
        int curr = attack->attacker->tile;
        while (curr != to) {
            make_straight_path(attack->attacker, curr, to, NULL, &critter, 32);
            if (critter != NULL) {
                if ((critter->flags & OBJECT_SHOOT_THRU) == 0) {
                    if (FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER) {
                        roll = ROLL_SUCCESS;
                        break;
                    }

                    if (critter != attack->defender) {
                        int v6 = determine_to_hit_func(attack->attacker, critter, attack->defenderHitLocation, attack->hitMode, 1) / 3;
                        if (critter_is_dead(critter)) {
                            v6 = 5;
                        }

                        if (roll_random(1, 100) <= v6) {
                            roll = ROLL_SUCCESS;
                            break;
                        }
                    }

                    curr = critter->tile;
                }
            }

            if (critter == NULL) {
                break;
            }
        }
    }

    attack->defenderHitLocation = HIT_LOCATION_TORSO;

    if (roll < ROLL_SUCCESS || critter == NULL || (critter->flags & OBJECT_SHOOT_THRU) == 0) {
        return false;
    }

    attack->defender = critter;
    attack->tile = critter->tile;
    attack->attackerFlags |= DAM_HIT;
    attack->defenderHitLocation = HIT_LOCATION_TORSO;
    compute_damage(attack, 1, 2);
    return true;
}

// 0x420FFC
static int shoot_along_path(Attack* attack, int endTile, int rounds, int anim)
{
    // 0x56BCA0
    static Attack temp_ctd;

    int remainingRounds = rounds;
    int roundsHitMainTarget = 0;
    int currentTile = attack->attacker->tile;

    Object* critter = attack->attacker;
    while (critter != NULL) {
        if ((remainingRounds <= 0 && anim != ANIM_FIRE_CONTINUOUS) || currentTile == endTile || attack->extrasLength >= 6) {
            break;
        }

        make_straight_path(attack->attacker, currentTile, endTile, NULL, &critter, 32);

        if (critter != NULL) {
            if (FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER) {
                break;
            }

            int accuracy = determine_to_hit_func(attack->attacker, critter, HIT_LOCATION_TORSO, attack->hitMode, 1);
            if (anim == ANIM_FIRE_CONTINUOUS) {
                remainingRounds = 1;
            }

            int roundsHit = 0;
            while (roll_random(1, 100) <= accuracy && remainingRounds > 0) {
                remainingRounds -= 1;
                roundsHit += 1;
            }

            if (roundsHit != 0) {
                if (critter == attack->defender) {
                    roundsHitMainTarget += roundsHit;
                } else {
                    int index;
                    for (index = 0; index < attack->extrasLength; index += 1) {
                        if (critter == attack->extras[index]) {
                            break;
                        }
                    }

                    attack->extrasHitLocation[index] = HIT_LOCATION_TORSO;
                    attack->extras[index] = critter;
                    combat_ctd_init(&temp_ctd, attack->attacker, critter, attack->hitMode, HIT_LOCATION_TORSO);
                    temp_ctd.attackerFlags |= DAM_HIT;
                    compute_damage(&temp_ctd, roundsHit, 2);

                    if (index == attack->extrasLength) {
                        attack->extrasDamage[index] = temp_ctd.defenderDamage;
                        attack->extrasFlags[index] = temp_ctd.defenderFlags;
                        attack->extrasKnockback[index] = temp_ctd.defenderKnockback;
                        attack->extrasLength++;
                    } else {
                        if (anim == ANIM_FIRE_BURST) {
                            attack->extrasDamage[index] += temp_ctd.defenderDamage;
                            attack->extrasFlags[index] |= temp_ctd.defenderFlags;
                            attack->extrasKnockback[index] += temp_ctd.defenderKnockback;
                        }
                    }
                }
            }

            currentTile = critter->tile;
        }
    }

    if (anim == ANIM_FIRE_CONTINUOUS) {
        roundsHitMainTarget = 0;
    }

    return roundsHitMainTarget;
}

// 0x4211F4
static int compute_spray(Attack* attack, int accuracy, int* roundsHitMainTargetPtr, int* roundsSpentPtr, int anim)
{
    *roundsHitMainTargetPtr = 0;

    int ammoQuantity = item_w_curr_ammo(attack->weapon);
    int burstRounds = item_w_rounds(attack->weapon);
    if (burstRounds < ammoQuantity) {
        ammoQuantity = burstRounds;
    }

    *roundsSpentPtr = ammoQuantity;

    int criticalChance = stat_level(attack->attacker, STAT_CRITICAL_CHANCE);
    int roll = roll_check(accuracy, criticalChance, NULL);

    if (roll == ROLL_CRITICAL_FAILURE) {
        return roll;
    }

    if (roll == ROLL_CRITICAL_SUCCESS) {
        accuracy += 20;
    }

    int leftRounds;
    int mainTargetRounds;
    int centerRounds;
    int rightRounds;
    if (anim == ANIM_FIRE_BURST) {
        centerRounds = ammoQuantity / 3;
        if (centerRounds == 0) {
            centerRounds = 1;
        }

        leftRounds = ammoQuantity / 3;
        rightRounds = ammoQuantity - centerRounds - leftRounds;
        mainTargetRounds = centerRounds / 2;
        if (mainTargetRounds == 0) {
            mainTargetRounds = 1;
            centerRounds -= 1;
        }
    } else {
        leftRounds = 1;
        mainTargetRounds = 1;
        centerRounds = 1;
        rightRounds = 1;
    }

    for (int index = 0; index < mainTargetRounds; index += 1) {
        if (roll_check(accuracy, 0, NULL) >= ROLL_SUCCESS) {
            *roundsHitMainTargetPtr += 1;
        }
    }

    if (*roundsHitMainTargetPtr == 0 && check_ranged_miss(attack)) {
        *roundsHitMainTargetPtr = 1;
    }

    int range = item_w_range(attack->attacker, attack->hitMode);
    int mainTargetEndTile = tile_num_beyond(attack->attacker->tile, attack->defender->tile, range);
    *roundsHitMainTargetPtr += shoot_along_path(attack, mainTargetEndTile, centerRounds - *roundsHitMainTargetPtr, anim);

    int centerTile;
    if (obj_dist(attack->attacker, attack->defender) <= 3) {
        centerTile = tile_num_beyond(attack->attacker->tile, attack->defender->tile, 3);
    } else {
        centerTile = attack->defender->tile;
    }

    int rotation = tile_dir(centerTile, attack->attacker->tile);

    int leftTile = tile_num_in_direction(centerTile, (rotation + 1) % ROTATION_COUNT, 1);
    int leftEndTile = tile_num_beyond(attack->attacker->tile, leftTile, range);
    *roundsHitMainTargetPtr += shoot_along_path(attack, leftEndTile, leftRounds, anim);

    int rightTile = tile_num_in_direction(centerTile, (rotation + 5) % ROTATION_COUNT, 1);
    int rightEndTile = tile_num_beyond(attack->attacker->tile, rightTile, range);
    *roundsHitMainTargetPtr += shoot_along_path(attack, rightEndTile, rightRounds, anim);

    if (roll != ROLL_FAILURE || (*roundsHitMainTargetPtr <= 0 && attack->extrasLength <= 0)) {
        if (roll >= ROLL_SUCCESS && *roundsHitMainTargetPtr == 0 && attack->extrasLength == 0) {
            roll = ROLL_FAILURE;
        }
    } else {
        roll = ROLL_SUCCESS;
    }

    return roll;
}

// 0x421490
static int compute_attack(Attack* attack)
{
    int weapon_range;
    int distance;
    int anim;
    int to_hit;
    int damage_type;
    bool is_grenade = false;
    int weapon_subtype;
    int critical_chance;
    int roll;

    weapon_range = item_w_range(attack->attacker, attack->hitMode);
    distance = obj_dist(attack->attacker, attack->defender);

    if (weapon_range < distance) {
        return -1;
    }

    anim = item_w_anim(attack->attacker, attack->hitMode);
    to_hit = determine_to_hit(attack->attacker, attack->defender, attack->defenderHitLocation, attack->hitMode);

    damage_type = item_w_damage_type(attack->weapon);
    if (anim == ANIM_THROW_ANIM && (damage_type == DAMAGE_TYPE_EXPLOSION || damage_type == DAMAGE_TYPE_PLASMA || damage_type == DAMAGE_TYPE_EMP)) {
        is_grenade = true;
    }

    if (attack->defenderHitLocation == HIT_LOCATION_UNCALLED) {
        attack->defenderHitLocation = HIT_LOCATION_TORSO;
    }

    weapon_subtype = item_w_subtype(attack->weapon, attack->hitMode);
    int roundsHitMainTarget = 1;
    int damage_multiplier = 2;
    int roundsSpent = 1;

    if (anim == ANIM_FIRE_BURST || anim == ANIM_FIRE_CONTINUOUS) {
        roll = compute_spray(attack, to_hit, &roundsHitMainTarget, &roundsSpent, anim);
    } else {
        critical_chance = stat_level(attack->attacker, STAT_CRITICAL_CHANCE);
        roll = roll_check(to_hit, critical_chance - hit_location_penalty[attack->defenderHitLocation], NULL);
    }

    if (roll == ROLL_FAILURE) {
        if (trait_level(TRAIT_JINXED)) {
            if (roll_random(0, 1) == 1) {
                roll = ROLL_CRITICAL_FAILURE;
            }
        }
    }

    if (weapon_subtype == ATTACK_TYPE_MELEE || weapon_subtype == ATTACK_TYPE_UNARMED) {
        if (roll == ROLL_SUCCESS) {
            if (attack->attacker == obj_dude) {
                if (perk_level(PERK_SLAYER)) {
                    roll = ROLL_CRITICAL_SUCCESS;
                }

                if (perk_level(PERK_SILENT_DEATH)
                    && !is_hit_from_front(obj_dude, attack->defender)
                    && is_pc_flag(PC_FLAG_SNEAKING)
                    && obj_dude != attack->defender->data.critter.combat.whoHitMe) {
                    damage_multiplier = 4;
                }
            }
        }
    }
    if (roll == ROLL_SUCCESS) {
        if ((weapon_subtype == ATTACK_TYPE_MELEE || weapon_subtype == ATTACK_TYPE_UNARMED) && attack->attacker == obj_dude) {
            if (perk_level(PERK_SLAYER)) {
                roll = ROLL_CRITICAL_SUCCESS;
            }

            if (perk_level(PERK_SILENT_DEATH)
                && !is_hit_from_front(obj_dude, attack->defender)
                && is_pc_flag(PC_FLAG_SNEAKING)
                && obj_dude != attack->defender->data.critter.combat.whoHitMe) {
                damage_multiplier = 4;
            }
        }
    }

    if (weapon_subtype == ATTACK_TYPE_RANGED) {
        attack->ammoQuantity = roundsSpent;

        if (roll == ROLL_SUCCESS && attack->attacker == obj_dude) {
            if (perk_level(PERK_SNIPER) != 0) {
                if (roll_random(1, 10) <= stat_level(obj_dude, STAT_LUCK)) {
                    roll = ROLL_CRITICAL_SUCCESS;
                }
            }
        }
    } else {
        if (item_w_max_ammo(attack->weapon) > 0) {
            attack->ammoQuantity = 1;
        }
    }

    switch (roll) {
    case ROLL_CRITICAL_SUCCESS:
        damage_multiplier = attack_crit_success(attack);
        // FALLTHROUGH
    case ROLL_SUCCESS:
        attack->attackerFlags |= DAM_HIT;
        compute_damage(attack, roundsHitMainTarget, damage_multiplier);
        break;
    case ROLL_FAILURE:
        if (weapon_subtype == ATTACK_TYPE_RANGED || weapon_subtype == ATTACK_TYPE_THROW) {
            check_ranged_miss(attack);
        }
        break;
    case ROLL_CRITICAL_FAILURE:
        attack_crit_failure(attack);
        break;
    }

    if (weapon_subtype == ATTACK_TYPE_RANGED || weapon_subtype == ATTACK_TYPE_THROW) {
        if ((attack->attackerFlags & (DAM_HIT | DAM_CRITICAL)) == 0) {
            int tile;
            int throw_distance;
            int rotation;
            Object* defender;

            if (is_grenade) {
                throw_distance = roll_random(1, distance / 2);
                if (throw_distance == 0) {
                    throw_distance = 1;
                }

                rotation = roll_random(0, 5);
                tile = tile_num_in_direction(attack->defender->tile, rotation, throw_distance);
            } else {
                tile = tile_num_beyond(attack->attacker->tile, attack->defender->tile, weapon_range);
            }

            attack->tile = tile;

            defender = attack->defender;
            make_straight_path(defender, attack->defender->tile, attack->tile, NULL, &defender, 32);
            if (defender != NULL && defender != attack->defender) {
                attack->tile = defender->tile;
            } else {
                defender = obj_blocking_at(NULL, attack->tile, attack->defender->elevation);
            }

            if (defender != NULL && (defender->flags & OBJECT_SHOOT_THRU) == 0) {
                attack->attackerFlags |= DAM_HIT;
                attack->defender = defender;
                compute_damage(attack, 1, 2);
            }
        }
    }

    if ((damage_type == DAMAGE_TYPE_EXPLOSION || is_grenade) && ((attack->attackerFlags & DAM_HIT) != 0 || (attack->attackerFlags & DAM_CRITICAL) == 0)) {
        compute_explosion_on_extras(attack, 0, is_grenade, 0);
    } else {
        if ((attack->attackerFlags & DAM_EXPLODE) != 0) {
            compute_explosion_on_extras(attack, 1, is_grenade, 0);
        }
    }

    death_checks(attack);

    return 0;
}

// 0x421800
void compute_explosion_on_extras(Attack* attack, int a2, bool isGrenade, int a4)
{
    // 0x56BD58
    static Attack temp_ctd;

    Object* attacker;

    if (a2) {
        attacker = attack->attacker;
    } else {
        if ((attack->attackerFlags & DAM_HIT) != 0) {
            attacker = attack->defender;
        } else {
            attacker = NULL;
        }
    }

    int origin_tile;
    if (attacker != NULL) {
        origin_tile = attacker->tile;
    } else {
        origin_tile = attack->tile;
    }

    int step;
    int radius = 0;
    int rotation = 0;
    int current_tile = -1;
    int current_center_tile = origin_tile;
    while (attack->extrasLength < 6) {
        if (radius != 0 && (current_tile = tile_num_in_direction(current_tile, rotation, 1)) != current_center_tile) {
            step++;
            if (step % radius == 0) {
                rotation += 1;
                if (rotation == ROTATION_COUNT) {
                    rotation = ROTATION_NE;
                }
            }
        } else {
            radius++;

            if (isGrenade && radius > 2) {
                current_tile = -1;
            } else if (!isGrenade && radius > 3) {
                current_tile = -1;
            } else {
                current_tile = tile_num_in_direction(current_center_tile, ROTATION_NE, 1);
            }

            current_center_tile = current_tile;
            rotation = ROTATION_SE;
            step = 0;
        }

        if (current_tile == -1) {
            break;
        }

        Object* obstacle = obj_blocking_at(attacker, current_tile, attack->attacker->elevation);
        if (obstacle != NULL
            && FID_TYPE(obstacle->fid) == OBJ_TYPE_CRITTER
            && (obstacle->data.critter.combat.results & DAM_DEAD) == 0
            && (obstacle->flags & OBJECT_SHOOT_THRU) == 0
            && !combat_is_shot_blocked(obstacle, obstacle->tile, origin_tile, NULL, NULL)) {
            if (obstacle == attack->attacker) {
                attack->attackerFlags &= ~DAM_HIT;
                compute_damage(attack, 1, 2);
                attack->attackerFlags |= DAM_HIT;
                attack->attackerFlags |= DAM_BACKWASH;
            } else {
                int index;
                for (index = 0; index < attack->extrasLength; index++) {
                    if (attack->extras[index] == obstacle) {
                        break;
                    }
                }

                if (index == attack->extrasLength) {
                    attack->extrasHitLocation[index] = HIT_LOCATION_TORSO;
                    attack->extras[index] = obstacle;
                    combat_ctd_init(&temp_ctd, attack->attacker, obstacle, attack->hitMode, HIT_LOCATION_TORSO);
                    if (!a4) {
                        temp_ctd.attackerFlags |= DAM_HIT;
                        compute_damage(&temp_ctd, 1, 2);
                    }

                    attack->extrasDamage[index] = temp_ctd.defenderDamage;
                    attack->extrasFlags[index] = temp_ctd.defenderFlags;
                    attack->extrasKnockback[index] = temp_ctd.defenderKnockback;
                    attack->extrasLength += 1;
                }
            }
        }
    }
}

// 0x421A6C
static int attack_crit_success(Attack* attack)
{
    Object* defender = attack->defender;
    if (defender != NULL && defender->pid == 16777224) {
        return 2;
    }

    attack->attackerFlags |= DAM_CRITICAL;

    int chance = roll_random(1, 100);

    chance += stat_level(attack->attacker, STAT_BETTER_CRITICALS);

    int effect;
    if (chance <= 20) {
        effect = 0;
    } else if (chance <= 45) {
        effect = 1;
    } else if (chance <= 70) {
        effect = 2;
    } else if (chance <= 90) {
        effect = 3;
    } else if (chance <= 100) {
        effect = 4;
    } else {
        effect = 5;
    }

    CriticalHitDescription* criticalHitDescription;
    if (defender == obj_dude) {
        criticalHitDescription = &(pc_crit_succ_eff[attack->defenderHitLocation][effect]);
    } else {
        int killType = critter_kill_count_type(defender);
        criticalHitDescription = &(crit_succ_eff[killType][attack->defenderHitLocation][effect]);
    }

    attack->defenderFlags |= criticalHitDescription->flags;

    // NOTE: Original code is slightly different, it does not set message in
    // advance, instead using "else" statement.
    attack->criticalMessageId = criticalHitDescription->messageId;

    if (criticalHitDescription->massiveCriticalStat != -1) {
        if (stat_result(defender, criticalHitDescription->massiveCriticalStat, criticalHitDescription->massiveCriticalStatModifier, NULL) <= ROLL_FAILURE) {
            attack->defenderFlags |= criticalHitDescription->massiveCriticalFlags;
            attack->criticalMessageId = criticalHitDescription->massiveCriticalMessageId;
        }
    }

    if ((attack->defenderFlags & DAM_CRIP_RANDOM) != 0) {
        // NOTE: Uninline.
        do_random_cripple(&(attack->defenderFlags));
    }

    return criticalHitDescription->damageMultiplier;
}

// 0x421BD8
static int attack_crit_failure(Attack* attack)
{
    attack->attackerFlags &= ~DAM_HIT;

    if (attack->attacker != NULL && attack->attacker->pid == 16777224) {
        return 0;
    }

    int attackType = item_w_subtype(attack->weapon, attack->hitMode);
    int criticalFailureTableIndex = item_w_crit_fail(attack->weapon);
    if (criticalFailureTableIndex == -1) {
        criticalFailureTableIndex = 0;
    }

    int chance = roll_random(1, 100) - 5 * (stat_level(attack->attacker, STAT_LUCK) - 5);

    int effect;
    if (chance <= 20) {
        effect = 0;
    } else if (chance <= 50) {
        effect = 1;
    } else if (chance <= 75) {
        effect = 2;
    } else if (chance <= 95) {
        effect = 3;
    } else {
        effect = 4;
    }

    int flags = cf_table[criticalFailureTableIndex][effect];
    if (flags == 0) {
        return 0;
    }

    attack->attackerFlags |= DAM_CRITICAL;
    attack->attackerFlags |= flags;

    if ((attack->attackerFlags & DAM_HIT_SELF) != 0) {
        int ammoQuantity = attackType == ATTACK_TYPE_RANGED ? attack->ammoQuantity : 1;
        compute_damage(attack, ammoQuantity, 2);
    } else if ((attack->attackerFlags & DAM_EXPLODE) != 0) {
        compute_damage(attack, 1, 2);
    }

    if ((attack->attackerFlags & DAM_HURT_SELF) != 0) {
        attack->attackerDamage += roll_random(1, 5);
    }

    if ((attack->attackerFlags & DAM_LOSE_TURN) != 0) {
        attack->attacker->data.critter.combat.ap = 0;
    }

    if ((attack->attackerFlags & DAM_LOSE_AMMO) != 0) {
        if (attackType == ATTACK_TYPE_RANGED) {
            attack->ammoQuantity = item_w_curr_ammo(attack->weapon);
        } else {
            attack->attackerFlags &= ~DAM_LOSE_AMMO;
        }
    }

    if ((attack->attackerFlags & DAM_CRIP_RANDOM) != 0) {
        // NOTE: Uninline.
        do_random_cripple(&(attack->attackerFlags));
    }

    if ((attack->attackerFlags & DAM_RANDOM_HIT) != 0) {
        attack->defender = combat_ai_random_target(attack);
        if (attack->defender != NULL) {
            attack->attackerFlags |= DAM_HIT;
            attack->defenderHitLocation = HIT_LOCATION_TORSO;
            attack->attackerFlags &= ~DAM_CRITICAL;

            int ammoQuantity = attackType == ATTACK_TYPE_RANGED ? attack->ammoQuantity : 1;
            compute_damage(attack, ammoQuantity, 2);
        } else {
            attack->defender = attack->oops;
        }

        if (attack->defender != NULL) {
            attack->tile = attack->defender->tile;
        }
    }

    return 0;
}

// 0x421DEC
static void do_random_cripple(int* flagsPtr)
{
    *flagsPtr &= ~DAM_CRIP_RANDOM;

    switch (roll_random(0, 3)) {
    case 0:
        *flagsPtr |= DAM_CRIP_LEG_LEFT;
        break;
    case 1:
        *flagsPtr |= DAM_CRIP_LEG_RIGHT;
        break;
    case 2:
        *flagsPtr |= DAM_CRIP_ARM_LEFT;
        break;
    case 3:
        *flagsPtr |= DAM_CRIP_ARM_RIGHT;
        break;
    }
}

// 0x421E2C
int determine_to_hit(Object* a1, Object* a2, int hitLocation, int hitMode)
{
    return determine_to_hit_func(a1, a2, hitLocation, hitMode, 1);
}

// 0x421E34
int determine_to_hit_no_range(Object* a1, Object* a2, int hitLocation, int hitMode)
{
    return determine_to_hit_func(a1, a2, hitLocation, hitMode, 0);
}

// 0x421E3C
static int determine_to_hit_func(Object* attacker, Object* defender, int hitLocation, int hitMode, int check_range)
{
    Object* weapon;
    bool is_ranged_weapon = false;
    int accuracy = 0;

    weapon = item_hit_with(attacker, hitMode);

    if (weapon == NULL) {
        accuracy = skill_level(attacker, SKILL_UNARMED);
    } else {
        int attack_type;
        int modifier;

        accuracy = item_w_skill_level(attacker, hitMode);

        attack_type = item_w_subtype(weapon, hitMode);
        if (attack_type == ATTACK_TYPE_RANGED || attack_type == ATTACK_TYPE_THROW) {
            is_ranged_weapon = true;

            if (check_range) {
                int range;
                int perception;
                int perception_modifier = 2;

                switch (item_w_perk(weapon)) {
                case PERK_WEAPON_LONG_RANGE:
                    perception_modifier = 4;
                    break;
                }

                perception = stat_level(attacker, STAT_PERCEPTION);
                range = obj_dist(attacker, defender) - perception_modifier * perception;
                if (range < -2 * perception) {
                    range = -2 * perception;
                }

                if (attacker == obj_dude) {
                    range -= 2 * perk_level(PERK_SHARPSHOOTER);
                }

                if (range >= 0 && (attacker->data.critter.combat.results & DAM_BLIND) != 0) {
                    modifier = -12 * range;
                } else {
                    modifier = -4 * range;
                }

                accuracy += modifier;
            }

            combat_is_shot_blocked(attacker, attacker->tile, defender->tile, defender, &modifier);
            accuracy -= 10 * modifier;
        }

        if (attacker == obj_dude) {
            if (trait_level(TRAIT_ONE_HANDER)) {
                if (item_w_is_2handed(weapon)) {
                    accuracy -= 40;
                } else {
                    accuracy += 20;
                }
            }
        }

        modifier = item_w_min_st(weapon) - stat_level(attacker, STAT_STRENGTH);
        if (modifier > 0) {
            accuracy -= 20 * modifier;
        }

        if (item_w_perk(weapon) == PERK_WEAPON_ACCURATE) {
            accuracy += 20;
        }
    }

    accuracy -= stat_level(defender, STAT_ARMOR_CLASS);

    if (is_ranged_weapon) {
        accuracy += hit_location_penalty[hitLocation];
    } else {
        accuracy += hit_location_penalty[hitLocation] / 2;
    }

    if (defender != NULL && (defender->flags & OBJECT_MULTIHEX) != 0) {
        accuracy += 15;
    }

    if (attacker == obj_dude) {
        int lightIntensity = obj_get_visible_light(defender);

        if (lightIntensity <= 26214)
            accuracy -= 40;
        else if (lightIntensity <= 39321)
            accuracy -= 25;
        else if (lightIntensity <= 52428)
            accuracy -= 10;
    }

    if (gcsd != NULL) {
        accuracy += gcsd->accuracyBonus;
    }

    if ((attacker->data.critter.combat.results & DAM_BLIND) != 0) {
        accuracy -= 25;
    }

    if ((defender->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) != 0) {
        accuracy += 40;
    }

    if (attacker->data.critter.combat.team != obj_dude->data.critter.combat.team) {
        int combatDifficuly = COMBAT_DIFFICULTY_NORMAL;
        config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_DIFFICULTY_KEY, &combatDifficuly);
        switch (combatDifficuly) {
        case COMBAT_DIFFICULTY_EASY:
            accuracy -= 20;
            break;
        case COMBAT_DIFFICULTY_HARD:
            accuracy += 20;
            break;
        }
    }

    if (accuracy > 95) {
        accuracy = 95;
    }

    if (accuracy < -100) {
        debug_printf("Whoa! Bad skill value in determine_to_hit!\n");
    }

    return accuracy;
}

// 0x422118
static void compute_damage(Attack* attack, int rounds, int damage_multiplier)
{
    int* damage_ptr;
    Object* critter;
    int* flags_ptr;
    int* knockback_distance_ptr;
    int damage_type;
    int damage_threshold;
    int damage_resistance;
    int bonus_ranged_damage;
    int combat_difficulty_multiplier;
    int combat_difficulty;
    int round;
    int round_damage;

    if ((attack->attackerFlags & DAM_HIT) != 0) {
        damage_ptr = &(attack->defenderDamage);
        critter = attack->defender;
        flags_ptr = &(attack->defenderFlags);
        knockback_distance_ptr = &(attack->defenderKnockback);
    } else {
        damage_ptr = &(attack->attackerDamage);
        critter = attack->attacker;
        flags_ptr = &(attack->attackerFlags);
        knockback_distance_ptr = NULL;
    }

    *damage_ptr = 0;

    if (FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER) {
        return;
    }

    damage_type = item_w_damage_type(attack->weapon);

    if ((*flags_ptr & DAM_BYPASS) == 0 || damage_type == DAMAGE_TYPE_EMP) {
        if (item_w_perk(attack->weapon) == PERK_WEAPON_PENETRATE) {
            damage_threshold = 0;
        } else {
            damage_threshold = stat_level(critter, STAT_DAMAGE_THRESHOLD + damage_type);
        }

        damage_resistance = stat_level(critter, STAT_DAMAGE_RESISTANCE + damage_type);

        if (attack->attacker == obj_dude) {
            if (trait_level(TRAIT_FINESSE)) {
                damage_resistance += 30;
            }
        }
    } else {
        damage_threshold = 0;
        damage_resistance = 0;
    }

    if (attack->attacker == obj_dude && item_w_subtype(attack->weapon, attack->hitMode) == ATTACK_TYPE_RANGED) {
        bonus_ranged_damage = 2 * perk_level(PERK_BONUS_RANGED_DAMAGE);
    } else {
        bonus_ranged_damage = 0;
    }

    combat_difficulty_multiplier = 100;
    if (attack->attacker->data.critter.combat.team != obj_dude->data.critter.combat.team) {
        combat_difficulty = COMBAT_DIFFICULTY_NORMAL;
        config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_DIFFICULTY_KEY, &combat_difficulty);

        switch (combat_difficulty) {
        case COMBAT_DIFFICULTY_EASY:
            combat_difficulty_multiplier = 75;
            break;
        case COMBAT_DIFFICULTY_HARD:
            combat_difficulty_multiplier = 125;
            break;
        }
    }

    for (round = 0; round < rounds; round++) {
        round_damage = item_w_damage(attack->attacker, attack->hitMode) + bonus_ranged_damage;
        round_damage *= damage_multiplier;
        round_damage /= 2;
        round_damage *= combat_difficulty_multiplier;
        round_damage /= 100;
        round_damage -= damage_threshold;

        if (round_damage > 0) {
            round_damage -= round_damage * damage_resistance / 100;

            if (round_damage > 0) {
                *damage_ptr += round_damage;
            }
        }
    }

    if (knockback_distance_ptr != NULL) {
        if ((critter->flags & OBJECT_MULTIHEX) == 0) {
            if (damage_type == DAMAGE_TYPE_EXPLOSION || attack->weapon == NULL || item_w_subtype(attack->weapon, attack->hitMode) == ATTACK_TYPE_MELEE) {
                if (item_w_perk(attack->weapon) == PERK_WEAPON_KNOCKBACK) {
                    *knockback_distance_ptr = *damage_ptr / 5;
                } else {
                    *knockback_distance_ptr = *damage_ptr / 10;
                }
            }
        }
    }
}

// 0x422348
void death_checks(Attack* attack)
{
    int index;

    check_for_death(attack->attacker, attack->attackerDamage, &(attack->attackerFlags));
    check_for_death(attack->defender, attack->defenderDamage, &(attack->defenderFlags));

    for (index = 0; index < attack->extrasLength; index++) {
        check_for_death(attack->extras[index], attack->extrasDamage[index], &(attack->extrasFlags[index]));
    }
}

// 0x422418
void apply_damage(Attack* attack, bool animated)
{
    Object* attacker = attack->attacker;
    bool attackerIsCritter = attacker != NULL && FID_TYPE(attacker->fid) == OBJ_TYPE_CRITTER;

    if (attackerIsCritter && (attacker->data.critter.combat.results & DAM_DEAD) == 0) {
        set_new_results(attacker, attack->attackerFlags);
        damage_object(attacker, attack->attackerDamage, animated, attack->defender != attack->oops);
    }

    if (attack->oops != NULL && attack->oops != attack->defender) {
        combatai_notify_onlookers(attack->oops);
    }

    Object* defender = attack->defender;
    bool defenderIsCritter = defender != NULL && FID_TYPE(defender->fid) == OBJ_TYPE_CRITTER;

    if (defenderIsCritter && (defender->data.critter.combat.results & DAM_DEAD) == 0) {
        set_new_results(defender, attack->defenderFlags);

        if (attackerIsCritter) {
            if ((defender->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
                critter_set_who_hit_me(defender, attack->attacker);
            } else if (defender == attack->oops || defender->data.critter.combat.team != attack->attacker->data.critter.combat.team) {
                combatai_check_retaliation(defender, attack->attacker);
            }
        }

        scr_set_objs(defender->sid, attack->attacker, NULL);
        damage_object(defender, attack->defenderDamage, animated, attack->defender != attack->oops);
        combatai_notify_onlookers(defender);

        if (attack->defenderDamage >= 0 && (attack->attackerFlags & DAM_HIT) != 0) {
            scr_set_objs(attack->attacker->sid, NULL, attack->defender);
            scr_set_ext_param(attack->attacker->sid, 2);
            exec_script_proc(attack->attacker->sid, SCRIPT_PROC_COMBAT);
        }
    }

    for (int index = 0; index < attack->extrasLength; index++) {
        Object* obj = attack->extras[index];
        if (FID_TYPE(obj->fid) == OBJ_TYPE_CRITTER && (obj->data.critter.combat.results & DAM_DEAD) == 0) {
            set_new_results(obj, attack->extrasFlags[index]);

            if (attackerIsCritter) {
                if ((obj->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
                    critter_set_who_hit_me(obj, attack->attacker);
                } else if (obj->data.critter.combat.team != attack->attacker->data.critter.combat.team) {
                    combatai_check_retaliation(obj, attack->attacker);
                }
            }

            scr_set_objs(obj->sid, attack->attacker, NULL);
            damage_object(obj, attack->extrasDamage[index], animated, attack->defender != attack->oops);
            combatai_notify_onlookers(obj);

            if (attack->extrasDamage[index] >= 0) {
                if ((attack->attackerFlags & DAM_HIT) != 0) {
                    scr_set_objs(attack->attacker->sid, NULL, obj);
                    scr_set_ext_param(attack->attacker->sid, 2);
                    exec_script_proc(attack->attacker->sid, SCRIPT_PROC_COMBAT);
                }
            }
        }
    }
}

// 0x422650
static void check_for_death(Object* object, int damage, int* flags)
{
    if (object == NULL || object->pid != 16777224) {
        if (damage > 0) {
            if (critter_get_hits(object) - damage <= 0) {
                *flags |= DAM_DEAD;
            }
        }
    }
}

// 0x422670
static void set_new_results(Object* critter, int flags)
{
    if (critter == NULL) {
        return;
    }

    if (FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER) {
        return;
    }

    if (critter->pid == 16777224) {
        return;
    }

    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        return;
    }

    if ((flags & DAM_DEAD) != 0) {
        queue_remove(critter);
    } else if ((flags & DAM_KNOCKED_OUT) != 0) {
        // SFALL: Fix multiple knockout events.
        queue_remove_this(critter, EVENT_TYPE_KNOCKOUT);

        int endurance = stat_level(critter, STAT_ENDURANCE);
        queue_add(10 * (35 - 3 * endurance), critter, NULL, EVENT_TYPE_KNOCKOUT);
    }

    if (critter == obj_dude && (flags & DAM_CRIP_ARM_ANY) != 0) {
        critter->data.critter.combat.results |= flags & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN | DAM_CRIP | DAM_DEAD | DAM_LOSE_TURN);

        intface_update_items(true);
    } else {
        critter->data.critter.combat.results |= flags & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN | DAM_CRIP | DAM_DEAD | DAM_LOSE_TURN);
    }
}

// 0x422734
static void damage_object(Object* obj, int damage, bool animated, bool a4)
{
    if (obj == NULL) {
        return;
    }

    if (FID_TYPE(obj->fid) != OBJ_TYPE_CRITTER) {
        return;
    }

    if (obj->pid == 16777224) {
        return;
    }

    if (damage <= 0) {
        return;
    }

    critter_adjust_hits(obj, -damage);

    if (obj == obj_dude) {
        intface_update_hit_points(animated);
    }

    obj->data.critter.combat.damageLastTurn += damage;

    if (!a4) {
        exec_script_proc(obj->sid, SCRIPT_PROC_DAMAGE);
    }

    if ((obj->data.critter.combat.results & DAM_DEAD) != 0) {
        scr_set_objs(obj->sid, obj->data.critter.combat.whoHitMe, NULL);
        exec_script_proc(obj->sid, SCRIPT_PROC_DESTROY);

        if (obj != obj_dude) {
            Object* whoHitMe = obj->data.critter.combat.whoHitMe;
            if (whoHitMe == obj_dude || (whoHitMe != NULL && whoHitMe->data.critter.combat.team == obj_dude->data.critter.combat.team)) {
                bool scriptOverrides = false;
                Script* scr;
                if (scr_ptr(obj->sid, &scr) != -1) {
                    scriptOverrides = scr->scriptOverrides;
                }

                if (!scriptOverrides) {
                    combat_exps += critter_kill_exps(obj);
                    critter_kill_count_inc(critter_kill_count_type(obj));
                }
            }
        }

        if (obj->sid != -1) {
            scr_remove(obj->sid);
            obj->sid = -1;
        }

        partyMemberRemove(obj);
    }
}

// Print attack description to monitor.
//
// 0x422844
void combat_display(Attack* attack)
{
    MessageListItem messageListItem;

    if (attack->attacker == obj_dude) {
        Object* weapon = item_hit_with(attack->attacker, attack->hitMode);
        int strengthRequired = item_w_min_st(weapon);

        if (weapon != NULL) {
            if (strengthRequired > stat_level(obj_dude, STAT_STRENGTH)) {
                // You are not strong enough to use this weapon properly.
                messageListItem.num = 107;
                if (message_search(&combat_message_file, &messageListItem)) {
                    display_print(messageListItem.text);
                }
            }
        }
    }

    Object* mainCritter;
    if ((attack->attackerFlags & DAM_HIT) != 0) {
        mainCritter = attack->defender;
    } else {
        mainCritter = attack->attacker;
    }

    char* mainCritterName = _a_1;

    char you[20];
    you[0] = '\0';

    // You (player)
    messageListItem.num = 506;
    if (message_search(&combat_message_file, &messageListItem)) {
        strcpy(you, messageListItem.text);
    }

    int baseMessageId;
    if (mainCritter == obj_dude) {
        mainCritterName = you;
        baseMessageId = 500;
    } else if (mainCritter != NULL) {
        mainCritterName = object_name(mainCritter);
        if (stat_level(mainCritter, STAT_GENDER) == GENDER_MALE) {
            baseMessageId = 600;
        } else {
            baseMessageId = 700;
        }
    }

    char text[280];
    if (attack->defender != NULL
        && attack->oops != NULL
        && attack->defender != attack->oops
        && (attack->attackerFlags & DAM_HIT) != 0) {
        if (FID_TYPE(attack->defender->fid) == OBJ_TYPE_CRITTER) {
            if (attack->oops == obj_dude) {
                // 608 (male) - Oops! %s was hit instead of you!
                // 708 (female) - Oops! %s was hit instead of you!
                messageListItem.num = baseMessageId + 8;
                if (message_search(&combat_message_file, &messageListItem)) {
                    snprintf(text, sizeof(text), messageListItem.text, mainCritterName);
                }
            } else {
                // 509 (player) - Oops! %s were hit instead of %s!
                const char* name = object_name(attack->oops);
                messageListItem.num = baseMessageId + 9;
                if (message_search(&combat_message_file, &messageListItem)) {
                    snprintf(text, sizeof(text), messageListItem.text, mainCritterName, name);
                }
            }
        } else {
            if (attack->attacker == obj_dude) {
                // (player) %s missed
                messageListItem.num = 515;

                if (message_search(&combat_message_file, &messageListItem)) {
                    snprintf(text, sizeof(text), messageListItem.text, you);
                }
            } else {
                const char* name = object_name(attack->attacker);
                if (stat_level(attack->attacker, STAT_GENDER) == GENDER_MALE) {
                    // (male) %s missed
                    messageListItem.num = 615;
                } else {
                    // (female) %s missed
                    messageListItem.num = 715;
                }

                if (message_search(&combat_message_file, &messageListItem)) {
                    snprintf(text, sizeof(text), messageListItem.text, name);
                }
            }
        }

        strcat(text, ".");

        display_print(text);
    }

    if ((attack->attackerFlags & DAM_HIT) != 0) {
        if (attack->defender != NULL && (attack->defender->data.critter.combat.results & DAM_DEAD) == 0) {
            text[0] = '\0';

            if (FID_TYPE(attack->defender->fid) == OBJ_TYPE_CRITTER) {
                if (attack->defenderHitLocation == HIT_LOCATION_TORSO) {
                    if ((attack->attackerFlags & DAM_CRITICAL) != 0) {
                        switch (attack->defenderDamage) {
                        case 0:
                            // 528 - %s were critically hit for no damage
                            messageListItem.num = baseMessageId + 28;
                            break;
                        case 1:
                            // 524 - %s were critically hit for 1 hit point
                            messageListItem.num = baseMessageId + 24;
                            break;
                        default:
                            // 520 - %s were critically hit for %d hit points
                            messageListItem.num = baseMessageId + 20;
                            break;
                        }

                        if (message_search(&combat_message_file, &messageListItem)) {
                            if (attack->defenderDamage <= 1) {
                                snprintf(text, sizeof(text), messageListItem.text, mainCritterName);
                            } else {
                                snprintf(text, sizeof(text), messageListItem.text, mainCritterName, attack->defenderDamage);
                            }
                        }
                    } else {
                        combat_display_hit(text, sizeof(text), attack->defender, attack->defenderDamage);
                    }
                } else {
                    const char* hitLocationName = combat_get_loc_name(attack->defender, attack->defenderHitLocation);
                    if (hitLocationName != NULL) {
                        if ((attack->attackerFlags & DAM_CRITICAL) != 0) {
                            switch (attack->defenderDamage) {
                            case 0:
                                // 525 - %s were critically hit in %s for no damage
                                messageListItem.num = baseMessageId + 25;
                                break;
                            case 1:
                                // 521 - %s were critically hit in %s for 1 damage
                                messageListItem.num = baseMessageId + 21;
                                break;
                            default:
                                // 511 - %s were critically hit in %s for %d hit points
                                messageListItem.num = baseMessageId + 11;
                                break;
                            }
                        } else {
                            switch (attack->defenderDamage) {
                            case 0:
                                // 526 - %s were hit in %s for no damage
                                messageListItem.num = baseMessageId + 26;
                                break;
                            case 1:
                                // 522 - %s were hit in %s for 1 damage
                                messageListItem.num = baseMessageId + 22;
                                break;
                            default:
                                // 512 - %s were hit in %s for %d hit points
                                messageListItem.num = baseMessageId + 12;
                                break;
                            }
                        }

                        if (message_search(&combat_message_file, &messageListItem)) {
                            if (attack->defenderDamage <= 1) {
                                snprintf(text, sizeof(text), messageListItem.text, mainCritterName, hitLocationName);
                            } else {
                                snprintf(text, sizeof(text), messageListItem.text, mainCritterName, hitLocationName, attack->defenderDamage);
                            }
                        }
                    }
                }

                int combatMessages = 1;
                config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_MESSAGES_KEY, &combatMessages);

                if (combatMessages == 1 && (attack->attackerFlags & DAM_CRITICAL) != 0 && attack->criticalMessageId != -1) {
                    messageListItem.num = attack->criticalMessageId;
                    if (message_search(&combat_message_file, &messageListItem)) {
                        strcat(text, messageListItem.text);
                    }

                    if ((attack->defenderFlags & DAM_DEAD) != 0) {
                        strcat(text, ".");
                        display_print(text);

                        if (attack->defender == obj_dude) {
                            if (stat_level(attack->defender, STAT_GENDER) == GENDER_MALE) {
                                // were killed
                                messageListItem.num = 207;
                            } else {
                                // were killed
                                messageListItem.num = 257;
                            }
                        } else {
                            if (stat_level(attack->defender, STAT_GENDER) == GENDER_MALE) {
                                // was killed
                                messageListItem.num = 307;
                            } else {
                                // was killed
                                messageListItem.num = 407;
                            }
                        }

                        if (message_search(&combat_message_file, &messageListItem)) {
                            snprintf(text, sizeof(text), "%s %s", mainCritterName, messageListItem.text);
                        }
                    }
                } else {
                    combat_display_flags(text, attack->defenderFlags, attack->defender);
                }

                strcat(text, ".");

                display_print(text);
            }
        }
    }

    if (attack->attacker != NULL && (attack->attacker->data.critter.combat.results & DAM_DEAD) == 0) {
        if ((attack->attackerFlags & DAM_HIT) == 0) {
            if ((attack->attackerFlags & DAM_CRITICAL) != 0) {
                switch (attack->attackerDamage) {
                case 0:
                    // 514 - %s critically missed
                    messageListItem.num = baseMessageId + 14;
                    break;
                case 1:
                    // 533 - %s critically missed and took 1 hit point
                    messageListItem.num = baseMessageId + 33;
                    break;
                default:
                    // 534 - %s critically missed and took %d hit points
                    messageListItem.num = baseMessageId + 34;
                    break;
                }
            } else {
                // 515 - %s missed
                messageListItem.num = baseMessageId + 15;
            }

            if (message_search(&combat_message_file, &messageListItem)) {
                if (attack->attackerDamage <= 1) {
                    snprintf(text, sizeof(text), messageListItem.text, mainCritterName);
                } else {
                    snprintf(text, sizeof(text), messageListItem.text, mainCritterName, attack->attackerDamage);
                }
            }

            combat_display_flags(text, attack->attackerFlags, attack->attacker);

            strcat(text, ".");

            display_print(text);
        }

        if ((attack->attackerFlags & DAM_HIT) != 0 || (attack->attackerFlags & DAM_CRITICAL) == 0) {
            if (attack->attackerDamage > 0) {
                combat_display_hit(text, sizeof(text), attack->attacker, attack->attackerDamage);
                combat_display_flags(text, attack->attackerFlags, attack->attacker);
                strcat(text, ".");
                display_print(text);
            }
        }
    }

    for (int index = 0; index < attack->extrasLength; index++) {
        Object* critter = attack->extras[index];
        if ((critter->data.critter.combat.results & DAM_DEAD) == 0) {
            combat_display_hit(text, sizeof(text), critter, attack->extrasDamage[index]);
            combat_display_flags(text, attack->extrasFlags[index], critter);
            strcat(text, ".");

            display_print(text);
        }
    }
}

// 0x4230EC
static void combat_display_hit(char* dest, size_t size, Object* critter, int damage)
{
    MessageListItem messageListItem;
    char text[40];
    char* name;

    int messageId;
    if (critter == obj_dude) {
        text[0] = '\0';

        messageId = 500;

        // 506 - You
        messageListItem.num = messageId + 6;
        if (message_search(&combat_message_file, &messageListItem)) {
            strcpy(text, messageListItem.text);
        }

        name = text;
    } else {
        name = object_name(critter);

        if (stat_level(critter, STAT_GENDER) == GENDER_MALE) {
            messageId = 600;
        } else {
            messageId = 700;
        }
    }

    switch (damage) {
    case 0:
        // 627 - %s was hit for no damage
        messageId += 27;
        break;
    case 1:
        // 623 - %s was hit for 1 hit point
        messageId += 23;
        break;
    default:
        // 613 - %s was hit for %d hit points
        messageId += 13;
        break;
    }

    messageListItem.num = messageId;
    if (message_search(&combat_message_file, &messageListItem)) {
        if (damage <= 1) {
            snprintf(dest, size, messageListItem.text, name);
        } else {
            snprintf(dest, size, messageListItem.text, name, damage);
        }
    }
}

// 0x4231D0
static void combat_display_flags(char* dest, int flags, Object* critter)
{
    MessageListItem messageListItem;

    int num;
    if (critter == obj_dude) {
        num = 200;
    } else {
        if (stat_level(critter, STAT_GENDER) == GENDER_MALE) {
            num = 300;
        } else {
            num = 400;
        }
    }

    if (flags == 0) {
        return;
    }

    if ((flags & DAM_DEAD) != 0) {
        // " and "
        messageListItem.num = 108;
        if (message_search(&combat_message_file, &messageListItem)) {
            strcat(dest, messageListItem.text);
        }

        // were killed
        messageListItem.num = num + 7;
        if (message_search(&combat_message_file, &messageListItem)) {
            strcat(dest, messageListItem.text);
        }

        return;
    }

    int bit = 1;
    int flagsListLength = 0;
    int flagsList[32];
    for (int index = 0; index < 32; index++) {
        if (bit != DAM_CRITICAL && bit != DAM_HIT && (bit & flags) != 0) {
            flagsList[flagsListLength++] = index;
        }
        bit <<= 1;
    }

    if (flagsListLength != 0) {
        for (int index = 0; index < flagsListLength - 1; index++) {
            strcat(dest, ", ");

            messageListItem.num = num + flagsList[index];
            if (message_search(&combat_message_file, &messageListItem)) {
                strcat(dest, messageListItem.text);
            }
        }

        // " and "
        messageListItem.num = 108;
        if (message_search(&combat_message_file, &messageListItem)) {
            strcat(dest, messageListItem.text);
        }

        messageListItem.num = num + flagsList[flagsListLength - 1];
        if (message_search(&combat_message_file, &messageListItem)) {
            strcat(dest, messageListItem.text);
        }
    }
}

// 0x42344C
void combat_anim_begin()
{
    if (++combat_turn_running == 1 && obj_dude == main_ctd.attacker) {
        game_ui_disable(1);
        gmouse_set_cursor(26);
        if (combat_highlight == 2) {
            combat_outline_off();
        }
    }
}

// 0x423490
void combat_anim_finished()
{
    combat_turn_running -= 1;
    if (combat_turn_running != 0) {
        return;
    }

    if (obj_dude == main_ctd.attacker) {
        game_ui_enable();
    }

    if (combat_cleanup_enabled) {
        combat_cleanup_enabled = false;

        Object* weapon = item_hit_with(main_ctd.attacker, main_ctd.hitMode);
        if (weapon != NULL) {
            if (item_w_max_ammo(weapon) > 0) {
                int ammoQuantity = item_w_curr_ammo(weapon);
                item_w_set_curr_ammo(weapon, ammoQuantity - main_ctd.ammoQuantity);

                if (main_ctd.attacker == obj_dude) {
                    intface_update_ammo_lights();
                }
            }
        }

        if (combat_call_display) {
            combat_display(&main_ctd);
            combat_call_display = false;
        }

        apply_damage(&main_ctd, true);

        Object* attacker = main_ctd.attacker;
        if (attacker == obj_dude && combat_highlight == 2) {
            combat_outline_on();
        }

        if (scr_end_combat()) {
            if ((obj_dude->data.critter.combat.results & DAM_KNOCKED_OUT) != 0) {
                if (attacker->data.critter.combat.team == obj_dude->data.critter.combat.team) {
                    combat_ending_guy = obj_dude->data.critter.combat.whoHitMe;
                } else {
                    combat_ending_guy = attacker;
                }
            }
        }

        combat_ctd_init(&main_ctd, main_ctd.attacker, NULL, HIT_MODE_PUNCH, HIT_LOCATION_TORSO);

        if ((attacker->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) != 0) {
            if ((attacker->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD | DAM_LOSE_TURN)) == 0) {
                // NOTE: Uninline.
                combat_standup(attacker);
            }
        }
    }
}

// 0x4235FC
static void combat_standup(Object* critter)
{
    if (critter->data.critter.combat.ap < 3) {
        critter->data.critter.combat.ap = 0;
    } else {
        critter->data.critter.combat.ap -= 3;
    }

    if (critter == obj_dude) {
        intface_update_move_points(obj_dude->data.critter.combat.ap);
    }

    dude_standup(critter);

    // NOTE: Uninline.
    combat_turn_run();
}

// 0x423650
static void print_tohit(unsigned char* dest, int destPitch, int accuracy)
{
    CacheEntry* numbersFrmHandle;
    int numbersFrmFid = art_id(OBJ_TYPE_INTERFACE, 82, 0, 0, 0);
    unsigned char* numbersFrmData = art_ptr_lock_data(numbersFrmFid, 0, 0, &numbersFrmHandle);
    if (numbersFrmData == NULL) {
        return;
    }

    if (accuracy >= 0) {
        buf_to_buf(numbersFrmData + 9 * (accuracy % 10), 9, 17, 360, dest + 9, destPitch);
        buf_to_buf(numbersFrmData + 9 * (accuracy / 10), 9, 17, 360, dest, destPitch);
    } else {
        buf_to_buf(numbersFrmData + 108, 6, 17, 360, dest + 9, destPitch);
        buf_to_buf(numbersFrmData + 108, 6, 17, 360, dest, destPitch);
    }

    art_ptr_unlock(numbersFrmHandle);
}

// 0x423740
static char* combat_get_loc_name(Object* critter, int hitLocation)
{
    MessageListItem messageListItem;
    messageListItem.num = 1000 + 10 * art_alias_num(critter->fid & 0xFFF) + hitLocation;
    if (message_search(&combat_message_file, &messageListItem)) {
        return messageListItem.text;
    }

    return NULL;
}

// 0x4237C8
static void draw_loc_off(int btn, int input)
{
    draw_loc(input, colorTable[992]);
}

// 0x4237D4
static void draw_loc_on(int btn, int input)
{
    draw_loc(input, colorTable[31744]);
}

// 0x4237E0
static void draw_loc(int input, int color)
{
    char* name;

    color |= 0x2000000 | 0x1000000;

    if (input >= 4) {
        name = combat_get_loc_name(call_target, hit_loc_right[input - 4]);
        if (name != NULL) {
            win_print(call_win, name, 0, 351 - text_width(name), call_ty[input - 4] - 86, color);
        }
    } else {
        name = combat_get_loc_name(call_target, hit_loc_left[input]);
        if (name != NULL) {
            win_print(call_win, name, 0, 74, call_ty[input] - 86, color);
        }
    }
}

// 0x42382C
static int get_called_shot_location(Object* critter, int* hit_location, int hit_mode)
{
    call_target = critter;

    int calledShotWindowX = (screenGetWidth() - CALLED_SHOT_WINDOW_WIDTH) / 2;
    int calledShotWindowY = screenGetHeight() != 480
        ? (screenGetHeight() - INTERFACE_BAR_HEIGHT - 1 - CALLED_SHOT_WINDOW_HEIGHT) / 2
        : CALLED_SHOT_WINDOW_Y;
    call_win = win_add(calledShotWindowX,
        calledShotWindowY,
        CALLED_SHOT_WINDOW_WIDTH,
        CALLED_SHOT_WINDOW_HEIGHT,
        colorTable[0],
        WINDOW_MODAL);
    if (call_win == -1) {
        return -1;
    }

    int fid;
    CacheEntry* handle;
    unsigned char* data;

    unsigned char* windowBuffer = win_get_buf(call_win);

    fid = art_id(OBJ_TYPE_INTERFACE, 118, 0, 0, 0);
    data = art_ptr_lock_data(fid, 0, 0, &handle);
    if (data == NULL) {
        win_delete(call_win);
        return -1;
    }

    buf_to_buf(data, CALLED_SHOT_WINDOW_WIDTH, CALLED_SHOT_WINDOW_HEIGHT, CALLED_SHOT_WINDOW_WIDTH, windowBuffer, CALLED_SHOT_WINDOW_WIDTH);
    art_ptr_unlock(handle);

    fid = art_id(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, ANIM_CALLED_SHOT_PIC, 0, 0);
    data = art_ptr_lock_data(fid, 0, 0, &handle);
    if (data != NULL) {
        buf_to_buf(data, 170, 225, 170, windowBuffer + CALLED_SHOT_WINDOW_WIDTH * 31 + 128, CALLED_SHOT_WINDOW_WIDTH);
        art_ptr_unlock(handle);
    }

    fid = art_id(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);

    CacheEntry* upHandle;
    unsigned char* up = art_ptr_lock_data(fid, 0, 0, &upHandle);
    if (up == NULL) {
        win_delete(call_win);
        return -1;
    }

    fid = art_id(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);

    CacheEntry* downHandle;
    unsigned char* down = art_ptr_lock_data(fid, 0, 0, &downHandle);
    if (down == NULL) {
        art_ptr_unlock(upHandle);
        win_delete(call_win);
        return -1;
    }

    // Cancel button
    int btn = win_register_button(call_win,
        170,
        268,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        up,
        down,
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
    }

    int oldFont = text_curr();
    text_font(101);

    for (int index = 0; index < 4; index++) {
        int probability;
        int btn;
        char* hit_location_name;
        int hit_location_name_width;

        probability = determine_to_hit(obj_dude, critter, hit_loc_left[index], hit_mode);
        print_tohit(windowBuffer + CALLED_SHOT_WINDOW_WIDTH * (call_ty[index] - 86) + 33, CALLED_SHOT_WINDOW_WIDTH, probability);

        btn = win_register_button(call_win,
            33,
            call_ty[index] - 82,
            88,
            10,
            index,
            index,
            -1,
            index,
            NULL,
            NULL,
            NULL,
            0);
        win_register_button_func(btn, draw_loc_on, draw_loc_off, NULL, NULL);
        draw_loc_off(btn, index);

        probability = determine_to_hit(obj_dude, critter, hit_loc_right[index], hit_mode);
        print_tohit(windowBuffer + CALLED_SHOT_WINDOW_WIDTH * (call_ty[index] - 86) + 373, CALLED_SHOT_WINDOW_WIDTH, probability);

        hit_location_name = combat_get_loc_name(critter, hit_loc_right[index]);
        if (hit_location_name != NULL) {
            hit_location_name_width = text_width(hit_location_name);
        } else {
            hit_location_name_width = 10;
        }

        btn = win_register_button(call_win,
            351 - hit_location_name_width,
            call_ty[index] - 82,
            88,
            10,
            index + 4,
            index + 4,
            -1,
            index + 4,
            NULL,
            NULL,
            NULL,
            0);
        win_register_button_func(btn, draw_loc_on, draw_loc_off, NULL, NULL);
        draw_loc_off(btn, index + 4);
    }

    win_draw(call_win);

    bool gameUiWasDisabled = game_ui_is_disabled();
    if (gameUiWasDisabled) {
        game_ui_enable();
    }

    gmouse_disable(0);
    gmouse_set_cursor(MOUSE_CURSOR_ARROW);

    int eventCode;
    while (true) {
        sharedFpsLimiter.mark();

        eventCode = get_input();

        if (eventCode == KEY_ESCAPE) {
            break;
        }

        if (eventCode >= 0 && eventCode < HIT_LOCATION_COUNT) {
            break;
        }

        if (game_user_wants_to_quit != 0) {
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    gmouse_enable();

    if (gameUiWasDisabled) {
        game_ui_disable(0);
    }

    text_font(oldFont);

    art_ptr_unlock(downHandle);
    art_ptr_unlock(upHandle);
    win_delete(call_win);

    if (eventCode == KEY_ESCAPE) {
        return -1;
    }

    *hit_location = eventCode < 4 ? hit_loc_left[eventCode] : hit_loc_right[eventCode - 4];

    gsound_play_sfx_file("icsxxxx1");

    return 0;
}

// 0x423C2C
int combat_check_bad_shot(Object* attacker, Object* defender, int hitMode, bool aiming)
{
    Object* weapon;
    int attack_type;

    if ((defender->data.critter.combat.results & DAM_DEAD) != 0) {
        return COMBAT_BAD_SHOT_ALREADY_DEAD;
    }

    weapon = item_hit_with(attacker, hitMode);
    if (weapon != NULL) {
        if ((attacker->data.critter.combat.results & DAM_CRIP_ARM_LEFT) != 0
            && (attacker->data.critter.combat.results & DAM_CRIP_ARM_RIGHT) != 0) {
            return COMBAT_BAD_SHOT_BOTH_ARMS_CRIPPLED;
        }

        if ((attacker->data.critter.combat.results & DAM_CRIP_ARM_ANY) != 0) {
            if (item_w_is_2handed(weapon)) {
                return COMBAT_BAD_SHOT_ARM_CRIPPLED;
            }
        }
    }

    if (item_w_mp_cost(attacker, hitMode, aiming) > attacker->data.critter.combat.ap) {
        return COMBAT_BAD_SHOT_NOT_ENOUGH_AP;
    }

    if (item_w_range(attacker, hitMode) < obj_dist(attacker, defender)) {
        return COMBAT_BAD_SHOT_OUT_OF_RANGE;
    }

    attack_type = item_w_subtype(weapon, hitMode);

    if (item_w_max_ammo(weapon) > 0) {
        if (item_w_curr_ammo(weapon) == 0) {
            return COMBAT_BAD_SHOT_NO_AMMO;
        }
    }

    if (attack_type == ATTACK_TYPE_RANGED || attack_type == ATTACK_TYPE_THROW) {
        if (combat_is_shot_blocked(attacker, attacker->tile, defender->tile, defender, NULL)) {
            return COMBAT_BAD_SHOT_AIM_BLOCKED;
        }
    }

    return COMBAT_BAD_SHOT_OK;
}

// 0x423D14
bool combat_to_hit(Object* target, int* accuracy)
{
    int hitMode;
    bool aiming;
    if (intface_get_attack(&hitMode, &aiming) == -1) {
        return false;
    }

    if (combat_check_bad_shot(obj_dude, target, hitMode, aiming) != COMBAT_BAD_SHOT_OK) {
        return false;
    }

    *accuracy = determine_to_hit(obj_dude, target, HIT_LOCATION_UNCALLED, hitMode);

    return true;
}

// 0x423D98
void combat_attack_this(Object* a1)
{
    if (a1 == NULL) {
        return;
    }

    if ((combat_state & 0x02) == 0) {
        return;
    }

    int hitMode;
    bool aiming;
    if (intface_get_attack(&hitMode, &aiming) == -1) {
        return;
    }

    MessageListItem messageListItem;
    Object* item;
    char formattedText[80];
    const char* sfx;

    int rc = combat_check_bad_shot(obj_dude, a1, hitMode, aiming);
    switch (rc) {
    case COMBAT_BAD_SHOT_NO_AMMO:
        item = item_hit_with(obj_dude, hitMode);
        messageListItem.num = 101; // Out of ammo.
        if (message_search(&combat_message_file, &messageListItem)) {
            display_print(messageListItem.text);
        }

        sfx = gsnd_build_weapon_sfx_name(WEAPON_SOUND_EFFECT_OUT_OF_AMMO, item, hitMode, NULL);
        gsound_play_sfx_file(sfx);
        return;
    case COMBAT_BAD_SHOT_OUT_OF_RANGE:
        messageListItem.num = 102; // Target out of range.
        if (message_search(&combat_message_file, &messageListItem)) {
            display_print(messageListItem.text);
        }
        return;
    case COMBAT_BAD_SHOT_NOT_ENOUGH_AP:
        item = item_hit_with(obj_dude, hitMode);
        messageListItem.num = 100; // You need %d action points.
        if (message_search(&combat_message_file, &messageListItem)) {
            int actionPointsRequired = item_w_mp_cost(obj_dude, hitMode, aiming);
            snprintf(formattedText, sizeof(formattedText), messageListItem.text, actionPointsRequired);
            display_print(formattedText);
        }
        return;
    case COMBAT_BAD_SHOT_ALREADY_DEAD:
        return;
    case COMBAT_BAD_SHOT_AIM_BLOCKED:
        messageListItem.num = 104; // Your aim is blocked.
        if (message_search(&combat_message_file, &messageListItem)) {
            display_print(messageListItem.text);
        }
        return;
    case COMBAT_BAD_SHOT_ARM_CRIPPLED:
        messageListItem.num = 106; // You cannot use two-handed weapons with a crippled arm.
        if (message_search(&combat_message_file, &messageListItem)) {
            display_print(messageListItem.text);
        }
        return;
    case COMBAT_BAD_SHOT_BOTH_ARMS_CRIPPLED:
        messageListItem.num = 105; // You cannot use weapons with both arms crippled.
        if (message_search(&combat_message_file, &messageListItem)) {
            display_print(messageListItem.text);
        }
        return;
    }

    if (!isInCombat()) {
        STRUCT_664980 stru;
        stru.attacker = obj_dude;
        stru.defender = a1;
        stru.actionPointsBonus = 0;
        stru.accuracyBonus = 0;
        stru.damageBonus = 0;
        stru.minDamage = 0;
        stru.maxDamage = INT_MAX;
        stru.field_1C = 0;
        combat(&stru);
        return;
    }

    if (!aiming) {
        combat_attack(obj_dude, a1, hitMode, HIT_LOCATION_UNCALLED);
        return;
    }

    if (aiming != 1) {
        debug_printf("Bad called shot value %d\n", aiming);
    }

    int hitLocation;
    if (get_called_shot_location(a1, &hitLocation, hitMode) != -1) {
        combat_attack(obj_dude, a1, hitMode, hitLocation);
    }
}

// 0x424074
void combat_outline_on()
{
    int index;
    int target_highlight = TARGET_HIGHLIGHT_TARGETING_ONLY;
    Object** critters;
    int critters_length;
    int outline_type;

    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TARGET_HIGHLIGHT_KEY, &target_highlight);
    if (target_highlight == TARGET_HIGHLIGHT_OFF) {
        return;
    }

    if (gmouse_3d_get_mode() != GAME_MOUSE_MODE_CROSSHAIR) {
        return;
    }

    if (isInCombat()) {
        for (index = 0; index < list_total; index++) {
            if (combat_list[index] != obj_dude && (combat_list[index]->data.critter.combat.results & DAM_DEAD) == 0) {
                obj_turn_on_outline(combat_list[index], NULL);
            }
        }
    } else {
        critters_length = obj_create_list(-1, map_elevation, OBJ_TYPE_CRITTER, &critters);
        for (index = 0; index < critters_length; index++) {
            if (critters[index] != obj_dude && (critters[index]->data.critter.combat.results & DAM_DEAD) == 0) {
                outline_type = OUTLINE_TYPE_HOSTILE;
                if (perk_level(PERK_FRIENDLY_FOE)) {
                    if (critters[index]->data.critter.combat.team == obj_dude->data.critter.combat.team) {
                        outline_type = OUTLINE_TYPE_FRIENDLY;
                    }
                }

                obj_outline_object(critters[index], outline_type, NULL);
                obj_turn_on_outline(critters[index], NULL);
            }
        }

        if (critters_length != 0) {
            obj_delete_list(critters);
        }
    }

    tile_refresh_display();
}

// 0x4241B0
void combat_outline_off()
{
    int i;
    int v5;
    Object** v9;

    if (combat_state & 1) {
        for (i = 0; i < list_total; i++) {
            obj_turn_off_outline(combat_list[i], NULL);
        }
    } else {
        v5 = obj_create_list(-1, map_elevation, 1, &v9);
        for (i = 0; i < v5; i++) {
            obj_turn_off_outline(v9[i], NULL);
            obj_remove_outline(v9[i], NULL);
        }
        if (v5) {
            obj_delete_list(v9);
        }
    }

    tile_refresh_display();
}

// 0x424254
void combat_highlight_change()
{
    int targetHighlight = 2;
    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TARGET_HIGHLIGHT_KEY, &targetHighlight);
    if (targetHighlight != combat_highlight && isInCombat()) {
        if (targetHighlight != 0) {
            if (combat_highlight == 0) {
                combat_outline_on();
            }
        } else {
            combat_outline_off();
        }
    }

    combat_highlight = targetHighlight;
}

// 0x4242B4
bool combat_is_shot_blocked(Object* a1, int from, int to, Object* a4, int* a5)
{
    Object* obstacle = a1;

    if (a5 != NULL) {
        *a5 = 0;
    }

    while (obstacle != NULL && from != to) {
        make_straight_path(a1, from, to, NULL, &obstacle, 32);
        if (obstacle != NULL) {
            if (FID_TYPE(obstacle->fid) != OBJ_TYPE_CRITTER) {
                return true;
            }

            if (a5 != NULL) {
                if (obstacle != a4) {
                    *a5 += 1;
                }
            }

            from = obstacle->tile;
        }
    }

    return false;
}

// 0x424338
int combat_player_knocked_out_by()
{
    if ((obj_dude->data.critter.combat.results & DAM_DEAD) != 0) {
        return -1;
    }

    if (combat_ending_guy == NULL) {
        return -1;
    }

    return combat_ending_guy->data.critter.combat.team;
}

// 0x42435C
int combat_explode_scenery(Object* a1, Object* a2)
{
    scr_explode_scenery(a1, a1->tile, 3, a1->elevation);
    return 0;
}

// 0x424374
void combat_delete_critter(Object* obj)
{
    // TODO: Check entire function.
    if (!isInCombat()) {
        return;
    }

    if (list_total == 0) {
        return;
    }

    int i;
    for (i = 0; i < list_total; i++) {
        if (obj == combat_list[i]) {
            break;
        }
    }

    if (i == list_total) {
        return;
    }

    list_total--;

    combat_list[list_total] = obj;

    if (i >= list_com) {
        if (i < (list_noncom + list_com)) {
            list_noncom--;
        }
    } else {
        list_com--;
    }

    obj->data.critter.combat.ap = 0;
    obj_remove_outline(obj, NULL);

    obj->data.critter.combat.whoHitMe = NULL;
    combatai_delete_critter(obj);
}

} // namespace fallout
