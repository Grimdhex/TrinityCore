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

#include "ScriptMgr.h"
#include "InstanceScript.h"
#include "molten_core.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "ScriptedCreature.h"

enum Texts
{
    EMOTE_LOWHP             = 0,
};

enum Spells
{
    // Golemagg
    SPELL_MAGMASPLASH       = 13879,
    SPELL_DOUBLE_ATTACK     = 18943,
    SPELL_EARTHQUAKE        = 19798,
    SPELL_PYROBLAST         = 20228,
    SPELL_GOLEMAGG_TRUST    = 20553,

    // Core Rager
    SPELL_MANGLE            = 19820
};

enum Events
{
    EVENT_PYROBLAST     = 1,
    EVENT_EARTHQUAKE    = 2
};

struct boss_golemagg : public BossAI
{
    boss_golemagg(Creature* creature) : BossAI(creature, BOSS_GOLEMAGG_THE_INCINERATOR), _isEnraged(false) { }

    void Reset() override
    {
        BossAI::Reset();

        _isEnraged = false;
        DoCastSelf(SPELL_MAGMASPLASH);
        DoCastSelf(SPELL_GOLEMAGG_TRUST);
        DoCastSelf(SPELL_DOUBLE_ATTACK);
    }

    void JustEngagedWith(Unit* victim) override
    {
        BossAI::JustEngagedWith(victim);

        events.ScheduleEvent(EVENT_PYROBLAST, 7s);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (!_isEnraged && me->HealthBelowPctDamaged(10, damage))
        {
            DoCastAOE(SPELL_EARTHQUAKE, true);
            events.ScheduleEvent(EVENT_EARTHQUAKE, 5s);
            _isEnraged = true;
        }
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_PYROBLAST:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                    DoCast(target, SPELL_PYROBLAST);
                events.Repeat(7s);
                break;
            case EVENT_EARTHQUAKE:
                DoCastSelf(SPELL_EARTHQUAKE);
                events.Repeat(5s);
                break;
            default:
                break;
        }
    }

private:
    bool _isEnraged;
};

struct npc_core_rager : public ScriptedAI
{
    npc_core_rager(Creature* creature) : ScriptedAI(creature)
    {
        _instance = creature->GetInstanceScript();
    }

    void Reset() override
    {
        _scheduler.CancelAll();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _scheduler.Schedule(7s, [this](TaskContext task)
        {
            DoCastVictim(SPELL_MANGLE);
            task.Repeat(10s);
        });
    }

    void JustDied(Unit* /*killer*/) override
    {
        _scheduler.CancelAll();
    }

    void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (HealthAbovePct(50) || !_instance)
            return;

        if (Creature* golemagg = ObjectAccessor::GetCreature(*me, _instance->GetGuidData(BOSS_GOLEMAGG_THE_INCINERATOR)))
        {
            if (golemagg->IsAlive())
            {
                Talk(EMOTE_LOWHP);
                me->SetFullHealth();
            }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _scheduler.Update(diff, [this]
        {
            DoMeleeAttackIfReady();
        });

        if (Creature* golemagg = ObjectAccessor::GetCreature(*me, _instance->GetGuidData(BOSS_GOLEMAGG_THE_INCINERATOR)))
        {
            if (golemagg->IsAlive())
            {
                if (me->GetDistance(golemagg) > 100.f)
                    ScriptedAI::EnterEvadeMode();
            }
            else
                me->KillSelf();
        }
    }

private:
    InstanceScript* _instance;
    TaskScheduler _scheduler;
};

void AddSC_boss_golemagg()
{
    RegisterMoltenCoreCreatureAI(boss_golemagg);
    RegisterMoltenCoreCreatureAI(npc_core_rager);
}
