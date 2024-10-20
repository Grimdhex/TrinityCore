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

#include "CombatAI.h"
#include "ConditionMgr.h"
#include "Creature.h"
#include "CreatureAIImpl.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Vehicle.h"

/////////////////
// AggressorAI
/////////////////

int32 AggressorAI::Permissible(Creature const* creature)
{
    // have some hostile factions, it will be selected by IsHostileTo check at MoveInLineOfSight
    if (!creature->IsCivilian() && !creature->IsNeutralToAll())
        return PERMIT_BASE_REACTIVE;

    return PERMIT_BASE_NO;
}

void AggressorAI::UpdateAI(uint32 /*diff*/)
{
    if (!UpdateVictim())
        return;

    DoMeleeAttackIfReady();
}

/////////////////
// CombatAI
/////////////////

void CombatAI::InitializeAI()
{
    for (uint32 spell : me->m_spells)
        if (spell && sSpellMgr->GetSpellInfo(spell))
            _spells.push_back(spell);

    CreatureAI::InitializeAI();
}

void CombatAI::Reset()
{
    _events.Reset();
}

void CombatAI::JustDied(Unit* killer)
{
    if (!killer)
        return;

    for (uint32 spell : _spells)
    {
        if (AISpellInfo[spell].condition == AICOND_DIE)
            me->CastSpell(killer, spell, true);
    }
}

void CombatAI::JustEngagedWith(Unit* who)
{
    for (uint32 spell : _spells)
    {
        if (AISpellInfo[spell].condition == AICOND_AGGRO)
            me->CastSpell(who, spell, false);
        else if (AISpellInfo[spell].condition == AICOND_COMBAT)
            _events.ScheduleEvent(spell, Milliseconds(AISpellInfo[spell].cooldown + rand32() % AISpellInfo[spell].cooldown));
    }
}

void CombatAI::UpdateAI(uint32 diff)
{
    if (!UpdateVictim())
        return;

    _events.Update(diff);

    if (me->HasUnitState(UNIT_STATE_CASTING))
        return;

    if (uint32 spellId = _events.ExecuteEvent())
    {
        DoCast(spellId);
        _events.ScheduleEvent(spellId, Milliseconds(AISpellInfo[spellId].cooldown + rand32() % AISpellInfo[spellId].cooldown));
    }
    else
        DoMeleeAttackIfReady();
}

void CombatAI::SpellInterrupted(uint32 spellId, uint32 unTimeMs)
{
    _events.RescheduleEvent(spellId, Milliseconds(unTimeMs));
}

/////////////////
// CasterAI
/////////////////

void CasterAI::InitializeAI()
{
    CombatAI::InitializeAI();

    _attackDistance = 30.0f;

    for (uint32 spell : _spells)
    {
        if (AISpellInfo[spell].condition == AICOND_COMBAT && _attackDistance > GetAISpellInfo(spell)->maxRange)
            _attackDistance = GetAISpellInfo(spell)->maxRange;
    }

    if (_attackDistance == 30.0f)
        _attackDistance = MELEE_RANGE;
}

void CasterAI::JustEngagedWith(Unit* who)
{
    if (!who)
        return;

    if (_spells.empty())
        return;

    uint32 spell = rand32() % _spells.size();
    uint32 count = 0;
    for (auto itr = _spells.begin(); itr != _spells.end(); ++itr, ++count)
    {
        if (AISpellInfo[*itr].condition == AICOND_AGGRO)
            me->CastSpell(who, *itr, false);
        else if (AISpellInfo[*itr].condition == AICOND_COMBAT)
        {
            uint32 cooldown = GetAISpellInfo(*itr)->realCooldown;
            if (count == spell)
            {
                DoCast(_spells[spell]);
                cooldown += me->GetCurrentSpellCastTime(*itr);
            }
            _events.ScheduleEvent(*itr, Milliseconds(cooldown));
        }
    }
}

void CasterAI::UpdateAI(uint32 diff)
{
    if (!UpdateVictim())
        return;

    _events.Update(diff);

    if (me->GetVictim() && me->EnsureVictim()->HasBreakableByDamageCrowdControlAura(me))
    {
        me->InterruptNonMeleeSpells(false);
        return;
    }

    if (me->HasUnitState(UNIT_STATE_CASTING))
        return;

    if (uint32 spellId = _events.ExecuteEvent())
    {
        DoCast(spellId);
        uint32 casttime = me->GetCurrentSpellCastTime(spellId);
        _events.ScheduleEvent(spellId, (casttime ? Milliseconds(casttime) : 500ms) + Milliseconds(GetAISpellInfo(spellId)->realCooldown));
    }
}

//////////////
// ArcherAI
//////////////

ArcherAI::ArcherAI(Creature* creature) : CreatureAI(creature)
{
    if (!creature->m_spells[0])
        TC_LOG_ERROR("scripts.ai", "ArcherAI set for creature with spell1 = 0. AI will do nothing ({})", creature->GetGUID().ToString());

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(creature->m_spells[0]);
    _minimumRange = spellInfo ? spellInfo->GetMinRange(false) : 0;

    if (!_minimumRange)
        _minimumRange = MELEE_RANGE;
    creature->m_CombatDistance = spellInfo ? spellInfo->GetMaxRange(false) : 0;
    creature->m_SightDistance = creature->m_CombatDistance;
}

void ArcherAI::AttackStart(Unit* who)
{
    if (!who)
        return;

    if (me->IsWithinCombatRange(who, _minimumRange))
    {
        if (me->Attack(who, true) && !who->IsFlying())
            me->GetMotionMaster()->MoveChase(who);
    }
    else
    {
        if (me->Attack(who, false) && !who->IsFlying())
            me->GetMotionMaster()->MoveChase(who, me->m_CombatDistance);
    }

    if (who->IsFlying())
        me->GetMotionMaster()->MoveIdle();
}

void ArcherAI::UpdateAI(uint32 /*diff*/)
{
    if (!UpdateVictim())
        return;

    if (!me->IsWithinCombatRange(me->GetVictim(), _minimumRange))
        DoSpellAttackIfReady(me->m_spells[0]);
    else
        DoMeleeAttackIfReady();
}

//////////////
// TurretAI
//////////////

TurretAI::TurretAI(Creature* creature) : CreatureAI(creature)
{
    if (!creature->m_spells[0])
        TC_LOG_ERROR("scripts.ai", "TurretAI set for creature with spell1 = 0. AI will do nothing ({})", creature->GetGUID().ToString());

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(creature->m_spells[0]);
    _minimumRange = spellInfo ? spellInfo->GetMinRange(false) : 0;
    creature->m_CombatDistance = spellInfo ? spellInfo->GetMaxRange(false) : 0;
    creature->m_SightDistance = creature->m_CombatDistance;
}

bool TurretAI::CanAIAttack(Unit const* who) const
{
    if (!who)
        return false;

    /// @todo use one function to replace it
    if (!me->IsWithinCombatRange(who, me->m_CombatDistance) || (_minimumRange && me->IsWithinCombatRange(who, _minimumRange)))
        return false;

    return true;
}

void TurretAI::AttackStart(Unit* who)
{
    if (who)
        me->Attack(who, false);
}

void TurretAI::UpdateAI(uint32 /*diff*/)
{
    if (!UpdateVictim())
        return;

    DoSpellAttackIfReady(me->m_spells[0]);
}

//////////////
// VehicleAI
//////////////

VehicleAI::VehicleAI(Creature* creature) : CreatureAI(creature), _hasConditions(false), _conditionsTimer(VEHICLE_CONDITION_CHECK_TIME)
{
    LoadConditions();
    _dismiss = false;
    _dismissTimer = VEHICLE_DISMISS_TIME;
}

// NOTE: VehicleAI::UpdateAI runs even while the vehicle is mounted
void VehicleAI::UpdateAI(uint32 diff)
{
    CheckConditions(diff);

    if (_dismiss)
    {
        if (_dismissTimer < diff)
        {
            _dismiss = false;
            me->DespawnOrUnsummon();
        }
        else
            _dismissTimer -= diff;
    }
}

void VehicleAI::OnCharmed(bool /*isNew*/)
{
    bool const charmed = me->IsCharmed();
    if (!me->GetVehicleKit()->IsVehicleInUse() && !charmed && _hasConditions) // was used and has conditions
    {
        _dismiss = true; // needs reset
    }
    else if (charmed)
        _dismiss = false; // in use again

    _dismissTimer = VEHICLE_DISMISS_TIME; // reset timer
}

void VehicleAI::LoadConditions()
{
    _hasConditions = sConditionMgr->HasConditionsForNotGroupedEntry(CONDITION_SOURCE_TYPE_CREATURE_TEMPLATE_VEHICLE, me->GetEntry());
}

void VehicleAI::CheckConditions(uint32 diff)
{
    if (!_hasConditions)
        return;

    if (_conditionsTimer <= diff)
    {
        if (Vehicle* vehicleKit = me->GetVehicleKit())
        {
            for (auto const& [i, vehicleSeat] : vehicleKit->Seats)
            {
                if (Unit* passenger = ObjectAccessor::GetUnit(*me, vehicleSeat.Passenger.Guid))
                {
                    if (Player* player = passenger->ToPlayer())
                    {
                        if (!sConditionMgr->IsObjectMeetingNotGroupedConditions(CONDITION_SOURCE_TYPE_CREATURE_TEMPLATE_VEHICLE, me->GetEntry(), player, me))
                        {
                            player->ExitVehicle();
                            return; // check other pessanger in next tick
                        }
                    }
                }
            }
        }

        _conditionsTimer = VEHICLE_CONDITION_CHECK_TIME;
    }
    else
        _conditionsTimer -= diff;
}

int32 VehicleAI::Permissible(Creature const* creature)
{
    if (creature->IsVehicle())
        return PERMIT_BASE_SPECIAL;

    return PERMIT_BASE_NO;
}
