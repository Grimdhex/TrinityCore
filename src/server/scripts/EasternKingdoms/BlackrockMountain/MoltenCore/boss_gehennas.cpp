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

 /// @todo: Mangos use two shaodw bolt spell: random - 19728 and tank - 19729. Need to research if it's correct.

#include "ScriptMgr.h"
#include "molten_core.h"
#include "ObjectMgr.h"
#include "ScriptedCreature.h"

enum Spells
{
    SPELL_GEHENNAS_CURSE    = 19716,
    SPELL_RAIN_OF_FIRE      = 19717,
    SPELL_SHADOW_BOLT       = 19728,
};

enum Events
{
    EVENT_GEHENNAS_CURSE    = 1,
    EVENT_RAIN_OF_FIRE      = 2,
    EVENT_SHADOW_BOLT       = 3,
};

struct boss_gehennas : public BossAI
{
    boss_gehennas(Creature* creature) : BossAI(creature, BOSS_GEHENNAS) { }

    void JustEngagedWith(Unit* victim) override
    {
        BossAI::JustEngagedWith(victim);

        events.ScheduleEvent(EVENT_GEHENNAS_CURSE, 6s, 10s);
        events.ScheduleEvent(EVENT_RAIN_OF_FIRE, 8s, 10s);
        events.ScheduleEvent(EVENT_SHADOW_BOLT, 3s, 6s);
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_GEHENNAS_CURSE:
                DoCastVictim(SPELL_GEHENNAS_CURSE);
                events.Repeat(30s, 30s);
                break;
            case EVENT_RAIN_OF_FIRE:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                    DoCast(target, SPELL_RAIN_OF_FIRE);
                events.Repeat(4s, 12s);
                break;
            case EVENT_SHADOW_BOLT:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 1))
                    DoCast(target, SPELL_SHADOW_BOLT);
                events.Repeat(3s, 6s);
                break;
            default:
                break;
        }
    }
};

void AddSC_boss_gehennas()
{
    RegisterMoltenCoreCreatureAI(boss_gehennas);
}
