#include "game/combatai.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/actions.h"
#include "game/anim.h"
#include "game/combat.h"
#include "game/config.h"
#include "game/critter.h"
#include "game/display.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gsound.h"
#include "game/intface.h"
#include "game/item.h"
#include "game/light.h"
#include "game/map.h"
#include "game/object.h"
#include "game/perk.h"
#include "game/protinst.h"
#include "game/proto.h"
#include "game/roll.h"
#include "game/scripts.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/textobj.h"
#include "game/tile.h"
#include "platform_compat.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"

namespace fallout {

typedef enum HurtTooMuch {
    HURT_BLIND,
    HURT_CRIPPLED,
    HURT_CRIPPLED_LEGS,
    HURT_CRIPPLED_ARMS,
    HURT_COUNT,
} HurtTooMuch;

static void parse_hurt_str(char* str, int* out_value);
static AiPacket* ai_cap(Object* obj);
static int ai_magic_hands(Object* critter, Object* item, int num);
static int ai_check_drugs(Object* critter);
static void ai_run_away(Object* critter);
static int compare_nearer(const void* critter_ptr1, const void* critter_ptr2);
static void ai_sort_list(Object** critterList, int length, Object* origin);
static Object* ai_find_nearest_team(Object* critter, Object* other, int flags);
static int ai_find_attackers(Object* critter, Object** a2, Object** a3, Object** a4);
static Object* ai_have_ammo(Object* critter, Object* weapon);
static Object* ai_best_weapon(Object* weapon1, Object* weapon2);
static bool ai_can_use_weapon(Object* critter, Object* weapon, int hitMode);
static Object* ai_search_environ(Object* critter, int itemType);
static Object* ai_retrieve_object(Object* critter, Object* item);
static int ai_pick_hit_mode(Object* critter, Object* weapon);
static int ai_move_closer(Object* critter, Object* target, int a3);
static int ai_switch_weapons(Object* critter, int* hit_mode, Object** weapon);
static int ai_called_shot(Object* critter, Object* target, int hit_mode);
static int ai_attack(Object* critter, Object* target, int hit_mode);
static int ai_try_attack(Object* critter, Object* target);
static int ai_print_msg(Object* critter, int type);
static int combatai_rating(Object* obj);
static int combatai_load_messages();
static int combatai_unload_messages();

// 0x504BF8
static Object* combat_obj = NULL;

// 0x504BFC
static int num_caps = 0;

// 0x504C00
static bool combatai_is_initialized = false;

// 0x504C04
static const char* matchHurtStrs[HURT_COUNT] = {
    "blind",
    "crippled",
    "crippled_legs",
    "crippled_arms",
};

// 0x518124
static int rmatchHurtVals[HURT_COUNT] = {
    DAM_BLIND,
    DAM_CRIP_LEG_LEFT | DAM_CRIP_LEG_RIGHT | DAM_CRIP_ARM_LEFT | DAM_CRIP_ARM_RIGHT,
    DAM_CRIP_LEG_LEFT | DAM_CRIP_LEG_RIGHT,
    DAM_CRIP_ARM_LEFT | DAM_CRIP_ARM_RIGHT,
};

// ai.msg
//
// 0x56BEB0
static MessageList ai_message_file;

// 0x56BEB8
static AiPacket* cap;

// 0x56BE10
static char target_str[80];

// 0x56BEBC
static int curr_crit_num;

// 0x56BEC0
static Object** curr_crit_list;

// 0x56BE60
static char attack_str[80];

// 0x424450
static void parse_hurt_str(char* str, int* value)
{
    size_t comma_pos;
    char comma;
    int index;

    *value = 0;

    compat_strlwr(str);

    while (*str != '\0') {
        str += strspn(str, " ");

        comma_pos = strcspn(str, ",");
        comma = str[comma_pos];
        str[comma_pos] = '\0';

        for (index = 0; index < HURT_COUNT; index++) {
            if (strcmp(str, matchHurtStrs[index]) == 0) {
                *value |= rmatchHurtVals[index];
                break;
            }
        }

        if (index == HURT_COUNT) {
            debug_printf("Unrecognized flag: %s\n", str);
        }

        str[comma_pos] = comma;

        if (comma == '\0') {
            break;
        }

        str += comma_pos + 1;
    }
}

// 0x4244F0
int combat_ai_init()
{
    int index;
    Config config;
    int rc = 0;

    if (combatai_load_messages() == -1) {
        return -1;
    }

    num_caps = 0;

    if (!config_init(&config)) {
        return -1;
    }

    if (config_load(&config, "data\\ai.txt", true)) {
        cap = (AiPacket*)mem_malloc(sizeof(*cap) * config.size);
        if (cap != NULL) {
            for (index = 0; index < config.size; index++) {
                cap[index].name = NULL;
            }

            for (index = 0; index < config.size; index++) {
                assoc_pair* sectionEntry = &(config.list[index]);
                AiPacket* ai = &(cap[index]);
                char* stringValue;

                ai->name = mem_strdup(sectionEntry->name);
                if (ai->name == NULL) break;

                if (!config_get_value(&config, sectionEntry->name, "packet_num", &(ai->packet_num))) break;
                if (!config_get_value(&config, sectionEntry->name, "max_dist", &(ai->max_dist))) break;
                if (!config_get_value(&config, sectionEntry->name, "min_to_hit", &(ai->min_to_hit))) break;
                if (!config_get_value(&config, sectionEntry->name, "min_hp", &(ai->min_hp))) break;
                if (!config_get_value(&config, sectionEntry->name, "aggression", &(ai->aggression))) break;
                if (!config_get_string(&config, sectionEntry->name, "hurt_too_much", &stringValue)) break;
                parse_hurt_str(stringValue, &(ai->hurt_too_much));
                if (!config_get_value(&config, sectionEntry->name, "secondary_freq", &(ai->secondary_freq))) break;
                if (!config_get_value(&config, sectionEntry->name, "called_freq", &(ai->called_freq))) break;
                if (!config_get_value(&config, sectionEntry->name, "font", &(ai->font))) break;
                if (!config_get_value(&config, sectionEntry->name, "color", &(ai->color))) break;
                if (!config_get_value(&config, sectionEntry->name, "outline_color", &(ai->outline_color))) break;
                if (!config_get_value(&config, sectionEntry->name, "chance", &(ai->chance))) break;
                if (!config_get_value(&config, sectionEntry->name, "run_start", &(ai->run_start))) break;
                if (!config_get_value(&config, sectionEntry->name, "move_start", &(ai->move_start))) break;
                if (!config_get_value(&config, sectionEntry->name, "attack_start", &(ai->attack_start))) break;
                if (!config_get_value(&config, sectionEntry->name, "miss_start", &(ai->miss_start))) break;
                if (!config_get_value(&config, sectionEntry->name, "hit_head_start", &(ai->hit_start[HIT_LOCATION_HEAD]))) break;
                if (!config_get_value(&config, sectionEntry->name, "hit_left_arm_start", &(ai->hit_start[HIT_LOCATION_LEFT_ARM]))) break;
                if (!config_get_value(&config, sectionEntry->name, "hit_right_arm_start", &(ai->hit_start[HIT_LOCATION_RIGHT_ARM]))) break;
                if (!config_get_value(&config, sectionEntry->name, "hit_torso_start", &(ai->hit_start[HIT_LOCATION_TORSO]))) break;
                if (!config_get_value(&config, sectionEntry->name, "hit_right_leg_start", &(ai->hit_start[HIT_LOCATION_RIGHT_LEG]))) break;
                if (!config_get_value(&config, sectionEntry->name, "hit_left_leg_start", &(ai->hit_start[HIT_LOCATION_LEFT_LEG]))) break;
                if (!config_get_value(&config, sectionEntry->name, "hit_eyes_start", &(ai->hit_start[HIT_LOCATION_EYES]))) break;
                if (!config_get_value(&config, sectionEntry->name, "hit_groin_start", &(ai->hit_start[HIT_LOCATION_GROIN]))) break;
                if (!config_get_value(&config, sectionEntry->name, "last_msg", &(ai->last_msg))) break;
            }

            if (index < config.size) {
                for (index = 0; index < config.size; index++) {
                    if (cap[index].name != NULL) {
                        mem_free(cap[index].name);
                    }
                }
                mem_free(cap);
                debug_printf("Error processing ai.txt");
                rc = -1;
            } else {
                num_caps = config.size;
            }
        } else {
            rc = -1;
        }
    } else {
        rc = -1;
    }

    config_exit(&config);

    if (rc == 0) {
        combatai_is_initialized = true;
    }

    return rc;
}

// 0x424A0C
void combat_ai_reset()
{
}

// 0x424A10
int combat_ai_exit()
{
    int index;

    for (index = 0; index < num_caps; index++) {
        AiPacket* ai = &(cap[index]);

        if (ai->name != NULL) {
            mem_free(ai->name);
            ai->name = NULL;
        }
    }

    mem_free(cap);
    num_caps = 0;

    combatai_is_initialized = false;

    // NOTE: Uninline.
    if (combatai_unload_messages() != 0) {
        return -1;
    }

    return 0;
}

// 0x424A0C
int combat_ai_load(DB_FILE* stream)
{
    return 0;
}

// 0x424A0C
int combat_ai_save(DB_FILE* stream)
{
    return 0;
}

// 0x424A7C
int combat_ai_num()
{
    return num_caps;
}

// 0x424A84
char* combat_ai_name(int packet_num)
{
    int index;

    if (packet_num < 0 || packet_num >= num_caps) {
        return NULL;
    }

    for (index = 0; index < num_caps; index++) {
        if (cap[index].packet_num == packet_num) {
            return cap[index].name;
        }
    }

    return NULL;
}

// 0x424AD8
static AiPacket* ai_cap(Object* obj)
{
    int index;
    int packet_num = obj->data.critter.combat.aiPacket;
    for (index = 0; index < num_caps; index++) {
        if (packet_num == cap[index].packet_num) {
            return &(cap[index]);
        }
    }

    debug_printf("Missing AI Packet\n");

    return &(cap[0]);
}

// 0x424B40
static int ai_magic_hands(Object* critter, Object* item, int num)
{
    register_begin(ANIMATION_REQUEST_RESERVED);
    register_object_animate(critter, ANIM_MAGIC_HANDS_MIDDLE, 0);
    if (register_end() == 0) {
        combat_turn_run();
    }

    if (num != -1) {
        MessageListItem messageListItem;
        messageListItem.num = num;
        if (message_search(&misc_message_file, &messageListItem)) {
            char text[200];
            if (item != NULL) {
                snprintf(text, sizeof(text), "%s %s %s.", object_name(critter), messageListItem.text, object_name(item));
            } else {
                snprintf(text, sizeof(text), "%s %s.", object_name(critter), messageListItem.text);
            }

            display_print(text);
        }
    }

    return 0;
}

// 0x424C00
static int ai_check_drugs(Object* critter)
{
    int bloodied;
    int index = -1;
    Object* drug;

    if (critter_body_type(critter) != BODY_TYPE_BIPED) {
        return 0;
    }

    bloodied = stat_level(critter, STAT_MAXIMUM_HIT_POINTS) / 2;
    while (stat_level(critter, STAT_CURRENT_HIT_POINTS) < bloodied) {
        if (critter->data.critter.combat.ap < 2) {
            break;
        }

        drug = inven_find_type(critter, ITEM_TYPE_DRUG, &index);
        if (drug == NULL) {
            break;
        }

        if (drug->pid == PROTO_ID_STIMPACK || drug->pid == PROTO_ID_SUPER_STIMPACK) {
            if (item_remove_mult(critter, drug, 1) == 0) {
                if (item_d_take_drug(critter, drug) == -1) {
                    item_add_force(critter, drug, 1);
                } else {
                    ai_magic_hands(critter, drug, 5000);
                    obj_connect(drug, critter->tile, critter->elevation, NULL);
                    obj_destroy(drug);
                }

                if (critter->data.critter.combat.ap < 2) {
                    critter->data.critter.combat.ap = 0;
                } else {
                    critter->data.critter.combat.ap -= 2;
                }

                index = -1;
            }
        }
    }

    return 0;
}

// 0x424D00
static void ai_run_away(Object* critter)
{
    CritterCombatData* combatData = &(critter->data.critter.combat);
    AiPacket* ai;
    int distance;

    ai = ai_cap(critter);
    distance = obj_dist(critter, obj_dude);
    if (distance < ai->max_dist) {
        int rotation;
        int destination;
        int action_points;

        combatData->maneuver |= CRITTER_MANUEVER_FLEEING;

        rotation = tile_dir(obj_dude->tile, critter->tile);
        for (action_points = combatData->ap; action_points > 0; action_points -= 1) {
            destination = tile_num_in_direction(critter->tile, rotation, action_points);
            if (make_path(critter, critter->tile, destination, NULL, 1) > 0) {
                break;
            }

            destination = tile_num_in_direction(critter->tile, (rotation + 1) % ROTATION_COUNT, action_points);
            if (make_path(critter, critter->tile, destination, NULL, 1) > 0) {
                break;
            }

            destination = tile_num_in_direction(critter->tile, (rotation + 5) % ROTATION_COUNT, action_points);
            if (make_path(critter, critter->tile, destination, NULL, 1) > 0) {
                break;
            }
        }

        if (action_points > 0) {
            register_begin(ANIMATION_REQUEST_RESERVED);
            combatai_msg(critter, NULL, AI_MESSAGE_TYPE_RUN, 0);
            register_object_run_to_tile(critter, destination, critter->elevation, combatData->ap, 0);
            if (register_end() == 0) {
                combat_turn_run();
            }
        }
    } else {
        combatData->maneuver |= CRITTER_MANEUVER_DISENGAGING;
    }
}

// Compare objects by distance to origin.
//
// 0x424E30
static int compare_nearer(const void* critter_ptr1, const void* critter_ptr2)
{
    Object* critter1 = *(Object**)critter_ptr1;
    Object* critter2 = *(Object**)critter_ptr2;
    int distance1;
    int distance2;

    if (critter1 == NULL) {
        if (critter2 == NULL) {
            return 0;
        }
        return 1;
    } else {
        if (critter2 == NULL) {
            return -1;
        }
    }

    distance1 = obj_dist(critter1, combat_obj);
    distance2 = obj_dist(critter2, combat_obj);

    if (distance1 < distance2) {
        return -1;
    } else if (distance1 > distance2) {
        return 1;
    } else {
        return 0;
    }
}

// 0x424E88
static void ai_sort_list(Object** critterList, int length, Object* origin)
{
    combat_obj = origin;
    qsort(critterList, length, sizeof(*critterList), compare_nearer);
}

// 0x424EA0
static Object* ai_find_nearest_team(Object* critter, Object* other, int flags)
{
    int index;
    Object* candidate;

    if (other == NULL) {
        return NULL;
    }

    if (curr_crit_num == 0) {
        return NULL;
    }

    // NOTE: Uninline.
    ai_sort_list(curr_crit_list, curr_crit_num, critter);

    for (index = 0; index < curr_crit_num; index++) {
        candidate = curr_crit_list[index];
        if (critter != candidate) {
            if ((candidate->data.critter.combat.results & DAM_DEAD) == 0) {
                if ((flags & 0x2) != 0) {
                    if (other->data.critter.combat.team != candidate->data.critter.combat.team) {
                        return candidate;
                    }
                }

                if ((flags & 0x1) != 0) {
                    if (other->data.critter.combat.team == candidate->data.critter.combat.team) {
                        return candidate;
                    }
                }
            }
        }
    }

    return NULL;
}

// 0x424F58
static int ai_find_attackers(Object* critter, Object** a2, Object** a3, Object** a4)
{
    if (a2 != NULL) {
        *a2 = NULL;
    }

    if (a3 != NULL) {
        *a3 = NULL;
    }

    if (*a4 != NULL) {
        *a4 = NULL;
    }

    if (curr_crit_num == 0) {
        return 0;
    }

    // NOTE: Uninline.
    ai_sort_list(curr_crit_list, curr_crit_num, critter);

    int foundTargetCount = 0;
    int team = critter->data.critter.combat.team;

    for (int index = 0; foundTargetCount < 3 && index < curr_crit_num; index++) {
        Object* candidate = curr_crit_list[index];
        if (candidate != critter) {
            if (a2 != NULL && *a2 == NULL) {
                if ((candidate->data.critter.combat.results & DAM_DEAD) == 0
                    && candidate->data.critter.combat.whoHitMe == critter) {
                    foundTargetCount++;
                    *a2 = candidate;
                }
            }

            if (a3 != NULL && *a3 == NULL) {
                if (team == candidate->data.critter.combat.team) {
                    Object* whoHitCandidate = candidate->data.critter.combat.whoHitMe;
                    if (whoHitCandidate != NULL
                        && whoHitCandidate != critter
                        && team != whoHitCandidate->data.critter.combat.team
                        && (whoHitCandidate->data.critter.combat.results & DAM_DEAD) == 0) {
                        foundTargetCount++;
                        *a3 = whoHitCandidate;
                    }
                }
            }

            if (a4 != NULL && *a4 == NULL) {
                if (candidate->data.critter.combat.team != team
                    && (candidate->data.critter.combat.results & DAM_DEAD) == 0) {
                    Object* whoHitCandidate = candidate->data.critter.combat.whoHitMe;
                    if (whoHitCandidate != NULL
                        && whoHitCandidate->data.critter.combat.team == team) {
                        foundTargetCount++;
                        *a4 = candidate;
                    }
                }
            }
        }
    }

    return 0;
}

// 0x4250C8
Object* ai_danger_source(Object* critter)
{
    Object* who_hit_me;
    Object* targets[4];
    int index;

    who_hit_me = critter->data.critter.combat.whoHitMe;
    if (who_hit_me == NULL || critter == who_hit_me) {
        targets[0] = NULL;
    } else {
        if ((who_hit_me->data.critter.combat.results & DAM_DEAD) == 0) {
            return who_hit_me;
        }

        if (who_hit_me->data.critter.combat.team != critter->data.critter.combat.team) {
            targets[0] = ai_find_nearest_team(critter, who_hit_me, 1);
        } else {
            targets[0] = NULL;
        }
    }

    ai_find_attackers(critter, &(targets[1]), &(targets[2]), &(targets[3]));
    ai_sort_list(targets, 4, critter);

    for (index = 0; index < 4; index++) {
        if (targets[index] != NULL && is_within_perception(critter, targets[index])) {
            return targets[index];
        }
    }

    return NULL;
}

// 0x425174
static Object* ai_have_ammo(Object* critter, Object* weapon)
{
    int inventory_item_index;
    Object* ammo;

    inventory_item_index = -1;

    while (1) {
        ammo = inven_find_type(critter, ITEM_TYPE_AMMO, &inventory_item_index);
        if (ammo == NULL) {
            break;
        }

        if (item_w_can_reload(weapon, ammo)) {
            return ammo;
        }
    }

    return NULL;
}

// 0x4251B0
static Object* ai_best_weapon(Object* weapon1, Object* weapon2)
{
    int attack_type1;
    int attack_type2;

    if (weapon1 == NULL) {
        return weapon2;
    }

    if (weapon2 == NULL) {
        return weapon1;
    }

    attack_type1 = item_w_subtype(weapon1, HIT_MODE_LEFT_WEAPON_PRIMARY);
    attack_type2 = item_w_subtype(weapon2, HIT_MODE_LEFT_WEAPON_PRIMARY);

    if (attack_type1 != attack_type2) {
        if (attack_type1 == ATTACK_TYPE_RANGED) {
            return weapon1;
        }

        if (attack_type2 == ATTACK_TYPE_RANGED) {
            return weapon2;
        }

        if (attack_type1 == ATTACK_TYPE_THROW) {
            return weapon1;
        }

        if (attack_type2 == ATTACK_TYPE_THROW) {
            return weapon2;
        }
    }

    if (item_cost(weapon2) > item_cost(weapon1)) {
        return weapon2;
    } else {
        return weapon1;
    }
}

// 0x425210
static bool ai_can_use_weapon(Object* critter, Object* weapon, int hitMode)
{
    int damageFlags;
    int fid;

    damageFlags = critter->data.critter.combat.results;
    if ((damageFlags & DAM_CRIP_ARM_LEFT) != 0 && (damageFlags & DAM_CRIP_ARM_RIGHT) != 0) {
        return false;
    }

    if ((damageFlags & DAM_CRIP_ARM_ANY) != 0 && item_w_is_2handed(weapon)) {
        return false;
    }

    fid = art_id(OBJ_TYPE_CRITTER,
        critter->fid & 0xFFF,
        item_w_anim_weap(weapon, hitMode),
        item_w_anim_code(weapon),
        critter->rotation + 1);
    if (!art_exists(fid)) {
        return false;
    }

    if (skill_level(critter, item_w_skill(weapon, hitMode)) < ai_cap(critter)->min_to_hit) {
        return false;
    }

    return true;
}

// 0x4299A0
Object* ai_search_inven(Object* critter, int check_action_points)
{
    int body_type;
    int inventory_item_index = -1;
    Object* best_weapon;
    Object* current_item;
    Object* candidate;

    body_type = critter_body_type(critter);
    if (body_type != BODY_TYPE_BIPED
        && body_type != BODY_TYPE_ROBOTIC) {
        return NULL;
    }

    best_weapon = NULL;
    current_item = inven_right_hand(critter);
    while (true) {
        candidate = inven_find_type(critter, ITEM_TYPE_WEAPON, &inventory_item_index);
        if (candidate == NULL) {
            break;
        }

        if (candidate == current_item) {
            continue;
        }

        if (check_action_points) {
            if (item_w_primary_mp_cost(candidate) > critter->data.critter.combat.ap) {
                continue;
            }
        }

        if (!ai_can_use_weapon(critter, candidate, HIT_MODE_RIGHT_WEAPON_PRIMARY)) {
            continue;
        }

        if (item_w_subtype(candidate, HIT_MODE_RIGHT_WEAPON_PRIMARY) == ATTACK_TYPE_RANGED) {
            if (item_w_curr_ammo(candidate) == 0) {
                if (ai_have_ammo(critter, candidate) == NULL) {
                    continue;
                }
            }
        }

        best_weapon = ai_best_weapon(best_weapon, candidate);
    }

    return best_weapon;
}

// 0x425358
static Object* ai_search_environ(Object* critter, int itemType)
{
    Object** objects;
    int count;
    int max_distance;
    Object* current_item;
    Object* found_item;
    int index;

    if (critter_body_type(critter) != BODY_TYPE_BIPED) {
        return NULL;
    }

    count = obj_create_list(-1, map_elevation, OBJ_TYPE_ITEM, &objects);
    if (count == 0) {
        return NULL;
    }

    // NOTE: Uninline.
    ai_sort_list(objects, count, critter);

    max_distance = stat_level(critter, STAT_PERCEPTION) + 5;
    current_item = inven_right_hand(critter);

    found_item = NULL;

    for (index = 0; index < count; index++) {
        int distance;
        Object* item;

        item = objects[index];
        distance = obj_dist(critter, item);
        if (distance > max_distance) {
            break;
        }

        if (item_get_type(item) == itemType) {
            switch (itemType) {
            case ITEM_TYPE_WEAPON:
                if (ai_can_use_weapon(critter, item, HIT_MODE_RIGHT_WEAPON_PRIMARY)) {
                    found_item = item;
                }
                break;
            case ITEM_TYPE_AMMO:
                if (item_w_can_reload(current_item, item)) {
                    found_item = item;
                }
                break;
            }

            if (found_item != NULL) {
                break;
            }
        }
    }

    obj_delete_list(objects);

    return found_item;
}

// 0x425468
static Object* ai_retrieve_object(Object* critter, Object* item)
{
    if (action_get_an_object(critter, item) != 0) {
        return NULL;
    }

    combat_turn_run();

    return inven_find_id(critter, item->id);
}

// 0x425490
static int ai_pick_hit_mode(Object* critter, Object* weapon)
{
    int attack_type;

    if (weapon == NULL) {
        return HIT_MODE_PUNCH;
    }

    if (item_get_type(weapon) != ITEM_TYPE_WEAPON) {
        return HIT_MODE_PUNCH;
    }

    attack_type = item_w_subtype(weapon, HIT_MODE_RIGHT_WEAPON_SECONDARY);
    if (attack_type == ATTACK_TYPE_NONE) {
        return HIT_MODE_RIGHT_WEAPON_PRIMARY;
    }

    if (!ai_can_use_weapon(critter, weapon, HIT_MODE_RIGHT_WEAPON_SECONDARY)) {
        return HIT_MODE_RIGHT_WEAPON_PRIMARY;
    }

    if (roll_random(1, ai_cap(critter)->secondary_freq) != 1) {
        return HIT_MODE_RIGHT_WEAPON_PRIMARY;
    }

    if (attack_type == ATTACK_TYPE_THROW) {
        if (ai_search_inven(critter, 0) == NULL) {
            if (stat_result(critter, STAT_INTELLIGENCE, 0, NULL) > 1) {
                return HIT_MODE_RIGHT_WEAPON_PRIMARY;
            }
        }
    }

    return HIT_MODE_RIGHT_WEAPON_SECONDARY;
}

// 0x425534
static int ai_move_closer(Object* critter, Object* target, int a3)
{
    if (obj_dist(critter, target) <= 1) {
        return -1;
    }

    register_begin(ANIMATION_REQUEST_RESERVED);

    if (a3) {
        combatai_msg(critter, NULL, AI_MESSAGE_TYPE_MOVE, 0);
    }

    register_object_move_to_object(critter, target, critter->data.critter.combat.ap, 0);

    if (register_end() != 0) {
        return -1;
    }

    combat_turn_run();

    return 0;
}

// 0x425590
static int ai_switch_weapons(Object* critter, int* hit_mode, Object** weapon)
{
    Object* best_weapon;
    Object* retrieved_best_weapon;

    *weapon = NULL;
    *hit_mode = HIT_MODE_PUNCH;

    best_weapon = ai_search_inven(critter, 1);
    if (best_weapon != NULL) {
        *weapon = best_weapon;
        *hit_mode = ai_pick_hit_mode(critter, best_weapon);
    } else {
        best_weapon = ai_search_environ(critter, ITEM_TYPE_WEAPON);
        if (best_weapon != NULL) {
            retrieved_best_weapon = ai_retrieve_object(critter, best_weapon);
            if (retrieved_best_weapon != NULL) {
                *weapon = retrieved_best_weapon;
                *hit_mode = ai_pick_hit_mode(critter, retrieved_best_weapon);
            }
        }
    }

    if (*weapon != NULL) {
        inven_wield(critter, *weapon, 1);
        combat_turn_run();
        if (item_w_mp_cost(critter, *hit_mode, 0) <= critter->data.critter.combat.ap) {
            return 0;
        }
    }

    return -1;
}

// 0x425654
static int ai_called_shot(Object* critter, Object* target, int hit_mode)
{
    AiPacket* ai;
    int hit_location;
    int min_intelligence;
    int to_hit;
    int combat_difficulty;

    hit_location = HIT_LOCATION_TORSO;

    if (item_w_mp_cost(critter, hit_mode, 1) <= critter->data.critter.combat.ap) {
        if (item_w_called_shot(critter, hit_mode)) {
            ai = ai_cap(critter);
            if (roll_random(1, ai->called_freq) == 1) {
                combat_difficulty = COMBAT_DIFFICULTY_NORMAL;
                config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_DIFFICULTY_KEY, &combat_difficulty);
                switch (combat_difficulty) {
                case COMBAT_DIFFICULTY_EASY:
                    min_intelligence = 7;
                    break;
                case COMBAT_DIFFICULTY_NORMAL:
                    min_intelligence = 5;
                    break;
                case COMBAT_DIFFICULTY_HARD:
                    min_intelligence = 3;
                    break;
                }

                if (stat_level(critter, STAT_INTELLIGENCE) >= min_intelligence) {
                    hit_location = roll_random(0, 8);
                    to_hit = determine_to_hit(critter, target, hit_location, hit_mode);
                    if (to_hit < ai->min_to_hit) {
                        hit_location = HIT_LOCATION_TORSO;
                    }
                }
            }
        }
    }

    return hit_location;
}

// 0x42572C
static int ai_attack(Object* critter, Object* target, int hit_mode)
{
    int hit_location;

    if (critter->data.critter.combat.maneuver & CRITTER_MANUEVER_FLEEING) {
        return -1;
    }

    register_begin(ANIMATION_REQUEST_RESERVED);
    register_object_turn_towards(critter, target->tile);
    register_end();
    combat_turn_run();

    hit_location = ai_called_shot(critter, target, hit_mode);
    if (combat_attack(critter, target, hit_mode, hit_location)) {
        return -1;
    }

    combat_turn_run();

    return 0;
}

// 0x4257BC
static int ai_try_attack(Object* critter, Object* target)
{
    int combat_taunts = 1;
    Object* weapon;
    int hit_mode;
    int attempt;
    int bad_shot;
    Object* ammo;
    int remaining_rounds;
    int volume;
    const char* sfx;
    int action_points;
    int to_hit;

    critter_set_who_hit_me(critter, target);

    weapon = inven_right_hand(critter);
    if (weapon != NULL && item_get_type(weapon) != ITEM_TYPE_WEAPON) {
        weapon = NULL;
    }

    hit_mode = ai_pick_hit_mode(critter, weapon);

    if (weapon == NULL) {
        if (critter_body_type(target) != BODY_TYPE_BIPED
            || (target->fid & 0xF000) >> 12 != 0
            || !art_exists(art_id(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, ANIM_THROW_PUNCH, 0, critter->rotation + 1))) {
            ai_switch_weapons(critter, &hit_mode, &weapon);
        }
    }

    for (attempt = 0; attempt < 10; attempt++) {
        if ((critter->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD | DAM_LOSE_TURN)) != 0) {
            break;
        }

        bad_shot = combat_check_bad_shot(critter, target, hit_mode, false);
        switch (bad_shot) {
        case COMBAT_BAD_SHOT_OK:
            to_hit = determine_to_hit(critter, target, HIT_LOCATION_UNCALLED, hit_mode);
            if (to_hit >= ai_cap(critter)->min_to_hit) {
                if (ai_attack(critter, target, hit_mode) == -1) {
                    return -1;
                }

                if (item_w_mp_cost(critter, hit_mode, 0) > critter->data.critter.combat.ap) {
                    return -1;
                }
            } else {
                to_hit = determine_to_hit_no_range(critter, target, HIT_LOCATION_UNCALLED, hit_mode);
                if (to_hit < ai_cap(critter)->min_to_hit) {
                    ai_run_away(critter);
                    return 0;
                }

                if (ai_move_closer(critter, target, combat_taunts) == -1) {
                    ai_run_away(critter);
                    return 0;
                }

                combat_taunts = 0;
            }
            break;
        case COMBAT_BAD_SHOT_NO_AMMO:
            ammo = ai_have_ammo(critter, weapon);
            if (ammo) {
                remaining_rounds = item_w_reload(weapon, ammo);
                if (remaining_rounds == 0) {
                    obj_destroy(ammo);
                }

                if (remaining_rounds != -1) {
                    volume = gsound_compute_relative_volume(critter);
                    sfx = gsnd_build_weapon_sfx_name(WEAPON_SOUND_EFFECT_READY, weapon, hit_mode, NULL);
                    gsound_play_sfx_file_volume(sfx, volume);
                    ai_magic_hands(critter, weapon, 5002);

                    action_points = critter->data.critter.combat.ap;
                    if (action_points >= 2) {
                        critter->data.critter.combat.ap = action_points - 2;
                    } else {
                        critter->data.critter.combat.ap = 0;
                    }
                }
            } else {
                ammo = ai_search_environ(critter, ITEM_TYPE_AMMO);
                if (ammo != NULL) {
                    ammo = ai_retrieve_object(critter, ammo);
                    if (ammo != NULL) {
                        remaining_rounds = item_w_reload(weapon, ammo);
                        if (remaining_rounds == 0) {
                            obj_destroy(ammo);
                        }

                        if (remaining_rounds != -1) {
                            volume = gsound_compute_relative_volume(critter);
                            sfx = gsnd_build_weapon_sfx_name(WEAPON_SOUND_EFFECT_READY, weapon, hit_mode, NULL);
                            gsound_play_sfx_file_volume(sfx, volume);
                            ai_magic_hands(critter, weapon, 5002);

                            action_points = critter->data.critter.combat.ap;
                            if (action_points >= 2) {
                                critter->data.critter.combat.ap = action_points - 2;
                            } else {
                                critter->data.critter.combat.ap = 0;
                            }
                        }
                    }
                } else {
                    volume = gsound_compute_relative_volume(critter);
                    sfx = gsnd_build_weapon_sfx_name(WEAPON_SOUND_EFFECT_OUT_OF_AMMO, weapon, hit_mode, NULL);
                    gsound_play_sfx_file_volume(sfx, volume);
                    ai_magic_hands(critter, weapon, 5001);

                    if (inven_unwield(critter, 1) == 0) {
                        combat_turn_run();
                    }

                    ai_switch_weapons(critter, &hit_mode, &weapon);
                }
            }
            break;
        case COMBAT_BAD_SHOT_OUT_OF_RANGE:
            to_hit = determine_to_hit_no_range(critter, target, HIT_LOCATION_UNCALLED, hit_mode);
            if (to_hit < ai_cap(critter)->min_to_hit) {
                ai_run_away(critter);
                return 0;
            }

            if (weapon != NULL || ai_switch_weapons(critter, &hit_mode, &weapon) == -1 || weapon == NULL) {
                if (ai_move_closer(critter, target, combat_taunts) == -1) {
                    return -1;
                }

                combat_taunts = 0;
            }
            break;
        case COMBAT_BAD_SHOT_NOT_ENOUGH_AP:
        case COMBAT_BAD_SHOT_ARM_CRIPPLED:
        case COMBAT_BAD_SHOT_BOTH_ARMS_CRIPPLED:
            if (ai_switch_weapons(critter, &hit_mode, &weapon) == -1) {
                return -1;
            }
            break;
        case COMBAT_BAD_SHOT_AIM_BLOCKED:
            if (ai_move_closer(critter, target, combat_taunts) == -1) {
                return -1;
            }
            combat_taunts = 0;
            break;
        }
    }

    return -1;
}

// 0x425BC8
void combat_ai_begin(int critters_count, Object** critters)
{
    curr_crit_num = critters_count;

    if (critters_count != 0) {
        curr_crit_list = (Object**)mem_malloc(sizeof(Object*) * critters_count);
        if (curr_crit_list) {
            memcpy(curr_crit_list, critters, sizeof(Object*) * critters_count);
        } else {
            curr_crit_num = 0;
        }
    }
}

// 0x425C0C
void combat_ai_over()
{
    if (curr_crit_num) {
        mem_free(curr_crit_list);
    }

    curr_crit_num = 0;
}

// 0x425C2C
Object* combat_ai(Object* critter, Object* target)
{
    AiPacket* ai;
    CritterCombatData* combatData;

    combatData = &(critter->data.critter.combat);
    ai = ai_cap(critter);

    if ((combatData->maneuver & CRITTER_MANUEVER_FLEEING) != 0
        || (combatData->results & ai->hurt_too_much) != 0
        || stat_level(critter, STAT_CURRENT_HIT_POINTS) < ai->min_hp) {
        ai_run_away(critter);
        return target;
    }

    if (target == NULL) {
        if (ai_check_drugs(critter) != 0) {
            ai_run_away(critter);
        } else {
            target = ai_danger_source(critter);
            if (target != NULL) {
                ai_try_attack(critter, target);
            }
        }
    } else {
        ai_try_attack(critter, target);
    }

    if (target != NULL) {
        if ((target->data.critter.combat.results & DAM_DEAD) == 0) {
            if (critter->data.critter.combat.ap != 0) {
                if (obj_dist(critter, target) > ai->max_dist) {
                    combatData->maneuver |= CRITTER_MANEUVER_DISENGAGING;
                }
            }
        }
    }

    if (target == NULL) {
        if (!isPartyMember(critter)) {
            Object* whoHitMe = combatData->whoHitMe;
            if (whoHitMe != NULL) {
                if ((whoHitMe->data.critter.combat.results & DAM_DEAD) == 0 && combatData->damageLastTurn > 0) {
                    ai_run_away(critter);
                }
            }
        }
    }

    if (target == NULL) {
        if (isPartyMember(critter)) {
            if (obj_dist(critter, obj_dude) > 5) {
                ai_move_closer(critter, obj_dude, 0);
            }
        }
    }

    return target;
}

// 0x425D20
bool combatai_want_to_join(Object* a1)
{
    process_bk();

    if ((a1->flags & OBJECT_HIDDEN) != 0) {
        return false;
    }

    if (a1->elevation != obj_dude->elevation) {
        return false;
    }

    if ((a1->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
        return false;
    }

    if (a1->data.critter.combat.damageLastTurn > 0) {
        return true;
    }

    if (a1->sid != -1) {
        scr_set_objs(a1->sid, NULL, NULL);
        scr_set_ext_param(a1->sid, 5);
        exec_script_proc(a1->sid, SCRIPT_PROC_COMBAT);
    }

    if ((a1->data.critter.combat.maneuver & CRITTER_MANEUVER_ENGAGING) != 0) {
        return true;
    }

    if ((a1->data.critter.combat.maneuver & CRITTER_MANEUVER_DISENGAGING) != 0) {
        return false;
    }

    if ((a1->data.critter.combat.maneuver & CRITTER_MANUEVER_FLEEING) != 0) {
        return false;
    }

    if (ai_danger_source(a1) == NULL) {
        return false;
    }

    return true;
}

// 0x425DCC
bool combatai_want_to_stop(Object* a1)
{
    Object* danger;

    process_bk();

    if ((a1->data.critter.combat.maneuver & CRITTER_MANEUVER_DISENGAGING) != 0) {
        return true;
    }

    if ((a1->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD)) != 0) {
        return true;
    }

    if ((a1->data.critter.combat.maneuver & CRITTER_MANUEVER_FLEEING) != 0) {
        return true;
    }

    danger = ai_danger_source(a1);
    if (danger == NULL) {
        return true;
    }

    if (!is_within_perception(a1, danger)) {
        return true;
    }

    if (a1->data.critter.combat.ap == stat_level(a1, STAT_MAXIMUM_ACTION_POINTS)) {
        return true;
    }

    return false;
}

// 0x425E40
int combatai_switch_team(Object* critter, int team)
{
    Object* who_hit_me;

    critter->data.critter.combat.team = team;

    if (critter->data.critter.combat.whoHitMeCid == -1) {
        critter_set_who_hit_me(critter, NULL);
        debug_printf("\nError: CombatData found with invalid who_hit_me!");
        return -1;
    }

    who_hit_me = critter->data.critter.combat.whoHitMe;
    if (who_hit_me != NULL) {
        if (who_hit_me->data.critter.combat.team == team) {
            critter_set_who_hit_me(critter, NULL);
        }
    }

    if (isInCombat()) {
        bool outline_was_enabled = critter->outline != 0 && (critter->outline & OUTLINE_DISABLED) == 0;
        int outline_type;

        obj_remove_outline(critter, NULL);

        outline_type = OUTLINE_TYPE_HOSTILE;
        if (perk_level(PERK_FRIENDLY_FOE)) {
            if (critter->data.critter.combat.team == obj_dude->data.critter.combat.team) {
                outline_type = OUTLINE_TYPE_FRIENDLY;
            }
        }

        obj_outline_object(critter, outline_type, NULL);

        if (outline_was_enabled) {
            Rect rect;
            obj_turn_on_outline(critter, &rect);
            tile_refresh_rect(&rect, critter->elevation);
        }
    }

    return 0;
}

// 0x425F18
int combatai_msg(Object* critter, Attack* attack, int type, int delay)
{
    int combat_taunts = 1;
    AiPacket* ai;
    int start;
    int end;
    char* string;
    MessageListItem messageListItem;

    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_TAUNTS_KEY, &combat_taunts);
    if (!combat_taunts) {
        return -1;
    }

    if (critter == obj_dude) {
        return -1;
    }

    if ((critter->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
        return -1;
    }

    ai = ai_cap(critter);

    debug_printf("%s is using %s packet with a %d%% chance to taunt\n", object_name(critter), ai->name, ai->chance);

    if (roll_random(1, 100) > ai->chance) {
        return -1;
    }

    switch (type) {
    case AI_MESSAGE_TYPE_RUN:
        start = ai->run_start;
        end = ai->move_start;
        string = attack_str;
        break;
    case AI_MESSAGE_TYPE_MOVE:
        start = ai->move_start;
        end = ai->attack_start;
        string = attack_str;
        break;
    case AI_MESSAGE_TYPE_ATTACK:
        start = ai->attack_start;
        end = ai->miss_start;
        string = attack_str;
        break;
    case AI_MESSAGE_TYPE_MISS:
        start = ai->miss_start;
        end = ai->hit_start[0];
        string = target_str;
        break;
    case AI_MESSAGE_TYPE_HIT:
        start = ai->hit_start[attack->defenderHitLocation];
        end = ai->hit_start[attack->defenderHitLocation + 1];
        string = target_str;
        break;
    default:
        return -1;
    }

    if (end < start) {
        return -1;
    }

    messageListItem.num = roll_random(start, end);
    if (!message_search(&ai_message_file, &messageListItem)) {
        return -1;
    }

    debug_printf("%s said message %d\n", object_name(critter), messageListItem.num);
    strcpy(string, messageListItem.text);

    // TODO: Get rid of casts.
    return register_object_call(critter, (void*)type, (AnimationCallback*)ai_print_msg, delay);
}

// 0x4260B0
static int ai_print_msg(Object* critter, int type)
{
    char* string;
    AiPacket* ai;
    Rect rect;

    if (text_object_count() > 0) {
        return 0;
    }

    switch (type) {
    case AI_MESSAGE_TYPE_HIT:
    case AI_MESSAGE_TYPE_MISS:
        string = target_str;
        break;
    default:
        string = attack_str;
        break;
    }

    ai = ai_cap(critter);

    if (text_object_create(critter, string, ai->font, ai->color, ai->outline_color, &rect) == 0) {
        tile_refresh_rect(&rect, critter->elevation);
    }

    return 0;
}

// Returns random critter for attacking as a result of critical weapon failure.
//
// 0x42610C
Object* combat_ai_random_target(Attack* attack)
{
    // Looks like this function does nothing because it's result is not used. I
    // suppose it was planned to use range as a condition below, but it was
    // later moved into 0x426614, but remained here.
    item_w_range(attack->attacker, attack->hitMode);

    Object* critter = NULL;

    if (curr_crit_num != 0) {
        // Randomize starting critter.
        int start = roll_random(0, curr_crit_num - 1);
        int index = start;
        while (true) {
            Object* obj = curr_crit_list[index];
            if (obj != attack->attacker
                && obj != attack->defender
                && can_see(attack->attacker, obj)
                && combat_check_bad_shot(attack->attacker, obj, attack->hitMode, false) == COMBAT_BAD_SHOT_OK) {
                critter = obj;
                break;
            }

            index += 1;
            if (index == curr_crit_num) {
                index = 0;
            }

            if (index == start) {
                break;
            }
        }
    }

    return critter;
}

// 0x4261B0
static int combatai_rating(Object* obj)
{
    int melee_damage;
    Object* item;
    int weapon_damage_min;
    int weapon_damage_max;

    if (obj == NULL) {
        return 0;
    }

    if (FID_TYPE(obj->fid) != OBJ_TYPE_CRITTER) {
        return 0;
    }

    if ((obj->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
        return 0;
    }

    melee_damage = stat_level(obj, STAT_MELEE_DAMAGE);

    item = inven_right_hand(obj);
    if (item != NULL && item_get_type(item) == ITEM_TYPE_WEAPON && item_w_damage_min_max(item, &weapon_damage_min, &weapon_damage_max) != -1 && melee_damage < weapon_damage_max) {
        melee_damage = weapon_damage_max;
    }

    item = inven_left_hand(obj);
    if (item != NULL && item_get_type(item) == ITEM_TYPE_WEAPON && item_w_damage_min_max(item, &weapon_damage_min, &weapon_damage_max) != -1 && melee_damage < weapon_damage_max) {
        melee_damage = weapon_damage_max;
    }

    return melee_damage + stat_level(obj, STAT_ARMOR_CLASS);
}

// 0x426278
void combatai_check_retaliation(Object* critter, Object* candidate)
{
    int rating_new;
    int rating_current;

    rating_new = combatai_rating(candidate);
    rating_current = combatai_rating(critter->data.critter.combat.whoHitMe);
    if (rating_new > rating_current) {
        critter_set_who_hit_me(critter, candidate);
    }
}

// 0x4262A0
bool is_within_perception(Object* critter1, Object* critter2)
{
    int distance;
    int perception;
    int max_distance;

    distance = obj_dist(critter2, critter1);
    perception = stat_level(critter1, STAT_PERCEPTION);
    if (can_see(critter1, critter2)) {
        max_distance = perception * 5;
        if ((critter2->flags & OBJECT_TRANS_GLASS) != 0) {
            max_distance /= 2;
        }

        if (critter2 == obj_dude) {
            if (is_pc_sneak_working()) {
                max_distance /= 4;
            }
        }

        if (distance <= max_distance) {
            return true;
        }
    } else {
        if (isInCombat()) {
            max_distance = perception * 2;
        } else {
            max_distance = perception;
        }

        if (critter2 == obj_dude) {
            if (is_pc_sneak_working()) {
                max_distance /= 4;
            }
        }

        if (distance <= max_distance) {
            return true;
        }
    }

    return false;
}

// Load combatai.msg and apply language filter.
//
// 0x426364
static int combatai_load_messages()
{
    char path[COMPAT_MAX_PATH];
    int language_filter = 0;

    if (!message_init(&ai_message_file)) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s%s", msg_path, "combatai.msg");

    if (!message_load(&ai_message_file, path)) {
        return -1;
    }

    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_LANGUAGE_FILTER_KEY, &language_filter);

    if (language_filter) {
        message_filter(&ai_message_file);
    }

    return 0;
}

// 0x426408
static int combatai_unload_messages()
{
    if (!message_exit(&ai_message_file)) {
        return -1;
    }

    return 0;
}

// 0x426420
void combatai_refresh_messages()
{
    // 0x504C24
    static int old_state = -1;

    int language_filter = 0;
    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_LANGUAGE_FILTER_KEY, &language_filter);

    if (language_filter != old_state) {
        old_state = language_filter;

        if (language_filter == 1) {
            message_filter(&ai_message_file);
        } else {
            // NOTE: Uninline.
            if (combatai_unload_messages() == 0) {
                combatai_load_messages();
            }
        }
    }
}

// 0x426490
void combatai_notify_onlookers(Object* critter)
{
    int index;

    for (index = 0; index < curr_crit_num; index++) {
        if (is_within_perception(curr_crit_list[index], critter)) {
            curr_crit_list[index]->data.critter.combat.maneuver |= CRITTER_MANEUVER_ENGAGING;
        }
    }
}

// 0x4264D8
void combatai_delete_critter(Object* critter)
{
    int index;

    for (index = 0; index < curr_crit_num; index++) {
        if (critter == curr_crit_list[index]) {
            curr_crit_num--;
            curr_crit_list[index] = curr_crit_list[curr_crit_num];
            curr_crit_list[curr_crit_num] = critter;
            break;
        }
    }
}

} // namespace fallout
