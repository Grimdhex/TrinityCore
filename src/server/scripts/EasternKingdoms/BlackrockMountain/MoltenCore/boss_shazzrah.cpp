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
#include "ScriptedCreature.h"
#include "SpellScript.h"

enum Spells
{
    SPELL_ARCANE_EXPLOSION      = 19712,
    SPELL_SHAZZRAH_CURSE        = 19713,
    SPELL_MAGIC_GROUNDING       = 19714,
    SPELL_COUNTERSPELL          = 19715,
    SPELL_SHAZZRAH_GATE_DUMMY   = 23138, // Teleports to and attacks a random target.
    SPELL_SHAZZRAH_GATE         = 23139
};

enum Events
{
    EVENT_ARCANE_EXPLOSION      = 1,
    EVENT_SHAZZRAH_CURSE        = 2,
    EVENT_MAGIC_GROUNDING       = 3,
    EVENT_COUNTERSPELL          = 4,
    EVENT_SHAZZRAH_GATE         = 5
};

struct boss_shazzrah : public BossAI
{
    boss_shazzrah(Creature* creature) : BossAI(creature, BOSS_SHAZZRAH) { }

    void JustEngagedWith(Unit* target) override
    {
        BossAI::JustEngagedWith(target);

        events.ScheduleEvent(EVENT_ARCANE_EXPLOSION, 6s);
        events.ScheduleEvent(EVENT_SHAZZRAH_CURSE, 10s, 15s);
        events.ScheduleEvent(EVENT_MAGIC_GROUNDING, 15s, 20s);
        events.ScheduleEvent(EVENT_COUNTERSPELL, 10s);
        events.ScheduleEvent(EVENT_SHAZZRAH_GATE, 30s);
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_ARCANE_EXPLOSION:
                DoCastVictim(SPELL_ARCANE_EXPLOSION);
                events.Repeat(4s,6s);
                break;
            case EVENT_SHAZZRAH_CURSE:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true, true, -SPELL_SHAZZRAH_CURSE))
                    DoCast(target, SPELL_SHAZZRAH_CURSE);
                events.Repeat(25s,30s);
                break;
            case EVENT_MAGIC_GROUNDING:
                DoCastSelf(SPELL_MAGIC_GROUNDING);
                events.Repeat(7s, 9s);
                break;
            case EVENT_COUNTERSPELL:
                DoCastVictim(SPELL_COUNTERSPELL);
                events.Repeat(15s,20s);
                break;
            case EVENT_SHAZZRAH_GATE:
                ResetThreatList();
                DoCastAOE(SPELL_SHAZZRAH_GATE_DUMMY);
                events.RescheduleEvent(EVENT_ARCANE_EXPLOSION, 3s, 6s);
                events.Repeat(30s);
                break;
            default:
                break;
        }
    }
};

// 23138 - Gate of Shazzrah
class spell_shazzrah_gate_dummy : public SpellScript
{
    PrepareSpellScript(spell_shazzrah_gate_dummy);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SHAZZRAH_GATE });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster();

        if (targets.empty())
            return;

        targets.remove_if([caster](WorldObject const* target) -> bool
        {
            Player const* playerTarget = target->ToPlayer();
            // Should not target non player targets
            if (!playerTarget)
            {
                return true;
            }

            // Should skip current victim
            if (caster->GetVictim() == playerTarget)
            {
                return true;
            }

            // Should not target enemies within melee range
            if (playerTarget->IsWithinMeleeRange(caster))
            {
                return true;
            }

            return false;
        });

        WorldObject* target = Trinity::Containers::SelectRandomContainerElement(targets);
        targets.clear();
        targets.push_back(target);
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();

        if (caster && target)
        {
            target->CastSpell(caster, SPELL_SHAZZRAH_GATE, true);
            caster->CastSpell(nullptr, SPELL_ARCANE_EXPLOSION);

            if (Creature* creatureCaster = caster->ToCreature())
            {
                creatureCaster->GetThreatManager().ResetAllThreat();
                creatureCaster->GetThreatManager().AddThreat(target, 1);
                creatureCaster->AI()->AttackStart(target); // Attack the target which caster will teleport to.
            }
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_shazzrah_gate_dummy::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_shazzrah_gate_dummy::HandleScript, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_boss_shazzrah()
{
    RegisterMoltenCoreCreatureAI(boss_shazzrah);
    RegisterSpellScript(spell_shazzrah_gate_dummy);
}
