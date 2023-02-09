#include "game/queue.h"

#include "game/actions.h"
#include "game/critter.h"
#include "game/display.h"
#include "game/game.h"
#include "game/gsound.h"
#include "game/item.h"
#include "game/map.h"
#include "game/message.h"
#include "game/object.h"
#include "game/perk.h"
#include "game/protinst.h"
#include "game/proto.h"
#include "game/scripts.h"
#include "plib/gnw/memory.h"

namespace fallout {

typedef struct QueueListNode {
    // TODO: Make unsigned.
    int time;
    int type;
    Object* owner;
    void* data;
    struct QueueListNode* next;
} QueueListNode;

static int queue_destroy(Object* obj, void* data);
static int queue_explode(Object* obj, void* data);
static int queue_explode_exit(Object* obj, void* data);
static int queue_do_explosion(Object* obj, bool a2);
static int queue_premature(Object* obj, void* data);

// 0x5076FC
EventTypeDescription q_func[EVENT_TYPE_COUNT] = {
    { item_d_process, mem_free, item_d_load, item_d_save, true, item_d_clear },
    { critter_wake_up, NULL, NULL, NULL, true, critter_wake_clear },
    { item_wd_process, mem_free, item_wd_load, item_wd_save, true, item_wd_clear },
    { script_q_process, mem_free, script_q_load, script_q_save, true, NULL },
    { gtime_q_process, NULL, NULL, NULL, true, NULL },
    { critter_check_poison, NULL, NULL, NULL, false, NULL },
    { critter_process_rads, mem_free, critter_load_rads, critter_save_rads, false, NULL },
    { queue_destroy, NULL, NULL, NULL, true, queue_destroy },
    { queue_explode, NULL, NULL, NULL, true, queue_explode_exit },
    { item_m_trickle, NULL, NULL, NULL, true, item_m_turn_off_from_queue },
    { critter_sneak_check, NULL, NULL, NULL, true, critter_sneak_clear },
    { queue_premature, NULL, NULL, NULL, true, queue_explode_exit },
    { scr_map_q_process, NULL, NULL, NULL, true, NULL },
};

// 0x662F4C
static QueueListNode* queue;

// 0x490670
void queue_init()
{
    queue = NULL;
}

// 0x490680
int queue_reset()
{
    queue_clear();
    return 0;
}

// 0x490680
int queue_exit()
{
    queue_clear();
    return 0;
}

// 0x490688
int queue_load(DB_FILE* stream)
{
    int count;
    if (db_freadInt(stream, &count) == -1) {
        return -1;
    }

    queue = NULL;

    QueueListNode** nextPtr = &queue;

    int rc = 0;
    for (int index = 0; index < count; index += 1) {
        QueueListNode* queueListNode = (QueueListNode*)mem_malloc(sizeof(*queueListNode));
        if (queueListNode == NULL) {
            rc = -1;
            break;
        }

        if (db_freadInt(stream, &(queueListNode->time)) == -1) {
            mem_free(queueListNode);
            rc = -1;
            break;
        }

        if (db_freadInt(stream, &(queueListNode->type)) == -1) {
            mem_free(queueListNode);
            rc = -1;
            break;
        }

        int objectId;
        if (db_freadInt(stream, &objectId) == -1) {
            mem_free(queueListNode);
            rc = -1;
            break;
        }

        Object* obj;
        if (objectId == -2) {
            obj = NULL;
        } else {
            obj = obj_find_first();
            while (obj != NULL) {
                obj = inven_find_id(obj, objectId);
                if (obj != NULL) {
                    break;
                }
                obj = obj_find_next();
            }
        }

        queueListNode->owner = obj;

        EventTypeDescription* eventTypeDescription = &(q_func[queueListNode->type]);
        if (eventTypeDescription->readProc != NULL) {
            if (eventTypeDescription->readProc(stream, &(queueListNode->data)) == -1) {
                mem_free(queueListNode);
                rc = -1;
                break;
            }
        } else {
            queueListNode->data = NULL;
        }

        queueListNode->next = NULL;

        *nextPtr = queueListNode;
        nextPtr = &(queueListNode->next);
    }

    if (rc == -1) {
        while (queue != NULL) {
            QueueListNode* next = queue->next;

            EventTypeDescription* eventTypeDescription = &(q_func[queue->type]);
            if (eventTypeDescription->freeProc != NULL) {
                eventTypeDescription->freeProc(queue->data);
            }

            mem_free(queue);

            queue = next;
        }
    }

    return rc;
}

// 0x4907F4
int queue_save(DB_FILE* stream)
{
    QueueListNode* queueListNode;

    int count = 0;

    queueListNode = queue;
    while (queueListNode != NULL) {
        count += 1;
        queueListNode = queueListNode->next;
    }

    if (db_fwriteInt(stream, count) == -1) {
        return -1;
    }

    queueListNode = queue;
    while (queueListNode != NULL) {
        Object* object = queueListNode->owner;
        int objectId = object != NULL ? object->id : -2;

        if (db_fwriteInt(stream, queueListNode->time) == -1) {
            return -1;
        }

        if (db_fwriteInt(stream, queueListNode->type) == -1) {
            return -1;
        }

        if (db_fwriteInt(stream, objectId) == -1) {
            return -1;
        }

        EventTypeDescription* eventTypeDescription = &(q_func[queueListNode->type]);
        if (eventTypeDescription->writeProc != NULL) {
            if (eventTypeDescription->writeProc(stream, queueListNode->data) == -1) {
                return -1;
            }
        }

        queueListNode = queueListNode->next;
    }

    return 0;
}

// 0x4908A0
int queue_add(int delay, Object* obj, void* data, int eventType)
{
    QueueListNode* newQueueListNode = (QueueListNode*)mem_malloc(sizeof(QueueListNode));
    if (newQueueListNode == NULL) {
        return -1;
    }

    int v1 = game_time();
    int v2 = v1 + delay;
    newQueueListNode->time = v2;
    newQueueListNode->type = eventType;
    newQueueListNode->owner = obj;
    newQueueListNode->data = data;

    if (obj != NULL) {
        obj->flags |= OBJECT_USED;
    }

    QueueListNode** v3 = &queue;

    if (queue != NULL) {
        QueueListNode* v4;

        do {
            v4 = *v3;
            if (v2 < v4->time) {
                break;
            }
            v3 = &(v4->next);
        } while (v4->next != NULL);
    }

    newQueueListNode->next = *v3;
    *v3 = newQueueListNode;

    return 0;
}

// 0x490908
int queue_remove(Object* owner)
{
    QueueListNode* queueListNode = queue;
    QueueListNode** queueListNodePtr = &queue;

    while (queueListNode) {
        if (queueListNode->owner == owner) {
            QueueListNode* temp = queueListNode;

            queueListNode = queueListNode->next;
            *queueListNodePtr = queueListNode;

            EventTypeDescription* eventTypeDescription = &(q_func[temp->type]);
            if (eventTypeDescription->freeProc != NULL) {
                eventTypeDescription->freeProc(temp->data);
            }

            mem_free(temp);
        } else {
            queueListNodePtr = &(queueListNode->next);
            queueListNode = queueListNode->next;
        }
    }

    return 0;
}

// 0x490960
int queue_remove_this(Object* owner, int eventType)
{
    QueueListNode* queueListNode = queue;
    QueueListNode** queueListNodePtr = &queue;

    while (queueListNode) {
        if (queueListNode->owner == owner && queueListNode->type == eventType) {
            QueueListNode* temp = queueListNode;

            queueListNode = queueListNode->next;
            *queueListNodePtr = queueListNode;

            EventTypeDescription* eventTypeDescription = &(q_func[temp->type]);
            if (eventTypeDescription->freeProc != NULL) {
                eventTypeDescription->freeProc(temp->data);
            }

            mem_free(temp);
        } else {
            queueListNodePtr = &(queueListNode->next);
            queueListNode = queueListNode->next;
        }
    }

    return 0;
}

// Returns true if there is at least one event of given type scheduled.
//
// 0x4909BC
bool queue_find(Object* owner, int eventType)
{
    QueueListNode* queueListEvent = queue;
    while (queueListEvent != NULL) {
        if (owner == queueListEvent->owner && eventType == queueListEvent->type) {
            return true;
        }

        queueListEvent = queueListEvent->next;
    }

    return false;
}

// 0x4909E4
int queue_process()
{
    int time = game_time();
    int v1 = 0;

    while (queue != NULL) {
        QueueListNode* queueListNode = queue;
        if (time < queueListNode->time || v1 != 0) {
            break;
        }

        queue = queueListNode->next;

        EventTypeDescription* eventTypeDescription = &(q_func[queueListNode->type]);
        v1 = eventTypeDescription->handlerProc(queueListNode->owner, queueListNode->data);

        if (eventTypeDescription->freeProc != NULL) {
            eventTypeDescription->freeProc(queueListNode->data);
        }

        mem_free(queueListNode);
    }

    return v1;
}

// 0x490A5C
void queue_clear()
{
    QueueListNode* queueListNode = queue;
    while (queueListNode != NULL) {
        QueueListNode* next = queueListNode->next;

        EventTypeDescription* eventTypeDescription = &(q_func[queueListNode->type]);
        if (eventTypeDescription->freeProc != NULL) {
            eventTypeDescription->freeProc(queueListNode->data);
        }

        mem_free(queueListNode);

        queueListNode = next;
    }

    queue = NULL;
}

// 0x490AA4
void queue_clear_type(int eventType, QueueEventHandler* fn)
{
    QueueListNode** ptr = &queue;
    QueueListNode* curr = *ptr;

    while (curr != NULL) {
        if (eventType == curr->type) {
            QueueListNode* tmp = curr;

            *ptr = curr->next;
            curr = *ptr;

            if (fn != NULL && fn(tmp->owner, tmp->data) != 1) {
                *ptr = tmp;
                ptr = &(tmp->next);
            } else {
                EventTypeDescription* eventTypeDescription = &(q_func[tmp->type]);
                if (eventTypeDescription->freeProc != NULL) {
                    eventTypeDescription->freeProc(tmp->data);
                }

                mem_free(tmp);
            }
        } else {
            ptr = &(curr->next);
            curr = *ptr;
        }
    }
}

// TODO: Make unsigned.
//
// 0x490B1C
int queue_next_time()
{
    if (queue == NULL) {
        return 0;
    }

    return queue->time;
}

// 0x490B30
static int queue_destroy(Object* obj, void* data)
{
    obj_destroy(obj);
    return 1;
}

// 0x490B3C
static int queue_explode(Object* obj, void* data)
{
    return queue_do_explosion(obj, true);
}

// 0x490B44
static int queue_explode_exit(Object* obj, void* data)
{
    return queue_do_explosion(obj, false);
}

// 0x490B48
static int queue_do_explosion(Object* explosive, bool premature)
{
    Object* owner;
    int tile;
    int elevation;
    int min_damage;
    int max_damage;

    owner = obj_top_environment(explosive);
    if (owner) {
        tile = owner->tile;
        elevation = owner->elevation;
    } else {
        tile = explosive->tile;
        elevation = explosive->elevation;
    }

    if (explosive->pid == PROTO_ID_DYNAMITE_I || explosive->pid == PROTO_ID_DYNAMITE_II) {
        // Dynamite
        min_damage = 30;
        max_damage = 50;
    } else {
        // Plastic explosive
        min_damage = 40;
        max_damage = 80;
    }

    if (action_explode(tile, elevation, min_damage, max_damage, obj_dude, premature) == -2) {
        queue_add(50, explosive, NULL, EVENT_TYPE_EXPLOSION);
    } else {
        obj_destroy(explosive);
    }

    return 1;
}

// 0x490BCC
static int queue_premature(Object* obj, void* data)
{
    MessageListItem msg;

    // Due to your inept handling, the explosive detonates prematurely.
    msg.num = 4000;
    if (message_search(&misc_message_file, &msg)) {
        display_print(msg.text);
    }

    return queue_do_explosion(obj, true);
}

// 0x490C08
void queue_leaving_map()
{
    int index;

    for (index = 0; index < EVENT_TYPE_COUNT; index++) {
        if (q_func[index].field_10) {
            queue_clear_type(index, q_func[index].field_14);
        }
    }
}

} // namespace fallout
