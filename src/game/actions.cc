#include "game/actions.h"

#include <limits.h>
#include <string.h>

#include "game/anim.h"
#include "game/combat.h"
#include "game/combatai.h"
#include "game/config.h"
#include "game/critter.h"
#include "game/display.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gmouse.h"
#include "game/gsound.h"
#include "game/intface.h"
#include "game/item.h"
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
#include "game/trait.h"
#include "plib/color/color.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/rect.h"
#include "plib/gnw/svga.h"

namespace fallout {

static int pick_death(Object* attacker, Object* defender, int damage, int damage_type, int anim, bool isFallingBack);
static int check_death(Object* obj, int anim, int minViolenceLevel, bool isFallingBack);
static int internal_destroy(Object* a1, Object* a2);
static int show_death(Object* obj, int anim);
static int action_melee(Attack* attack, int a2);
static int action_ranged(Attack* attack, int a2);
static int is_next_to(Object* a1, Object* a2);
static int action_climb_ladder(Object* a1, Object* a2);
static int report_explosion(Attack* attack, Object* a2);
static int finished_explosion(Object* a1, Object* a2);
static int compute_explosion_damage(int min, int max, Object* a3, int* a4);
static int can_talk_to(Object* a1, Object* a2);
static int talk_to(Object* a1, Object* a2);
static int report_dmg(Attack* attack, Object* a2);
static int compute_dmg_damage(int min, int max, Object* obj, int* a4, int damage_type);

// 0x4FEA50
static bool action_in_explode = false;

// 0x4FEA54
unsigned int rotation = 0;

// 0x4FEA58
int obj_fid = -1;

// 0x4FEA5C
int obj_pid_old = -1;

// 0x4FEA60
static int death_2[DAMAGE_TYPE_COUNT] = {
    ANIM_DANCING_AUTOFIRE,
    ANIM_SLICED_IN_HALF,
    ANIM_CHARRED_BODY,
    ANIM_CHARRED_BODY,
    ANIM_ELECTRIFY,
    ANIM_FALL_BACK,
    ANIM_BIG_HOLE,
};

// 0x4FEA7C
static int death_3[DAMAGE_TYPE_COUNT] = {
    ANIM_CHUNKS_OF_FLESH,
    ANIM_SLICED_IN_HALF,
    ANIM_FIRE_DANCE,
    ANIM_MELTED_TO_NOTHING,
    ANIM_ELECTRIFIED_TO_NOTHING,
    ANIM_FALL_BACK,
    ANIM_EXPLODED_TO_NOTHING,
};

// 0x410410
void switch_dude()
{
    Object* critter;
    int gender;

    critter = pick_object(OBJ_TYPE_CRITTER, false);
    if (critter != NULL) {
        gender = stat_level(critter, STAT_GENDER);
        stat_set_base(obj_dude, STAT_GENDER, gender);

        obj_dude = critter;
        obj_fid = critter->fid;
        obj_pid_old = critter->pid;
        critter->pid = 0x1000000;
    }
}

// 0x410468
int action_knockback(Object* obj, int* anim, int maxDistance, int rotation, int delay)
{
    if (*anim == ANIM_FALL_FRONT) {
        int fid = art_id(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, *anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
        if (!art_exists(fid)) {
            *anim = ANIM_FALL_BACK;
        }
    }

    int distance;
    int tile;
    for (distance = 1; distance <= maxDistance; distance++) {
        tile = tile_num_in_direction(obj->tile, rotation, distance);
        if (obj_blocking_at(obj, tile, obj->elevation) != NULL) {
            distance--;
            break;
        }
    }

    const char* soundEffectName = gsnd_build_character_sfx_name(obj, *anim, CHARACTER_SOUND_EFFECT_KNOCKDOWN);
    register_object_play_sfx(obj, soundEffectName, delay);

    // TODO: Check, probably step back because we've started with 1?
    distance--;

    if (distance <= 0) {
        tile = obj->tile;
        register_object_animate(obj, *anim, 0);
    } else {
        tile = tile_num_in_direction(obj->tile, rotation, distance);
        register_object_animate_and_move_straight(obj, tile, obj->elevation, *anim, 0);
    }

    return tile;
}

// 0x410548
int action_blood(Object* obj, int anim, int delay)
{

    int violence_level = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &violence_level);
    if (violence_level == VIOLENCE_LEVEL_NONE) {
        return anim;
    }

    int bloodyAnim;
    if (anim == ANIM_FALL_BACK) {
        bloodyAnim = ANIM_FALL_BACK_BLOOD;
    } else if (anim == ANIM_FALL_FRONT) {
        bloodyAnim = ANIM_FALL_FRONT_BLOOD;
    } else {
        return anim;
    }

    int fid = art_id(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, bloodyAnim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    if (art_exists(fid)) {
        register_object_animate(obj, bloodyAnim, delay);
    } else {
        bloodyAnim = anim;
    }

    return bloodyAnim;
}

// 0x4105EC
static int pick_death(Object* attacker, Object* defender, int damage, int damage_type, int anim, bool hit_from_front)
{
    int violence_level = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
    bool has_bloody_mess = false;
    int death_anim;

    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &violence_level);

    if (defender->pid == 16777239 || defender->pid == 16777266 || defender->pid == 16777265) {
        return check_death(defender, ANIM_EXPLODED_TO_NOTHING, VIOLENCE_LEVEL_NORMAL, hit_from_front);
    }

    if (attacker == obj_dude) {
        if (trait_level(TRAIT_BLOODY_MESS)) {
            has_bloody_mess = true;
        }
    }

    if (anim == ANIM_THROW_PUNCH
        || anim == ANIM_KICK_LEG
        || anim == ANIM_THRUST_ANIM
        || anim == ANIM_SWING_ANIM
        || anim == ANIM_THROW_ANIM) {
        if (violence_level == VIOLENCE_LEVEL_MAXIMUM_BLOOD && has_bloody_mess) {
            death_anim = ANIM_BIG_HOLE;
        } else {
            death_anim = ANIM_FALL_BACK;
        }
    } else if (anim == ANIM_FIRE_SINGLE && damage_type == DAMAGE_TYPE_NORMAL) {
        if (violence_level == VIOLENCE_LEVEL_MAXIMUM_BLOOD && (has_bloody_mess || damage >= 45)) {
            death_anim = ANIM_BIG_HOLE;
        } else {
            death_anim = ANIM_FALL_BACK;
        }
    } else {
        if (violence_level > VIOLENCE_LEVEL_NORMAL && (has_bloody_mess || damage >= 45)) {
            death_anim = death_3[damage_type];
            if (check_death(defender, death_anim, VIOLENCE_LEVEL_MAXIMUM_BLOOD, hit_from_front) != death_anim) {
                death_anim = death_2[damage_type];
            }
        } else if (violence_level > VIOLENCE_LEVEL_MINIMAL && (has_bloody_mess || damage >= 15)) {
            death_anim = death_2[damage_type];
        } else {
            death_anim = ANIM_FALL_BACK;
        }
    }

    if (!hit_from_front && death_anim == ANIM_FALL_BACK) {
        death_anim = ANIM_FALL_FRONT;
    }

    return check_death(defender, death_anim, VIOLENCE_LEVEL_NONE, hit_from_front);
}

// 0x410754
static int check_death(Object* obj, int anim, int min_violence_level, bool hit_from_front)
{
    int fid;
    int violence_level = VIOLENCE_LEVEL_MAXIMUM_BLOOD;

    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &violence_level);
    if (violence_level >= min_violence_level) {
        fid = art_id(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
        if (art_exists(fid)) {
            return anim;
        }
    }

    if (hit_from_front) {
        return ANIM_FALL_BACK;
    }

    fid = art_id(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, ANIM_FALL_FRONT, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    if (art_exists(fid)) {
        return ANIM_FALL_BACK;
    }

    return ANIM_FALL_FRONT;
}

// 0x410808
static int internal_destroy(Object* a1, Object* a2)
{
    return obj_destroy(a2);
}

// TODO: Check very carefully, lots of conditions and jumps.
//
// 0x410810
void show_damage_to_object(Object* defender, int damage, int flags, Object* weapon, bool hit_from_front, int knockback_distance, int knockback_rotation, int a8, Object* attacker, int delay)
{
    int anim;
    int fid;
    const char* sfx_name;

    anim = FID_ANIM_TYPE(defender->fid);
    if (!critter_is_prone(defender)) {
        if ((flags & DAM_DEAD) != 0) {
            fid = art_id(OBJ_TYPE_MISC, 10, 0, 0, 0);
            if (fid == attacker->fid) {
                anim = check_death(defender, ANIM_EXPLODED_TO_NOTHING, VIOLENCE_LEVEL_MAXIMUM_BLOOD, hit_from_front);
            } else if (attacker->pid == PROTO_ID_0x20001EB) {
                anim = check_death(defender, ANIM_ELECTRIFIED_TO_NOTHING, VIOLENCE_LEVEL_MAXIMUM_BLOOD, hit_from_front);
            } else if (attacker->fid == FID_0x20001F5) {
                anim = check_death(defender, a8, VIOLENCE_LEVEL_MAXIMUM_BLOOD, hit_from_front);
            } else {
                anim = pick_death(attacker, defender, damage, item_w_damage_type(weapon), a8, hit_from_front);
            }

            if (anim != ANIM_FIRE_DANCE) {
                if (knockback_distance != 0 && (anim == ANIM_FALL_FRONT || anim == ANIM_FALL_BACK)) {
                    action_knockback(defender, &anim, knockback_distance, knockback_rotation, delay);
                    anim = action_blood(defender, anim, -1);
                } else {
                    sfx_name = gsnd_build_character_sfx_name(defender, anim, CHARACTER_SOUND_EFFECT_DIE);
                    register_object_play_sfx(defender, sfx_name, delay);

                    anim = pick_fall(defender, anim);
                    register_object_animate(defender, anim, 0);

                    if (anim == ANIM_FALL_FRONT || anim == ANIM_FALL_BACK) {
                        anim = action_blood(defender, anim, -1);
                    }
                }
            } else {
                fid = art_id(OBJ_TYPE_CRITTER, defender->fid & 0xFFF, ANIM_FIRE_DANCE, (defender->fid & 0xF000) >> 12, defender->rotation + 1);
                if (art_exists(fid)) {
                    sfx_name = gsnd_build_character_sfx_name(defender, anim, CHARACTER_SOUND_EFFECT_UNUSED);
                    register_object_play_sfx(defender, sfx_name, delay);

                    register_object_animate(defender, anim, 0);

                    int randomDistance = roll_random(2, 5);
                    int randomRotation = roll_random(0, 5);

                    while (randomDistance > 0) {
                        int tile = tile_num_in_direction(defender->tile, randomRotation, randomDistance);
                        Object* v35 = NULL;
                        make_straight_path(defender, defender->tile, tile, NULL, &v35, 4);
                        if (v35 == NULL) {
                            register_object_turn_towards(defender, tile);
                            register_object_move_straight_to_tile(defender, tile, defender->elevation, anim, 0);
                            break;
                        }
                        randomDistance--;
                    }
                }

                anim = ANIM_BURNED_TO_NOTHING;
                sfx_name = gsnd_build_character_sfx_name(defender, anim, CHARACTER_SOUND_EFFECT_UNUSED);
                register_object_play_sfx(defender, sfx_name, -1);
                register_object_animate(defender, anim, 0);
            }
        } else {
            if ((flags & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) != 0) {
                anim = hit_from_front ? ANIM_FALL_BACK : ANIM_FALL_FRONT;
                sfx_name = gsnd_build_character_sfx_name(defender, anim, CHARACTER_SOUND_EFFECT_UNUSED);
                register_object_play_sfx(defender, sfx_name, delay);
                if (knockback_distance != 0) {
                    action_knockback(defender, &anim, knockback_distance, knockback_rotation, 0);
                } else {
                    anim = pick_fall(defender, anim);
                    register_object_animate(defender, anim, 0);
                }
            } else if ((flags & DAM_ON_FIRE) != 0 && art_exists(art_id(OBJ_TYPE_CRITTER, defender->fid & 0xFFF, ANIM_FIRE_DANCE, (defender->fid & 0xF000) >> 12, defender->rotation + 1))) {
                register_object_animate(defender, ANIM_FIRE_DANCE, delay);

                fid = art_id(OBJ_TYPE_CRITTER, defender->fid & 0xFFF, ANIM_STAND, (defender->fid & 0xF000) >> 12, defender->rotation + 1);
                register_object_change_fid(defender, fid, -1);
            } else {
                if (knockback_distance != 0) {
                    anim = hit_from_front ? ANIM_FALL_BACK : ANIM_FALL_FRONT;
                    action_knockback(defender, &anim, knockback_distance, knockback_rotation, delay);
                    if (anim == ANIM_FALL_BACK) {
                        register_object_animate(defender, ANIM_BACK_TO_STANDING, -1);
                    } else {
                        register_object_animate(defender, ANIM_PRONE_TO_STANDING, -1);
                    }
                } else {
                    if (hit_from_front || !art_exists(art_id(OBJ_TYPE_CRITTER, defender->fid & 0xFFF, ANIM_HIT_FROM_BACK, (defender->fid & 0xF000) >> 12, defender->rotation + 1))) {
                        anim = ANIM_HIT_FROM_FRONT;
                    } else {
                        anim = ANIM_HIT_FROM_BACK;
                    }

                    sfx_name = gsnd_build_character_sfx_name(defender, anim, CHARACTER_SOUND_EFFECT_UNUSED);
                    register_object_play_sfx(defender, sfx_name, delay);

                    register_object_animate(defender, anim, 0);
                }
            }
        }
    } else {
        if ((flags & DAM_DEAD) != 0 && (defender->data.critter.combat.results & DAM_DEAD) == 0) {
            anim = action_blood(defender, anim, delay);
        } else {
            return;
        }
    }

    if (weapon != NULL) {
        if ((flags & DAM_EXPLODE) != 0) {
            register_object_must_call(defender, weapon, (AnimationCallback*)obj_drop, -1);
            fid = art_id(OBJ_TYPE_MISC, 10, 0, 0, 0);
            register_object_change_fid(weapon, fid, 0);
            register_object_animate_and_hide(weapon, ANIM_STAND, 0);

            sfx_name = gsnd_build_weapon_sfx_name(WEAPON_SOUND_EFFECT_HIT, weapon, HIT_MODE_RIGHT_WEAPON_PRIMARY, defender);
            register_object_play_sfx(weapon, sfx_name, 0);

            register_object_must_erase(weapon);
        } else if ((flags & DAM_DESTROY) != 0) {
            register_object_must_call(defender, weapon, (AnimationCallback*)internal_destroy, -1);
        } else if ((flags & DAM_DROP) != 0) {
            register_object_must_call(defender, weapon, (AnimationCallback*)obj_drop, -1);
        }
    }

    if ((flags & DAM_DEAD) != 0) {
        // TODO: Get rid of casts.
        register_object_must_call(defender, (void*)anim, (AnimationCallback*)show_death, -1);
    }
}

// 0x410D50
static int show_death(Object* obj, int anim)
{
    Rect temp_rect;
    Rect dirty_rect;

    obj_bound(obj, &dirty_rect);

    if (obj->pid != 16777266 && obj->pid != 16777265 && obj->pid != 16777224) {
        obj->flags |= OBJECT_NO_BLOCK;
        if (obj_toggle_flat(obj, &temp_rect) == 0) {
            rect_min_bound(&dirty_rect, &temp_rect, &dirty_rect);
        }
    }

    if (obj_turn_off_outline(obj, &temp_rect) == 0) {
        rect_min_bound(&dirty_rect, &temp_rect, &dirty_rect);
    }

    if (anim >= ANIM_ELECTRIFIED_TO_NOTHING && anim <= ANIM_EXPLODED_TO_NOTHING) {
        if (obj->pid != 16777265 && obj->pid != 16777266 && obj->pid != 16777239) {
            item_drop_all(obj, obj->tile);
        }
    }

    tile_refresh_rect(&dirty_rect, obj->elevation);

    return 0;
}

// 0x410E74
int show_damage_target(Attack* attack)
{
    int frontHit;

    if (FID_TYPE(attack->defender->fid) == OBJ_TYPE_CRITTER) {
        // NOTE: Uninline.
        frontHit = is_hit_from_front(attack->attacker, attack->defender);

        register_begin(ANIMATION_REQUEST_RESERVED);
        register_priority(1);
        show_damage_to_object(attack->defender,
            attack->defenderDamage,
            attack->defenderFlags,
            attack->weapon,
            frontHit,
            attack->defenderKnockback,
            tile_dir(attack->attacker->tile, attack->defender->tile),
            item_w_anim(attack->attacker, attack->hitMode),
            attack->attacker,
            0);
        register_end();
    }

    return 0;
}

// 0x410F18
int show_damage_extras(Attack* attack)
{
    int v6;
    int v8;
    int v9;

    for (int index = 0; index < attack->extrasLength; index++) {
        Object* obj = attack->extras[index];
        if (FID_TYPE(obj->fid) == OBJ_TYPE_CRITTER) {
            int delta = attack->attacker->rotation - obj->rotation;
            if (delta < 0) {
                delta = -delta;
            }

            v6 = delta != 0 && delta != 1 && delta != 5;
            register_begin(ANIMATION_REQUEST_RESERVED);
            register_priority(1);
            v8 = item_w_anim(attack->attacker, attack->hitMode);
            v9 = tile_dir(attack->attacker->tile, obj->tile);
            show_damage_to_object(obj, attack->extrasDamage[index], attack->extrasFlags[index], attack->weapon, v6, attack->extrasKnockback[index], v9, v8, attack->attacker, 0);
            register_end();
        }
    }

    return 0;
}

// 0x410FD8
void show_damage(Attack* attack, int a2, int delay)
{
    bool hit_from_front;

    for (int index = 0; index < attack->extrasLength; index++) {
        Object* object = attack->extras[index];
        if (FID_TYPE(object->fid) == OBJ_TYPE_CRITTER) {
            register_ping(2, delay);
            delay = 0;
        }
    }

    if ((attack->attackerFlags & DAM_HIT) == 0) {
        if ((attack->attackerFlags & DAM_CRITICAL) != 0) {
            show_damage_to_object(attack->attacker, attack->attackerDamage, attack->attackerFlags, attack->weapon, 1, 0, 0, a2, attack->attacker, -1);
        } else if ((attack->attackerFlags & DAM_BACKWASH) != 0) {
            show_damage_to_object(attack->attacker, attack->attackerDamage, attack->attackerFlags, attack->weapon, 1, 0, 0, a2, attack->attacker, -1);
        }
    } else {
        if (attack->defender != NULL) {
            hit_from_front = is_hit_from_front(attack->attacker, attack->defender);

            if (FID_TYPE(attack->defender->fid) == OBJ_TYPE_CRITTER) {
                if (attack->attacker->fid == 33554933) {
                    show_damage_to_object(attack->defender,
                        attack->defenderDamage,
                        attack->defenderFlags,
                        attack->weapon,
                        hit_from_front,
                        attack->defenderKnockback,
                        tile_dir(attack->attacker->tile, attack->defender->tile),
                        a2,
                        attack->attacker, delay);
                } else {
                    show_damage_to_object(attack->defender,
                        attack->defenderDamage,
                        attack->defenderFlags,
                        attack->weapon,
                        hit_from_front,
                        attack->defenderKnockback,
                        tile_dir(attack->attacker->tile, attack->defender->tile),
                        item_w_anim(attack->attacker, attack->hitMode),
                        attack->attacker,
                        delay);
                }
            }
        }

        if ((attack->attackerFlags & DAM_DUD) != 0) {
            show_damage_to_object(attack->attacker, attack->attackerDamage, attack->attackerFlags, attack->weapon, 1, 0, 0, a2, attack->attacker, -1);
        }
    }
}

// 0x411134
int action_attack(Attack* attack)
{
    if (register_clear(attack->attacker) == -2) {
        return -1;
    }

    if (register_clear(attack->defender) == -2) {
        return -1;
    }

    for (int index = 0; index < attack->extrasLength; index++) {
        if (register_clear(attack->extras[index]) == -2) {
            return -1;
        }
    }

    int anim = item_w_anim(attack->attacker, attack->hitMode);
    if (anim < ANIM_FIRE_SINGLE && anim != ANIM_THROW_ANIM) {
        return action_melee(attack, anim);
    } else {
        return action_ranged(attack, anim);
    }
}

// 0x4111C4
static int action_melee(Attack* attack, int anim)
{
    int fid;
    Art* art;
    CacheEntry* cache_entry;
    int v17;
    int v18;
    int delta;
    int flag;
    const char* sfx_name;
    char sfx_name_temp[16];

    register_begin(ANIMATION_REQUEST_RESERVED);
    register_priority(1);

    fid = art_id(OBJ_TYPE_CRITTER, attack->attacker->fid & 0xFFF, anim, (attack->attacker->fid & 0xF000) >> 12, attack->attacker->rotation + 1);
    art = art_ptr_lock(fid, &cache_entry);
    if (art != NULL) {
        v17 = art_frame_action_frame(art);
    } else {
        v17 = 0;
    }
    art_ptr_unlock(cache_entry);

    tile_num_in_direction(attack->attacker->tile, attack->attacker->rotation, 1);
    register_object_turn_towards(attack->attacker, attack->defender->tile);

    delta = attack->attacker->rotation - attack->defender->rotation;
    if (delta < 0) {
        delta = -delta;
    }
    flag = delta != 0 && delta != 1 && delta != 5;

    if (anim != ANIM_THROW_PUNCH && anim != ANIM_KICK_LEG) {
        sfx_name = gsnd_build_weapon_sfx_name(WEAPON_SOUND_EFFECT_ATTACK, attack->weapon, attack->hitMode, attack->defender);
    } else {
        sfx_name = gsnd_build_character_sfx_name(attack->attacker, anim, CHARACTER_SOUND_EFFECT_UNUSED);
    }

    strcpy(sfx_name_temp, sfx_name);

    combatai_msg(attack->attacker, attack, AI_MESSAGE_TYPE_ATTACK, 0);

    if (attack->attackerFlags & 0x0300) {
        register_object_play_sfx(attack->attacker, sfx_name_temp, 0);
        if (anim != ANIM_THROW_PUNCH && anim != ANIM_KICK_LEG) {
            sfx_name = gsnd_build_weapon_sfx_name(WEAPON_SOUND_EFFECT_HIT, attack->weapon, attack->hitMode, attack->defender);
        } else {
            sfx_name = gsnd_build_character_sfx_name(attack->attacker, anim, CHARACTER_SOUND_EFFECT_CONTACT);
        }

        strcpy(sfx_name_temp, sfx_name);

        register_object_animate(attack->attacker, anim, 0);
        register_object_play_sfx(attack->attacker, sfx_name_temp, v17);
        show_damage(attack, anim, 0);
    } else {
        if (attack->defender->data.critter.combat.results & 0x03) {
            register_object_play_sfx(attack->attacker, sfx_name_temp, -1);
            register_object_animate(attack->attacker, anim, 0);
        } else {
            fid = art_id(OBJ_TYPE_CRITTER, attack->defender->fid & 0xFFF, ANIM_DODGE_ANIM, (attack->defender->fid & 0xF000) >> 12, attack->defender->rotation + 1);
            art = art_ptr_lock(fid, &cache_entry);
            if (art != NULL) {
                v18 = art_frame_action_frame(art);
                art_ptr_unlock(cache_entry);

                if (v18 <= v17) {
                    register_object_play_sfx(attack->attacker, sfx_name_temp, -1);
                    register_object_animate(attack->attacker, anim, 0);

                    sfx_name = gsnd_build_character_sfx_name(attack->defender, ANIM_DODGE_ANIM, CHARACTER_SOUND_EFFECT_UNUSED);
                    register_object_play_sfx(attack->defender, sfx_name, v17 - v18);
                    register_object_animate(attack->defender, ANIM_DODGE_ANIM, 0);
                } else {
                    sfx_name = gsnd_build_character_sfx_name(attack->defender, ANIM_DODGE_ANIM, CHARACTER_SOUND_EFFECT_UNUSED);
                    register_object_play_sfx(attack->defender, sfx_name, -1);
                    register_object_animate(attack->defender, ANIM_DODGE_ANIM, 0);
                    register_object_play_sfx(attack->attacker, sfx_name_temp, v18 - v17);
                    register_object_animate(attack->attacker, anim, 0);
                }
            }
        }
    }

    if ((attack->attackerFlags & DAM_HIT) != 0) {
        if ((attack->defenderFlags & DAM_DEAD) == 0) {
            combatai_msg(attack->attacker, attack, AI_MESSAGE_TYPE_HIT, -1);
        }
    } else {
        combatai_msg(attack->attacker, attack, AI_MESSAGE_TYPE_MISS, -1);
    }

    if (register_end() == -1) {
        return -1;
    }

    show_damage_extras(attack);

    return 0;
}

// 0x4114DC
static int action_ranged(Attack* attack, int anim)
{
    Object* neighboors[6];
    memset(neighboors, 0, sizeof(neighboors));

    register_begin(ANIMATION_REQUEST_RESERVED);
    register_priority(1);

    Object* projectile = NULL;
    Object* v50 = NULL;
    int weaponFid = -1;

    Proto* weaponProto;
    Object* weapon = attack->weapon;
    proto_ptr(weapon->pid, &weaponProto);

    int fid = art_id(OBJ_TYPE_CRITTER, attack->attacker->fid & 0xFFF, anim, (attack->attacker->fid & 0xF000) >> 12, attack->attacker->rotation + 1);
    CacheEntry* artHandle;
    Art* art = art_ptr_lock(fid, &artHandle);
    int actionFrame = (art != NULL) ? art_frame_action_frame(art) : 0;
    art_ptr_unlock(artHandle);

    item_w_range(attack->attacker, attack->hitMode);

    int damageType = item_w_damage_type(attack->weapon);

    tile_num_in_direction(attack->attacker->tile, attack->attacker->rotation, 1);

    register_object_turn_towards(attack->attacker, attack->defender->tile);

    bool isGrenade = false;
    if (anim == ANIM_THROW_ANIM) {
        if (damageType == DAMAGE_TYPE_EXPLOSION || damageType == DAMAGE_TYPE_PLASMA || damageType == DAMAGE_TYPE_EMP) {
            isGrenade = true;
        }
    } else {
        register_object_animate(attack->attacker, ANIM_POINT, -1);
    }

    combatai_msg(attack->attacker, attack, AI_MESSAGE_TYPE_ATTACK, 0);

    const char* sfx;
    if (((attack->attacker->fid & 0xF000) >> 12) != 0) {
        sfx = gsnd_build_weapon_sfx_name(WEAPON_SOUND_EFFECT_ATTACK, weapon, attack->hitMode, attack->defender);
    } else {
        sfx = gsnd_build_character_sfx_name(attack->attacker, anim, CHARACTER_SOUND_EFFECT_UNUSED);
    }
    register_object_play_sfx(attack->attacker, sfx, -1);

    register_object_animate(attack->attacker, anim, 0);

    if (anim != ANIM_FIRE_CONTINUOUS) {
        if ((attack->attackerFlags & DAM_HIT) != 0 || (attack->attackerFlags & DAM_CRITICAL) == 0) {
            bool l56 = false;

            int projectilePid = item_w_proj_pid(weapon);
            Proto* projectileProto;
            if (proto_ptr(projectilePid, &projectileProto) != -1 && projectileProto->fid != -1) {
                if (anim == ANIM_THROW_ANIM) {
                    projectile = weapon;
                    weaponFid = weapon->fid;
                    int weaponFlags = weapon->flags;

                    item_remove_mult(attack->attacker, weapon, 1);
                    v50 = item_replace(attack->attacker, weapon, weaponFlags & OBJECT_IN_ANY_HAND);
                    obj_change_fid(projectile, projectileProto->fid, NULL);

                    if (attack->attacker == obj_dude) {
                        intface_update_items(false);
                    }

                    obj_connect(weapon, attack->attacker->tile, attack->attacker->elevation, NULL);
                } else {
                    obj_new(&projectile, projectileProto->fid, -1);
                }

                obj_turn_off(projectile, NULL);

                obj_set_light(projectile, 9, projectile->lightIntensity, NULL);

                int projectileOrigin = combat_bullet_start(attack->attacker, attack->defender);
                obj_move_to_tile(projectile, projectileOrigin, attack->attacker->elevation, NULL);

                int projectileRotation = tile_dir(attack->attacker->tile, attack->defender->tile);
                obj_set_rotation(projectile, projectileRotation, NULL);

                register_object_funset(projectile, OBJECT_HIDDEN, actionFrame);

                const char* sfx = gsnd_build_weapon_sfx_name(WEAPON_SOUND_EFFECT_AMMO_FLYING, weapon, attack->hitMode, attack->defender);
                register_object_play_sfx(projectile, sfx, 0);

                int v24;
                if ((attack->attackerFlags & DAM_HIT) != 0) {
                    register_object_move_straight_to_tile(projectile, attack->defender->tile, attack->defender->elevation, ANIM_WALK, 0);
                    actionFrame = make_straight_path(projectile, projectileOrigin, attack->defender->tile, NULL, NULL, 32) - 1;
                    v24 = attack->defender->tile;
                } else {
                    register_object_move_straight_to_tile(projectile, attack->tile, attack->defender->elevation, ANIM_WALK, 0);
                    actionFrame = 0;
                    v24 = attack->tile;
                }

                if (isGrenade || damageType == DAMAGE_TYPE_EXPLOSION) {
                    if ((attack->attackerFlags & DAM_DROP) == 0) {
                        int explosionFrmId;
                        if (isGrenade) {
                            switch (damageType) {
                            case DAMAGE_TYPE_EMP:
                                explosionFrmId = 2;
                                break;
                            case DAMAGE_TYPE_PLASMA:
                                explosionFrmId = 31;
                                break;
                            default:
                                explosionFrmId = 29;
                                break;
                            }
                        } else {
                            explosionFrmId = 10;
                        }

                        if (isGrenade) {
                            register_object_change_fid(projectile, weaponFid, -1);
                        }

                        int explosionFid = art_id(OBJ_TYPE_MISC, explosionFrmId, 0, 0, 0);
                        register_object_change_fid(projectile, explosionFid, -1);

                        const char* sfx = gsnd_build_weapon_sfx_name(WEAPON_SOUND_EFFECT_HIT, weapon, attack->hitMode, attack->defender);
                        register_object_play_sfx(projectile, sfx, 0);

                        register_object_animate_and_hide(projectile, ANIM_STAND, 0);

                        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
                            if (obj_new(&(neighboors[rotation]), explosionFid, -1) != -1) {
                                obj_turn_off(neighboors[rotation], NULL);

                                int v31 = tile_num_in_direction(v24, rotation, 1);
                                obj_move_to_tile(neighboors[rotation], v31, projectile->elevation, NULL);

                                int delay;
                                if (rotation != ROTATION_NE) {
                                    delay = 0;
                                } else {
                                    if (damageType == DAMAGE_TYPE_PLASMA) {
                                        delay = 4;
                                    } else {
                                        delay = 2;
                                    }
                                }

                                register_object_funset(neighboors[rotation], OBJECT_HIDDEN, delay);
                                register_object_animate_and_hide(neighboors[rotation], ANIM_STAND, 0);
                            }
                        }

                        l56 = true;
                    }
                } else {
                    if (anim != ANIM_THROW_ANIM) {
                        register_object_must_erase(projectile);
                    }
                }

                if (!l56) {
                    const char* sfx = gsnd_build_weapon_sfx_name(WEAPON_SOUND_EFFECT_HIT, weapon, attack->hitMode, attack->defender);
                    register_object_play_sfx(weapon, sfx, actionFrame);
                }

                actionFrame = 0;
            } else {
                if ((attack->attackerFlags & DAM_HIT) == 0) {
                    Object* defender = attack->defender;
                    if ((defender->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) == 0) {
                        register_object_animate(defender, ANIM_DODGE_ANIM, actionFrame);
                        l56 = true;
                    }
                }
            }
        }
    }

    show_damage(attack, anim, actionFrame);

    if ((attack->attackerFlags & DAM_HIT) == 0) {
        combatai_msg(attack->defender, attack, AI_MESSAGE_TYPE_MISS, -1);
    } else {
        if ((attack->defenderFlags & DAM_DEAD) == 0) {
            combatai_msg(attack->defender, attack, AI_MESSAGE_TYPE_HIT, -1);
        }
    }

    if (projectile != NULL && (isGrenade || damageType == DAMAGE_TYPE_EXPLOSION)) {
        register_object_must_erase(projectile);
    } else if (anim == ANIM_THROW_ANIM && projectile != NULL) {
        register_object_change_fid(projectile, weaponFid, -1);
    }

    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        if (neighboors[rotation] != NULL) {
            register_object_must_erase(neighboors[rotation]);
        }
    }

    if ((attack->attackerFlags & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN | DAM_DEAD)) == 0) {
        if (anim == ANIM_THROW_ANIM) {
            bool l9 = false;
            if (v50 != NULL) {
                int v38 = item_w_anim_code(v50);
                if (v38 != 0) {
                    register_object_take_out(attack->attacker, v38, -1);
                    l9 = true;
                }
            }

            if (!l9) {
                int fid = art_id(OBJ_TYPE_CRITTER, attack->attacker->fid & 0xFFF, ANIM_STAND, 0, attack->attacker->rotation + 1);
                register_object_change_fid(attack->attacker, fid, -1);
            }
        } else {
            register_object_animate(attack->attacker, ANIM_UNPOINT, -1);
        }
    }

    if (register_end() == -1) {
        debug_printf("Something went wrong with a ranged attack sequence!\n");
        if (projectile != NULL && (isGrenade || damageType == DAMAGE_TYPE_EXPLOSION || anim != ANIM_THROW_ANIM)) {
            obj_erase_object(projectile, NULL);
        }

        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
            if (neighboors[rotation] != NULL) {
                obj_erase_object(neighboors[rotation], NULL);
            }
        }

        return -1;
    }

    show_damage_extras(attack);

    return 0;
}

// 0x411BCC
int use_an_object(Object* item)
{
    return action_use_an_object(obj_dude, item);
}

// 0x411BE4
static int is_next_to(Object* a1, Object* a2)
{
    MessageListItem messageListItem;

    if (obj_dist(a1, a2) > 1) {
        if (a2 == obj_dude) {
            // You cannot get there.
            messageListItem.num = 2000;
            if (message_search(&misc_message_file, &messageListItem)) {
                display_print(messageListItem.text);
            }
        }
        return -1;
    }

    return 0;
}

// 0x411C30
static int action_climb_ladder(Object* a1, Object* a2)
{
    if (a1 == obj_dude) {
        int anim = FID_ANIM_TYPE(obj_dude->fid);
        if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
            register_clear(obj_dude);
        }
    }

    int animationRequestOptions;
    int actionPoints;
    if (isInCombat()) {
        animationRequestOptions = ANIMATION_REQUEST_RESERVED;
        actionPoints = a1->data.critter.combat.ap;
    } else {
        animationRequestOptions = ANIMATION_REQUEST_UNRESERVED;
        actionPoints = -1;
    }

    if (a1 == obj_dude) {
        animationRequestOptions = ANIMATION_REQUEST_RESERVED;
    }

    animationRequestOptions |= ANIMATION_REQUEST_NO_STAND;
    register_begin(animationRequestOptions);

    int tile = tile_num_in_direction(a2->tile, ROTATION_SE, 1);
    if (actionPoints != -1 || obj_dist(a1, a2) < 5) {
        register_object_move_to_tile(a1, tile, a2->elevation, actionPoints, 0);
    } else {
        register_object_run_to_tile(a1, tile, a2->elevation, actionPoints, 0);
    }

    register_object_must_call(a1, a2, (AnimationCallback*)is_next_to, -1);
    register_object_turn_towards(a1, a2->tile);
    register_object_must_call(a1, a2, (AnimationCallback*)check_scenery_ap_cost, -1);

    int weaponAnimationCode = (a1->fid & 0xF000) >> 12;
    if (weaponAnimationCode != 0) {
        const char* puttingAwaySfx = gsnd_build_character_sfx_name(a1, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
        register_object_play_sfx(a1, puttingAwaySfx, -1);
        register_object_animate(a1, ANIM_PUT_AWAY, 0);
    }

    const char* climbingSfx = gsnd_build_character_sfx_name(a1, ANIM_CLIMB_LADDER, CHARACTER_SOUND_EFFECT_UNUSED);
    register_object_play_sfx(a1, climbingSfx, -1);
    register_object_animate(a1, ANIM_CLIMB_LADDER, 0);
    register_object_call(a1, a2, (AnimationCallback*)obj_use, -1);

    if (weaponAnimationCode != 0) {
        register_object_take_out(a1, weaponAnimationCode, -1);
    }

    return register_end();
}

// 0x411DA8
int a_use_obj(Object* a1, Object* a2, Object* a3)
{
    Proto* scenery_proto;
    int scenery_type = -1;
    int anim_request_options;
    int action_points;
    int anim;
    int weapon_anim_code;

    if (FID_TYPE(a2->fid) == OBJ_TYPE_SCENERY) {
        if (proto_ptr(a2->pid, &scenery_proto) == -1) {
            return -1;
        }

        scenery_type = scenery_proto->scenery.type;
    }

    if (scenery_type == SCENERY_TYPE_LADDER_UP && a3 == NULL) {
        return action_climb_ladder(a1, a2);
    }

    if (a1 == obj_dude) {
        int anim = FID_ANIM_TYPE(obj_dude->fid);
        if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
            register_clear(obj_dude);
        }
    }

    if (isInCombat()) {
        anim_request_options = ANIMATION_REQUEST_RESERVED;
        action_points = a1->data.critter.combat.ap;
    } else {
        anim_request_options = ANIMATION_REQUEST_UNRESERVED;
        action_points = -1;
    }

    if (a1 == obj_dude) {
        anim_request_options = ANIMATION_REQUEST_RESERVED;
    }

    register_begin(anim_request_options);

    if (action_points != -1 || obj_dist(a1, a2) < 5) {
        register_object_move_to_object(a1, a2, action_points, 0);
    } else {
        register_object_run_to_object(a1, a2, -1, 0);
    }

    register_object_must_call(a1, a2, (AnimationCallback*)is_next_to, -1);
    register_object_call(a1, a2, (AnimationCallback*)check_scenery_ap_cost, -1);

    weapon_anim_code = (a1->fid & 0xF000) >> 12;
    if (weapon_anim_code != 0) {
        const char* sfx = gsnd_build_character_sfx_name(a1, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
        register_object_play_sfx(a1, sfx, -1);
        register_object_animate(a1, ANIM_PUT_AWAY, 0);
    }

    if (FID_TYPE(a2->fid) == OBJ_TYPE_CRITTER && critter_is_prone(a2)) {
        anim = ANIM_MAGIC_HANDS_GROUND;
    } else if (FID_TYPE(a2->fid) == OBJ_TYPE_SCENERY && (scenery_proto->scenery.extendedFlags & 0x01) != 0) {
        anim = ANIM_MAGIC_HANDS_GROUND;
    } else {
        anim = ANIM_MAGIC_HANDS_MIDDLE;
    }

    register_object_animate(a1, anim, -1);

    if (a3 != NULL) {
        // TODO: Get rid of cast.
        register_object_call3(a1, a2, a3, (AnimationCallback3*)obj_use_item_on, -1);
    } else {
        register_object_call(a1, a2, (AnimationCallback*)obj_use, -1);
    }

    if (weapon_anim_code != 0) {
        register_object_take_out(a1, weapon_anim_code, -1);
    }

    return register_end();
}

// 0x411F2C
int action_use_an_item_on_object(Object* critter, Object* item, Object* target)
{
    return a_use_obj(critter, item, target);
}

// 0x411F78
int action_use_an_object(Object* critter, Object* item)
{
    return a_use_obj(critter, item, NULL);
}

// 0x411F84
int get_an_object(Object* item)
{
    return action_get_an_object(obj_dude, item);
}

// 0x411F98
int action_get_an_object(Object* critter, Object* item)
{
    if (FID_TYPE(item->fid) != OBJ_TYPE_ITEM) {
        return -1;
    }

    if (critter == obj_dude) {
        int animationCode = FID_ANIM_TYPE(obj_dude->fid);
        if (animationCode == ANIM_WALK || animationCode == ANIM_RUNNING) {
            register_clear(obj_dude);
        }
    }

    if (isInCombat()) {
        register_begin(ANIMATION_REQUEST_RESERVED);
        register_object_move_to_object(critter, item, critter->data.critter.combat.ap, 0);
    } else {
        register_begin(critter == obj_dude ? ANIMATION_REQUEST_RESERVED : ANIMATION_REQUEST_UNRESERVED);
        if (obj_dist(critter, item) >= 5) {
            register_object_run_to_object(critter, item, -1, 0);
        } else {
            register_object_move_to_object(critter, item, -1, 0);
        }
    }

    register_object_must_call(critter, item, (AnimationCallback*)is_next_to, -1);
    register_object_call(critter, item, (AnimationCallback*)check_scenery_ap_cost, -1);

    Proto* itemProto;
    proto_ptr(item->pid, &itemProto);

    if (itemProto->item.type != ITEM_TYPE_CONTAINER || proto_action_can_pickup(item->pid)) {
        register_object_animate(critter, ANIM_MAGIC_HANDS_GROUND, 0);

        int fid = art_id(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, ANIM_MAGIC_HANDS_GROUND, (critter->fid & 0xF000) >> 12, critter->rotation + 1);

        int actionFrame;
        CacheEntry* cacheEntry;
        Art* art = art_ptr_lock(fid, &cacheEntry);
        if (art != NULL) {
            actionFrame = art_frame_action_frame(art);
        } else {
            actionFrame = -1;
        }

        char sfx[16];
        if (art_get_base_name(FID_TYPE(item->fid), item->fid & 0xFFF, sfx) == 0) {
            // NOTE: looks like they copy sfx one more time, what for?
            register_object_play_sfx(item, sfx, actionFrame);
        }

        register_object_call(critter, item, (AnimationCallback*)obj_pickup, actionFrame);
    } else {
        int weaponAnimationCode = (critter->fid & 0xF000) >> 12;
        if (weaponAnimationCode != 0) {
            const char* sfx = gsnd_build_character_sfx_name(critter, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
            register_object_play_sfx(critter, sfx, -1);
            register_object_animate(critter, ANIM_PUT_AWAY, -1);
        }

        // ground vs middle animation
        int anim = (itemProto->item.data.container.openFlags & 0x01) == 0
            ? ANIM_MAGIC_HANDS_MIDDLE
            : ANIM_MAGIC_HANDS_GROUND;
        register_object_animate(critter, anim, 0);

        int fid = art_id(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, anim, 0, critter->rotation + 1);

        int actionFrame;
        CacheEntry* cacheEntry;
        Art* art = art_ptr_lock(fid, &cacheEntry);
        if (art == NULL) {
            actionFrame = art_frame_action_frame(art);
            art_ptr_unlock(cacheEntry);
        } else {
            actionFrame = -1;
        }

        if (item->pid != 213) {
            register_object_call(critter, item, (AnimationCallback*)obj_use_container, actionFrame);
        }

        if (weaponAnimationCode != 0) {
            register_object_take_out(critter, weaponAnimationCode, -1);
        }

        if (item->frame == 0 || item->pid == 213) {
            register_object_call(critter, item, (AnimationCallback*)scripts_request_loot_container, -1);
        }
    }

    return register_end();
}

// 0x412254
int action_loot_container(Object* critter, Object* container)
{
    if (FID_TYPE(container->fid) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    if (critter == obj_dude) {
        int anim = FID_ANIM_TYPE(obj_dude->fid);
        if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
            register_clear(obj_dude);
        }
    }

    if (isInCombat()) {
        register_begin(ANIMATION_REQUEST_RESERVED);
        register_object_move_to_object(critter, container, critter->data.critter.combat.ap, 0);
    } else {
        register_begin(critter == obj_dude ? ANIMATION_REQUEST_RESERVED : ANIMATION_REQUEST_UNRESERVED);

        if (obj_dist(critter, container) < 5) {
            register_object_move_to_object(critter, container, -1, 0);
        } else {
            register_object_run_to_object(critter, container, -1, 0);
        }
    }

    register_object_must_call(critter, container, (AnimationCallback*)is_next_to, -1);
    register_object_call(critter, container, (AnimationCallback*)check_scenery_ap_cost, -1);
    register_object_call(critter, container, (AnimationCallback*)scripts_request_loot_container, -1);
    return register_end();
}

// 0x41234C
int action_skill_use(int skill)
{
    if (skill == SKILL_SNEAK) {
        register_clear(obj_dude);
        pc_flag_toggle(PC_FLAG_SNEAKING);
        return 0;
    }

    return -1;
}

// 0x41236C
int action_use_skill_in_combat_error(Object* critter)
{
    MessageListItem messageListItem;

    if (critter == obj_dude) {
        messageListItem.num = 902;
        if (message_search(&proto_main_msg_file, &messageListItem) == 1) {
            display_print(messageListItem.text);
        }
    }

    return -1;
}

// 0x4123C8
int action_use_skill_on(Object* a1, Object* a2, int skill)
{
    switch (skill) {
    case SKILL_FIRST_AID:
    case SKILL_DOCTOR:
        if (isInCombat()) {
            // NOTE: Uninline.
            return action_use_skill_in_combat_error(a1);
        }

        if (PID_TYPE(a2->pid) != OBJ_TYPE_CRITTER) {
            return -1;
        }
        break;
    case SKILL_SNEAK:
        pc_flag_toggle(PC_FLAG_SNEAKING);
        return 0;
    case SKILL_LOCKPICK:
        if (isInCombat()) {
            // NOTE: Uninline.
            return action_use_skill_in_combat_error(a1);
        }

        if (PID_TYPE(a2->pid) != OBJ_TYPE_ITEM && PID_TYPE(a2->pid) != OBJ_TYPE_SCENERY) {
            return -1;
        }

        break;
    case SKILL_STEAL:
        if (isInCombat()) {
            // NOTE: Uninline.
            return action_use_skill_in_combat_error(a1);
        }

        if (PID_TYPE(a2->pid) != OBJ_TYPE_ITEM && PID_TYPE(a2->pid) != OBJ_TYPE_CRITTER && a2->pid != 0x2000384) {
            return -1;
        }

        if (a2 == a1) {
            return -1;
        }

        break;
    case SKILL_TRAPS:
        if (isInCombat()) {
            // NOTE: Uninline.
            return action_use_skill_in_combat_error(a1);
        }

        if (PID_TYPE(a2->pid) == OBJ_TYPE_CRITTER) {
            return -1;
        }

        break;
    case SKILL_SCIENCE:
    case SKILL_REPAIR:
        if (isInCombat()) {
            // NOTE: Uninline.
            return action_use_skill_in_combat_error(a1);
        }

        if (PID_TYPE(a2->pid) != OBJ_TYPE_CRITTER) {
            break;
        }

        if (critter_kill_count_type(a2) == KILL_TYPE_ROBOT) {
            break;
        }

        if (critter_kill_count_type(a2) == KILL_TYPE_BRAHMIN
            && skill == SKILL_SCIENCE) {
            break;
        }

        return -1;
    default:
        debug_printf("\nskill_use: invalid skill used.");
        break;
    }

    if (a1 == obj_dude) {
        int anim = FID_ANIM_TYPE(a1->fid);
        if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
            register_clear(a1);
        }
    }

    if (isInCombat()) {
        register_begin(ANIMATION_REQUEST_RESERVED);
        register_object_move_to_object(a1, a2, a1->data.critter.combat.ap, 0);
    } else {
        if (a1 == obj_dude) {
            register_begin(ANIMATION_REQUEST_RESERVED);
        } else {
            register_begin(ANIMATION_REQUEST_UNRESERVED);
        }

        if (a2 != obj_dude) {
            if (obj_dist(a1, a2) >= 5) {
                register_object_run_to_object(a1, a2, -1, 0);
            } else {
                register_object_move_to_object(a1, a2, -1, 0);
            }
        }
    }

    register_object_must_call(a1, a2, (AnimationCallback*)is_next_to, -1);

    int anim = (FID_TYPE(a2->fid) == OBJ_TYPE_CRITTER && critter_is_prone(a2))
        ? ANIM_MAGIC_HANDS_GROUND
        : ANIM_MAGIC_HANDS_MIDDLE;
    int fid = art_id(OBJ_TYPE_CRITTER, a1->fid & 0xFFF, anim, 0, a1->rotation + 1);

    CacheEntry* artHandle;
    Art* art = art_ptr_lock(fid, &artHandle);
    if (art != NULL) {
        art_frame_action_frame(art);
        art_ptr_unlock(artHandle);
    }

    register_object_animate(a1, anim, -1);
    // TODO: Get rid of casts.
    register_object_call3(a1, a2, (void*)skill, (AnimationCallback3*)obj_use_skill_on, -1);
    return register_end();
}

// 0x412758
Object* pick_object(int objectType, bool a2)
{
    Object* foundObject;
    int mouseEvent;
    int keyCode;

    foundObject = NULL;

    do {
        get_input();
    } while ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0);

    gmouse_set_cursor(MOUSE_CURSOR_PLUS);
    gmouse_3d_off();

    do {
        if (get_input() == -2) {
            mouseEvent = mouse_get_buttons();
            if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
                keyCode = 0;
                foundObject = object_under_mouse(objectType, a2, map_elevation);
                break;
            }

            if ((mouseEvent & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0) {
                keyCode = KEY_ESCAPE;
                break;
            }
        }
    } while (game_user_wants_to_quit == 0);

    gmouse_set_cursor(MOUSE_CURSOR_ARROW);
    gmouse_3d_on();

    if (keyCode == KEY_ESCAPE) {
        return NULL;
    }

    return foundObject;
}

// 0x4127E0
int pick_hex()
{
    int elevation;
    int inputEvent;
    int tile;
    Rect rect;

    elevation = map_elevation;

    while (1) {
        inputEvent = get_input();
        if (inputEvent == -2) {
            break;
        }

        if (inputEvent == KEY_CTRL_ARROW_RIGHT) {
            rotation++;
            if (rotation > 5) {
                rotation = 0;
            }

            obj_set_rotation(obj_mouse, rotation, &rect);
            tile_refresh_rect(&rect, obj_mouse->elevation);
        }

        if (inputEvent == KEY_CTRL_ARROW_LEFT) {
            rotation--;
            if (rotation == -1) {
                rotation = 5;
            }

            obj_set_rotation(obj_mouse, rotation, &rect);
            tile_refresh_rect(&rect, obj_mouse->elevation);
        }

        if (inputEvent == KEY_PAGE_UP || inputEvent == KEY_PAGE_DOWN) {
            if (inputEvent == KEY_PAGE_UP) {
                map_set_elevation(map_elevation + 1);
            } else {
                map_set_elevation(map_elevation - 1);
            }

            rect.ulx = 30;
            rect.uly = 62;
            rect.lrx = 50;
            rect.lry = 88;
        }

        if (game_user_wants_to_quit != 0) {
            return -1;
        }
    }

    if ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_DOWN) == 0) {
        return -1;
    }

    if (!mouse_click_in(0, 0, scr_size.lrx - scr_size.ulx, scr_size.lry - scr_size.uly - 100)) {
        return -1;
    }

    mouse_get_position(&(rect.ulx), &(rect.uly));

    tile = tile_num(rect.ulx, rect.uly, elevation);
    if (tile == -1) {
        return -1;
    }

    return tile;
}

// 0x412950
bool is_hit_from_front(Object* a1, Object* a2)
{
    int diff = a1->rotation - a2->rotation;
    if (diff < 0) {
        diff = -diff;
    }

    return diff != 0 && diff != 1 && diff != 5;
}

// 0x412974
bool can_see(Object* a1, Object* a2)
{
    int diff;

    diff = a1->rotation - tile_dir(a1->tile, a2->tile);
    if (diff < 0) {
        diff = -diff;
    }

    return diff == 0 || diff == 1 || diff == 5;
}

// 0x4129A4
int pick_fall(Object* obj, int anim)
{
    int i;
    int rotation;
    int tile_num;
    int fid;

    if (anim == ANIM_FALL_FRONT) {
        rotation = obj->rotation;
        for (i = 1; i < 3; i++) {
            tile_num = tile_num_in_direction(obj->tile, rotation, i);
            if (obj_blocking_at(obj, tile_num, obj->elevation) != NULL) {
                anim = ANIM_FALL_BACK;
                break;
            }
        }
    } else if (anim == ANIM_FALL_BACK) {
        rotation = (obj->rotation + 3) % ROTATION_COUNT;
        for (i = 1; i < 3; i++) {
            tile_num = tile_num_in_direction(obj->tile, rotation, i);
            if (obj_blocking_at(obj, tile_num, obj->elevation) != NULL) {
                anim = ANIM_FALL_FRONT;
                break;
            }
        }
    }

    if (anim == ANIM_FALL_FRONT) {
        fid = art_id(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, ANIM_FALL_FRONT, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
        if (!art_exists(fid)) {
            anim = ANIM_FALL_BACK;
        }
    }

    return anim;
}

// 0x412A6C
int action_explode(int tile, int elevation, int minDamage, int maxDamage, Object* a5, bool premature)
{
    if (premature && action_in_explode) {
        return -2;
    }

    Attack* attack = (Attack*)mem_malloc(sizeof(*attack));
    if (attack == NULL) {
        return -1;
    }

    Object* explosion;
    int fid = art_id(OBJ_TYPE_MISC, 10, 0, 0, 0);
    if (obj_new(&explosion, fid, -1) == -1) {
        mem_free(attack);
        return -1;
    }

    obj_turn_off(explosion, NULL);
    explosion->flags |= OBJECT_NO_SAVE;

    obj_move_to_tile(explosion, tile, elevation, NULL);

    Object* adjacentExplosions[ROTATION_COUNT];
    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        int fid = art_id(OBJ_TYPE_MISC, 10, 0, 0, 0);
        if (obj_new(&(adjacentExplosions[rotation]), fid, -1) == -1) {
            while (--rotation >= 0) {
                obj_erase_object(adjacentExplosions[rotation], NULL);
            }

            obj_erase_object(explosion, NULL);
            mem_free(attack);
            return -1;
        }

        obj_turn_off(adjacentExplosions[rotation], NULL);
        adjacentExplosions[rotation]->flags |= OBJECT_NO_SAVE;

        int adjacentTile = tile_num_in_direction(tile, rotation, 1);
        obj_move_to_tile(adjacentExplosions[rotation], adjacentTile, elevation, NULL);
    }

    Object* critter = obj_blocking_at(NULL, tile, elevation);
    if (critter != NULL) {
        if (FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER || (critter->data.critter.combat.results & DAM_DEAD) != 0) {
            critter = NULL;
        }
    }

    combat_ctd_init(attack, explosion, critter, HIT_MODE_PUNCH, HIT_LOCATION_TORSO);

    attack->tile = tile;
    attack->attackerFlags = DAM_HIT;

    game_ui_disable(1);

    if (critter != NULL) {
        if (register_clear(critter) == -2) {
            debug_printf("Cannot clear target's animation for action_explode!\n");
        }
        attack->defenderDamage = compute_explosion_damage(minDamage, maxDamage, critter, &(attack->defenderKnockback));
    }

    compute_explosion_on_extras(attack, 0, 0, 1);

    for (int index = 0; index < attack->extrasLength; index++) {
        Object* critter = attack->extras[index];
        if (register_clear(critter) == -2) {
            debug_printf("Cannot clear extra's animation for action_explode!\n");
        }

        attack->extrasDamage[index] = compute_explosion_damage(minDamage, maxDamage, critter, &(attack->extrasKnockback[index]));
    }

    death_checks(attack);

    if (premature) {
        action_in_explode = true;

        register_begin(ANIMATION_REQUEST_RESERVED);
        register_priority(1);
        register_object_play_sfx(explosion, "whn1xxx1", 0);
        register_object_funset(explosion, OBJECT_HIDDEN, 0);
        register_object_animate_and_hide(explosion, ANIM_STAND, 0);
        show_damage(attack, 0, 1);

        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
            register_object_funset(adjacentExplosions[rotation], OBJECT_HIDDEN, 0);
            register_object_animate_and_hide(adjacentExplosions[rotation], ANIM_STAND, 0);
        }

        register_object_must_call(explosion, 0, (AnimationCallback*)combat_explode_scenery, -1);
        register_object_must_erase(explosion);

        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
            register_object_must_erase(adjacentExplosions[rotation]);
        }

        register_object_must_call(attack, a5, (AnimationCallback*)report_explosion, -1);
        register_object_must_call(NULL, NULL, (AnimationCallback*)finished_explosion, -1);
        if (register_end() == -1) {
            action_in_explode = false;

            obj_erase_object(explosion, NULL);

            for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
                obj_erase_object(adjacentExplosions[rotation], NULL);
            }

            mem_free(attack);

            game_ui_enable();
            return -1;
        }

        show_damage_extras(attack);
    } else {
        if (critter != NULL) {
            if ((attack->defenderFlags & DAM_DEAD) != 0) {
                critter_kill(critter, -1, false);
            }
        }

        for (int index = 0; index < attack->extrasLength; index++) {
            if ((attack->extrasFlags[index] & DAM_DEAD) != 0) {
                critter_kill(attack->extras[index], -1, false);
            }
        }

        report_explosion(attack, a5);

        combat_explode_scenery(explosion, NULL);

        obj_erase_object(explosion, NULL);

        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
            obj_erase_object(adjacentExplosions[rotation], NULL);
        }
    }

    return 0;
}

// 0x412EBC
static int report_explosion(Attack* attack, Object* a2)
{
    bool mainTargetWasDead;
    if (attack->defender != NULL) {
        mainTargetWasDead = (attack->defender->data.critter.combat.results & DAM_DEAD) != 0;
    } else {
        mainTargetWasDead = false;
    }

    bool extrasWasDead[6];
    for (int index = 0; index < attack->extrasLength; index++) {
        extrasWasDead[index] = (attack->extras[index]->data.critter.combat.results & DAM_DEAD) != 0;
    }

    death_checks(attack);
    combat_display(attack);
    apply_damage(attack, false);

    Object* anyDefender = NULL;
    int xp = 0;
    if (a2 != NULL) {
        if (attack->defender != NULL && attack->defender != a2) {
            if ((attack->defender->data.critter.combat.results & DAM_DEAD) != 0) {
                if (a2 == obj_dude && !mainTargetWasDead) {
                    xp += critter_kill_exps(attack->defender);
                }
            } else {
                critter_set_who_hit_me(attack->defender, a2);
                anyDefender = attack->defender;
            }
        }

        for (int index = 0; index < attack->extrasLength; index++) {
            Object* critter = attack->extras[index];
            if (critter != a2) {
                if ((critter->data.critter.combat.results & DAM_DEAD) != 0) {
                    if (a2 == obj_dude && !extrasWasDead[index]) {
                        xp += critter_kill_exps(critter);
                    }
                } else {
                    critter_set_who_hit_me(critter, a2);

                    if (anyDefender == NULL) {
                        anyDefender = critter;
                    }
                }
            }
        }

        if (anyDefender != NULL) {
            if (!isInCombat()) {
                STRUCT_664980 combat;
                combat.attacker = anyDefender;
                combat.defender = a2;
                combat.actionPointsBonus = 0;
                combat.accuracyBonus = 0;
                combat.damageBonus = 0;
                combat.minDamage = 0;
                combat.maxDamage = INT_MAX;
                combat.field_1C = 0;
                scripts_request_combat(&combat);
            }
        }
    }

    mem_free(attack);
    game_ui_enable();

    if (a2 == obj_dude) {
        combat_give_exps(xp);
    }

    return 0;
}

// 0x413038
static int finished_explosion(Object* a1, Object* a2)
{
    action_in_explode = false;
    return 0;
}

// 0x413044
static int compute_explosion_damage(int min, int max, Object* a3, int* a4)
{
    int v5 = roll_random(min, max);
    int v7 = v5 - stat_level(a3, STAT_DAMAGE_THRESHOLD_EXPLOSION);
    if (v7 > 0) {
        v7 -= stat_level(a3, STAT_DAMAGE_RESISTANCE_EXPLOSION) * v7 / 100;
    }

    if (v7 < 0) {
        v7 = 0;
    }

    if (a4 != NULL) {
        if ((a3->flags & OBJECT_MULTIHEX) == 0) {
            *a4 = v7 / 10;
        }
    }

    return v7;
}

// 0x4130A8
int action_talk_to(Object* a1, Object* a2)
{
    if (a1 != obj_dude) {
        return -1;
    }

    if (FID_TYPE(a2->fid) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    int anim = FID_ANIM_TYPE(obj_dude->fid);
    if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
        register_clear(obj_dude);
    }

    if (isInCombat()) {
        register_begin(ANIMATION_REQUEST_RESERVED);
        register_object_move_to_object(a1, a2, a1->data.critter.combat.ap, 0);
    } else {
        register_begin(a1 == obj_dude ? ANIMATION_REQUEST_RESERVED : ANIMATION_REQUEST_UNRESERVED);

        if (obj_dist(a1, a2) >= 9 || combat_is_shot_blocked(a1, a1->tile, a2->tile, a2, NULL)) {
            register_object_run_to_object(a1, a2, -1, 0);
        }
    }

    register_object_must_call(a1, a2, (AnimationCallback*)can_talk_to, -1);
    register_object_call(a1, a2, (AnimationCallback*)talk_to, -1);
    return register_end();
}

// 0x413198
static int can_talk_to(Object* a1, Object* a2)
{
    MessageListItem messageListItem;

    if (combat_is_shot_blocked(a1, a1->tile, a2->tile, a2, NULL) || obj_dist(a1, a2) >= 9) {
        if (a1 == obj_dude) {
            // You cannot get there. (used in actions.c)
            messageListItem.num = 2000;
            if (message_search(&misc_message_file, &messageListItem)) {
                display_print(messageListItem.text);
            }
        }

        return -1;
    }

    return 0;
}

// 0x413200
static int talk_to(Object* a1, Object* a2)
{
    scripts_request_dialog(a2);
    return 0;
}

// 0x41320C
void action_dmg(int tile, int elevation, int minDamage, int maxDamage, int damageType, bool animated, bool bypassArmor)
{
    Attack* attack = (Attack*)mem_malloc(sizeof(*attack));
    if (attack == NULL) {
        return;
    }

    Object* attacker;
    if (obj_new(&attacker, FID_0x20001F5, -1) == -1) {
        mem_free(attack);
        return;
    }

    obj_turn_off(attacker, NULL);

    attacker->flags |= OBJECT_NO_SAVE;

    obj_move_to_tile(attacker, tile, elevation, NULL);

    Object* defender = obj_blocking_at(NULL, tile, elevation);
    combat_ctd_init(attack, attacker, defender, HIT_MODE_PUNCH, HIT_LOCATION_TORSO);
    attack->tile = tile;
    attack->attackerFlags = DAM_HIT;
    game_ui_disable(1);

    if (defender != NULL) {
        register_clear(defender);

        int damage;
        if (bypassArmor) {
            damage = maxDamage;
        } else {
            damage = compute_dmg_damage(minDamage, maxDamage, defender, &(attack->defenderKnockback), damageType);
        }

        attack->defenderDamage = damage;
    }

    death_checks(attack);

    if (animated) {
        register_begin(ANIMATION_REQUEST_RESERVED);
        register_object_play_sfx(attacker, "whc1xxx1", 0);
        show_damage(attack, death_3[damageType], 0);
        register_object_must_call(attack, NULL, (AnimationCallback*)report_dmg, 0);
        register_object_must_erase(attacker);

        if (register_end() == -1) {
            obj_erase_object(attacker, NULL);
            mem_free(attack);
            return;
        }
    } else {
        if (defender != NULL) {
            if ((attack->defenderFlags & DAM_DEAD) != 0) {
                critter_kill(defender, -1, 1);
            }
        }

        // NOTE: Uninline.
        report_dmg(attack, NULL);

        obj_erase_object(attacker, NULL);
    }

    game_ui_enable();
}

// 0x4133B4
static int report_dmg(Attack* attack, Object* a2)
{
    combat_display(attack);
    apply_damage(attack, false);
    mem_free(attack);
    game_ui_enable();
    return 0;
}

// Calculate damage by applying threshold and resistances.
//
// 0x4133D8
static int compute_dmg_damage(int min_damage, int max_damage, Object* obj, int* knockback_distance, int damage_type)
{
    int damage;

    damage = roll_random(min_damage, max_damage) - stat_level(obj, STAT_DAMAGE_THRESHOLD + damage_type);
    if (damage > 0) {
        damage -= stat_level(obj, STAT_DAMAGE_RESISTANCE + damage_type) * damage / 100;
    }

    if (damage < 0) {
        damage = 0;
    }

    if (knockback_distance != NULL) {
        if ((obj->flags & OBJECT_MULTIHEX) == 0 && damage_type != DAMAGE_TYPE_ELECTRICAL) {
            *knockback_distance = damage / 10;
        }
    }

    return damage;
}

} // namespace fallout
