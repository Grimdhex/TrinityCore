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
#include "Containers.h"
#include "molten_core.h"
#include "ObjectMgr.h"
#include "ScriptedCreature.h"

enum Spells
{
    // Sulfuron Harbringer
    SPELL_DEMORALIZING_SHOUT    = 19778,
    SPELL_INSPIRE               = 19779,
    SPELL_KNOCKDOWN             = 19780,
    SPELL_FLAMESPEAR            = 19781,

    // Flmaewaker Priest
    SPELL_DARK_MENDING          = 19775,
    SPELL_SHADOWWORDPAIN        = 19776,
    SPELL_DARK_STRIKE           = 19777,
    SPELL_IMMOLATE              = 20294,
};

enum Events
{
    // Sulfuron Harbringer
    EVENT_DEMORALIZING_SHOUT    = 2,
    EVENT_INSPIRE               = 3,
    EVENT_KNOCKDOWN             = 4,
    EVENT_FLAMESPEAR            = 5,

    // Flmaewaker Priest
    EVENT_DARK_STRIKE           = 1,
    EVENT_DARK_MENDING          = 6,
    EVENT_SHADOW_WORD_PAIN      = 7,
    EVENT_IMMOLATE              = 8,
};

struct boss_sulfuron : public BossAI
{
    boss_sulfuron(Creature* creature) : BossAI(creature, BOSS_SULFURON_HARBINGER) { }

    void JustEngagedWith(Unit* victim) override
    {
        BossAI::JustEngagedWith(victim);

        events.ScheduleEvent(EVENT_DEMORALIZING_SHOUT, 15s);
        events.ScheduleEvent(EVENT_INSPIRE, 10s);
        events.ScheduleEvent(EVENT_KNOCKDOWN, 6s);
        events.ScheduleEvent(EVENT_FLAMESPEAR, 2s);
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_DEMORALIZING_SHOUT:
                DoCastVictim(SPELL_DEMORALIZING_SHOUT);
                events.Repeat(15s, 20s);
                break;
            case EVENT_INSPIRE:
            {
                std::list<Creature*> healers = DoFindFriendlyMissingBuff(45.0f, SPELL_INSPIRE);
                if (!healers.empty())
                    DoCast(Trinity::Containers::SelectRandomContainerElement(healers), SPELL_INSPIRE);

                DoCastSelf(SPELL_INSPIRE);
                events.Repeat(15s,22s);
                break;
            }
            case EVENT_KNOCKDOWN:
                DoCastVictim(SPELL_KNOCKDOWN);
                events.Repeat(12s,20s);
                break;
            case EVENT_FLAMESPEAR:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true))
                    DoCast(target, SPELL_FLAMESPEAR);
                events.Repeat(12s,16s);
                break;
            default:
                break;
        }
    }
};

struct npc_flamewaker_priest : public ScriptedAI
{
    npc_flamewaker_priest(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        events.Reset();
    }

    void JustDied(Unit* /*killer*/) override
    {
        events.Reset();
    }

    void JustEngagedWith(Unit* victim) override
    {
        ScriptedAI::JustEngagedWith(victim);

        events.ScheduleEvent(EVENT_DARK_STRIKE, 10s);
        events.ScheduleEvent(EVENT_DARK_MENDING, 15s, 30s);
        events.ScheduleEvent(EVENT_SHADOW_WORD_PAIN, 2s);
        events.ScheduleEvent(EVENT_IMMOLATE, 4s, 8s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_DARK_STRIKE:
                    DoCastSelf(SPELL_DARK_STRIKE);
                    events.Repeat(4s,8s);
                    break;
                case EVENT_DARK_MENDING:
                    if (Unit* target = DoSelectLowestHpFriendly(60.0f, 1))
                        DoCast(target, SPELL_DARK_MENDING);
                    events.Repeat(15s, 20s);
                break;
                case EVENT_SHADOW_WORD_PAIN:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true, true, -SPELL_SHADOWWORDPAIN))
                        DoCast(target, SPELL_SHADOWWORDPAIN);
                    events.Repeat(3s, 6s);
                    break;
                case EVENT_IMMOLATE:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true, true, -SPELL_IMMOLATE))
                        DoCast(target, SPELL_IMMOLATE);
                    events.Repeat(5s, 10s);
                    break;
                default:
                    break;
            }

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }

        DoMeleeAttackIfReady();
    }

private:
    EventMap events;
};

void AddSC_boss_sulfuron()
{
    RegisterMoltenCoreCreatureAI(boss_sulfuron);
    RegisterMoltenCoreCreatureAI(npc_flamewaker_priest);
}
