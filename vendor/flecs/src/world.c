/**
 * @file world.c
 * @brief World-level API.
 */

#include "private_api.h"

/* Id flags */
const ecs_id_t ECS_PAIR =                                          (1ull << 63);
const ecs_id_t ECS_AUTO_OVERRIDE =                                 (1ull << 62);
const ecs_id_t ECS_TOGGLE =                                        (1ull << 61);

/** Builtin component ids */
const ecs_entity_t ecs_id(EcsComponent) =                                   1;
const ecs_entity_t ecs_id(EcsIdentifier) =                                  2;
const ecs_entity_t ecs_id(EcsPoly) =                                        3;

/* Poly target components */
const ecs_entity_t EcsQuery =                                               5;
const ecs_entity_t EcsObserver =                                            6;
const ecs_entity_t EcsSystem =                                              7;

/* Core scopes & entities */
const ecs_entity_t EcsWorld =                       FLECS_HI_COMPONENT_ID + 0;
const ecs_entity_t EcsFlecs =                       FLECS_HI_COMPONENT_ID + 1;
const ecs_entity_t EcsFlecsCore =                   FLECS_HI_COMPONENT_ID + 2;
const ecs_entity_t EcsFlecsInternals =              FLECS_HI_COMPONENT_ID + 3;
const ecs_entity_t EcsModule =                      FLECS_HI_COMPONENT_ID + 4;
const ecs_entity_t EcsPrivate =                     FLECS_HI_COMPONENT_ID + 5;
const ecs_entity_t EcsPrefab =                      FLECS_HI_COMPONENT_ID + 6;
const ecs_entity_t EcsDisabled =                    FLECS_HI_COMPONENT_ID + 7;
const ecs_entity_t EcsNotQueryable =                FLECS_HI_COMPONENT_ID + 8;

const ecs_entity_t EcsSlotOf =                      FLECS_HI_COMPONENT_ID + 9;
const ecs_entity_t EcsFlag =                        FLECS_HI_COMPONENT_ID + 10;

/* Marker entities for query encoding */
const ecs_entity_t EcsWildcard =                    FLECS_HI_COMPONENT_ID + 11;
const ecs_entity_t EcsAny =                         FLECS_HI_COMPONENT_ID + 12;
const ecs_entity_t EcsThis =                        FLECS_HI_COMPONENT_ID + 13;
const ecs_entity_t EcsVariable =                    FLECS_HI_COMPONENT_ID + 14;

/* Traits */
const ecs_entity_t EcsTransitive =                  FLECS_HI_COMPONENT_ID + 15;
const ecs_entity_t EcsReflexive =                   FLECS_HI_COMPONENT_ID + 16;
const ecs_entity_t EcsSymmetric =                   FLECS_HI_COMPONENT_ID + 17;
const ecs_entity_t EcsFinal =                       FLECS_HI_COMPONENT_ID + 18;
const ecs_entity_t EcsInheritable =                 FLECS_HI_COMPONENT_ID + 19;

const ecs_entity_t EcsOnInstantiate =               FLECS_HI_COMPONENT_ID + 20;
const ecs_entity_t EcsOverride =                    FLECS_HI_COMPONENT_ID + 21;
const ecs_entity_t EcsInherit =                     FLECS_HI_COMPONENT_ID + 22;
const ecs_entity_t EcsDontInherit =                 FLECS_HI_COMPONENT_ID + 23;
const ecs_entity_t EcsPairIsTag =                   FLECS_HI_COMPONENT_ID + 24;
const ecs_entity_t EcsExclusive =                   FLECS_HI_COMPONENT_ID + 25;
const ecs_entity_t EcsAcyclic =                     FLECS_HI_COMPONENT_ID + 26;
const ecs_entity_t EcsTraversable =                 FLECS_HI_COMPONENT_ID + 27;
const ecs_entity_t EcsWith =                        FLECS_HI_COMPONENT_ID + 28;
const ecs_entity_t EcsOneOf =                       FLECS_HI_COMPONENT_ID + 29;
const ecs_entity_t EcsCanToggle =                   FLECS_HI_COMPONENT_ID + 30;
const ecs_entity_t EcsTrait =                       FLECS_HI_COMPONENT_ID + 31;
const ecs_entity_t EcsRelationship =                FLECS_HI_COMPONENT_ID + 32;
const ecs_entity_t EcsTarget =                      FLECS_HI_COMPONENT_ID + 33;


/* Builtin relationships */
const ecs_entity_t EcsChildOf =                     FLECS_HI_COMPONENT_ID + 34;
const ecs_entity_t EcsIsA =                         FLECS_HI_COMPONENT_ID + 35;
const ecs_entity_t EcsDependsOn =                   FLECS_HI_COMPONENT_ID + 36;

/* Identifier tags */
const ecs_entity_t EcsName =                        FLECS_HI_COMPONENT_ID + 37;
const ecs_entity_t EcsSymbol =                      FLECS_HI_COMPONENT_ID + 38;
const ecs_entity_t EcsAlias =                       FLECS_HI_COMPONENT_ID + 39;

/* Events */
const ecs_entity_t EcsOnAdd =                       FLECS_HI_COMPONENT_ID + 40;
const ecs_entity_t EcsOnRemove =                    FLECS_HI_COMPONENT_ID + 41;
const ecs_entity_t EcsOnSet =                       FLECS_HI_COMPONENT_ID + 42;
const ecs_entity_t EcsOnDelete =                    FLECS_HI_COMPONENT_ID + 43;
const ecs_entity_t EcsOnDeleteTarget =              FLECS_HI_COMPONENT_ID + 44;
const ecs_entity_t EcsOnTableCreate =               FLECS_HI_COMPONENT_ID + 45;
const ecs_entity_t EcsOnTableDelete =               FLECS_HI_COMPONENT_ID + 46;

/* Timers */
const ecs_entity_t ecs_id(EcsTickSource) =          FLECS_HI_COMPONENT_ID + 49;
const ecs_entity_t ecs_id(EcsTimer) =               FLECS_HI_COMPONENT_ID + 50;
const ecs_entity_t ecs_id(EcsRateFilter) =          FLECS_HI_COMPONENT_ID + 51;

/* Actions */
const ecs_entity_t EcsRemove =                      FLECS_HI_COMPONENT_ID + 52;
const ecs_entity_t EcsDelete =                      FLECS_HI_COMPONENT_ID + 53;
const ecs_entity_t EcsPanic =                       FLECS_HI_COMPONENT_ID + 54;

/* Storage */
const ecs_entity_t EcsSparse =                      FLECS_HI_COMPONENT_ID + 55;
const ecs_entity_t EcsUnion =                       FLECS_HI_COMPONENT_ID + 56;

/* Misc */
const ecs_entity_t ecs_id(EcsDefaultChildComponent) = FLECS_HI_COMPONENT_ID + 57;

/* Builtin predicate ids (used by query engine) */
const ecs_entity_t EcsPredEq =                      FLECS_HI_COMPONENT_ID + 58;
const ecs_entity_t EcsPredMatch =                   FLECS_HI_COMPONENT_ID + 59;
const ecs_entity_t EcsPredLookup =                  FLECS_HI_COMPONENT_ID + 60;
const ecs_entity_t EcsScopeOpen =                   FLECS_HI_COMPONENT_ID + 61;
const ecs_entity_t EcsScopeClose =                  FLECS_HI_COMPONENT_ID + 62;

/* Systems */
const ecs_entity_t EcsMonitor =                     FLECS_HI_COMPONENT_ID + 63;
const ecs_entity_t EcsEmpty =                       FLECS_HI_COMPONENT_ID + 64;
const ecs_entity_t ecs_id(EcsPipeline) =            FLECS_HI_COMPONENT_ID + 65;
const ecs_entity_t EcsOnStart =                     FLECS_HI_COMPONENT_ID + 66;
const ecs_entity_t EcsPreFrame =                    FLECS_HI_COMPONENT_ID + 67;
const ecs_entity_t EcsOnLoad =                      FLECS_HI_COMPONENT_ID + 68;
const ecs_entity_t EcsPostLoad =                    FLECS_HI_COMPONENT_ID + 69;
const ecs_entity_t EcsPreUpdate =                   FLECS_HI_COMPONENT_ID + 70;
const ecs_entity_t EcsOnUpdate =                    FLECS_HI_COMPONENT_ID + 71;
const ecs_entity_t EcsOnValidate =                  FLECS_HI_COMPONENT_ID + 72;
const ecs_entity_t EcsPostUpdate =                  FLECS_HI_COMPONENT_ID + 73;
const ecs_entity_t EcsPreStore =                    FLECS_HI_COMPONENT_ID + 74;
const ecs_entity_t EcsOnStore =                     FLECS_HI_COMPONENT_ID + 75;
const ecs_entity_t EcsPostFrame =                   FLECS_HI_COMPONENT_ID + 76;
const ecs_entity_t EcsPhase =                       FLECS_HI_COMPONENT_ID + 77;

/* Meta primitive components (don't use low ids to save id space) */
#ifdef FLECS_META
const ecs_entity_t ecs_id(ecs_bool_t) =             FLECS_HI_COMPONENT_ID + 80;
const ecs_entity_t ecs_id(ecs_char_t) =             FLECS_HI_COMPONENT_ID + 81;
const ecs_entity_t ecs_id(ecs_byte_t) =             FLECS_HI_COMPONENT_ID + 82;
const ecs_entity_t ecs_id(ecs_u8_t) =               FLECS_HI_COMPONENT_ID + 83;
const ecs_entity_t ecs_id(ecs_u16_t) =              FLECS_HI_COMPONENT_ID + 84;
const ecs_entity_t ecs_id(ecs_u32_t) =              FLECS_HI_COMPONENT_ID + 85;
const ecs_entity_t ecs_id(ecs_u64_t) =              FLECS_HI_COMPONENT_ID + 86;
const ecs_entity_t ecs_id(ecs_uptr_t) =             FLECS_HI_COMPONENT_ID + 87;
const ecs_entity_t ecs_id(ecs_i8_t) =               FLECS_HI_COMPONENT_ID + 88;
const ecs_entity_t ecs_id(ecs_i16_t) =              FLECS_HI_COMPONENT_ID + 89;
const ecs_entity_t ecs_id(ecs_i32_t) =              FLECS_HI_COMPONENT_ID + 90;
const ecs_entity_t ecs_id(ecs_i64_t) =              FLECS_HI_COMPONENT_ID + 91;
const ecs_entity_t ecs_id(ecs_iptr_t) =             FLECS_HI_COMPONENT_ID + 92;
const ecs_entity_t ecs_id(ecs_f32_t) =              FLECS_HI_COMPONENT_ID + 93;
const ecs_entity_t ecs_id(ecs_f64_t) =              FLECS_HI_COMPONENT_ID + 94;
const ecs_entity_t ecs_id(ecs_string_t) =           FLECS_HI_COMPONENT_ID + 95;
const ecs_entity_t ecs_id(ecs_entity_t) =           FLECS_HI_COMPONENT_ID + 96;
const ecs_entity_t ecs_id(ecs_id_t) =               FLECS_HI_COMPONENT_ID + 97;

/** Meta module component ids */
const ecs_entity_t ecs_id(EcsType) =                FLECS_HI_COMPONENT_ID + 98;
const ecs_entity_t ecs_id(EcsTypeSerializer) =      FLECS_HI_COMPONENT_ID + 99;
const ecs_entity_t ecs_id(EcsPrimitive) =           FLECS_HI_COMPONENT_ID + 100;
const ecs_entity_t ecs_id(EcsEnum) =                FLECS_HI_COMPONENT_ID + 101;
const ecs_entity_t ecs_id(EcsBitmask) =             FLECS_HI_COMPONENT_ID + 102;
const ecs_entity_t ecs_id(EcsMember) =              FLECS_HI_COMPONENT_ID + 103;
const ecs_entity_t ecs_id(EcsMemberRanges) =        FLECS_HI_COMPONENT_ID + 104;
const ecs_entity_t ecs_id(EcsStruct) =              FLECS_HI_COMPONENT_ID + 105;
const ecs_entity_t ecs_id(EcsArray) =               FLECS_HI_COMPONENT_ID + 106;
const ecs_entity_t ecs_id(EcsVector) =              FLECS_HI_COMPONENT_ID + 107;
const ecs_entity_t ecs_id(EcsOpaque) =              FLECS_HI_COMPONENT_ID + 108;
const ecs_entity_t ecs_id(EcsUnit) =                FLECS_HI_COMPONENT_ID + 109;
const ecs_entity_t ecs_id(EcsUnitPrefix) =          FLECS_HI_COMPONENT_ID + 110;
const ecs_entity_t EcsQuantity =                    FLECS_HI_COMPONENT_ID + 112;
#endif

const ecs_entity_t EcsConstant =                    FLECS_HI_COMPONENT_ID + 111;

/* Doc module components */
#ifdef FLECS_DOC
const ecs_entity_t ecs_id(EcsDocDescription) =      FLECS_HI_COMPONENT_ID + 113;
const ecs_entity_t EcsDocBrief =                    FLECS_HI_COMPONENT_ID + 114;
const ecs_entity_t EcsDocDetail =                   FLECS_HI_COMPONENT_ID + 115;
const ecs_entity_t EcsDocLink =                     FLECS_HI_COMPONENT_ID + 116;
const ecs_entity_t EcsDocColor =                    FLECS_HI_COMPONENT_ID + 117;
const ecs_entity_t EcsDocUuid =                     FLECS_HI_COMPONENT_ID + 118;
#endif

/* REST module components */
#ifdef FLECS_REST
const ecs_entity_t ecs_id(EcsRest) =                FLECS_HI_COMPONENT_ID + 119;
#endif

/* Max static id:
 * #define EcsFirstUserEntityId (FLECS_HI_COMPONENT_ID + 128) */

/* Default lookup path */
static ecs_entity_t ecs_default_lookup_path[2] = { 0, 0 };

/* Declarations for addons. Located in world.c to avoid issues during linking of
 * static library */

#ifdef FLECS_ALERTS
ECS_COMPONENT_DECLARE(EcsAlert);
ECS_COMPONENT_DECLARE(EcsAlertInstance);
ECS_COMPONENT_DECLARE(EcsAlertsActive);
ECS_TAG_DECLARE(EcsAlertInfo);
ECS_TAG_DECLARE(EcsAlertWarning);
ECS_TAG_DECLARE(EcsAlertError);
ECS_TAG_DECLARE(EcsAlertCritical);
#endif
#ifdef FLECS_UNITS
ecs_entity_t EcsUnitPrefixes;

ecs_entity_t EcsYocto;
ecs_entity_t EcsZepto;
ecs_entity_t EcsAtto;
ecs_entity_t EcsFemto;
ecs_entity_t EcsPico;
ecs_entity_t EcsNano;
ecs_entity_t EcsMicro;
ecs_entity_t EcsMilli;
ecs_entity_t EcsCenti;
ecs_entity_t EcsDeci;
ecs_entity_t EcsDeca;
ecs_entity_t EcsHecto;
ecs_entity_t EcsKilo;
ecs_entity_t EcsMega;
ecs_entity_t EcsGiga;
ecs_entity_t EcsTera;
ecs_entity_t EcsPeta;
ecs_entity_t EcsExa;
ecs_entity_t EcsZetta;
ecs_entity_t EcsYotta;

ecs_entity_t EcsKibi;
ecs_entity_t EcsMebi;
ecs_entity_t EcsGibi;
ecs_entity_t EcsTebi;
ecs_entity_t EcsPebi;
ecs_entity_t EcsExbi;
ecs_entity_t EcsZebi;
ecs_entity_t EcsYobi;

ecs_entity_t EcsDuration;
    ecs_entity_t EcsPicoSeconds;
    ecs_entity_t EcsNanoSeconds;
    ecs_entity_t EcsMicroSeconds;
    ecs_entity_t EcsMilliSeconds;
    ecs_entity_t EcsSeconds;
    ecs_entity_t EcsMinutes;
    ecs_entity_t EcsHours;
    ecs_entity_t EcsDays;

ecs_entity_t EcsTime;
    ecs_entity_t EcsDate;

ecs_entity_t EcsMass;
    ecs_entity_t EcsGrams;
    ecs_entity_t EcsKiloGrams;

ecs_entity_t EcsElectricCurrent;
    ecs_entity_t EcsAmpere;

ecs_entity_t EcsAmount;
    ecs_entity_t EcsMole;

ecs_entity_t EcsLuminousIntensity;
    ecs_entity_t EcsCandela;

ecs_entity_t EcsForce;
    ecs_entity_t EcsNewton;

ecs_entity_t EcsLength;
    ecs_entity_t EcsMeters;
        ecs_entity_t EcsPicoMeters;
        ecs_entity_t EcsNanoMeters;
        ecs_entity_t EcsMicroMeters;
        ecs_entity_t EcsMilliMeters;
        ecs_entity_t EcsCentiMeters;
        ecs_entity_t EcsKiloMeters;
    ecs_entity_t EcsMiles;
    ecs_entity_t EcsPixels;

ecs_entity_t EcsPressure;
    ecs_entity_t EcsPascal;
    ecs_entity_t EcsBar;

ecs_entity_t EcsSpeed;
    ecs_entity_t EcsMetersPerSecond;
    ecs_entity_t EcsKiloMetersPerSecond;
    ecs_entity_t EcsKiloMetersPerHour;
    ecs_entity_t EcsMilesPerHour;

ecs_entity_t EcsAcceleration;

ecs_entity_t EcsTemperature;
    ecs_entity_t EcsKelvin;
    ecs_entity_t EcsCelsius;
    ecs_entity_t EcsFahrenheit;

ecs_entity_t EcsData;
    ecs_entity_t EcsBits;
        ecs_entity_t EcsKiloBits;
        ecs_entity_t EcsMegaBits;
        ecs_entity_t EcsGigaBits;
    ecs_entity_t EcsBytes;
        ecs_entity_t EcsKiloBytes;
        ecs_entity_t EcsMegaBytes;
        ecs_entity_t EcsGigaBytes;
        ecs_entity_t EcsKibiBytes;
        ecs_entity_t EcsGibiBytes;
        ecs_entity_t EcsMebiBytes;

ecs_entity_t EcsDataRate;
    ecs_entity_t EcsBitsPerSecond;
    ecs_entity_t EcsKiloBitsPerSecond;
    ecs_entity_t EcsMegaBitsPerSecond;
    ecs_entity_t EcsGigaBitsPerSecond;
    ecs_entity_t EcsBytesPerSecond;
    ecs_entity_t EcsKiloBytesPerSecond;
    ecs_entity_t EcsMegaBytesPerSecond;
    ecs_entity_t EcsGigaBytesPerSecond;

ecs_entity_t EcsPercentage;

ecs_entity_t EcsAngle;
    ecs_entity_t EcsRadians;
    ecs_entity_t EcsDegrees;

ecs_entity_t EcsColor;
    ecs_entity_t EcsColorRgb;
    ecs_entity_t EcsColorHsl;
    ecs_entity_t EcsColorCss;

ecs_entity_t EcsBel;
ecs_entity_t EcsDeciBel;

ecs_entity_t EcsFrequency;
    ecs_entity_t EcsHertz;
    ecs_entity_t EcsKiloHertz;
    ecs_entity_t EcsMegaHertz;
    ecs_entity_t EcsGigaHertz;

ecs_entity_t EcsUri;
    ecs_entity_t EcsUriHyperlink;
    ecs_entity_t EcsUriImage;
    ecs_entity_t EcsUriFile;
#endif

/* -- Private functions -- */

ecs_stage_t* flecs_stage_from_readonly_world(
    const ecs_world_t *world)
{
    ecs_assert(flecs_poly_is(world, ecs_world_t) ||
               flecs_poly_is(world, ecs_stage_t),
               ECS_INTERNAL_ERROR,
               NULL);

    if (flecs_poly_is(world, ecs_world_t)) {
        return ECS_CONST_CAST(ecs_stage_t*, world->stages[0]);
    } else if (flecs_poly_is(world, ecs_stage_t)) {
        return ECS_CONST_CAST(ecs_stage_t*, world);
    }

    return NULL;
}

ecs_stage_t* flecs_stage_from_world(
    ecs_world_t **world_ptr)
{
    ecs_world_t *world = *world_ptr;

    ecs_assert(flecs_poly_is(world, ecs_world_t) ||
               flecs_poly_is(world, ecs_stage_t),
               ECS_INTERNAL_ERROR,
               NULL);

    if (flecs_poly_is(world, ecs_world_t)) {
        return world->stages[0];
    }

    *world_ptr = ((ecs_stage_t*)world)->world;
    return ECS_CONST_CAST(ecs_stage_t*, world);
}

ecs_world_t* flecs_suspend_readonly(
    const ecs_world_t *stage_world,
    ecs_suspend_readonly_state_t *state)
{
    ecs_assert(stage_world != NULL, ECS_INTERNAL_ERROR, NULL);
    ecs_assert(state != NULL, ECS_INTERNAL_ERROR, NULL);

    ecs_world_t *world =
        ECS_CONST_CAST(ecs_world_t*, ecs_get_world(stage_world));
    flecs_poly_assert(world, ecs_world_t);

    bool is_readonly = ECS_BIT_IS_SET(world->flags, EcsWorldReadonly);
    ecs_world_t *temp_world = world;
    ecs_stage_t *stage = flecs_stage_from_world(&temp_world);

    if (!is_readonly && !stage->defer) {
        state->is_readonly = false;
        state->is_deferred = false;
        return world;
    }

    ecs_dbg_3("suspending readonly mode");

    /* Cannot suspend when running with multiple threads */
    ecs_assert(!(world->flags & EcsWorldReadonly) ||
        !(world->flags & EcsWorldMultiThreaded), 
            ECS_INVALID_WHILE_READONLY, NULL);

    state->is_readonly = is_readonly;
    state->is_deferred = stage->defer != 0;
    state->cmd_flushing = stage->cmd_flushing;

    /* Silence readonly checks */
    world->flags &= ~EcsWorldReadonly;
    stage->cmd_flushing = false;

    /* Hack around safety checks (this ought to look ugly) */
    state->defer_count = stage->defer;
    state->cmd_stack[0] = stage->cmd_stack[0];
    state->cmd_stack[1] = stage->cmd_stack[1];
    state->cmd = stage->cmd;

    flecs_commands_init(stage, &stage->cmd_stack[0]);
    flecs_commands_init(stage, &stage->cmd_stack[1]);
    stage->cmd = &stage->cmd_stack[0];

    state->scope = stage->scope;
    state->with = stage->with;
    stage->defer = 0;

    return world;
}

void flecs_resume_readonly(
    ecs_world_t *world,
    ecs_suspend_readonly_state_t *state)
{
    flecs_poly_assert(world, ecs_world_t);
    ecs_assert(state != NULL, ECS_INTERNAL_ERROR, NULL);

    ecs_world_t *temp_world = world;
    ecs_stage_t *stage = flecs_stage_from_world(&temp_world);

    if (state->is_readonly || state->is_deferred) {
        ecs_dbg_3("resuming readonly mode");

        ecs_run_aperiodic(world, 0);

        /* Restore readonly state / defer count */
        ECS_BIT_COND(world->flags, EcsWorldReadonly, state->is_readonly);
        stage->defer = state->defer_count;
        stage->cmd_flushing = state->cmd_flushing;
        flecs_commands_fini(stage, &stage->cmd_stack[0]);
        flecs_commands_fini(stage, &stage->cmd_stack[1]);
        stage->cmd_stack[0] = state->cmd_stack[0];
        stage->cmd_stack[1] = state->cmd_stack[1];
        stage->cmd = state->cmd;
        
        stage->scope = state->scope;
        stage->with = state->with;
    }
}

/* Evaluate component monitor. If a monitored entity changed it will have set a
 * flag in one of the world's component monitors. Queries can register
 * themselves with component monitors to determine whether they need to rematch
 * with tables. */
static
void flecs_eval_component_monitor(
    ecs_world_t *world)
{
    flecs_poly_assert(world, ecs_world_t);

    if (!world->monitors.is_dirty) {
        return;
    }

    world->info.eval_comp_monitors_total ++;

    ecs_os_perf_trace_push("flecs.component_monitor.eval");

    world->monitors.is_dirty = false;

    ecs_map_iter_t it = ecs_map_iter(&world->monitors.monitors);
    while (ecs_map_next(&it)) {
        ecs_monitor_t *m = ecs_map_ptr(&it);
        if (!m->is_dirty) {
            continue;
        }

        m->is_dirty = false;

        int32_t i, count = ecs_vec_count(&m->queries);
        ecs_query_t **elems = ecs_vec_first(&m->queries);
        for (i = 0; i < count; i ++) {
            ecs_query_t *q = elems[i];
            flecs_poly_assert(q, ecs_query_t);
            flecs_query_cache_notify(world, q, &(ecs_query_cache_event_t) {
                .kind = EcsQueryTableRematch
            });
        }
    }

    ecs_os_perf_trace_pop("flecs.component_monitor.eval");
}

void flecs_monitor_mark_dirty(
    ecs_world_t *world,
    ecs_entity_t id)
{
    ecs_map_t *monitors = &world->monitors.monitors;

    /* Only flag if there are actually monitors registered, so that we
     * don't waste cycles evaluating monitors if there's no interest */
    if (ecs_map_is_init(monitors)) {
        ecs_monitor_t *m = ecs_map_get_deref(monitors, ecs_monitor_t, id);
        if (m) {
            if (!world->monitors.is_dirty) {
                world->monitor_generation ++;
            }
            m->is_dirty = true;
            world->monitors.is_dirty = true;
        }
    }
}

void flecs_monitor_register(
    ecs_world_t *world,
    ecs_entity_t id,
    ecs_query_t *query)
{
    ecs_assert(world != NULL, ECS_INTERNAL_ERROR, NULL);
    ecs_assert(id != 0, ECS_INTERNAL_ERROR, NULL);
    ecs_assert(query != NULL, ECS_INTERNAL_ERROR, NULL);
    flecs_poly_assert(query, ecs_query_t);

    ecs_map_t *monitors = &world->monitors.monitors;
    ecs_map_init_if(monitors, &world->allocator);
    ecs_monitor_t *m = ecs_map_ensure_alloc_t(monitors, ecs_monitor_t, id);
    ecs_vec_init_if_t(&m->queries, ecs_query_t*);
    ecs_query_t **q = ecs_vec_append_t(
        &world->allocator, &m->queries, ecs_query_t*);
    *q = query;
}

void flecs_monitor_unregister(
    ecs_world_t *world,
    ecs_entity_t id,
    ecs_query_t *query)
{
    ecs_assert(world != NULL, ECS_INTERNAL_ERROR, NULL);
    ecs_assert(id != 0, ECS_INTERNAL_ERROR, NULL);
    ecs_assert(query != NULL, ECS_INTERNAL_ERROR, NULL);
    flecs_poly_assert(query, ecs_query_t);

    ecs_map_t *monitors = &world->monitors.monitors;
    if (!ecs_map_is_init(monitors)) {
        return;
    }

    ecs_monitor_t *m = ecs_map_get_deref(monitors, ecs_monitor_t, id);
    if (!m) {
        return;
    }

    int32_t i, count = ecs_vec_count(&m->queries);
    ecs_query_t **queries = ecs_vec_first(&m->queries);
    for (i = 0; i < count; i ++) {
        if (queries[i] == query) {
            ecs_vec_remove_t(&m->queries, ecs_query_t*, i);
            count --;
            break;
        }
    }

    if (!count) {
        ecs_vec_fini_t(&world->allocator, &m->queries, ecs_query_t*);
        ecs_map_remove_free(monitors, id);
    }

    if (!ecs_map_count(monitors)) {
        ecs_map_fini(monitors);
    }
}

static
void flecs_init_store(
    ecs_world_t *world)
{
    ecs_os_memset(&world->store, 0, ECS_SIZEOF(ecs_store_t));

    ecs_allocator_t *a = &world->allocator;
    ecs_vec_init_t(a, &world->store.records, ecs_table_record_t, 0);
    ecs_vec_init_t(a, &world->store.marked_ids, ecs_marked_id_t, 0);
    ecs_vec_init_t(a, &world->store.deleted_components, ecs_entity_t, 0);

    /* Initialize entity index */
    flecs_entities_init(world);

    /* Initialize table sparse set */
    flecs_sparse_init_t(&world->store.tables, 
        a, &world->allocators.sparse_chunk, ecs_table_t);

    /* Initialize table map */
    flecs_table_hashmap_init(world, &world->store.table_map);
}

static
void flecs_clean_tables(
    ecs_world_t *world)
{
    int32_t i, count = flecs_sparse_count(&world->store.tables);
    for (i = 1; i < count; i ++) {
        ecs_table_t *t = flecs_sparse_get_dense_t(&world->store.tables,
            ecs_table_t, i);
        flecs_table_fini(world, t);
    }

    /* Free table types separately so that if application destructors rely on
     * a type it's still valid. */
    for (i = 1; i < count; i ++) {
        ecs_table_t *t = flecs_sparse_get_dense_t(&world->store.tables,
            ecs_table_t, i);
        flecs_table_free_type(world, t);
    }

    /* Clear the root table */
    if (count) {
        flecs_table_reset(world, &world->store.root);
    }
}

static
void flecs_fini_root_tables(
    ecs_world_t *world,
    ecs_component_record_t *cr,
    bool fini_targets)
{
    ecs_stage_t *stage0 = world->stages[0];
    bool finished = false;
    const ecs_size_t MAX_DEFERRED_DELETE_QUEUE_SIZE = 4096;
    while (!finished) {
        ecs_table_cache_iter_t it;
        ecs_size_t queue_size = 0;
        finished = true;

        bool has_roots = flecs_table_cache_iter(&cr->cache, &it);
        ecs_assert(has_roots == true, ECS_INTERNAL_ERROR, NULL);
        (void)has_roots;

        const ecs_table_record_t *tr;
        while ((tr = flecs_table_cache_next(&it, ecs_table_record_t))) {
            ecs_table_t *table = tr->hdr.table;
            if (table->flags & EcsTableHasBuiltins) {
                continue; /* Query out modules */
            }

            int32_t i, count = ecs_table_count(table);
            const ecs_entity_t *entities = ecs_table_entities(table);

            if (fini_targets) {
                /* Only delete entities that are used as pair target. Iterate
                * backwards to minimize moving entities around in table. */
                for (i = count - 1; i >= 0; i --) {
                    ecs_record_t *r = flecs_entities_get(world, entities[i]);
                    ecs_assert(r != NULL, ECS_INTERNAL_ERROR, NULL);
                    ecs_assert(r->table == table, ECS_INTERNAL_ERROR, NULL);
                    if (ECS_RECORD_TO_ROW_FLAGS(r->row) & EcsEntityIsTarget) {
                        ecs_delete(world, entities[i]);
                        queue_size++;
                        /* Flush the queue before it grows too big: */                     
                        if(queue_size >= MAX_DEFERRED_DELETE_QUEUE_SIZE) {
                            finished = false;
                            break; /* restart iteration */
                        }
                    }
                }
            } else {
                /* Delete remaining entities that are not in use (added to another
                * entity). This limits table moves during cleanup and delays
                * cleanup of tags. */
                for (i = count - 1; i >= 0; i --) {
                    ecs_record_t *r = flecs_entities_get(world, entities[i]);
                    ecs_assert(r != NULL, ECS_INTERNAL_ERROR, NULL);
                    ecs_assert(r->table == table, ECS_INTERNAL_ERROR, NULL);
                    if (!ECS_RECORD_TO_ROW_FLAGS(r->row)) {
                        ecs_delete(world, entities[i]);
                        queue_size++;                     
                        /* Flush the queue before it grows too big: */                     
                        if(queue_size >= MAX_DEFERRED_DELETE_QUEUE_SIZE) {
                            finished = false;
                            break; /* restart iteration */
                        }
                    }
                }
            }
            if(!finished) {
                /* flush queue and restart iteration */
                flecs_defer_end(world, stage0);
                flecs_defer_begin(world, stage0);
                break;
            }
        }
    }
}

static
void flecs_fini_roots(
    ecs_world_t *world)
{
    ecs_component_record_t *cr = flecs_components_get(world, ecs_pair(EcsChildOf, 0));

    /* Delete root entities that are not modules. This prioritizes deleting
     * regular entities first, which reduces the chance of components getting
     * destructed in random order because it got deleted before entities,
     * thereby bypassing the OnDeleteTarget policy. */
    flecs_defer_begin(world, world->stages[0]);
    flecs_fini_root_tables(world, cr, true);
    flecs_defer_end(world, world->stages[0]);

    flecs_defer_begin(world, world->stages[0]);
    flecs_fini_root_tables(world, cr, false);
    flecs_defer_end(world, world->stages[0]);
}

static
void flecs_fini_store(ecs_world_t *world) {
    flecs_clean_tables(world);
    flecs_sparse_fini(&world->store.tables);
    flecs_table_fini(world, &world->store.root);
    flecs_entities_clear(world);
    flecs_hashmap_fini(&world->store.table_map);

    ecs_assert(ecs_vec_count(&world->store.marked_ids) == 0, 
        ECS_INTERNAL_ERROR, NULL);
    ecs_assert(ecs_vec_count(&world->store.deleted_components) == 0, 
        ECS_INTERNAL_ERROR, NULL);

    ecs_allocator_t *a = &world->allocator;
    ecs_vec_fini_t(a, &world->store.records, ecs_table_record_t);
    ecs_vec_fini_t(a, &world->store.marked_ids, ecs_marked_id_t);
    ecs_vec_fini_t(a, &world->store.deleted_components, ecs_entity_t);
}

static 
void flecs_world_allocators_init(
    ecs_world_t *world)
{
    ecs_world_allocators_t *a = &world->allocators;

    flecs_allocator_init(&world->allocator);

    ecs_map_params_init(&a->ptr, &world->allocator);
    ecs_map_params_init(&a->query_table_list, &world->allocator);

    flecs_ballocator_init_t(&a->query_table, ecs_query_cache_table_t);
    flecs_ballocator_init_t(&a->query_table_match, ecs_query_cache_table_match_t);
    flecs_ballocator_init_n(&a->graph_edge_lo, ecs_graph_edge_t, FLECS_HI_COMPONENT_ID);
    flecs_ballocator_init_t(&a->graph_edge, ecs_graph_edge_t);
    flecs_ballocator_init_t(&a->id_record, ecs_component_record_t);
    flecs_ballocator_init_t(&a->pair_id_record, ecs_pair_id_record_t);
    flecs_ballocator_init_n(&a->id_record_chunk, ecs_component_record_t, FLECS_SPARSE_PAGE_SIZE);
    flecs_ballocator_init_t(&a->table_diff, ecs_table_diff_t);
    flecs_ballocator_init_n(&a->sparse_chunk, int32_t, FLECS_SPARSE_PAGE_SIZE);
    flecs_ballocator_init_t(&a->hashmap, ecs_hashmap_t);
    flecs_table_diff_builder_init(world, &world->allocators.diff_builder);
}

static
void flecs_world_allocators_fini(
    ecs_world_t *world)
{
    ecs_world_allocators_t *a = &world->allocators;

    ecs_map_params_fini(&a->ptr);
    ecs_map_params_fini(&a->query_table_list);
    flecs_ballocator_fini(&a->query_table);
    flecs_ballocator_fini(&a->query_table_match);
    flecs_ballocator_fini(&a->graph_edge_lo);
    flecs_ballocator_fini(&a->graph_edge);
    flecs_ballocator_fini(&a->id_record);
    flecs_ballocator_fini(&a->pair_id_record);
    flecs_ballocator_fini(&a->id_record_chunk);
    flecs_ballocator_fini(&a->table_diff);
    flecs_ballocator_fini(&a->sparse_chunk);
    flecs_ballocator_fini(&a->hashmap);
    flecs_table_diff_builder_fini(world, &world->allocators.diff_builder);

    flecs_allocator_fini(&world->allocator);
}

#define ECS_STRINGIFY_INNER(x) #x
#define ECS_STRINGIFY(x) ECS_STRINGIFY_INNER(x)

static const char flecs_compiler_info[]
#if defined(__clang__)
    = "clang " __clang_version__;
#elif defined(__GNUC__)
    = "gcc " ECS_STRINGIFY(__GNUC__) "." ECS_STRINGIFY(__GNUC_MINOR__);
#elif defined(_MSC_VER)
    = "msvc " ECS_STRINGIFY(_MSC_VER);
#elif defined(__TINYC__)
    = "tcc " ECS_STRINGIFY(__TINYC__);
#else
    = "unknown compiler";
#endif

static const char *flecs_addons_info[] = {
#ifdef FLECS_CPP
    "FLECS_CPP",
#endif
#ifdef FLECS_MODULE
    "FLECS_MODULE",
#endif
#ifdef FLECS_STATS
    "FLECS_STATS",
#endif
#ifdef FLECS_METRICS
    "FLECS_METRICS",
#endif
#ifdef FLECS_ALERTS
    "FLECS_ALERTS",
#endif
#ifdef FLECS_SYSTEM
    "FLECS_SYSTEM",
#endif
#ifdef FLECS_PIPELINE
    "FLECS_PIPELINE",
#endif
#ifdef FLECS_TIMER
    "FLECS_TIMER",
#endif
#ifdef FLECS_META
    "FLECS_META",
#endif
#ifdef FLECS_UNITS
    "FLECS_UNITS",
#endif
#ifdef FLECS_JSON
    "FLECS_JSON",
#endif
#ifdef FLECS_DOC
    "FLECS_DOC",
#endif
#ifdef FLECS_LOG
    "FLECS_LOG",
#endif
#ifdef FLECS_JOURNAL
    "FLECS_JOURNAL",
#endif
#ifdef FLECS_APP
    "FLECS_APP",
#endif
#ifdef FLECS_OS_API_IMPL
    "FLECS_OS_API_IMPL",
#endif
#ifdef FLECS_PARSER
    "FLECS_PARSER",
#endif
#ifdef FLECS_QUERY_DSL
    "FLECS_QUERY_DSL",
#endif
#ifdef FLECS_SCRIPT
    "FLECS_SCRIPT",
#endif
#ifdef FLECS_HTTP
    "FLECS_HTTP",
#endif
#ifdef FLECS_REST
    "FLECS_REST",
#endif
NULL
};

static const ecs_build_info_t flecs_build_info = {
    .compiler = flecs_compiler_info,
    .addons = flecs_addons_info,
#ifdef FLECS_DEBUG
    .debug = true,
#endif
#ifdef FLECS_SANITIZE
    .sanitize = true,
#endif
#ifdef FLECS_PERF_TRACE
    .perf_trace = true,
#endif
    .version = FLECS_VERSION,
    .version_major = FLECS_VERSION_MAJOR,
    .version_minor = FLECS_VERSION_MINOR,
    .version_patch = FLECS_VERSION_PATCH
};

static
void flecs_log_build_info(void) {
    const ecs_build_info_t *bi = ecs_get_build_info();
    ecs_assert(bi != NULL, ECS_INTERNAL_ERROR, NULL);

    ecs_trace("flecs version %s", bi->version);

    ecs_trace("addons included in build:");
    ecs_log_push();

    const char **addon = bi->addons;
    do {
        ecs_trace(addon[0]);
    } while ((++ addon)[0]);
    ecs_log_pop();

    if (bi->sanitize) {
        ecs_trace("sanitize build, rebuild without FLECS_SANITIZE for (much) "
            "improved performance");
    } else if (bi->debug) {
        ecs_trace("debug build, rebuild with NDEBUG or FLECS_NDEBUG for "
            "improved performance");
    } else {
        ecs_trace("#[green]release#[reset] build");
    }

    ecs_trace("compiled with %s", bi->compiler);
}

/* -- Public functions -- */

const ecs_build_info_t* ecs_get_build_info(void) {
    return &flecs_build_info;
}

const ecs_world_info_t* ecs_get_world_info(
    const ecs_world_t *world)
{
    world = ecs_get_world(world);
    return &world->info;
}

ecs_world_t *ecs_mini(void) {
#ifdef FLECS_OS_API_IMPL
    ecs_set_os_api_impl();
#endif
    ecs_os_init();

    ecs_trace("#[bold]bootstrapping world");
    ecs_log_push();

    ecs_trace("tracing enabled, call ecs_log_set_level(-1) to disable");

    if (!ecs_os_has_heap()) {
        ecs_abort(ECS_MISSING_OS_API, NULL);
    }

    if (!ecs_os_has_threading()) {
        ecs_trace("threading unavailable, to use threads set OS API first (see examples)");
    }

    if (!ecs_os_has_time()) {
        ecs_trace("time management not available");
    }

    flecs_log_build_info();

    ecs_world_t *world = ecs_os_calloc_t(ecs_world_t);
    ecs_assert(world != NULL, ECS_OUT_OF_MEMORY, NULL);
    flecs_poly_init(world, ecs_world_t);

    world->flags |= EcsWorldInit;

    flecs_world_allocators_init(world);
    ecs_allocator_t *a = &world->allocator;

    ecs_map_init(&world->type_info, a);
    ecs_map_init_w_params(&world->id_index_hi, &world->allocators.ptr);
    world->id_index_lo = ecs_os_calloc_n(
        ecs_component_record_t*, FLECS_HI_ID_RECORD_ID);
    flecs_observable_init(&world->observable);

    flecs_name_index_init(&world->aliases, a);
    flecs_name_index_init(&world->symbols, a);
    ecs_vec_init_t(a, &world->fini_actions, ecs_action_elem_t, 0);
    ecs_vec_init_t(a, &world->component_ids, ecs_id_t, 0);

    world->info.time_scale = 1.0;
    if (ecs_os_has_time()) {
        ecs_os_get_time(&world->world_start_time);
    }

    ecs_set_stage_count(world, 1);
    ecs_default_lookup_path[0] = EcsFlecsCore;
    ecs_set_lookup_path(world, ecs_default_lookup_path);
    flecs_init_store(world);

    flecs_bootstrap(world);

    world->flags &= ~EcsWorldInit;

    #ifdef FLECS_LOW_FOOTPRINT
    ecs_shrink(world);
    #endif

    ecs_trace("world ready!");
    ecs_log_pop();

    return world;
}

ecs_world_t *ecs_init(void) {
    ecs_world_t *world = ecs_mini();

#ifdef FLECS_MODULE_H
    ecs_trace("#[bold]import addons");
    ecs_log_push();
    ecs_trace("use ecs_mini to create world without importing addons");
#ifdef FLECS_SYSTEM
    ECS_IMPORT(world, FlecsSystem);
#endif
#ifdef FLECS_PIPELINE
    ECS_IMPORT(world, FlecsPipeline);
#endif
#ifdef FLECS_TIMER
    ECS_IMPORT(world, FlecsTimer);
#endif
#ifdef FLECS_META
    ECS_IMPORT(world, FlecsMeta);
#endif
#ifdef FLECS_DOC
    ECS_IMPORT(world, FlecsDoc);
#endif
#ifdef FLECS_SCRIPT
    ECS_IMPORT(world, FlecsScript);
#endif
#ifdef FLECS_REST
    ECS_IMPORT(world, FlecsRest);
#endif
#ifdef FLECS_UNITS
    ecs_trace("#[green]module#[reset] flecs.units is not automatically imported");
#endif
    ecs_trace("addons imported!");
    ecs_log_pop();
#endif
#ifdef FLECS_LOW_FOOTPRINT
    ecs_shrink(world);
#endif
    return world;
}

ecs_world_t* ecs_init_w_args(
    int argc,
    char *argv[])
{
    ecs_world_t *world = ecs_init();

    (void)argc;
    (void)argv;

#ifdef FLECS_DOC
    if (argc) {
        char *app = argv[0];
        char *last_elem = strrchr(app, '/');
        if (!last_elem) {
            last_elem = strrchr(app, '\\');
        }
        if (last_elem) {
            app = last_elem + 1;
        }
        ecs_set_pair(world, EcsWorld, EcsDocDescription, EcsName, {app});
    }
#endif

    return world;
}

void ecs_quit(
    ecs_world_t *world)
{
    ecs_check(world != NULL, ECS_INVALID_PARAMETER, NULL);
    flecs_stage_from_world(&world);
    world->flags |= EcsWorldQuit;
error:
    return;
}

bool ecs_should_quit(
    const ecs_world_t *world)
{
    ecs_check(world != NULL, ECS_INVALID_PARAMETER, NULL);
    world = ecs_get_world(world);
    return ECS_BIT_IS_SET(world->flags, EcsWorldQuit);
error:
    return true;
}

void flecs_notify_tables(
    ecs_world_t *world,
    ecs_id_t id,
    ecs_table_event_t *event)
{
    flecs_poly_assert(world, ecs_world_t);

    /* If no id is specified, broadcast to all tables */
    if (!id || id == EcsAny) {
        ecs_sparse_t *tables = &world->store.tables;
        int32_t i, count = flecs_sparse_count(tables);
        for (i = 0; i < count; i ++) {
            ecs_table_t *table = flecs_sparse_get_dense_t(tables, ecs_table_t, i);
            flecs_table_notify(world, table, id, event);
        }

    /* If id is specified, only broadcast to tables with id */
    } else {
        ecs_component_record_t *cr = flecs_components_get(world, id);
        if (!cr) {
            return;
        }

        ecs_table_cache_iter_t it;
        const ecs_table_record_t *tr;

        flecs_table_cache_all_iter(&cr->cache, &it);
        while ((tr = flecs_table_cache_next(&it, ecs_table_record_t))) {
            flecs_table_notify(world, tr->hdr.table, id, event);
        }
    }
}

void flecs_default_ctor(
    void *ptr, 
    int32_t count, 
    const ecs_type_info_t *ti)
{
    ecs_os_memset(ptr, 0, ti->size * count);
}

static
void flecs_default_copy_ctor(void *dst_ptr, const void *src_ptr,
    int32_t count, const ecs_type_info_t *ti)
{
    const ecs_type_hooks_t *cl = &ti->hooks;
    cl->ctor(dst_ptr, count, ti);
    cl->copy(dst_ptr, src_ptr, count, ti);
}

static
void flecs_default_move_ctor(void *dst_ptr, void *src_ptr,
    int32_t count, const ecs_type_info_t *ti)
{
    const ecs_type_hooks_t *cl = &ti->hooks;
    cl->ctor(dst_ptr, count, ti);
    cl->move(dst_ptr, src_ptr, count, ti);
}

static
void flecs_default_ctor_w_move_w_dtor(void *dst_ptr, void *src_ptr,
    int32_t count, const ecs_type_info_t *ti)
{
    const ecs_type_hooks_t *cl = &ti->hooks;
    cl->ctor(dst_ptr, count, ti);
    cl->move(dst_ptr, src_ptr, count, ti);
    cl->dtor(src_ptr, count, ti);
}

static
void flecs_default_move_ctor_w_dtor(void *dst_ptr, void *src_ptr,
    int32_t count, const ecs_type_info_t *ti)
{
    const ecs_type_hooks_t *cl = &ti->hooks;
    cl->move_ctor(dst_ptr, src_ptr, count, ti);
    cl->dtor(src_ptr, count, ti);
}

static
void flecs_default_move(void *dst_ptr, void *src_ptr,
    int32_t count, const ecs_type_info_t *ti)
{
    const ecs_type_hooks_t *cl = &ti->hooks;
    cl->move(dst_ptr, src_ptr, count, ti);
}

static
void flecs_default_dtor(void *dst_ptr, void *src_ptr,
    int32_t count, const ecs_type_info_t *ti)
{
    /* When there is no move, destruct the destination component & memcpy the
     * component to dst. The src component does not have to be destructed when
     * a component has a trivial move. */
    const ecs_type_hooks_t *cl = &ti->hooks;
    cl->dtor(dst_ptr, count, ti);
    ecs_os_memcpy(dst_ptr, src_ptr, flecs_uto(ecs_size_t, ti->size) * count);
}

static
void flecs_default_move_w_dtor(void *dst_ptr, void *src_ptr,
    int32_t count, const ecs_type_info_t *ti)
{
    /* If a component has a move, the move will take care of memcpying the data
     * and destroying any data in dst. Because this is not a trivial move, the
     * src component must also be destructed. */
    const ecs_type_hooks_t *cl = &ti->hooks;
    cl->move(dst_ptr, src_ptr, count, ti);
    cl->dtor(src_ptr, count, ti);
}

static
bool flecs_default_equals(const void *a_ptr, const void *b_ptr, const ecs_type_info_t* ti) {
    return ti->hooks.cmp(a_ptr, b_ptr, ti) == 0;
}

ECS_NORETURN static
void flecs_ctor_illegal(
    void * dst,
    int32_t count,
    const ecs_type_info_t *ti) {
    (void)dst; /* silence unused warning */
    (void)count;
    ecs_abort(ECS_INVALID_OPERATION, "invalid constructor for %s", ti->name);
}

ECS_NORETURN static
void flecs_dtor_illegal(
    void *dst,
    int32_t count,
    const ecs_type_info_t *ti) {
    (void)dst; /* silence unused warning */
    (void)count;
    ecs_abort(ECS_INVALID_OPERATION, "invalid destructor for %s", ti->name);
}

ECS_NORETURN static
void flecs_copy_illegal(
    void *dst,
    const void *src,
    int32_t count,
    const ecs_type_info_t *ti)
{
    (void)dst; /* silence unused warning */
    (void)src; 
    (void)count;
    ecs_abort(ECS_INVALID_OPERATION, "invalid copy assignment for %s", ti->name);
}

ECS_NORETURN static
void flecs_move_illegal(
    void * dst,
    void * src,
    int32_t count,
    const ecs_type_info_t *ti) {
    (void)dst; /* silence unused warning */
    (void)src;
    (void)count;
    ecs_abort(ECS_INVALID_OPERATION, "invalid move assignment for %s", ti->name);
}

ECS_NORETURN static
void flecs_copy_ctor_illegal(
    void *dst,
    const void *src,
    int32_t count,
    const ecs_type_info_t *ti)
{
    (void)dst; /* silence unused warning */
    (void)src;
    (void)count;
    ecs_abort(ECS_INVALID_OPERATION, "invalid copy construct for %s", ti->name);
}

ECS_NORETURN static
void flecs_move_ctor_illegal(
    void *dst,
    void *src,
    int32_t count,
    const ecs_type_info_t *ti)
{
    (void)dst; /* silence unused warning */
    (void)src;
    (void)count;
    ecs_abort(ECS_INVALID_OPERATION, "invalid move construct for %s", ti->name);
}

ECS_NORETURN static
int flecs_comp_illegal(
    const void *dst,
    const void *src,
    const ecs_type_info_t *ti)
{
    (void)dst; /* silence unused warning */
    (void)src;
    ecs_abort(ECS_INVALID_OPERATION, "invalid compare hook for %s", ti->name);
}

ECS_NORETURN static
bool flecs_equals_illegal(
    const void *dst,
    const void *src,
    const ecs_type_info_t *ti)
{
    (void)dst; /* silence unused warning */
    (void)src;
    ecs_abort(ECS_INVALID_OPERATION, "invalid equals hook for %s", ti->name);
}

void ecs_set_hooks_id(
    ecs_world_t *world,
    ecs_entity_t component,
    const ecs_type_hooks_t *h)
{
    ecs_check(world != NULL, ECS_INVALID_PARAMETER, NULL);

    /* TODO: Refactor to enforce flags consistency: */
    ecs_flags32_t flags = h->flags;
    flags &= ~((ecs_flags32_t)ECS_TYPE_HOOKS);

    ecs_check(!(flags & ECS_TYPE_HOOK_CTOR_ILLEGAL && 
        h->ctor != NULL && 
        h->ctor != flecs_ctor_illegal),
        ECS_INVALID_PARAMETER, "cannot specify both ctor hook and illegal flag");

    ecs_check(!(flags & ECS_TYPE_HOOK_DTOR_ILLEGAL && 
        h->dtor != NULL && 
        h->dtor != flecs_dtor_illegal),
        ECS_INVALID_PARAMETER, "cannot specify both dtor hook and illegal flag");

    ecs_check(!(flags & ECS_TYPE_HOOK_COPY_ILLEGAL && 
        h->copy != NULL && 
        h->copy != flecs_copy_illegal),
        ECS_INVALID_PARAMETER, "cannot specify both copy hook and illegal flag");

    ecs_check(!(flags & ECS_TYPE_HOOK_MOVE_ILLEGAL && 
        h->move != NULL && 
        h->move != flecs_move_illegal),
        ECS_INVALID_PARAMETER, "cannot specify both move hook and illegal flag");

    ecs_check(!(flags & ECS_TYPE_HOOK_COPY_CTOR_ILLEGAL && 
        h->copy_ctor != NULL && 
        h->copy_ctor != flecs_copy_ctor_illegal),
        ECS_INVALID_PARAMETER, "cannot specify both copy ctor hook and illegal flag");

    ecs_check(!(flags & ECS_TYPE_HOOK_MOVE_CTOR_ILLEGAL && 
        h->move_ctor != NULL && 
        h->move_ctor != flecs_move_ctor_illegal),
        ECS_INVALID_PARAMETER, "cannot specify both move ctor hook and illegal flag");

    ecs_check(!(flags & ECS_TYPE_HOOK_CTOR_MOVE_DTOR_ILLEGAL && 
        h->ctor_move_dtor != NULL && 
        h->ctor_move_dtor != flecs_move_ctor_illegal),
        ECS_INVALID_PARAMETER, "cannot specify both ctor move dtor hook and illegal flag");

    ecs_check(!(flags & ECS_TYPE_HOOK_MOVE_DTOR_ILLEGAL && 
        h->move_dtor != NULL && 
        h->move_dtor != flecs_move_ctor_illegal),
        ECS_INVALID_PARAMETER, "cannot specify both move dtor hook and illegal flag");

    ecs_check(!(flags & ECS_TYPE_HOOK_CMP_ILLEGAL && 
        h->cmp != NULL && 
        h->cmp != flecs_comp_illegal),
        ECS_INVALID_PARAMETER, "cannot specify both compare hook and illegal flag");

    ecs_check(!(flags & ECS_TYPE_HOOK_EQUALS_ILLEGAL && 
        h->equals != NULL && 
        h->equals != flecs_equals_illegal),
        ECS_INVALID_PARAMETER, "cannot specify both equals hook and illegal flag");


    flecs_stage_from_world(&world);

    /* Ensure that no tables have yet been created for the component */
    ecs_check( ecs_id_in_use(world, component) == false,
        ECS_ALREADY_IN_USE, ecs_get_name(world, component));
    ecs_check( ecs_id_in_use(world, ecs_pair(component, EcsWildcard)) == false,
        ECS_ALREADY_IN_USE, ecs_get_name(world, component));

    ecs_type_info_t *ti = flecs_type_info_ensure(world, component);
    ecs_assert(ti != NULL, ECS_INTERNAL_ERROR, NULL);

    ecs_check(!ti->component || ti->component == component,
        ECS_INCONSISTENT_COMPONENT_ACTION, NULL);

    if (!ti->size) {
        const EcsComponent *component_ptr = ecs_get(
            world, component, EcsComponent);

        /* Cannot register lifecycle actions for things that aren't a component */
        ecs_check(component_ptr != NULL, ECS_INVALID_PARAMETER, 
            "provided entity is not a component");
        ecs_check(component_ptr->size != 0, ECS_INVALID_PARAMETER,
            "cannot register type hooks for type with size 0");

        ti->size = component_ptr->size;
        ti->alignment = component_ptr->alignment;
    }

    if (h->ctor) ti->hooks.ctor = h->ctor;
    if (h->dtor) ti->hooks.dtor = h->dtor;
    if (h->copy) ti->hooks.copy = h->copy;
    if (h->move) ti->hooks.move = h->move;
    if (h->copy_ctor) ti->hooks.copy_ctor = h->copy_ctor;
    if (h->move_ctor) ti->hooks.move_ctor = h->move_ctor;
    if (h->ctor_move_dtor) ti->hooks.ctor_move_dtor = h->ctor_move_dtor;
    if (h->move_dtor) ti->hooks.move_dtor = h->move_dtor;
    if (h->cmp) ti->hooks.cmp = h->cmp;
    if (h->equals) ti->hooks.equals = h->equals;

    if (h->on_add) ti->hooks.on_add = h->on_add;
    if (h->on_remove) ti->hooks.on_remove = h->on_remove;
    if (h->on_set) ti->hooks.on_set = h->on_set;

    if (h->ctx) ti->hooks.ctx = h->ctx;
    if (h->binding_ctx) ti->hooks.binding_ctx = h->binding_ctx;
    if (h->lifecycle_ctx) ti->hooks.lifecycle_ctx = h->lifecycle_ctx;
    if (h->ctx_free) ti->hooks.ctx_free = h->ctx_free;
    if (h->binding_ctx_free) ti->hooks.binding_ctx_free = h->binding_ctx_free;
    if (h->lifecycle_ctx_free) ti->hooks.lifecycle_ctx_free = h->lifecycle_ctx_free;

    /* If no constructor is set, invoking any of the other lifecycle actions
     * is not safe as they will potentially access uninitialized memory. For
     * ease of use, if no constructor is specified, set a default one that
     * initializes the component to 0. */
    if (!h->ctor && (h->dtor || h->copy || h->move)) {
        ti->hooks.ctor = flecs_default_ctor;   
    }

    /* Set default copy ctor, move ctor and merge */
    if (!h->copy_ctor) {
        if(flags & ECS_TYPE_HOOK_COPY_ILLEGAL || flags & ECS_TYPE_HOOK_CTOR_ILLEGAL) {
            flags |= ECS_TYPE_HOOK_COPY_CTOR_ILLEGAL;
        } else if(h->copy) {
            ti->hooks.copy_ctor = flecs_default_copy_ctor;
        }
    }

    if (!h->move_ctor) {
        if(flags & ECS_TYPE_HOOK_MOVE_ILLEGAL || flags & ECS_TYPE_HOOK_CTOR_ILLEGAL) {
            flags |= ECS_TYPE_HOOK_MOVE_CTOR_ILLEGAL;
        } else if (h->move) {
            ti->hooks.move_ctor = flecs_default_move_ctor;
        }
    }

    if (!h->ctor_move_dtor) {
        ecs_flags32_t illegal_check = 0;
        if (h->move) {
            illegal_check |= ECS_TYPE_HOOK_MOVE_ILLEGAL;
            if (h->dtor) {
                illegal_check |= ECS_TYPE_HOOK_DTOR_ILLEGAL;
                if (h->move_ctor) {
                    illegal_check |= ECS_TYPE_HOOK_MOVE_CTOR_ILLEGAL;
                    /* If an explicit move ctor has been set, use callback
                     * that uses the move ctor vs. using a ctor+move */
                    ti->hooks.ctor_move_dtor = flecs_default_move_ctor_w_dtor;
                } else {
                    illegal_check |= ECS_TYPE_HOOK_CTOR_ILLEGAL;
                    /* If no explicit move_ctor has been set, use
                     * combination of ctor + move + dtor */
                    ti->hooks.ctor_move_dtor = flecs_default_ctor_w_move_w_dtor;
                }
            } else {
                illegal_check |= ECS_TYPE_HOOK_MOVE_CTOR_ILLEGAL;
                /* If no dtor has been set, this is just a move ctor */
                ti->hooks.ctor_move_dtor = ti->hooks.move_ctor;
            }
        } else {
            /* If move is not set but move_ctor and dtor is, we can still set
             * ctor_move_dtor. */
            if (h->move_ctor) {
                illegal_check |= ECS_TYPE_HOOK_MOVE_CTOR_ILLEGAL;
                if (h->dtor) {
                    illegal_check |= ECS_TYPE_HOOK_DTOR_ILLEGAL;
                    ti->hooks.ctor_move_dtor = flecs_default_move_ctor_w_dtor;
                } else {
                    ti->hooks.ctor_move_dtor = ti->hooks.move_ctor;
                }
            }
        }
        if(flags & illegal_check) {
            flags |= ECS_TYPE_HOOK_CTOR_MOVE_DTOR_ILLEGAL;
        }
    }

    if (!h->move_dtor) {
        ecs_flags32_t illegal_check = 0;
        if (h->move) {
            illegal_check |= ECS_TYPE_HOOK_MOVE_ILLEGAL;
            if (h->dtor) {
                illegal_check |= ECS_TYPE_HOOK_DTOR_ILLEGAL;
                ti->hooks.move_dtor = flecs_default_move_w_dtor;
            } else {
                ti->hooks.move_dtor = flecs_default_move;
            }
        } else {
            if (h->dtor) {
                illegal_check |= ECS_TYPE_HOOK_DTOR_ILLEGAL;
                ti->hooks.move_dtor = flecs_default_dtor;
            }
        }
        if(flags & illegal_check) {
            flags |= ECS_TYPE_HOOK_MOVE_DTOR_ILLEGAL;
        }
    }

    if(!h->cmp) {
        flags |= ECS_TYPE_HOOK_CMP_ILLEGAL;
    }

    if (!h->equals || h->equals == flecs_equals_illegal) {
        if(flags & ECS_TYPE_HOOK_CMP_ILLEGAL) {
            flags |= ECS_TYPE_HOOK_EQUALS_ILLEGAL;
        } else {
            ti->hooks.equals = flecs_default_equals;
            flags &= ~ECS_TYPE_HOOK_EQUALS_ILLEGAL;
        }
    }

    ti->hooks.flags = flags;

    if (ti->hooks.ctor) ti->hooks.flags |= ECS_TYPE_HOOK_CTOR;
    if (ti->hooks.dtor) ti->hooks.flags |= ECS_TYPE_HOOK_DTOR;
    if (ti->hooks.move) ti->hooks.flags |= ECS_TYPE_HOOK_MOVE;
    if (ti->hooks.move_ctor) ti->hooks.flags |= ECS_TYPE_HOOK_MOVE_CTOR;
    if (ti->hooks.ctor_move_dtor) ti->hooks.flags |= ECS_TYPE_HOOK_CTOR_MOVE_DTOR;
    if (ti->hooks.move_dtor) ti->hooks.flags |= ECS_TYPE_HOOK_MOVE_DTOR;
    if (ti->hooks.copy) ti->hooks.flags |= ECS_TYPE_HOOK_COPY;
    if (ti->hooks.copy_ctor) ti->hooks.flags |= ECS_TYPE_HOOK_COPY_CTOR;
    if (ti->hooks.cmp) ti->hooks.flags |= ECS_TYPE_HOOK_CMP;
    if (ti->hooks.equals) ti->hooks.flags |= ECS_TYPE_HOOK_EQUALS;

    if(flags & ECS_TYPE_HOOK_CTOR_ILLEGAL) ti->hooks.ctor = flecs_ctor_illegal;
    if(flags & ECS_TYPE_HOOK_DTOR_ILLEGAL) ti->hooks.dtor = flecs_dtor_illegal;
    if(flags & ECS_TYPE_HOOK_COPY_ILLEGAL) ti->hooks.copy = flecs_copy_illegal;
    if(flags & ECS_TYPE_HOOK_MOVE_ILLEGAL) ti->hooks.move = flecs_move_illegal;
    if(flags & ECS_TYPE_HOOK_CMP_ILLEGAL) ti->hooks.cmp = flecs_comp_illegal;
    if(flags & ECS_TYPE_HOOK_EQUALS_ILLEGAL) ti->hooks.equals = flecs_equals_illegal;

    if(flags & ECS_TYPE_HOOK_COPY_CTOR_ILLEGAL) {
        ti->hooks.copy_ctor = flecs_copy_ctor_illegal;
    }

    if(ti->hooks.flags & ECS_TYPE_HOOK_MOVE_CTOR_ILLEGAL) {
        ti->hooks.move_ctor = flecs_move_ctor_illegal;
    }

    if(ti->hooks.flags & ECS_TYPE_HOOK_CTOR_MOVE_DTOR_ILLEGAL) {
        ti->hooks.ctor_move_dtor = flecs_move_ctor_illegal;
    }

    if(ti->hooks.flags & ECS_TYPE_HOOK_MOVE_DTOR_ILLEGAL) {
        ti->hooks.move_dtor = flecs_move_ctor_illegal;
    }


error:
    return;
}

const ecs_type_hooks_t* ecs_get_hooks_id(
    const ecs_world_t *world,
    ecs_entity_t id)
{
    const ecs_type_info_t *ti = ecs_get_type_info(world, id);
    if (ti) {
        return &ti->hooks;
    }
    return NULL;
}

void ecs_atfini(
    ecs_world_t *world,
    ecs_fini_action_t action,
    void *ctx)
{
    flecs_poly_assert(world, ecs_world_t);
    ecs_check(action != NULL, ECS_INVALID_PARAMETER, NULL);

    ecs_action_elem_t *elem = ecs_vec_append_t(NULL, &world->fini_actions,
        ecs_action_elem_t);
    ecs_assert(elem != NULL, ECS_INTERNAL_ERROR, NULL);

    elem->action = action;
    elem->ctx = ctx;
error:
    return;
}

void ecs_run_post_frame(
    ecs_world_t *world,
    ecs_fini_action_t action,
    void *ctx)
{
    ecs_check(world != NULL, ECS_INVALID_PARAMETER, NULL);
    ecs_check(action != NULL, ECS_INVALID_PARAMETER, NULL);

    ecs_stage_t *stage = flecs_stage_from_world(&world);
    ecs_check((world->flags & EcsWorldFrameInProgress), ECS_INVALID_OPERATION, 
        "cannot register post frame action while frame is not in progress");

    ecs_action_elem_t *elem = ecs_vec_append_t(&stage->allocator,
        &stage->post_frame_actions, ecs_action_elem_t);
    ecs_assert(elem != NULL, ECS_INTERNAL_ERROR, NULL);

    elem->action = action;
    elem->ctx = ctx;
error:
    return;
}

/* Unset data in tables */
static
void flecs_fini_unset_tables(
    ecs_world_t *world)
{
    ecs_sparse_t *tables = &world->store.tables;
    int32_t i, count = flecs_sparse_count(tables);

    for (i = 0; i < count; i ++) {
        ecs_table_t *table = flecs_sparse_get_dense_t(tables, ecs_table_t, i);
        flecs_table_remove_actions(world, table);
    }
}

/* Invoke fini actions */
static
void flecs_fini_actions(
    ecs_world_t *world)
{
    int32_t i, count = ecs_vec_count(&world->fini_actions);
    ecs_action_elem_t *elems = ecs_vec_first(&world->fini_actions);
    for (i = 0; i < count; i ++) {
        elems[i].action(world, elems[i].ctx);
    }

    ecs_vec_fini_t(NULL, &world->fini_actions, ecs_action_elem_t);
}

/* Cleanup remaining type info elements */
static
void flecs_fini_type_info(
    ecs_world_t *world)
{
    ecs_map_iter_t it = ecs_map_iter(&world->type_info);
    while (ecs_map_next(&it)) {
        ecs_type_info_t *ti = ecs_map_ptr(&it);
        flecs_type_info_fini(ti);
        ecs_os_free(ti);
    }
    ecs_map_fini(&world->type_info);
}

ecs_entity_t flecs_get_oneof(
    const ecs_world_t *world,
    ecs_entity_t e)
{
    if (ecs_is_alive(world, e)) {
        if (ecs_has_id(world, e, EcsOneOf)) {
            return e;
        } else {
            return ecs_get_target(world, e, EcsOneOf, 0);
        }
    } else {
        return 0;
    }
}

/* The destroyer of worlds */
int ecs_fini(
    ecs_world_t *world)
{
    flecs_poly_assert(world, ecs_world_t);
    ecs_assert(!(world->flags & EcsWorldReadonly), ECS_INVALID_OPERATION,
        "cannot fini world while it is in readonly mode");
    ecs_assert(!(world->flags & EcsWorldFini), ECS_INVALID_OPERATION,
        "cannot fini world when it is already being deleted");
    ecs_assert(world->stages[0]->defer == 0, ECS_INVALID_OPERATION, 
        "call defer_end before destroying world");

    ecs_trace("#[bold]shutting down world");
    ecs_log_push();

    world->flags |= EcsWorldQuit;

    /* Delete root entities first using regular APIs. This ensures that cleanup
     * policies get a chance to execute. */
    ecs_dbg_1("#[bold]cleanup root entities");
    ecs_log_push_1();
    flecs_fini_roots(world);
    ecs_log_pop_1();

    world->flags |= EcsWorldFini;

    /* Run fini actions (simple callbacks ran when world is deleted) before
     * destroying the storage */
    ecs_dbg_1("#[bold]run fini actions");
    ecs_log_push_1();
    flecs_fini_actions(world);
    ecs_log_pop_1();

    ecs_dbg_1("#[bold]cleanup remaining entities");
    ecs_log_push_1();

    /* Operations invoked during OnRemove/destructors are deferred and
     * will be discarded after world cleanup */
    flecs_defer_begin(world, world->stages[0]);

    /* Run OnRemove actions for components while the store is still
     * unmodified by cleanup. */
    flecs_fini_unset_tables(world);

    /* This will destroy all entities and components. */
    flecs_fini_store(world);

    /* Purge deferred operations from the queue. This discards operations but
     * makes sure that any resources in the queue are freed */
    flecs_defer_purge(world, world->stages[0]);
    ecs_log_pop_1();

    /* All queries are cleaned up, so monitors should've been cleaned up too */
    ecs_assert(!ecs_map_is_init(&world->monitors.monitors),
        ECS_INTERNAL_ERROR, NULL);

    /* Cleanup world ctx and binding_ctx */
    if (world->ctx_free) {
        world->ctx_free(world->ctx);
    }
    if (world->binding_ctx_free) {
        world->binding_ctx_free(world->binding_ctx);
    }

    /* After this point no more user code is invoked */

    ecs_dbg_1("#[bold]cleanup world data structures");
    ecs_log_push_1();
    flecs_entities_fini(world);
    flecs_components_fini(world);
    flecs_fini_type_info(world);
    flecs_observable_fini(&world->observable);
    flecs_name_index_fini(&world->aliases);
    flecs_name_index_fini(&world->symbols);
    ecs_set_stage_count(world, 0);
    ecs_vec_fini_t(&world->allocator, &world->component_ids, ecs_id_t);
    ecs_log_pop_1();

    flecs_world_allocators_fini(world);

    /* End of the world */
    flecs_poly_free(world, ecs_world_t);
    ecs_os_fini();

    ecs_trace("world destroyed, bye!");
    ecs_log_pop();

    return 0;
}

bool ecs_is_fini(
    const ecs_world_t *world)
{
    ecs_assert(world != NULL, ECS_INVALID_PARAMETER, NULL);
    world = ecs_get_world(world);
    return ECS_BIT_IS_SET(world->flags, EcsWorldFini);
}

void ecs_dim(
    ecs_world_t *world,
    int32_t entity_count)
{
    flecs_poly_assert(world, ecs_world_t);
    flecs_entities_set_size(world, entity_count + FLECS_HI_COMPONENT_ID);
}

void flecs_eval_component_monitors(
    ecs_world_t *world)
{
    flecs_poly_assert(world, ecs_world_t); 
    flecs_eval_component_monitor(world);
}

void ecs_measure_frame_time(
    ecs_world_t *world,
    bool enable)
{
    flecs_poly_assert(world, ecs_world_t);
    ecs_check(ecs_os_has_time(), ECS_MISSING_OS_API, NULL);

    if (ECS_EQZERO(world->info.target_fps) || enable) {
        ECS_BIT_COND(world->flags, EcsWorldMeasureFrameTime, enable);
    }
error:
    return;
}

void ecs_measure_system_time(
    ecs_world_t *world,
    bool enable)
{
    flecs_poly_assert(world, ecs_world_t);
    ecs_check(ecs_os_has_time(), ECS_MISSING_OS_API, NULL);
    ECS_BIT_COND(world->flags, EcsWorldMeasureSystemTime, enable);
error:
    return;
}

void ecs_set_target_fps(
    ecs_world_t *world,
    ecs_ftime_t fps)
{
    flecs_poly_assert(world, ecs_world_t);
    ecs_check(ecs_os_has_time(), ECS_MISSING_OS_API, NULL);

    ecs_measure_frame_time(world, true);
    world->info.target_fps = fps;
error:
    return;
}

void ecs_set_default_query_flags(
    ecs_world_t *world,
    ecs_flags32_t flags)
{
    flecs_poly_assert(world, ecs_world_t);
    world->default_query_flags = flags;
}

void* ecs_get_ctx(
    const ecs_world_t *world)
{
    ecs_check(world != NULL, ECS_INVALID_PARAMETER, NULL);
    world = ecs_get_world(world);
    return world->ctx;
error:
    return NULL;
}

void* ecs_get_binding_ctx(
    const ecs_world_t *world)
{
    ecs_check(world != NULL, ECS_INVALID_PARAMETER, NULL);
    world = ecs_get_world(world);
    return world->binding_ctx;
error:
    return NULL;
}

void ecs_set_ctx(
    ecs_world_t *world,
    void *ctx,
    ecs_ctx_free_t ctx_free)
{
    flecs_poly_assert(world, ecs_world_t);
    world->ctx = ctx;
    world->ctx_free = ctx_free;
}

void ecs_set_binding_ctx(
    ecs_world_t *world,
    void *ctx,
    ecs_ctx_free_t ctx_free)
{
    flecs_poly_assert(world, ecs_world_t);
    world->binding_ctx = ctx;
    world->binding_ctx_free = ctx_free;
}

void ecs_set_entity_range(
    ecs_world_t *world,
    ecs_entity_t id_start,
    ecs_entity_t id_end)
{
    flecs_poly_assert(world, ecs_world_t);
    ecs_check(!id_end || id_end > id_start, ECS_INVALID_PARAMETER, NULL);

    if (id_start == 0) {
      id_start = flecs_entities_max_id(world) + 1;
    }

    uint32_t start = (uint32_t)id_start;
    uint32_t end = (uint32_t)id_end;

    flecs_entities_max_id(world) = start - 1;

    world->info.min_id = start;
    world->info.max_id = end;
error:
    return;
}

bool ecs_enable_range_check(
    ecs_world_t *world,
    bool enable)
{
    flecs_poly_assert(world, ecs_world_t);    
    bool old_value = world->range_check_enabled;
    world->range_check_enabled = enable;
    return old_value;
}

ecs_entity_t ecs_get_max_id(
    const ecs_world_t *world)
{
    ecs_check(world != NULL, ECS_INVALID_PARAMETER, NULL);
    world = ecs_get_world(world);
    return flecs_entities_max_id(world);
error:
    return 0;
}

void flecs_increment_table_version(
    ecs_world_t *world,
    ecs_table_t *table)
{
    flecs_poly_assert(world, ecs_world_t);
    ecs_assert(table != NULL, ECS_INVALID_PARAMETER, NULL);

    world->table_version[table->id & ECS_TABLE_VERSION_ARRAY_BITMASK] ++;
    table->version ++;

    return;
}

uint32_t flecs_get_table_version_fast(
    const ecs_world_t *world,
    const uint64_t table_id)
{
    flecs_poly_assert(world, ecs_world_t);
    return world->table_version[table_id & ECS_TABLE_VERSION_ARRAY_BITMASK];
}

const ecs_type_info_t* flecs_type_info_get(
    const ecs_world_t *world,
    ecs_entity_t component)
{
    flecs_poly_assert(world, ecs_world_t);   

    ecs_assert(component != 0, ECS_INTERNAL_ERROR, NULL);
    ecs_assert(!(component & ECS_ID_FLAGS_MASK), ECS_INTERNAL_ERROR, NULL);

    return ecs_map_get_deref(&world->type_info, ecs_type_info_t, component);
}

ecs_type_info_t* flecs_type_info_ensure(
    ecs_world_t *world,
    ecs_entity_t component)
{
    flecs_poly_assert(world, ecs_world_t);
    ecs_assert(component != 0, ECS_INTERNAL_ERROR, NULL);

    const ecs_type_info_t *ti = flecs_type_info_get(world, component);
    ecs_type_info_t *ti_mut = NULL;
    if (!ti) {
        ti_mut = ecs_map_ensure_alloc_t(
            &world->type_info, ecs_type_info_t, component);
        ecs_assert(ti_mut != NULL, ECS_INTERNAL_ERROR, NULL);
        ti_mut->component = component;
    } else {
        ti_mut = ECS_CONST_CAST(ecs_type_info_t*, ti);
    }

    if (!ti_mut->name) {
        const char *sym = ecs_get_symbol(world, component);
        if (sym) {
            ti_mut->name = ecs_os_strdup(sym);
        } else {
            const char *name = ecs_get_name(world, component);
            if (name) {
                ti_mut->name = ecs_os_strdup(name);
            }
        }
    }

    return ti_mut;
}

bool flecs_type_info_init_id(
    ecs_world_t *world,
    ecs_entity_t component,
    ecs_size_t size,
    ecs_size_t alignment,
    const ecs_type_hooks_t *li)
{
    bool changed = false;

    flecs_entities_ensure(world, component);

    ecs_type_info_t *ti = NULL;
    if (!size || !alignment) {
        ecs_assert(size == 0 && alignment == 0,
            ECS_INVALID_COMPONENT_SIZE, NULL);
        ecs_assert(li == NULL, ECS_INCONSISTENT_COMPONENT_ACTION, NULL);
        ecs_map_remove_free(&world->type_info, component);
    } else {
        ti = flecs_type_info_ensure(world, component);
        ecs_assert(ti != NULL, ECS_INTERNAL_ERROR, NULL);
        changed |= ti->size != size;
        changed |= ti->alignment != alignment;
        ti->size = size;
        ti->alignment = alignment;
        if (li) {
            ecs_set_hooks_id(world, component, li);
        }
    }

    /* Set type info for component record of component */
    ecs_component_record_t *cr = flecs_components_ensure(world, component);
    changed |= flecs_component_set_type_info(world, cr, ti);
    bool is_tag = cr->flags & EcsIdTag;

    /* All id records with component as relationship inherit type info */
    cr = flecs_components_ensure(world, ecs_pair(component, EcsWildcard));
    do {
        if (is_tag) {
            changed |= flecs_component_set_type_info(world, cr, NULL);
        } else if (ti) {
            changed |= flecs_component_set_type_info(world, cr, ti);
        } else if ((cr->type_info != NULL) &&
            (cr->type_info->component == component))
        {
            changed |= flecs_component_set_type_info(world, cr, NULL);
        }
    } while ((cr = flecs_component_first_next(cr)));

    /* All non-tag id records with component as object inherit type info,
     * if relationship doesn't have type info */
    cr = flecs_components_ensure(world, ecs_pair(EcsWildcard, component));
    do {
        if (!(cr->flags & EcsIdTag) && !cr->type_info) {
            changed |= flecs_component_set_type_info(world, cr, ti);
        }
    } while ((cr = flecs_component_first_next(cr)));

    /* Type info of (*, component) should always point to component */
    ecs_assert(flecs_components_get(world, ecs_pair(EcsWildcard, component))->
        type_info == ti, ECS_INTERNAL_ERROR, NULL);

    return changed;
}

void flecs_type_info_fini(
    ecs_type_info_t *ti)
{
    if (ti->hooks.ctx_free) {
        ti->hooks.ctx_free(ti->hooks.ctx);
    }
    if (ti->hooks.binding_ctx_free) {
        ti->hooks.binding_ctx_free(ti->hooks.binding_ctx);
    }
    if (ti->hooks.lifecycle_ctx_free) {
        ti->hooks.lifecycle_ctx_free(ti->hooks.lifecycle_ctx);
    }
    if (ti->name) {
        /* Safe to cast away const, world has ownership over string */
        ecs_os_free(ECS_CONST_CAST(char*, ti->name));
        ti->name = NULL;
    }

    ti->size = 0;
    ti->alignment = 0;
}

void flecs_type_info_free(
    ecs_world_t *world,
    ecs_entity_t component)
{
    flecs_poly_assert(world, ecs_world_t);

    if (world->flags & EcsWorldQuit) {
        /* If world is in the final teardown stages, cleanup policies are no
         * longer applied and it can't be guaranteed that a component is not
         * deleted before entities that use it. The remaining type info elements
         * will be deleted after the store is finalized. */
        return;
    }

    ecs_type_info_t *ti = ecs_map_get_deref(
        &world->type_info, ecs_type_info_t, component);
    if (ti) {
        flecs_type_info_fini(ti);
        ecs_map_remove_free(&world->type_info, component);
    }
}

static
ecs_ftime_t flecs_insert_sleep(
    ecs_world_t *world,
    ecs_time_t *stop)
{
    flecs_poly_assert(world, ecs_world_t);  

    ecs_time_t start = *stop, now = start;
    ecs_ftime_t delta_time = (ecs_ftime_t)ecs_time_measure(stop);

    if (ECS_EQZERO(world->info.target_fps)) {
        return delta_time;
    }

    ecs_os_perf_trace_push("flecs.insert_sleep");

    ecs_ftime_t target_delta_time =
        ((ecs_ftime_t)1.0 / (ecs_ftime_t)world->info.target_fps);

    /* Calculate the time we need to sleep by taking the measured delta from the
     * previous frame, and subtracting it from target_delta_time. */
    ecs_ftime_t sleep = target_delta_time - delta_time;

    /* Pick a sleep interval that is 4 times smaller than the time one frame
     * should take. */
    ecs_ftime_t sleep_time = sleep / (ecs_ftime_t)4.0;

    do {
        /* Only call sleep when sleep_time is not 0. On some platforms, even
         * a sleep with a timeout of 0 can cause stutter. */
        if (ECS_NEQZERO(sleep_time)) {
            ecs_sleepf((double)sleep_time);
        }

        now = start;
        delta_time = (ecs_ftime_t)ecs_time_measure(&now);
    } while ((target_delta_time - delta_time) >
        (sleep_time / (ecs_ftime_t)2.0));

    ecs_os_perf_trace_pop("flecs.insert_sleep");

    *stop = now;
    return delta_time;
}

static
ecs_ftime_t flecs_start_measure_frame(
    ecs_world_t *world,
    ecs_ftime_t user_delta_time)
{
    flecs_poly_assert(world, ecs_world_t);  

    ecs_ftime_t delta_time = 0;

    if ((world->flags & EcsWorldMeasureFrameTime) ||
        (ECS_EQZERO(user_delta_time)))
    {
        ecs_time_t t = world->frame_start_time;
        do {
            if (world->frame_start_time.nanosec || world->frame_start_time.sec){
                delta_time = flecs_insert_sleep(world, &t);
            } else {
                ecs_time_measure(&t);
                if (ECS_NEQZERO(world->info.target_fps)) {
                    delta_time = (ecs_ftime_t)1.0 / world->info.target_fps;
                } else {
                    /* Best guess */
                    delta_time = (ecs_ftime_t)1.0 / (ecs_ftime_t)60.0;

                    if (ECS_EQZERO(delta_time)) {
                        delta_time = user_delta_time;
                        break;
                    }
                }
            }

        /* Keep trying while delta_time is zero */
        } while (ECS_EQZERO(delta_time));

        world->frame_start_time = t;

        /* Keep track of total time passed in world */
        world->info.world_time_total_raw += (double)delta_time;
    }

    return (ecs_ftime_t)delta_time;
}

static
void flecs_stop_measure_frame(
    ecs_world_t* world)
{
    flecs_poly_assert(world, ecs_world_t);  

    if (world->flags & EcsWorldMeasureFrameTime) {
        ecs_time_t t = world->frame_start_time;
        world->info.frame_time_total += (ecs_ftime_t)ecs_time_measure(&t);
    }
}

ecs_ftime_t ecs_frame_begin(
    ecs_world_t *world,
    ecs_ftime_t user_delta_time)
{
    flecs_poly_assert(world, ecs_world_t);
    ecs_check(!(world->flags & EcsWorldReadonly), ECS_INVALID_OPERATION, 
        "cannot begin frame while world is in readonly mode");
    ecs_check(!(world->flags & EcsWorldFrameInProgress), ECS_INVALID_OPERATION, 
        "cannot begin frame while frame is already in progress");
    ecs_check(ECS_NEQZERO(user_delta_time) || ecs_os_has_time(), 
        ECS_MISSING_OS_API, "get_time");

    /* Start measuring total frame time */
    ecs_ftime_t delta_time = flecs_start_measure_frame(world, user_delta_time);
    if (ECS_EQZERO(user_delta_time)) {
        user_delta_time = delta_time;
    }

    world->info.delta_time_raw = user_delta_time;
    world->info.delta_time = user_delta_time * world->info.time_scale;

    /* Keep track of total scaled time passed in world */
    world->info.world_time_total += (double)world->info.delta_time;

    /* Command buffer capturing */
    world->on_commands_active = world->on_commands;
    world->on_commands = NULL;

    world->on_commands_ctx_active = world->on_commands_ctx;
    world->on_commands_ctx = NULL;

    ecs_run_aperiodic(world, 0);

    world->flags |= EcsWorldFrameInProgress;

    return world->info.delta_time;
error:
    return (ecs_ftime_t)0;
}

void ecs_frame_end(
    ecs_world_t *world)
{
    flecs_poly_assert(world, ecs_world_t);
    ecs_check(!(world->flags & EcsWorldReadonly), ECS_INVALID_OPERATION, 
        "cannot end frame while world is in readonly mode");
    ecs_check((world->flags & EcsWorldFrameInProgress), ECS_INVALID_OPERATION, 
        "cannot end frame while frame is not in progress");

    world->info.frame_count_total ++;
    
    int32_t i, count = world->stage_count;
    for (i = 0; i < count; i ++) {
        flecs_stage_merge_post_frame(world, world->stages[i]);
    }

    flecs_stop_measure_frame(world);

    /* Reset command handler each frame */
    world->on_commands_active = NULL;
    world->on_commands_ctx_active = NULL;

    world->flags &= ~EcsWorldFrameInProgress;
    
error:
    return;
}

void flecs_delete_table(
    ecs_world_t *world,
    ecs_table_t *table)
{
    flecs_poly_assert(world, ecs_world_t); 
    flecs_table_fini(world, table);
}

static
void flecs_process_empty_queries(
    ecs_world_t *world)
{
    flecs_poly_assert(world, ecs_world_t); 

    ecs_component_record_t *cr = flecs_components_get(world,
        ecs_pair(ecs_id(EcsPoly), EcsQuery));
    if (!cr) {
        return;
    }

    /* Make sure that we defer adding the inactive tags until after iterating
     * the query */
    flecs_defer_begin(world, world->stages[0]);

    ecs_table_cache_iter_t it;
    const ecs_table_record_t *tr;
    if (flecs_table_cache_iter(&cr->cache, &it)) {
        while ((tr = flecs_table_cache_next(&it, ecs_table_record_t))) {
            ecs_table_t *table = tr->hdr.table;
            EcsPoly *queries = ecs_table_get_column(table, tr->column, 0);
            int32_t i, count = ecs_table_count(table);

            for (i = 0; i < count; i ++) {
                ecs_query_t *query = queries[i].poly;
                const ecs_entity_t *entities = ecs_table_entities(table);
                if (!ecs_query_is_true(query)) {
                    ecs_add_id(world, entities[i], EcsEmpty);
                }
            }
        }
    }

    flecs_defer_end(world, world->stages[0]);
}

bool ecs_id_in_use(
    const ecs_world_t *world,
    ecs_id_t id)
{
    ecs_component_record_t *cr = flecs_components_get(world, id);
    if (!cr) {
        return false;
    }

    return (flecs_table_cache_count(&cr->cache) != 0);
}

void ecs_run_aperiodic(
    ecs_world_t *world,
    ecs_flags32_t flags)
{
    flecs_poly_assert(world, ecs_world_t);

    if ((flags & EcsAperiodicEmptyQueries)) {
        flecs_process_empty_queries(world);
    }

    if (!flags || (flags & EcsAperiodicComponentMonitors)) {
        flecs_eval_component_monitors(world);
    }
}

int32_t ecs_delete_empty_tables(
    ecs_world_t *world,
    const ecs_delete_empty_tables_desc_t *desc)
{
    flecs_poly_assert(world, ecs_world_t);

    ecs_os_perf_trace_push("flecs.delete_empty_tables");

    ecs_time_t start = {0}, cur = {0};
    int32_t delete_count = 0;
    bool time_budget = false;
    int32_t measure_budget_after = 100;

    uint16_t clear_generation = desc->clear_generation;
    uint16_t delete_generation = desc->delete_generation;
    double time_budget_seconds = desc->time_budget_seconds;

    if (ECS_NEQZERO(time_budget_seconds) || (ecs_should_log_1() && ecs_os_has_time())) {
        ecs_time_measure(&start);
    }

    if (ECS_NEQZERO(time_budget_seconds)) {
        time_budget = true;
    }

    int32_t i, count = flecs_sparse_count(&world->store.tables);

    for (i = count - 1; i >= 0; i --) {
        ecs_table_t *table = flecs_sparse_get_dense_t(&world->store.tables,
            ecs_table_t, i);

        measure_budget_after --;

        if (time_budget && !measure_budget_after) {
            cur = start;
            if (ecs_time_measure(&cur) > time_budget_seconds) {
                goto done;
            }

            measure_budget_after = 100;
        }

        if (!table->id || ecs_table_count(table) != 0) {
            continue;
        }

        uint16_t gen = ++ table->_->generation;
        if (delete_generation && (gen > delete_generation)) {
            flecs_table_fini(world, table);
            delete_count ++;
            measure_budget_after = 1;
        } else if (clear_generation && (gen > clear_generation)) {
            flecs_table_shrink(world, table);
            measure_budget_after = 1;
        }
    }

done:
    ecs_os_perf_trace_pop("flecs.delete_empty_tables");

    return delete_count;
}

ecs_entities_t ecs_get_entities(
    const ecs_world_t *world)
{
    ecs_entities_t result;
    result.ids = flecs_entities_ids(world);
    result.count = flecs_entities_size(world);
    result.alive_count = flecs_entities_count(world);
    return result;
}

ecs_flags32_t ecs_world_get_flags(
    const ecs_world_t *world)
{
    if (flecs_poly_is(world, ecs_world_t)) {
        return world->flags;
    } else {
        flecs_poly_assert(world, ecs_stage_t);
        const ecs_stage_t *stage = (const ecs_stage_t*)world;
        return stage->world->flags;
    }
}

static int32_t flecs_component_ids_last_index = 0;

int32_t flecs_component_ids_index_get(void) {
    if (ecs_os_api.ainc_) {
        return ecs_os_ainc(&flecs_component_ids_last_index);
    } else {
        return ++ flecs_component_ids_last_index;
    }
}

ecs_entity_t flecs_component_ids_get(
    const ecs_world_t *stage_world, 
    int32_t index)
{
    ecs_world_t *world =
        ECS_CONST_CAST(ecs_world_t*, ecs_get_world(stage_world));
    flecs_poly_assert(world, ecs_world_t);

    if (index >= ecs_vec_count(&world->component_ids)) {
        return 0;
    }

    return ecs_vec_get_t(
        &world->component_ids, ecs_entity_t, index)[0];
}

ecs_entity_t flecs_component_ids_get_alive(
    const ecs_world_t *stage_world, 
    int32_t index)
{
    ecs_world_t *world =
        ECS_CONST_CAST(ecs_world_t*, ecs_get_world(stage_world));
    flecs_poly_assert(world, ecs_world_t);

    if (index >= ecs_vec_count(&world->component_ids)) {
        return 0;
    }

    ecs_entity_t result = ecs_vec_get_t(
        &world->component_ids, ecs_entity_t, index)[0];
    if (!flecs_entities_is_alive(world, result)) {
        return 0;
    }

    return result;
}

void flecs_component_ids_set(
    ecs_world_t *stage_world, 
    int32_t index,
    ecs_entity_t component)
{
    ecs_world_t *world =
        ECS_CONST_CAST(ecs_world_t*, ecs_get_world(stage_world));
    flecs_poly_assert(world, ecs_world_t);

    ecs_vec_set_min_count_zeromem_t(
        &world->allocator, &world->component_ids, ecs_entity_t, index + 1);
    ecs_vec_get_t(&world->component_ids, ecs_entity_t, index)[0] = component;
}

void ecs_shrink(
    ecs_world_t *world)
{
    flecs_entity_index_shrink(&world->store.entity_index);

    ecs_sparse_t *tables = &world->store.tables;
    int32_t i, count = flecs_sparse_count(tables);

    for (i = count - 1; i > 0; i --) {
        ecs_table_t *table = flecs_sparse_get_dense_t(tables, ecs_table_t, i);
        if (ecs_table_count(table)) {
            flecs_table_shrink(world, table);
        } else {
            flecs_table_fini(world, table);
        }
    }

    flecs_table_shrink(world, &world->store.root);

    flecs_sparse_shrink(&world->store.tables);

    for (i = 0; i < world->stage_count; i ++) {
        ecs_stage_shrink(world->stages[i]);
    }
}
