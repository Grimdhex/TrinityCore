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
#include "molten_core.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "ObjectMgr.h"

enum Emotes
{
    EMOTE_SERVICE       = 0
};

enum Spells
{
    SPELL_INFERNO       = 19695,
    SPELL_INFERNO_DMG   = 19698,
    SPELL_IGNITE_MANA   = 19659,
    SPELL_LIVING_BOMB   = 20475,
    SPELL_ARMAGEDDON    = 20478,

    AURA_ARMAGEDDON     = 20479,
};

enum Events
{
    EVENT_INFERNO       = 1,
    EVENT_IGNITE_MANA   = 2,
    EVENT_LIVING_BOMB   = 3,
};

struct boss_baron_geddon : public BossAI
{
    boss_baron_geddon(Creature* creature) : BossAI(creature, BOSS_BARON_GEDDON) { }

    void JustEngagedWith(Unit* victim) override
    {
        BossAI::JustEngagedWith(victim);

        events.ScheduleEvent(EVENT_INFERNO, 15s);
        events.ScheduleEvent(EVENT_IGNITE_MANA, 9s);
        events.ScheduleEvent(EVENT_LIVING_BOMB, 15s);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (!HealthBelowPct(2) || me->HasAura(AURA_ARMAGEDDON))
            return;

        me->InterruptNonMeleeSpells(true);
        DoCastSelf(SPELL_ARMAGEDDON);
        Talk(EMOTE_SERVICE);
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_INFERNO:
                DoCastAOE(SPELL_INFERNO);
                events.Repeat(20s, 30s);
                break;
            case EVENT_IGNITE_MANA:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true, true, -SPELL_IGNITE_MANA))
                    DoCast(target, SPELL_IGNITE_MANA);
                events.Repeat(30s);
                break;
            case EVENT_LIVING_BOMB:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true))
                    DoCast(target, SPELL_LIVING_BOMB);
                events.Repeat(15s);
                break;
            default:
                break;
        }
    }
};

// 19695 - Inferno
class spell_baron_geddon_inferno : public AuraScript
{
    PrepareAuraScript(spell_baron_geddon_inferno);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_INFERNO_DMG });
    }

    void OnPeriodic(AuraEffect const* aurEff)
    {
        PreventDefaultAction();

        static const int32 damageForTick[8] = { 500, 500, 500, 1000, 1000, 2000, 3000, 5000 };
        CastSpellExtraArgs args;
        args.TriggerFlags = TRIGGERED_FULL_MASK;
        args.TriggeringAura = aurEff;
        args.AddSpellMod(SPELLVALUE_BASE_POINT0, damageForTick[aurEff->GetTickNumber() - 1]);

        GetTarget()->CastSpell(nullptr, SPELL_INFERNO_DMG, args);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_baron_geddon_inferno::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

void AddSC_boss_baron_geddon()
{
    RegisterMoltenCoreCreatureAI(boss_baron_geddon);
    RegisterSpellScript(spell_baron_geddon_inferno);
}
