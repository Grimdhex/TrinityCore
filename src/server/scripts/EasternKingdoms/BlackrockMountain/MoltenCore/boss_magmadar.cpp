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

/// @note: Compare to Mangos, it's confirmed that Lava bomb spell is the same for melee and ranged targets.

#include "ScriptMgr.h"
#include "molten_core.h"
#include "ObjectMgr.h"
#include "ScriptedCreature.h"

enum Texts
{
    EMOTE_FRENZY            = 0
};

enum Spells
{
    SPELL_FRENZY                    = 19451,
    SPELL_MAGMA_SPIT                = 19449,
    SPELL_PANIC                     = 19408,
    SPELL_LAVA_BOMB                 = 19411,
    SPELL_LAVA_BOMB_VISUAL          = 20494

};

enum Events
{
    EVENT_FRENZY            = 1,
    EVENT_PANIC             = 2,
    EVENT_LAVA_BOMB         = 3
};

struct boss_magmadar : public BossAI
{
    boss_magmadar(Creature* creature) : BossAI(creature, BOSS_MAGMADAR) { }

    void Reset() override
    {
        BossAI::Reset();
        DoCastSelf(SPELL_MAGMA_SPIT, true);
    }

    void JustEngagedWith(Unit* victim) override
    {
        BossAI::JustEngagedWith(victim);

        events.ScheduleEvent(EVENT_FRENZY, 8500ms);
        events.ScheduleEvent(EVENT_PANIC, 5s, 10s);
        events.ScheduleEvent(EVENT_LAVA_BOMB, 12s);
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_FRENZY:
                Talk(EMOTE_FRENZY);
                DoCastSelf(SPELL_FRENZY);
                events.Repeat(15s);
                break;
            case EVENT_PANIC:
                DoCastVictim(SPELL_PANIC);
                events.Repeat(30s, 35s);
                break;
            case EVENT_LAVA_BOMB:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true, true, -SPELL_LAVA_BOMB))
                    DoCast(target, SPELL_LAVA_BOMB);
                events.Repeat(12s, 15s);
                break;
            default:
                break;
        }
    }
};

// Lava Bomb - Dummy visual
class spell_magmadar_lava_bomb : public SpellScript
{
    PrepareSpellScript(spell_magmadar_lava_bomb);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_LAVA_BOMB_VISUAL });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
        {
            uint32 spellId = 0;
            switch (m_scriptSpellId)
            {
                case SPELL_LAVA_BOMB:
                {
                    spellId = SPELL_LAVA_BOMB_VISUAL;
                    break;
                }
                default:
                {
                    return;
                }
            }
            target->CastSpell(target, spellId, true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_magmadar_lava_bomb::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_boss_magmadar()
{
    RegisterMoltenCoreCreatureAI(boss_magmadar);
    RegisterSpellScript(spell_magmadar_lava_bomb);
}
