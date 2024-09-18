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

#include "SpellMgr.h"
#include "BattlefieldMgr.h"
#include "BattlegroundMgr.h"
#include "Chat.h"
#include "Containers.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"

bool IsPrimaryProfessionSkill(uint32 skill)
{
    SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(skill);
    if (!pSkill)
        return false;

    if (pSkill->CategoryID != SKILL_CATEGORY_PROFESSION)
        return false;

    return true;
}

bool IsPartOfSkillLine(uint32 skillId, uint32 spellId)
{
    SkillLineAbilityMapBounds skillBounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
    for (SkillLineAbilityMap::const_iterator itr = skillBounds.first; itr != skillBounds.second; ++itr)
        if (itr->second->SkillLine == skillId)
            return true;

    return false;
}

SpellMgr::SpellMgr() { }

SpellMgr::~SpellMgr()
{
    UnloadSpellInfoStore();
}

SpellMgr* SpellMgr::instance()
{
    static SpellMgr instance;
    return &instance;
}

/// Some checks for spells, to prevent adding deprecated/broken spells for trainers, spell book, etc
bool SpellMgr::IsSpellValid(SpellInfo const* spellInfo, Player* player, bool msg)
{
    // not exist
    if (!spellInfo)
        return false;

    bool needCheckReagents = false;

    // check effects
    for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
    {
        switch (spellEffectInfo.Effect)
        {
            // craft spell for crafting non-existed item (break client recipes list show)
            case SPELL_EFFECT_CREATE_ITEM:
            case SPELL_EFFECT_CREATE_ITEM_2:
            {
                if (spellEffectInfo.ItemType == 0)
                {
                    // skip auto-loot crafting spells, it does not need explicit item info (but has special fake items sometimes).
                    if (!spellInfo->IsLootCrafting())
                    {
                        if (msg)
                        {
                            if (player)
                                ChatHandler(player->GetSession()).PSendSysMessage("The craft spell %u does not have a create item entry.", spellInfo->Id);
                            else
                                TC_LOG_ERROR("sql.sql", "The craft spell {} does not have a create item entry.", spellInfo->Id);
                        }
                        return false;
                    }

                }
                // also possible IsLootCrafting case but fake items must exist anyway
                else if (!sObjectMgr->GetItemTemplate(spellEffectInfo.ItemType))
                {
                    if (msg)
                    {
                        if (player)
                            ChatHandler(player->GetSession()).PSendSysMessage("Craft spell %u has created a non-existing item in DB (Entry: %u) and then...", spellInfo->Id, spellEffectInfo.ItemType);
                        else
                            TC_LOG_ERROR("sql.sql", "Craft spell {} has created a non-existing item in DB (Entry: {}) and then...", spellInfo->Id, spellEffectInfo.ItemType);
                    }
                    return false;
                }

                needCheckReagents = true;
                break;
            }
            case SPELL_EFFECT_LEARN_SPELL:
            {
                SpellInfo const* spellInfo2 = sSpellMgr->GetSpellInfo(spellEffectInfo.TriggerSpell);
                if (!IsSpellValid(spellInfo2, player, msg))
                {
                    if (msg)
                    {
                        if (player)
                            ChatHandler(player->GetSession()).PSendSysMessage("Spell %u learn to broken spell %u, and then...", spellInfo->Id, spellEffectInfo.TriggerSpell);
                        else
                            TC_LOG_ERROR("sql.sql", "Spell {} learn to invalid spell {}, and then...", spellInfo->Id, spellEffectInfo.TriggerSpell);
                    }
                    return false;
                }
                break;
            }
            default:
                break;
        }
    }

    if (needCheckReagents)
    {
        for (uint8 j = 0; j < MAX_SPELL_REAGENTS; ++j)
        {
            if (spellInfo->Reagent[j] > 0 && !sObjectMgr->GetItemTemplate(spellInfo->Reagent[j]))
            {
                if (msg)
                {
                    if (player)
                        ChatHandler(player->GetSession()).PSendSysMessage("Craft spell %u refers a non-existing reagent in DB item (Entry: %u) and then...", spellInfo->Id, spellInfo->Reagent[j]);
                    else
                        TC_LOG_ERROR("sql.sql", "Craft spell {} refers to a non-existing reagent in DB, item (Entry: {}) and then...", spellInfo->Id, spellInfo->Reagent[j]);
                }
                return false;
            }
        }
    }

    return true;
}

uint32 SpellMgr::GetSpellDifficultyId(uint32 spellId) const
{
    SpellDifficultySearcherMap::const_iterator i = mSpellDifficultySearcherMap.find(spellId);
    return i == mSpellDifficultySearcherMap.end() ? 0 : i->second;
}

void SpellMgr::SetSpellDifficultyId(uint32 spellId, uint32 id)
{
    if (uint32 i = GetSpellDifficultyId(spellId))
        TC_LOG_ERROR("spells", "SpellMgr::SetSpellDifficultyId: The spell {} already has spellDifficultyId {}. Will override with spellDifficultyId {}.", spellId, i, id);
    mSpellDifficultySearcherMap[spellId] = id;
}

uint32 SpellMgr::GetSpellIdForDifficulty(uint32 spellId, WorldObject const* caster) const
{
    if (!GetSpellInfo(spellId))
        return spellId;

    if (!caster || !caster->GetMap() || (!caster->GetMap()->IsDungeon() && !caster->GetMap()->IsBattleground()))
        return spellId;

    uint32 mode = uint32(caster->GetMap()->GetSpawnMode());
    if (mode >= MAX_DIFFICULTY)
    {
        TC_LOG_ERROR("spells", "SpellMgr::GetSpellIdForDifficulty: Incorrect difficulty for spell {}.", spellId);
        return spellId; //return source spell
    }

    uint32 difficultyId = GetSpellDifficultyId(spellId);
    if (!difficultyId)
        return spellId; //return source spell, it has only REGULAR_DIFFICULTY

    SpellDifficultyEntry const* difficultyEntry = sSpellDifficultyStore.LookupEntry(difficultyId);
    if (!difficultyEntry)
    {
        TC_LOG_ERROR("spells", "SpellMgr::GetSpellIdForDifficulty: SpellDifficultyEntry was not found for spell {}. This should never happen.", spellId);
        return spellId; //return source spell
    }

    if (difficultyEntry->DifficultySpellID[mode] <= 0 && mode > DUNGEON_DIFFICULTY_HEROIC)
    {
        TC_LOG_DEBUG("spells", "SpellMgr::GetSpellIdForDifficulty: spell {} mode {} spell is NULL, using mode {}", spellId, mode, mode - 2);
        mode -= 2;
    }

    if (difficultyEntry->DifficultySpellID[mode] <= 0)
    {
        TC_LOG_ERROR("sql.sql", "SpellMgr::GetSpellIdForDifficulty: spell {} mode {} spell is 0. Check spelldifficulty_dbc!", spellId, mode);
        return spellId;
    }

    TC_LOG_DEBUG("spells", "SpellMgr::GetSpellIdForDifficulty: spellid for spell {} in mode {} is {}", spellId, mode, difficultyEntry->DifficultySpellID[mode]);
    return uint32(difficultyEntry->DifficultySpellID[mode]);
}

SpellInfo const* SpellMgr::GetSpellForDifficultyFromSpell(SpellInfo const* spell, WorldObject const* caster) const
{
    if (!spell)
        return nullptr;

    uint32 newSpellId = GetSpellIdForDifficulty(spell->Id, caster);
    SpellInfo const* newSpell = GetSpellInfo(newSpellId);
    if (!newSpell)
    {
        TC_LOG_DEBUG("spells", "SpellMgr::GetSpellForDifficultyFromSpell: spell {} not found. Check spelldifficulty_dbc!", newSpellId);
        return spell;
    }

    TC_LOG_DEBUG("spells", "SpellMgr::GetSpellForDifficultyFromSpell: Spell id for instance mode is {} (original {})", newSpell->Id, spell->Id);
    return newSpell;
}

SpellChainNode const* SpellMgr::GetSpellChainNode(uint32 spell_id) const
{
    SpellChainMap::const_iterator itr = mSpellChains.find(spell_id);
    if (itr == mSpellChains.end())
        return nullptr;

    return &itr->second;
}

uint32 SpellMgr::GetFirstSpellInChain(uint32 spell_id) const
{
    if (SpellChainNode const* node = GetSpellChainNode(spell_id))
        return node->first->Id;

    return spell_id;
}

uint32 SpellMgr::GetLastSpellInChain(uint32 spell_id) const
{
    if (SpellChainNode const* node = GetSpellChainNode(spell_id))
        return node->last->Id;

    return spell_id;
}

uint32 SpellMgr::GetNextSpellInChain(uint32 spell_id) const
{
    if (SpellChainNode const* node = GetSpellChainNode(spell_id))
        if (node->next)
            return node->next->Id;

    return 0;
}

uint32 SpellMgr::GetPrevSpellInChain(uint32 spell_id) const
{
    if (SpellChainNode const* node = GetSpellChainNode(spell_id))
        if (node->prev)
            return node->prev->Id;

    return 0;
}

uint8 SpellMgr::GetSpellRank(uint32 spell_id) const
{
    if (SpellChainNode const* node = GetSpellChainNode(spell_id))
        return node->rank;

    return 0;
}

uint32 SpellMgr::GetSpellWithRank(uint32 spell_id, uint32 rank, bool strict) const
{
    if (SpellChainNode const* node = GetSpellChainNode(spell_id))
    {
        if (rank != node->rank)
            return GetSpellWithRank(node->rank < rank ? node->next->Id : node->prev->Id, rank, strict);
    }
    else if (strict && rank > 1)
        return 0;
    return spell_id;
}

Trinity::IteratorPair<SpellRequiredMap::const_iterator> SpellMgr::GetSpellsRequiredForSpellBounds(uint32 spell_id) const
{
    return Trinity::Containers::MapEqualRange(mSpellReq, spell_id);
}

SpellsRequiringSpellMapBounds SpellMgr::GetSpellsRequiringSpellBounds(uint32 spell_id) const
{
    return mSpellsReqSpell.equal_range(spell_id);
}

bool SpellMgr::IsSpellRequiringSpell(uint32 spellid, uint32 req_spellid) const
{
    SpellsRequiringSpellMapBounds spellsRequiringSpell = GetSpellsRequiringSpellBounds(req_spellid);
    for (SpellsRequiringSpellMap::const_iterator itr = spellsRequiringSpell.first; itr != spellsRequiringSpell.second; ++itr)
    {
        if (itr->second == spellid)
            return true;
    }
    return false;
}

SpellLearnSkillNode const* SpellMgr::GetSpellLearnSkill(uint32 spell_id) const
{
    SpellLearnSkillMap::const_iterator itr = mSpellLearnSkills.find(spell_id);
    if (itr != mSpellLearnSkills.end())
        return &itr->second;
    else
        return nullptr;
}

SpellLearnSpellMapBounds SpellMgr::GetSpellLearnSpellMapBounds(uint32 spell_id) const
{
    return mSpellLearnSpells.equal_range(spell_id);
}

bool SpellMgr::IsSpellLearnSpell(uint32 spell_id) const
{
    return mSpellLearnSpells.find(spell_id) != mSpellLearnSpells.end();
}

bool SpellMgr::IsSpellLearnToSpell(uint32 spell_id1, uint32 spell_id2) const
{
    SpellLearnSpellMapBounds bounds = GetSpellLearnSpellMapBounds(spell_id1);
    for (SpellLearnSpellMap::const_iterator i = bounds.first; i != bounds.second; ++i)
        if (i->second.spell == spell_id2)
            return true;
    return false;
}

SpellTargetPosition const* SpellMgr::GetSpellTargetPosition(uint32 spell_id, SpellEffIndex effIndex) const
{
    SpellTargetPositionMap::const_iterator itr = mSpellTargetPositions.find(std::make_pair(spell_id, effIndex));
    if (itr != mSpellTargetPositions.end())
        return &itr->second;
    return nullptr;
}

SpellSpellGroupMapBounds SpellMgr::GetSpellSpellGroupMapBounds(uint32 spell_id) const
{
    spell_id = GetFirstSpellInChain(spell_id);
    return mSpellSpellGroup.equal_range(spell_id);
}

bool SpellMgr::IsSpellMemberOfSpellGroup(uint32 spellid, SpellGroup groupid) const
{
    SpellSpellGroupMapBounds spellGroup = GetSpellSpellGroupMapBounds(spellid);
    for (SpellSpellGroupMap::const_iterator itr = spellGroup.first; itr != spellGroup.second; ++itr)
    {
        if (itr->second == groupid)
            return true;
    }
    return false;
}

SpellGroupSpellMapBounds SpellMgr::GetSpellGroupSpellMapBounds(SpellGroup group_id) const
{
    return mSpellGroupSpell.equal_range(group_id);
}

void SpellMgr::GetSetOfSpellsInSpellGroup(SpellGroup group_id, std::set<uint32>& foundSpells) const
{
    std::set<SpellGroup> usedGroups;
    GetSetOfSpellsInSpellGroup(group_id, foundSpells, usedGroups);
}

void SpellMgr::GetSetOfSpellsInSpellGroup(SpellGroup group_id, std::set<uint32>& foundSpells, std::set<SpellGroup>& usedGroups) const
{
    if (usedGroups.find(group_id) != usedGroups.end())
        return;
    usedGroups.insert(group_id);

    SpellGroupSpellMapBounds groupSpell = GetSpellGroupSpellMapBounds(group_id);
    for (SpellGroupSpellMap::const_iterator itr = groupSpell.first; itr != groupSpell.second; ++itr)
    {
        if (itr->second < 0)
        {
            SpellGroup currGroup = (SpellGroup)abs(itr->second);
            GetSetOfSpellsInSpellGroup(currGroup, foundSpells, usedGroups);
        }
        else
        {
            foundSpells.insert(itr->second);
        }
    }
}

bool SpellMgr::AddSameEffectStackRuleSpellGroups(SpellInfo const* spellInfo, uint32 auraType, int32 amount, std::map<SpellGroup, int32>& groups) const
{
    uint32 spellId = spellInfo->GetFirstRankSpell()->Id;
    auto spellGroupBounds = GetSpellSpellGroupMapBounds(spellId);
    // Find group with SPELL_GROUP_STACK_RULE_EXCLUSIVE_SAME_EFFECT if it belongs to one
    for (auto itr = spellGroupBounds.first; itr != spellGroupBounds.second; ++itr)
    {
        SpellGroup group = itr->second;
        auto found = mSpellSameEffectStack.find(group);
        if (found != mSpellSameEffectStack.end())
        {
            // check auraTypes
            if (!found->second.count(auraType))
                continue;

            // Put the highest amount in the map
            auto groupItr = groups.find(group);
            if (groupItr == groups.end())
                groups.emplace(group, amount);
            else
            {
                int32 curr_amount = groups[group];
                // Take absolute value because this also counts for the highest negative aura
                if (std::abs(curr_amount) < std::abs(amount))
                    groupItr->second = amount;
            }
            // return because a spell should be in only one SPELL_GROUP_STACK_RULE_EXCLUSIVE_SAME_EFFECT group per auraType
            return true;
        }
    }
    // Not in a SPELL_GROUP_STACK_RULE_EXCLUSIVE_SAME_EFFECT group, so return false
    return false;
}

SpellGroupStackRule SpellMgr::CheckSpellGroupStackRules(SpellInfo const* spellInfo1, SpellInfo const* spellInfo2) const
{
    ASSERT(spellInfo1);
    ASSERT(spellInfo2);

    uint32 spellid_1 = spellInfo1->GetFirstRankSpell()->Id;
    uint32 spellid_2 = spellInfo2->GetFirstRankSpell()->Id;

    // find SpellGroups which are common for both spells
    SpellSpellGroupMapBounds spellGroup1 = GetSpellSpellGroupMapBounds(spellid_1);
    std::set<SpellGroup> groups;
    for (SpellSpellGroupMap::const_iterator itr = spellGroup1.first; itr != spellGroup1.second; ++itr)
    {
        if (IsSpellMemberOfSpellGroup(spellid_2, itr->second))
        {
            bool add = true;
            SpellGroupSpellMapBounds groupSpell = GetSpellGroupSpellMapBounds(itr->second);
            for (SpellGroupSpellMap::const_iterator itr2 = groupSpell.first; itr2 != groupSpell.second; ++itr2)
            {
                if (itr2->second < 0)
                {
                    SpellGroup currGroup = (SpellGroup)abs(itr2->second);
                    if (IsSpellMemberOfSpellGroup(spellid_1, currGroup) && IsSpellMemberOfSpellGroup(spellid_2, currGroup))
                    {
                        add = false;
                        break;
                    }
                }
            }
            if (add)
                groups.insert(itr->second);
        }
    }

    SpellGroupStackRule rule = SPELL_GROUP_STACK_RULE_DEFAULT;

    for (std::set<SpellGroup>::iterator itr = groups.begin(); itr!= groups.end(); ++itr)
    {
        SpellGroupStackMap::const_iterator found = mSpellGroupStack.find(*itr);
        if (found != mSpellGroupStack.end())
            rule = found->second;
        if (rule)
            break;
    }
    return rule;
}

SpellGroupStackRule SpellMgr::GetSpellGroupStackRule(SpellGroup group) const
{
    SpellGroupStackMap::const_iterator itr = mSpellGroupStack.find(group);
    if (itr != mSpellGroupStack.end())
        return itr->second;

    return SPELL_GROUP_STACK_RULE_DEFAULT;
}

SpellProcEntry const* SpellMgr::GetSpellProcEntry(uint32 spellId) const
{
    SpellProcMap::const_iterator itr = mSpellProcMap.find(spellId);
    if (itr != mSpellProcMap.end())
        return &itr->second;
    return nullptr;
}

bool SpellMgr::CanSpellTriggerProcOnEvent(SpellProcEntry const& procEntry, ProcEventInfo& eventInfo)
{
    // proc type doesn't match
    if (!(eventInfo.GetTypeMask() & procEntry.ProcFlags))
        return false;

    // check XP or honor target requirement
    if (procEntry.AttributesMask & PROC_ATTR_REQ_EXP_OR_HONOR)
        if (Player* actor = eventInfo.GetActor()->ToPlayer())
            if (eventInfo.GetActionTarget() && !actor->isHonorOrXPTarget(eventInfo.GetActionTarget()))
                return false;

    // check mana requirement
    if (procEntry.AttributesMask & PROC_ATTR_REQ_MANA_COST)
        if (SpellInfo const* eventSpellInfo = eventInfo.GetSpellInfo())
            if (!eventSpellInfo->ManaCost && !eventSpellInfo->ManaCostPercentage)
                return false;

    // always trigger for these types
    if (eventInfo.GetTypeMask() & (PROC_FLAG_HEARTBEAT | PROC_FLAG_KILL | PROC_FLAG_DEATH))
        return true;

    // do triggered cast checks
    // Do not consider autoattacks as triggered spells
    if (!(procEntry.AttributesMask & PROC_ATTR_TRIGGERED_CAN_PROC) && !(eventInfo.GetTypeMask() & AUTO_ATTACK_PROC_FLAG_MASK))
    {
        if (Spell const* spell = eventInfo.GetProcSpell())
        {
            if (spell->IsTriggered())
            {
                SpellInfo const* spellInfo = spell->GetSpellInfo();
                if (!spellInfo->HasAttribute(SPELL_ATTR3_TRIGGERED_CAN_TRIGGER_PROC_2) &&
                    !spellInfo->HasAttribute(SPELL_ATTR2_TRIGGERED_CAN_TRIGGER_PROC))
                    return false;
            }
        }
    }

    // check school mask (if set) for other trigger types
    if (procEntry.SchoolMask && !(eventInfo.GetSchoolMask() & procEntry.SchoolMask))
        return false;

    // check spell family name/flags (if set) for spells
    if (eventInfo.GetTypeMask() & SPELL_PROC_FLAG_MASK)
    {
        if (SpellInfo const* eventSpellInfo = eventInfo.GetSpellInfo())
            if (!eventSpellInfo->IsAffected(procEntry.SpellFamilyName, procEntry.SpellFamilyMask))
                return false;

        // check spell type mask (if set)
        if (procEntry.SpellTypeMask && !(eventInfo.GetSpellTypeMask() & procEntry.SpellTypeMask))
            return false;
    }

    // check spell phase mask
    if (eventInfo.GetTypeMask() & REQ_SPELL_PHASE_PROC_FLAG_MASK)
    {
        if (!(eventInfo.GetSpellPhaseMask() & procEntry.SpellPhaseMask))
            return false;
    }

    // check hit mask (on taken hit or on done hit, but not on spell cast phase)
    if ((eventInfo.GetTypeMask() & TAKEN_HIT_PROC_FLAG_MASK) || ((eventInfo.GetTypeMask() & DONE_HIT_PROC_FLAG_MASK) && !(eventInfo.GetSpellPhaseMask() & PROC_SPELL_PHASE_CAST)))
    {
        uint32 hitMask = procEntry.HitMask;
        // get default values if hit mask not set
        if (!hitMask)
        {
            // for taken procs allow normal + critical hits by default
            if (eventInfo.GetTypeMask() & TAKEN_HIT_PROC_FLAG_MASK)
                hitMask |= PROC_HIT_NORMAL | PROC_HIT_CRITICAL;
            // for done procs allow normal + critical + absorbs by default
            else
                hitMask |= PROC_HIT_NORMAL | PROC_HIT_CRITICAL | PROC_HIT_ABSORB;
        }
        if (!(eventInfo.GetHitMask() & hitMask))
            return false;
    }

    return true;
}

SpellBonusEntry const* SpellMgr::GetSpellBonusData(uint32 spellId) const
{
    // Lookup data
    SpellBonusMap::const_iterator itr = mSpellBonusMap.find(spellId);
    if (itr != mSpellBonusMap.end())
        return &itr->second;
    // Not found, try lookup for 1 spell rank if exist
    if (uint32 rank_1 = GetFirstSpellInChain(spellId))
    {
        SpellBonusMap::const_iterator itr2 = mSpellBonusMap.find(rank_1);
        if (itr2 != mSpellBonusMap.end())
            return &itr2->second;
    }
    return nullptr;
}

SpellThreatEntry const* SpellMgr::GetSpellThreatEntry(uint32 spellID) const
{
    SpellThreatMap::const_iterator itr = mSpellThreatMap.find(spellID);
    if (itr != mSpellThreatMap.end())
        return &itr->second;
    else
    {
        uint32 firstSpell = GetFirstSpellInChain(spellID);
        itr = mSpellThreatMap.find(firstSpell);
        if (itr != mSpellThreatMap.end())
            return &itr->second;
    }
    return nullptr;
}

SkillLineAbilityMapBounds SpellMgr::GetSkillLineAbilityMapBounds(uint32 spell_id) const
{
    return mSkillLineAbilityMap.equal_range(spell_id);
}

PetAura const* SpellMgr::GetPetAura(uint32 spell_id, uint8 eff) const
{
    SpellPetAuraMap::const_iterator itr = mSpellPetAuraMap.find((spell_id<<8) + eff);
    if (itr != mSpellPetAuraMap.end())
        return &itr->second;
    else
        return nullptr;
}

SpellEnchantProcEntry const* SpellMgr::GetSpellEnchantProcEvent(uint32 enchId) const
{
    SpellEnchantProcEventMap::const_iterator itr = mSpellEnchantProcEventMap.find(enchId);
    if (itr != mSpellEnchantProcEventMap.end())
        return &itr->second;
    return nullptr;
}

bool SpellMgr::IsArenaAllowedEnchancment(uint32 ench_id) const
{
    return mEnchantCustomAttr[ench_id];
}

std::vector<int32> const* SpellMgr::GetSpellLinked(int32 spell_id) const
{
    return Trinity::Containers::MapGetValuePtr(mSpellLinkedMap, spell_id);
}

PetLevelupSpellSet const* SpellMgr::GetPetLevelupSpellList(uint32 petFamily) const
{
    PetLevelupSpellMap::const_iterator itr = mPetLevelupSpellMap.find(petFamily);
    if (itr != mPetLevelupSpellMap.end())
        return &itr->second;
    else
        return nullptr;
}

PetDefaultSpellsEntry const* SpellMgr::GetPetDefaultSpellsEntry(int32 id) const
{
    PetDefaultSpellsMap::const_iterator itr = mPetDefaultSpellsMap.find(id);
    if (itr != mPetDefaultSpellsMap.end())
        return &itr->second;
    return nullptr;
}

SpellAreaMapBounds SpellMgr::GetSpellAreaMapBounds(uint32 spell_id) const
{
    return mSpellAreaMap.equal_range(spell_id);
}

SpellAreaForQuestMapBounds SpellMgr::GetSpellAreaForQuestMapBounds(uint32 quest_id) const
{
    return mSpellAreaForQuestMap.equal_range(quest_id);
}

SpellAreaForQuestMapBounds SpellMgr::GetSpellAreaForQuestEndMapBounds(uint32 quest_id) const
{
    return mSpellAreaForQuestEndMap.equal_range(quest_id);
}

SpellAreaForAuraMapBounds SpellMgr::GetSpellAreaForAuraMapBounds(uint32 spell_id) const
{
    return mSpellAreaForAuraMap.equal_range(spell_id);
}

SpellAreaForAreaMapBounds SpellMgr::GetSpellAreaForAreaMapBounds(uint32 area_id) const
{
    return mSpellAreaForAreaMap.equal_range(area_id);
}

bool SpellArea::IsFitToRequirements(Player const* player, uint32 newZone, uint32 newArea) const
{
    if (gender != GENDER_NONE)                   // is not expected gender
        if (!player || gender != player->GetNativeGender())
            return false;

    if (raceMask)                                // is not expected race
        if (!player || !(raceMask & player->GetRaceMask()))
            return false;

    if (areaId)                                  // is not in expected zone
        if (newZone != areaId && newArea != areaId)
            return false;

    if (questStart)                              // is not in expected required quest state
        if (!player || (((1 << player->GetQuestStatus(questStart)) & questStartStatus) == 0))
            return false;

    if (questEnd)                                // is not in expected forbidden quest state
        if (!player || (((1 << player->GetQuestStatus(questEnd)) & questEndStatus) == 0))
            return false;

    if (auraSpell)                               // does not have expected aura
        if (!player || (auraSpell > 0 && !player->HasAura(auraSpell)) || (auraSpell < 0 && player->HasAura(-auraSpell)))
            return false;

    if (player)
    {
        if (Battleground* bg = player->GetBattleground())
            return bg->IsSpellAllowed(spellId, player);
    }

    // Extra conditions
    switch (spellId)
    {
        case 58600: // No fly Zone - Dalaran
        {
            if (!player)
                return false;

            AreaTableEntry const* pArea = sAreaTableStore.LookupEntry(player->GetAreaId());
            if (!(pArea && pArea->Flags & AREA_FLAG_NO_FLY_ZONE))
                return false;
            if (!player->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED) && !player->HasAuraType(SPELL_AURA_FLY))
                return false;
            break;
        }
        case 58730: // No fly Zone - Wintergrasp
        {
            if (!player)
                return false;

            Battlefield* Bf = sBattlefieldMgr->GetBattlefieldToZoneId(player->GetZoneId());
            if (!Bf || Bf->CanFlyIn() || (!player->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED) && !player->HasAuraType(SPELL_AURA_FLY)))
                return false;
            break;
        }
        case 56618: // Horde Controls Factory Phase Shift
        case 56617: // Alliance Controls Factory Phase Shift
        {
            if (!player)
                return false;

            Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(player->GetZoneId());

            if (!bf || bf->GetTypeId() != BATTLEFIELD_WG)
                return false;

            // team that controls the workshop in the specified area
            uint32 team = bf->GetData(newArea);

            if (team == TEAM_HORDE)
                return spellId == 56618;
            else if (team == TEAM_ALLIANCE)
                return spellId == 56617;
            break;
        }
        case 57940: // Essence of Wintergrasp - Northrend
        case 58045: // Essence of Wintergrasp - Wintergrasp
        {
            if (!player)
                return false;

            if (Battlefield* battlefieldWG = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG))
                return battlefieldWG->IsEnabled() && (player->GetTeamId() == battlefieldWG->GetDefenderTeam()) && !battlefieldWG->IsWarTime();
            break;
        }
        case 74411: // Battleground - Dampening
        {
            if (!player)
                return false;

            if (Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(player->GetZoneId()))
                return bf->IsWarTime();
            break;
        }

    }

    return true;
}

void SpellMgr::UnloadSpellInfoChains()
{
    for (SpellChainMap::iterator itr = mSpellChains.begin(); itr != mSpellChains.end(); ++itr)
        mSpellInfoMap[itr->first]->ChainEntry = nullptr;

    mSpellChains.clear();
}

void SpellMgr::LoadSpellTalentRanks()
{
    // cleanup core data before reload - remove reference to ChainNode from SpellInfo
    UnloadSpellInfoChains();

    for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
        if (!talentInfo)
            continue;

        SpellInfo const* lastSpell = nullptr;
        for (uint8 rank = MAX_TALENT_RANK - 1; rank > 0; --rank)
        {
            if (talentInfo->SpellRank[rank])
            {
                lastSpell = GetSpellInfo(talentInfo->SpellRank[rank]);
                break;
            }
        }

        if (!lastSpell)
            continue;

        SpellInfo const* firstSpell = GetSpellInfo(talentInfo->SpellRank[0]);
        if (!firstSpell)
        {
            TC_LOG_ERROR("spells", "SpellMgr::LoadSpellTalentRanks: First Rank Spell {} for TalentEntry {} does not exist.", talentInfo->SpellRank[0], i);
            continue;
        }

        SpellInfo const* prevSpell = nullptr;
        for (uint8 rank = 0; rank < MAX_TALENT_RANK; ++rank)
        {
            uint32 spellId = talentInfo->SpellRank[rank];
            if (!spellId)
                break;

            SpellInfo const* currentSpell = GetSpellInfo(spellId);
            if (!currentSpell)
            {
                TC_LOG_ERROR("spells", "SpellMgr::LoadSpellTalentRanks: Spell {} (Rank: {}) for TalentEntry {} does not exist.", spellId, rank + 1, i);
                break;
            }

            SpellChainNode node;
            node.first = firstSpell;
            node.last  = lastSpell;
            node.rank  = rank + 1;

            node.prev = prevSpell;
            node.next = node.rank < MAX_TALENT_RANK ? GetSpellInfo(talentInfo->SpellRank[node.rank]) : nullptr;

            mSpellChains[spellId] = node;
            mSpellInfoMap[spellId]->ChainEntry = &mSpellChains[spellId];

            prevSpell = currentSpell;
        }
    }
}

void SpellMgr::LoadSpellRanks()
{
    // cleanup data and load spell ranks for talents from dbc
    LoadSpellTalentRanks();

    uint32 oldMSTime = getMSTime();

    //                                                     0             1       2
    QueryResult result = WorldDatabase.Query("SELECT first_spell_id, spell_id, `rank` from spell_ranks ORDER BY first_spell_id, `rank`");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 spell rank records. DB table `spell_ranks` is empty.");
        return;
    }

    uint32 count = 0;
    bool finished = false;

    do
    {
                        // spellid, rank
        std::list < std::pair < int32, int32 > > rankChain;
        int32 currentSpell = -1;
        int32 lastSpell = -1;

        // fill one chain
        while (currentSpell == lastSpell && !finished)
        {
            Field* fields = result->Fetch();

            currentSpell = fields[0].GetUInt32();
            if (lastSpell == -1)
                lastSpell = currentSpell;
            uint32 spell_id = fields[1].GetUInt32();
            uint32 rank = fields[2].GetUInt8();

            // don't drop the row if we're moving to the next rank
            if (currentSpell == lastSpell)
            {
                rankChain.push_back(std::make_pair(spell_id, rank));
                if (!result->NextRow())
                    finished = true;
            }
            else
                break;
        }
        // check if chain is made with valid first spell
        SpellInfo const* first = GetSpellInfo(lastSpell);
        if (!first)
        {
            TC_LOG_ERROR("sql.sql", "The spell rank identifier(first_spell_id) {} listed in `spell_ranks` does not exist!", lastSpell);
            continue;
        }
        // check if chain is long enough
        if (rankChain.size() < 2)
        {
            TC_LOG_ERROR("sql.sql", "There is only 1 spell rank for identifier(first_spell_id) {} in `spell_ranks`, entry is not needed!", lastSpell);
            continue;
        }
        int32 curRank = 0;
        bool valid = true;
        // check spells in chain
        for (std::list<std::pair<int32, int32> >::iterator itr = rankChain.begin(); itr!= rankChain.end(); ++itr)
        {
            SpellInfo const* spell = GetSpellInfo(itr->first);
            if (!spell)
            {
                TC_LOG_ERROR("sql.sql", "The spell {} (rank {}) listed in `spell_ranks` for chain {} does not exist!", itr->first, itr->second, lastSpell);
                valid = false;
                break;
            }
            ++curRank;
            if (itr->second != curRank)
            {
                TC_LOG_ERROR("sql.sql", "The spell {} (rank {}) listed in `spell_ranks` for chain {} does not have a proper rank value (should be {})!", itr->first, itr->second, lastSpell, curRank);
                valid = false;
                break;
            }
        }
        if (!valid)
            continue;
        int32 prevRank = 0;
        // insert the chain
        std::list<std::pair<int32, int32> >::iterator itr = rankChain.begin();
        do
        {
            ++count;
            int32 addedSpell = itr->first;

            if (mSpellInfoMap[addedSpell]->ChainEntry)
                TC_LOG_ERROR("sql.sql", "The spell {} (rank: {}, first: {}) listed in `spell_ranks` already has ChainEntry from dbc.", addedSpell, itr->second, lastSpell);

            mSpellChains[addedSpell].first = GetSpellInfo(lastSpell);
            mSpellChains[addedSpell].last = GetSpellInfo(rankChain.back().first);
            mSpellChains[addedSpell].rank = itr->second;
            mSpellChains[addedSpell].prev = GetSpellInfo(prevRank);
            mSpellInfoMap[addedSpell]->ChainEntry = &mSpellChains[addedSpell];
            prevRank = addedSpell;
            ++itr;

            if (itr == rankChain.end())
            {
                mSpellChains[addedSpell].next = nullptr;
                break;
            }
            else
                mSpellChains[addedSpell].next = GetSpellInfo(itr->first);
        }
        while (true);
    }
    while (!finished);

    TC_LOG_INFO("server.loading", ">> Loaded {} spell rank records in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellRequired()
{
    uint32 oldMSTime = getMSTime();

    mSpellsReqSpell.clear();                                   // need for reload case
    mSpellReq.clear();                                         // need for reload case

    //                                                   0        1
    QueryResult result = WorldDatabase.Query("SELECT spell_id, req_spell from spell_required");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 spell required records. DB table `spell_required` is empty.");

        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 spell_id = fields[0].GetUInt32();
        uint32 spell_req = fields[1].GetUInt32();

        // check if chain is made with valid first spell
        SpellInfo const* spell = GetSpellInfo(spell_id);
        if (!spell)
        {
            TC_LOG_ERROR("sql.sql", "spell_id {} in `spell_required` table could not be found in dbc, skipped.", spell_id);
            continue;
        }

        SpellInfo const* reqSpell = GetSpellInfo(spell_req);
        if (!reqSpell)
        {
            TC_LOG_ERROR("sql.sql", "req_spell {} in `spell_required` table could not be found in dbc, skipped.", spell_req);
            continue;
        }

        if (spell->IsRankOf(reqSpell))
        {
            TC_LOG_ERROR("sql.sql", "req_spell {} and spell_id {} in `spell_required` table are ranks of the same spell, entry not needed, skipped.", spell_req, spell_id);
            continue;
        }

        if (IsSpellRequiringSpell(spell_id, spell_req))
        {
            TC_LOG_ERROR("sql.sql", "Duplicate entry of req_spell {} and spell_id {} in `spell_required`, skipped.", spell_req, spell_id);
            continue;
        }

        mSpellReq.insert (std::pair<uint32, uint32>(spell_id, spell_req));
        mSpellsReqSpell.insert (std::pair<uint32, uint32>(spell_req, spell_id));
        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} spell required records in {} ms", count, GetMSTimeDiffToNow(oldMSTime));

}

void SpellMgr::LoadSpellLearnSkills()
{
    uint32 oldMSTime = getMSTime();

    mSpellLearnSkills.clear();                              // need for reload case

    // search auto-learned skills and add its to map also for use in unlearn spells/talents
    uint32 dbc_count = 0;
    for (SpellInfo const* entry : mSpellInfoMap)
    {
        if (!entry)
            continue;

        for (SpellEffectInfo const& spellEffectInfo : entry->GetEffects())
        {
            SpellLearnSkillNode dbc_node;
            switch (spellEffectInfo.Effect)
            {
                case SPELL_EFFECT_SKILL:
                    dbc_node.skill = spellEffectInfo.MiscValue;
                    dbc_node.step = spellEffectInfo.CalcValue();
                    if (dbc_node.skill != SKILL_RIDING)
                        dbc_node.value = 1;
                    else
                        dbc_node.value = dbc_node.step * 75;
                    dbc_node.maxvalue = dbc_node.step * 75;
                    break;
                case SPELL_EFFECT_DUAL_WIELD:
                    dbc_node.skill = SKILL_DUAL_WIELD;
                    dbc_node.step = 1;
                    dbc_node.value = 1;
                    dbc_node.maxvalue = 1;
                    break;
                default:
                    continue;
            }

            mSpellLearnSkills[entry->Id] = dbc_node;
            ++dbc_count;
            break;
        }
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} Spell Learn Skills from DBC in {} ms", dbc_count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellLearnSpells()
{
    uint32 oldMSTime = getMSTime();

    mSpellLearnSpells.clear();                              // need for reload case

    //                                                  0      1        2
    QueryResult result = WorldDatabase.Query("SELECT entry, SpellID, Active FROM spell_learn_spell");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 spell learn spells. DB table `spell_learn_spell` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 spell_id = fields[0].GetUInt32();

        SpellLearnSpellNode node;
        node.spell       = fields[1].GetUInt32();
        node.active      = fields[2].GetBool();
        node.autoLearned = false;

        if (!GetSpellInfo(spell_id))
        {
            TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_learn_spell` does not exist.", spell_id);
            continue;
        }

        if (!GetSpellInfo(node.spell))
        {
            TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_learn_spell` learning non-existing spell {}.", spell_id, node.spell);
            continue;
        }

        if (GetTalentSpellCost(node.spell))
        {
            TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_learn_spell` attempts learning talent spell {}, skipped.", spell_id, node.spell);
            continue;
        }

        mSpellLearnSpells.insert(SpellLearnSpellMap::value_type(spell_id, node));

        ++count;
    } while (result->NextRow());

    // search auto-learned spells and add its to map also for use in unlearn spells/talents
    uint32 dbc_count = 0;
    for (uint32 spell = 0; spell < GetSpellInfoStoreSize(); ++spell)
    {
        SpellInfo const* entry = GetSpellInfo(spell);

        if (!entry)
            continue;

        for (SpellEffectInfo const& spellEffectInfo : entry->GetEffects())
        {
            if (spellEffectInfo.IsEffect(SPELL_EFFECT_LEARN_SPELL))
            {
                SpellLearnSpellNode dbc_node;
                dbc_node.spell = spellEffectInfo.TriggerSpell;
                dbc_node.active = true;                     // all dbc based learned spells is active (show in spell book or hide by client itself)

                // ignore learning not existed spells (broken/outdated/or generic learnig spell 483
                if (!GetSpellInfo(dbc_node.spell))
                    continue;

                // talent or passive spells or skill-step spells auto-cast and not need dependent learning,
                // pet teaching spells must not be dependent learning (cast)
                // other required explicit dependent learning
                dbc_node.autoLearned = spellEffectInfo.TargetA.GetTarget() == TARGET_UNIT_PET || GetTalentSpellCost(spell) > 0 || entry->IsPassive() || entry->HasEffect(SPELL_EFFECT_SKILL_STEP);

                SpellLearnSpellMapBounds db_node_bounds = GetSpellLearnSpellMapBounds(spell);

                bool found = false;
                for (SpellLearnSpellMap::const_iterator itr = db_node_bounds.first; itr != db_node_bounds.second; ++itr)
                {
                    if (itr->second.spell == dbc_node.spell)
                    {
                        TC_LOG_ERROR("sql.sql", "The spell {} is an auto-learn spell {} in spell.dbc and the record in `spell_learn_spell` is redundant. Please update your DB.",
                            spell, dbc_node.spell);
                        found = true;
                        break;
                    }
                }

                if (!found)                                  // add new spell-spell pair if not found
                {
                    mSpellLearnSpells.insert(SpellLearnSpellMap::value_type(spell, dbc_node));
                    ++dbc_count;
                }
            }
        }
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} spell learn spells + {} found in DBC in {} ms", count, dbc_count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellTargetPositions()
{
    uint32 oldMSTime = getMSTime();

    mSpellTargetPositions.clear();                                // need for reload case

    //                                                0      1          2        3         4           5            6
    QueryResult result = WorldDatabase.Query("SELECT ID, EffectIndex, MapID, PositionX, PositionY, PositionZ, Orientation FROM spell_target_position");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 spell target coordinates. DB table `spell_target_position` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 Spell_ID = fields[0].GetUInt32();
        SpellEffIndex effIndex = SpellEffIndex(fields[1].GetUInt8());

        SpellTargetPosition st;

        st.target_mapId       = fields[2].GetUInt16();
        st.target_X           = fields[3].GetFloat();
        st.target_Y           = fields[4].GetFloat();
        st.target_Z           = fields[5].GetFloat();
        st.target_Orientation = fields[6].GetFloat();

        MapEntry const* mapEntry = sMapStore.LookupEntry(st.target_mapId);
        if (!mapEntry)
        {
            TC_LOG_ERROR("sql.sql", "Spell (Id: {}, effIndex: {}) target map (ID: {}) does not exist in `Map.dbc`.", Spell_ID, effIndex, st.target_mapId);
            continue;
        }

        if (st.target_X==0 && st.target_Y==0 && st.target_Z==0)
        {
            TC_LOG_ERROR("sql.sql", "Spell (Id: {}, effIndex: {}) target coordinates not provided.", Spell_ID, effIndex);
            continue;
        }

        SpellInfo const* spellInfo = GetSpellInfo(Spell_ID);
        if (!spellInfo)
        {
            TC_LOG_ERROR("sql.sql", "Spell (Id: {}) listed in `spell_target_position` does not exist.", Spell_ID);
            continue;
        }

        if (spellInfo->GetEffect(effIndex).TargetA.GetTarget() == TARGET_DEST_DB || spellInfo->GetEffect(effIndex).TargetB.GetTarget() == TARGET_DEST_DB)
        {
            std::pair<uint32, SpellEffIndex> key = std::make_pair(Spell_ID, effIndex);
            mSpellTargetPositions[key] = st;
            ++count;
        }
        else
        {
            TC_LOG_ERROR("sql.sql", "Spell (Id: {}, effIndex: {}) listed in `spell_target_position` does not have a target TARGET_DEST_DB (17).", Spell_ID, effIndex);
            continue;
        }

    } while (result->NextRow());

    /*
    // Check all spells
    for (uint32 i = 1; i < GetSpellInfoStoreSize; ++i)
    {
        SpellInfo const* spellInfo = GetSpellInfo(i);
        if (!spellInfo)
            continue;

        bool found = false;
        for (int j = 0; j < MAX_SPELL_EFFECTS; ++j)
        {
            switch (spellInfo->Effects[j].TargetA)
            {
                case TARGET_DEST_DB:
                    found = true;
                    break;
            }
            if (found)
                break;
            switch (spellInfo->Effects[j].TargetB)
            {
                case TARGET_DEST_DB:
                    found = true;
                    break;
            }
            if (found)
                break;
        }
        if (found)
        {
            if (!sSpellMgr->GetSpellTargetPosition(i))
                TC_LOG_DEBUG("spells", "Spell (ID: {}) does not have a record in `spell_target_position`.", i);
        }
    }*/

    TC_LOG_INFO("server.loading", ">> Loaded {} spell teleport coordinates in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellGroups()
{
    uint32 oldMSTime = getMSTime();

    mSpellSpellGroup.clear();                                  // need for reload case
    mSpellGroupSpell.clear();

    //                                                0     1
    QueryResult result = WorldDatabase.Query("SELECT id, spell_id FROM spell_group");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 spell group definitions. DB table `spell_group` is empty.");
        return;
    }

    std::set<uint32> groups;
    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 group_id = fields[0].GetUInt32();
        if (group_id <= SPELL_GROUP_DB_RANGE_MIN && group_id >= SPELL_GROUP_CORE_RANGE_MAX)
        {
            TC_LOG_ERROR("sql.sql", "SpellGroup id {} listed in `spell_group` is in core range, but is not defined in core!", group_id);
            continue;
        }
        int32 spell_id = fields[1].GetInt32();

        groups.insert(group_id);
        mSpellGroupSpell.emplace(SpellGroup(group_id), spell_id);

    } while (result->NextRow());

    for (auto itr = mSpellGroupSpell.begin(); itr!= mSpellGroupSpell.end();)
    {
        if (itr->second < 0)
        {
            if (groups.find(abs(itr->second)) == groups.end())
            {
                TC_LOG_ERROR("sql.sql", "SpellGroup id {} listed in `spell_group` does not exist", abs(itr->second));
                itr = mSpellGroupSpell.erase(itr);
            }
            else
                ++itr;
        }
        else
        {
            SpellInfo const* spellInfo = GetSpellInfo(itr->second);
            if (!spellInfo)
            {
                TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_group` does not exist", itr->second);
                itr = mSpellGroupSpell.erase(itr);
            }
            else if (spellInfo->GetRank() > 1)
            {
                TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_group` is not the first rank of the spell.", itr->second);
                itr = mSpellGroupSpell.erase(itr);
            }
            else
                ++itr;
        }
    }

    for (auto groupItr = groups.begin(); groupItr != groups.end(); ++groupItr)
    {
        std::set<uint32> spells;
        GetSetOfSpellsInSpellGroup(SpellGroup(*groupItr), spells);

        for (auto spellItr = spells.begin(); spellItr != spells.end(); ++spellItr)
        {
            ++count;
            mSpellSpellGroup.emplace(*spellItr, SpellGroup(*groupItr));
        }
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} spell group definitions in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellGroupStackRules()
{
    uint32 oldMSTime = getMSTime();

    mSpellGroupStack.clear();                                  // need for reload case
    mSpellSameEffectStack.clear();

    std::vector<uint32> sameEffectGroups;

    //                                                       0         1
    QueryResult result = WorldDatabase.Query("SELECT group_id, stack_rule FROM spell_group_stack_rules");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 spell group stack rules. DB table `spell_group_stack_rules` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 group_id = fields[0].GetUInt32();
        uint8 stack_rule = fields[1].GetInt8();
        if (stack_rule >= SPELL_GROUP_STACK_RULE_MAX)
        {
            TC_LOG_ERROR("sql.sql", "SpellGroupStackRule {} listed in `spell_group_stack_rules` does not exist.", stack_rule);
            continue;
        }

        auto bounds = GetSpellGroupSpellMapBounds((SpellGroup)group_id);
        if (bounds.first == bounds.second)
        {
            TC_LOG_ERROR("sql.sql", "SpellGroup id {} listed in `spell_group_stack_rules` does not exist.", group_id);
            continue;
        }

        mSpellGroupStack.emplace(SpellGroup(group_id), SpellGroupStackRule(stack_rule));

        // different container for same effect stack rules, need to check effect types
        if (stack_rule == SPELL_GROUP_STACK_RULE_EXCLUSIVE_SAME_EFFECT)
            sameEffectGroups.push_back(group_id);

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} spell group stack rules in {} ms", count, GetMSTimeDiffToNow(oldMSTime));

    count = 0;
    oldMSTime = getMSTime();
    TC_LOG_INFO("server.loading", ">> Parsing SPELL_GROUP_STACK_RULE_EXCLUSIVE_SAME_EFFECT stack rules...");

    for (uint32 group_id : sameEffectGroups)
    {
        std::set<uint32> spellIds;
        GetSetOfSpellsInSpellGroup(SpellGroup(group_id), spellIds);

        std::unordered_set<uint32> auraTypes;

        // we have to 'guess' what effect this group corresponds to
        {
            std::unordered_multiset<uint32 /*auraName*/> frequencyContainer;

            // only waylay for the moment (shared group)
            std::vector<std::vector<uint32 /*auraName*/>> const SubGroups =
            {
                { SPELL_AURA_MOD_MELEE_HASTE, SPELL_AURA_MOD_MELEE_RANGED_HASTE, SPELL_AURA_MOD_RANGED_HASTE }
            };

            for (uint32 spellId : spellIds)
            {
                SpellInfo const* spellInfo = AssertSpellInfo(spellId);
                for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
                {
                    if (!spellEffectInfo.IsAura())
                        continue;

                    uint32 auraName = spellEffectInfo.ApplyAuraName;
                    for (std::vector<uint32> const& subGroup : SubGroups)
                    {
                        if (std::find(subGroup.begin(), subGroup.end(), auraName) != subGroup.end())
                        {
                            // count as first aura
                            auraName = subGroup.front();
                            break;
                        }
                    }

                    frequencyContainer.insert(auraName);
                }
            }

            uint32 auraType = 0;
            size_t auraTypeCount = 0;
            for (uint32 auraName : frequencyContainer)
            {
                size_t currentCount = frequencyContainer.count(auraName);
                if (currentCount > auraTypeCount)
                {
                    auraType = auraName;
                    auraTypeCount = currentCount;
                }
            }

            for (std::vector<uint32> const& subGroup : SubGroups)
            {
                if (auraType == subGroup.front())
                {
                    auraTypes.insert(subGroup.begin(), subGroup.end());
                    break;
                }
            }

            if (auraTypes.empty())
                auraTypes.insert(auraType);
        }

        // re-check spells against guessed group
        for (uint32 spellId : spellIds)
        {
            SpellInfo const* spellInfo = AssertSpellInfo(spellId);

            bool found = false;
            while (spellInfo)
            {
                for (uint32 auraType : auraTypes)
                {
                    if (spellInfo->HasAura(AuraType(auraType)))
                    {
                        found = true;
                        break;
                    }
                }

                if (found)
                    break;

                spellInfo = spellInfo->GetNextRankSpell();
            }

            // not found either, log error
            if (!found)
                TC_LOG_ERROR("sql.sql", "SpellId {} listed in `spell_group` with stack rule 3 does not share aura assigned for group {}", spellId, group_id);
        }

        mSpellSameEffectStack[SpellGroup(group_id)] = auraTypes;
        ++count;
    }

    TC_LOG_INFO("server.loading", ">> Parsed {} SPELL_GROUP_STACK_RULE_EXCLUSIVE_SAME_EFFECT stack rules in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellProcs()
{
    uint32 oldMSTime = getMSTime();

    mSpellProcMap.clear();                             // need for reload case

    //                                                     0           1                2                 3                 4                 5
    QueryResult result = WorldDatabase.Query("SELECT SpellId, SchoolMask, SpellFamilyName, SpellFamilyMask0, SpellFamilyMask1, SpellFamilyMask2, "
    //           6              7               8        9               10                  11              12      13        14       15
        "ProcFlags, SpellTypeMask, SpellPhaseMask, HitMask, AttributesMask, DisableEffectsMask, ProcsPerMinute, Chance, Cooldown, Charges FROM spell_proc");

    uint32 count = 0;
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            int32 spellId = fields[0].GetInt32();

            bool allRanks = false;
            if (spellId < 0)
            {
                allRanks = true;
                spellId = -spellId;
            }

            SpellInfo const* spellInfo = GetSpellInfo(spellId);
            if (!spellInfo)
            {
                TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_proc` does not exist", spellId);
                continue;
            }

            if (allRanks)
            {
                if (!spellInfo->IsRanked())
                    TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_proc` with all ranks, but spell has no ranks.", spellId);

                if (spellInfo->GetFirstRankSpell()->Id != uint32(spellId))
                {
                    TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_proc` is not the first rank of the spell.", spellId);
                    continue;
                }
            }

            SpellProcEntry baseProcEntry;

            baseProcEntry.SchoolMask = fields[1].GetInt8();
            baseProcEntry.SpellFamilyName = fields[2].GetUInt16();
            baseProcEntry.SpellFamilyMask[0] = fields[3].GetUInt32();
            baseProcEntry.SpellFamilyMask[1] = fields[4].GetUInt32();
            baseProcEntry.SpellFamilyMask[2] = fields[5].GetUInt32();
            baseProcEntry.ProcFlags = fields[6].GetUInt32();
            baseProcEntry.SpellTypeMask = fields[7].GetUInt32();
            baseProcEntry.SpellPhaseMask = fields[8].GetUInt32();
            baseProcEntry.HitMask = fields[9].GetUInt32();
            baseProcEntry.AttributesMask = fields[10].GetUInt32();
            baseProcEntry.DisableEffectsMask = fields[11].GetUInt32();
            baseProcEntry.ProcsPerMinute = fields[12].GetFloat();
            baseProcEntry.Chance = fields[13].GetFloat();
            baseProcEntry.Cooldown = Milliseconds(fields[14].GetUInt32());
            baseProcEntry.Charges = fields[15].GetUInt8();

            while (spellInfo)
            {
                if (mSpellProcMap.find(spellInfo->Id) != mSpellProcMap.end())
                {
                    TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_proc` already has its first rank in the table.", spellInfo->Id);
                    break;
                }

                SpellProcEntry procEntry = SpellProcEntry(baseProcEntry);

                // take defaults from dbcs
                if (!procEntry.ProcFlags)
                    procEntry.ProcFlags = spellInfo->ProcFlags;
                if (!procEntry.Charges)
                    procEntry.Charges = spellInfo->ProcCharges;
                if (!procEntry.Chance && !procEntry.ProcsPerMinute)
                    procEntry.Chance = float(spellInfo->ProcChance);

                // validate data
                if (procEntry.SchoolMask & ~SPELL_SCHOOL_MASK_ALL)
                    TC_LOG_ERROR("sql.sql", "`spell_proc` table entry for spellId {} has wrong `SchoolMask` set: {}", spellInfo->Id, procEntry.SchoolMask);
                if (procEntry.SpellFamilyName && (procEntry.SpellFamilyName < SPELLFAMILY_MAGE || procEntry.SpellFamilyName > SPELLFAMILY_PET || procEntry.SpellFamilyName == 14 || procEntry.SpellFamilyName == 16))
                    TC_LOG_ERROR("sql.sql", "`spell_proc` table entry for spellId {} has wrong `SpellFamilyName` set: {}", spellInfo->Id, procEntry.SpellFamilyName);
                if (procEntry.Chance < 0)
                {
                    TC_LOG_ERROR("sql.sql", "`spell_proc` table entry for spellId {} has negative value in the `Chance` field", spellInfo->Id);
                    procEntry.Chance = 0;
                }
                if (procEntry.ProcsPerMinute < 0)
                {
                    TC_LOG_ERROR("sql.sql", "`spell_proc` table entry for spellId {} has negative value in the `ProcsPerMinute` field", spellInfo->Id);
                    procEntry.ProcsPerMinute = 0;
                }
                if (!procEntry.ProcFlags)
                    TC_LOG_ERROR("sql.sql", "The `spell_proc` table entry for spellId {} doesn't have any `ProcFlags` value defined, proc will not be triggered.", spellInfo->Id);
                if (procEntry.SpellTypeMask & ~PROC_SPELL_TYPE_MASK_ALL)
                    TC_LOG_ERROR("sql.sql", "`spell_proc` table entry for spellId {} has wrong `SpellTypeMask` set: {}", spellInfo->Id, procEntry.SpellTypeMask);
                if (procEntry.SpellTypeMask && !(procEntry.ProcFlags & SPELL_PROC_FLAG_MASK))
                    TC_LOG_ERROR("sql.sql", "The `spell_proc` table entry for spellId {} has `SpellTypeMask` value defined, but it will not be used for the defined `ProcFlags` value.", spellInfo->Id);
                if (!procEntry.SpellPhaseMask && procEntry.ProcFlags & REQ_SPELL_PHASE_PROC_FLAG_MASK)
                    TC_LOG_ERROR("sql.sql", "The `spell_proc` table entry for spellId {} doesn't have any `SpellPhaseMask` value defined, but it is required for the defined `ProcFlags` value. Proc will not be triggered.", spellInfo->Id);
                if (procEntry.SpellPhaseMask & ~PROC_SPELL_PHASE_MASK_ALL)
                    TC_LOG_ERROR("sql.sql", "The `spell_proc` table entry for spellId {} has wrong `SpellPhaseMask` set: {}", spellInfo->Id, procEntry.SpellPhaseMask);
                if (procEntry.SpellPhaseMask && !(procEntry.ProcFlags & REQ_SPELL_PHASE_PROC_FLAG_MASK))
                    TC_LOG_ERROR("sql.sql", "The `spell_proc` table entry for spellId {} has a `SpellPhaseMask` value defined, but it will not be used for the defined `ProcFlags` value.", spellInfo->Id);
                if (procEntry.HitMask & ~PROC_HIT_MASK_ALL)
                    TC_LOG_ERROR("sql.sql", "The `spell_proc` table entry for spellId {} has wrong `HitMask` set: {}", spellInfo->Id, procEntry.HitMask);
                if (procEntry.HitMask && !(procEntry.ProcFlags & TAKEN_HIT_PROC_FLAG_MASK || (procEntry.ProcFlags & DONE_HIT_PROC_FLAG_MASK && (!procEntry.SpellPhaseMask || procEntry.SpellPhaseMask & (PROC_SPELL_PHASE_HIT | PROC_SPELL_PHASE_FINISH)))))
                    TC_LOG_ERROR("sql.sql", "The `spell_proc` table entry for spellId {} has `HitMask` value defined, but it will not be used for defined `ProcFlags` and `SpellPhaseMask` values.", spellInfo->Id);
                for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
                    if ((procEntry.DisableEffectsMask & (1u << spellEffectInfo.EffectIndex)) && !spellEffectInfo.IsAura())
                        TC_LOG_ERROR("sql.sql", "The `spell_proc` table entry for spellId {} has DisableEffectsMask with effect {}, but effect {} is not an aura effect", spellInfo->Id, static_cast<uint32>(spellEffectInfo.EffectIndex), static_cast<uint32>(spellEffectInfo.EffectIndex));
                if (procEntry.AttributesMask & PROC_ATTR_REQ_SPELLMOD)
                {
                    bool found = false;
                    for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
                    {
                        if (!spellEffectInfo.IsAura())
                            continue;

                        if (spellEffectInfo.ApplyAuraName == SPELL_AURA_ADD_PCT_MODIFIER || spellEffectInfo.ApplyAuraName == SPELL_AURA_ADD_FLAT_MODIFIER)
                        {
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                        TC_LOG_ERROR("sql.sql", "The `spell_proc` table entry for spellId {} has Attribute PROC_ATTR_REQ_SPELLMOD, but spell has no spell mods. Proc will not be triggered", spellInfo->Id);
                }

                mSpellProcMap[spellInfo->Id] = procEntry;

                if (allRanks)
                    spellInfo = spellInfo->GetNextRankSpell();
                else
                    break;
            }
            ++count;
        } while (result->NextRow());
    }
    else
        TC_LOG_INFO("server.loading", ">> Loaded 0 spell proc conditions and data. DB table `spell_proc` is empty.");

    TC_LOG_INFO("server.loading", ">> Loaded {} spell proc conditions and data in {} ms", count, GetMSTimeDiffToNow(oldMSTime));

    // Define can trigger auras
    bool isTriggerAura[TOTAL_AURAS];
    // Triggered always, even from triggered spells
    bool isAlwaysTriggeredAura[TOTAL_AURAS];
    // SpellTypeMask to add to the proc
    uint32 spellTypeMask[TOTAL_AURAS];

    // List of auras that CAN trigger but may not exist in spell_proc
    // in most cases needed to drop charges

    // some aura types need additional checks (eg SPELL_AURA_MECHANIC_IMMUNITY needs mechanic check)
    // see AuraEffect::CheckEffectProc
    for (uint16 i = 0; i < TOTAL_AURAS; ++i)
    {
        isTriggerAura[i] = false;
        isAlwaysTriggeredAura[i] = false;
        spellTypeMask[i] = PROC_SPELL_TYPE_MASK_ALL;
    }

    isTriggerAura[SPELL_AURA_DUMMY] = true;                                 // Most dummy auras should require scripting, but there are some exceptions (ie 12311)
    isTriggerAura[SPELL_AURA_MOD_CONFUSE] = true;                           // "Any direct damaging attack will revive targets"
    isTriggerAura[SPELL_AURA_MOD_THREAT] = true;                            // Only one spell: 28762 part of Mage T3 8p bonus
    isTriggerAura[SPELL_AURA_MOD_STUN] = true;                              // Aura does not have charges but needs to be removed on trigger
    isTriggerAura[SPELL_AURA_MOD_DAMAGE_DONE] = true;
    isTriggerAura[SPELL_AURA_MOD_DAMAGE_TAKEN] = true;
    isTriggerAura[SPELL_AURA_MOD_RESISTANCE] = true;
    isTriggerAura[SPELL_AURA_MOD_STEALTH] = true;
    isTriggerAura[SPELL_AURA_MOD_FEAR] = true;                              // Aura does not have charges but needs to be removed on trigger
    isTriggerAura[SPELL_AURA_MOD_ROOT] = true;
    isTriggerAura[SPELL_AURA_TRANSFORM] = true;
    isTriggerAura[SPELL_AURA_REFLECT_SPELLS] = true;
    isTriggerAura[SPELL_AURA_DAMAGE_IMMUNITY] = true;
    isTriggerAura[SPELL_AURA_PROC_TRIGGER_SPELL] = true;
    isTriggerAura[SPELL_AURA_PROC_TRIGGER_DAMAGE] = true;
    isTriggerAura[SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK] = true;
    isTriggerAura[SPELL_AURA_MOD_POWER_COST_SCHOOL_PCT] = true;
    isTriggerAura[SPELL_AURA_MOD_POWER_COST_SCHOOL] = true;
    isTriggerAura[SPELL_AURA_REFLECT_SPELLS_SCHOOL] = true;
    isTriggerAura[SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN] = true;
    isTriggerAura[SPELL_AURA_MOD_ATTACK_POWER] = true;
    isTriggerAura[SPELL_AURA_ADD_CASTER_HIT_TRIGGER] = true;
    isTriggerAura[SPELL_AURA_OVERRIDE_CLASS_SCRIPTS] = true;
    isTriggerAura[SPELL_AURA_MOD_MELEE_HASTE] = true;
    isTriggerAura[SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE] = true;
    isTriggerAura[SPELL_AURA_RAID_PROC_FROM_CHARGE] = true;
    isTriggerAura[SPELL_AURA_RAID_PROC_FROM_CHARGE_WITH_VALUE] = true;
    isTriggerAura[SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE] = true;
    isTriggerAura[SPELL_AURA_MOD_SPELL_CRIT_CHANCE] = true;
    isTriggerAura[SPELL_AURA_ADD_FLAT_MODIFIER] = true;
    isTriggerAura[SPELL_AURA_ADD_PCT_MODIFIER] = true;
    isTriggerAura[SPELL_AURA_ABILITY_IGNORE_AURASTATE] = true;
    isTriggerAura[SPELL_AURA_MOD_INVISIBILITY] = true;
    isTriggerAura[SPELL_AURA_FORCE_REACTION] = true;
    isTriggerAura[SPELL_AURA_MOD_TAUNT] = true;
    isTriggerAura[SPELL_AURA_MOD_DETAUNT] = true;
    isTriggerAura[SPELL_AURA_MOD_DAMAGE_PERCENT_DONE] = true;
    isTriggerAura[SPELL_AURA_MOD_ATTACK_POWER_PCT] = true;
    isTriggerAura[SPELL_AURA_MOD_HIT_CHANCE] = true;
    isTriggerAura[SPELL_AURA_MOD_WEAPON_CRIT_PERCENT] = true;
    isTriggerAura[SPELL_AURA_MOD_BLOCK_PERCENT] = true;

    isAlwaysTriggeredAura[SPELL_AURA_OVERRIDE_CLASS_SCRIPTS] = true;
    isAlwaysTriggeredAura[SPELL_AURA_MOD_STEALTH] = true;
    isAlwaysTriggeredAura[SPELL_AURA_MOD_CONFUSE] = true;
    isAlwaysTriggeredAura[SPELL_AURA_MOD_FEAR] = true;
    isAlwaysTriggeredAura[SPELL_AURA_MOD_ROOT] = true;
    isAlwaysTriggeredAura[SPELL_AURA_MOD_STUN] = true;
    isAlwaysTriggeredAura[SPELL_AURA_TRANSFORM] = true;
    isAlwaysTriggeredAura[SPELL_AURA_MOD_INVISIBILITY] = true;

    spellTypeMask[SPELL_AURA_MOD_STEALTH] = PROC_SPELL_TYPE_DAMAGE | PROC_SPELL_TYPE_NO_DMG_HEAL;
    spellTypeMask[SPELL_AURA_MOD_CONFUSE] = PROC_SPELL_TYPE_DAMAGE;
    spellTypeMask[SPELL_AURA_MOD_FEAR] = PROC_SPELL_TYPE_DAMAGE;
    spellTypeMask[SPELL_AURA_MOD_ROOT] = PROC_SPELL_TYPE_DAMAGE;
    spellTypeMask[SPELL_AURA_MOD_STUN] = PROC_SPELL_TYPE_DAMAGE;
    spellTypeMask[SPELL_AURA_TRANSFORM] = PROC_SPELL_TYPE_DAMAGE;
    spellTypeMask[SPELL_AURA_MOD_INVISIBILITY] = PROC_SPELL_TYPE_DAMAGE;

    // This generates default procs to retain compatibility with previous proc system
    TC_LOG_INFO("server.loading", "Generating spell proc data from SpellMap...");
    count = 0;
    oldMSTime = getMSTime();

    for (SpellInfo const* spellInfo : mSpellInfoMap)
    {
        if (!spellInfo)
            continue;

        // Data already present in DB, overwrites default proc
        if (mSpellProcMap.find(spellInfo->Id) != mSpellProcMap.end())
            continue;

        // Nothing to do if no flags set
        if (!spellInfo->ProcFlags)
            continue;

        bool addTriggerFlag = false;
        uint32 procSpellTypeMask = PROC_SPELL_TYPE_NONE;
        uint32 nonProcMask = 0;
        for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
        {
            if (!spellEffectInfo.IsEffect())
                continue;

            uint32 auraName = spellEffectInfo.ApplyAuraName;
            if (!auraName)
                continue;

            if (!isTriggerAura[auraName])
            {
                // explicitly disable non proccing auras to avoid losing charges on self proc
                nonProcMask |= 1 << spellEffectInfo.EffectIndex;
                continue;
            }

            procSpellTypeMask |= spellTypeMask[auraName];
            if (isAlwaysTriggeredAura[auraName])
                addTriggerFlag = true;

            // many proc auras with taken procFlag mask don't have attribute "can proc with triggered"
            // they should proc nevertheless (example mage armor spells with judgement)
            if (!addTriggerFlag && (spellInfo->ProcFlags & TAKEN_HIT_PROC_FLAG_MASK) != 0)
            {
                switch (auraName)
                {
                    case SPELL_AURA_PROC_TRIGGER_SPELL:
                    case SPELL_AURA_PROC_TRIGGER_DAMAGE:
                        addTriggerFlag = true;
                        break;
                    default:
                        break;
                }
            }
        }

        if (!procSpellTypeMask)
        {
            for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
            {
                if (spellEffectInfo.IsAura())
                {
                    TC_LOG_ERROR("sql.sql", "Spell Id {} has DBC ProcFlags {}, but it's of non-proc aura type, it probably needs an entry in `spell_proc` table to be handled correctly.", spellInfo->Id, spellInfo->ProcFlags);
                    break;
                }
            }

            continue;
        }

        SpellProcEntry procEntry;
        procEntry.SchoolMask      = 0;
        procEntry.ProcFlags = spellInfo->ProcFlags;
        procEntry.SpellFamilyName = 0;
        for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
            if (spellEffectInfo.IsEffect() && isTriggerAura[spellEffectInfo.ApplyAuraName])
                procEntry.SpellFamilyMask |= spellEffectInfo.SpellClassMask;

        if (procEntry.SpellFamilyMask)
            procEntry.SpellFamilyName = spellInfo->SpellFamilyName;

        procEntry.SpellTypeMask   = procSpellTypeMask;
        procEntry.SpellPhaseMask  = PROC_SPELL_PHASE_HIT;
        procEntry.HitMask         = PROC_HIT_NONE; // uses default proc @see SpellMgr::CanSpellTriggerProcOnEvent

        for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
        {
            if (!spellEffectInfo.IsAura())
                continue;

            switch (spellEffectInfo.ApplyAuraName)
            {
                // Reflect auras should only proc off reflects
                case SPELL_AURA_REFLECT_SPELLS:
                case SPELL_AURA_REFLECT_SPELLS_SCHOOL:
                    procEntry.HitMask = PROC_HIT_REFLECT;
                    break;
                // Only drop charge on crit
                case SPELL_AURA_MOD_WEAPON_CRIT_PERCENT:
                    procEntry.HitMask = PROC_HIT_CRITICAL;
                    break;
                // Only drop charge on block
                case SPELL_AURA_MOD_BLOCK_PERCENT:
                    procEntry.HitMask = PROC_HIT_BLOCK;
                    break;
                // proc auras with another aura reducing hit chance (eg 63767) only proc on missed attack
                case SPELL_AURA_MOD_HIT_CHANCE:
                    if (spellEffectInfo.CalcValue() <= -100)
                        procEntry.HitMask = PROC_HIT_MISS;
                    break;
                default:
                    continue;
            }
            break;
        }

        procEntry.AttributesMask  = 0;
        procEntry.DisableEffectsMask = nonProcMask;
        if (spellInfo->ProcFlags & PROC_FLAG_KILL)
            procEntry.AttributesMask |= PROC_ATTR_REQ_EXP_OR_HONOR;
        if (addTriggerFlag)
            procEntry.AttributesMask |= PROC_ATTR_TRIGGERED_CAN_PROC;

        procEntry.ProcsPerMinute  = 0;
        procEntry.Chance          = spellInfo->ProcChance;
        procEntry.Cooldown        = Milliseconds::zero();
        procEntry.Charges         = spellInfo->ProcCharges;

        mSpellProcMap[spellInfo->Id] = procEntry;
        ++count;
    }

    TC_LOG_INFO("server.loading", ">> Generated spell proc data for {} spells in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellBonuses()
{
    uint32 oldMSTime = getMSTime();

    mSpellBonusMap.clear();                             // need for reload case

    //                                                0      1             2          3         4
    QueryResult result = WorldDatabase.Query("SELECT entry, direct_bonus, dot_bonus, ap_bonus, ap_dot_bonus FROM spell_bonus_data");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 spell bonus data. DB table `spell_bonus_data` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 entry = fields[0].GetUInt32();

        SpellInfo const* spell = GetSpellInfo(entry);
        if (!spell)
        {
            TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_bonus_data` does not exist.", entry);
            continue;
        }

        SpellBonusEntry& sbe = mSpellBonusMap[entry];
        sbe.direct_damage = fields[1].GetFloat();
        sbe.dot_damage    = fields[2].GetFloat();
        sbe.ap_bonus      = fields[3].GetFloat();
        sbe.ap_dot_bonus   = fields[4].GetFloat();

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} extra spell bonus data in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellThreats()
{
    uint32 oldMSTime = getMSTime();

    mSpellThreatMap.clear();                                // need for reload case

    //                                                0      1        2       3
    QueryResult result = WorldDatabase.Query("SELECT entry, flatMod, pctMod, apPctMod FROM spell_threat");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 aggro generating spells. DB table `spell_threat` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();

        if (!GetSpellInfo(entry))
        {
            TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_threat` does not exist.", entry);
            continue;
        }

        SpellThreatEntry ste;
        ste.flatMod  = fields[1].GetInt32();
        ste.pctMod   = fields[2].GetFloat();
        ste.apPctMod = fields[3].GetFloat();

        mSpellThreatMap[entry] = ste;
        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} SpellThreatEntries in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSkillLineAbilityMap()
{
    uint32 oldMSTime = getMSTime();

    mSkillLineAbilityMap.clear();

    uint32 count = 0;

    for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
    {
        SkillLineAbilityEntry const* SkillInfo = sSkillLineAbilityStore.LookupEntry(i);
        if (!SkillInfo)
            continue;

        mSkillLineAbilityMap.insert(SkillLineAbilityMap::value_type(SkillInfo->Spell, SkillInfo));
        ++count;
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} SkillLineAbility MultiMap Data in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellPetAuras()
{
    uint32 oldMSTime = getMSTime();

    mSpellPetAuraMap.clear();                                  // need for reload case

    //                                                  0       1       2    3
    QueryResult result = WorldDatabase.Query("SELECT spell, effectId, pet, aura FROM spell_pet_auras");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 spell pet auras. DB table `spell_pet_auras` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 spell = fields[0].GetUInt32();
        SpellEffIndex eff = SpellEffIndex(fields[1].GetUInt8());
        uint32 pet = fields[2].GetUInt32();
        uint32 aura = fields[3].GetUInt32();

        SpellPetAuraMap::iterator itr = mSpellPetAuraMap.find((spell << 8) + eff);
        if (itr != mSpellPetAuraMap.end())
            itr->second.AddAura(pet, aura);
        else
        {
            SpellInfo const* spellInfo = GetSpellInfo(spell);
            if (!spellInfo)
            {
                TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_pet_auras` does not exist.", spell);
                continue;
            }
            if (spellInfo->GetEffect(eff).Effect != SPELL_EFFECT_DUMMY &&
               (spellInfo->GetEffect(eff).Effect != SPELL_EFFECT_APPLY_AURA ||
                spellInfo->GetEffect(eff).ApplyAuraName != SPELL_AURA_DUMMY))
            {
                TC_LOG_ERROR("spells", "The spell {} listed in `spell_pet_auras` does not have any dummy aura or dummy effect.", spell);
                continue;
            }

            SpellInfo const* spellInfo2 = GetSpellInfo(aura);
            if (!spellInfo2)
            {
                TC_LOG_ERROR("sql.sql", "The aura {} listed in `spell_pet_auras` does not exist.", aura);
                continue;
            }

            PetAura pa(pet, aura, spellInfo->GetEffect(eff).TargetA.GetTarget() == TARGET_UNIT_PET, spellInfo->GetEffect(eff).CalcValue());
            mSpellPetAuraMap[(spell<<8) + eff] = pa;
        }

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} spell pet auras in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

// Fill custom data about enchancments
void SpellMgr::LoadEnchantCustomAttr()
{
    uint32 oldMSTime = getMSTime();

    uint32 size = sSpellItemEnchantmentStore.GetNumRows();
    mEnchantCustomAttr.resize(size, false);

    uint32 count = 0;
    for (uint32 i = 0; i < GetSpellInfoStoreSize(); ++i)
    {
        SpellInfo const* spellInfo = GetSpellInfo(i);
        if (!spellInfo)
            continue;

        /// @todo find a better check
        if (!spellInfo->HasAttribute(SPELL_ATTR2_PRESERVE_ENCHANT_IN_ARENA) || !spellInfo->HasAttribute(SPELL_ATTR0_NOT_SHAPESHIFT))
            continue;

        for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
        {
            if (spellEffectInfo.Effect == SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY)
            {
                uint32 enchId = spellEffectInfo.MiscValue;
                SpellItemEnchantmentEntry const* ench = sSpellItemEnchantmentStore.LookupEntry(enchId);
                if (!ench)
                    continue;
                mEnchantCustomAttr[enchId] = true;
                ++count;
                break;
            }
        }
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} custom enchant attributes in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellEnchantProcData()
{
    uint32 oldMSTime = getMSTime();

    mSpellEnchantProcEventMap.clear();                             // need for reload case

    //                                                       0       1               2        3               4
    QueryResult result = WorldDatabase.Query("SELECT EnchantID, Chance, ProcsPerMinute, HitMask, AttributesMask FROM spell_enchant_proc_data");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 spell enchant proc event conditions. DB table `spell_enchant_proc_data` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 enchantId = fields[0].GetUInt32();

        SpellItemEnchantmentEntry const* ench = sSpellItemEnchantmentStore.LookupEntry(enchantId);
        if (!ench)
        {
            TC_LOG_ERROR("sql.sql", "The enchancment {} listed in `spell_enchant_proc_data` does not exist.", enchantId);
            continue;
        }

        SpellEnchantProcEntry spe;
        spe.Chance = fields[1].GetFloat();
        spe.ProcsPerMinute = fields[2].GetFloat();
        spe.HitMask = fields[3].GetUInt32();
        spe.AttributesMask = fields[4].GetUInt32();

        mSpellEnchantProcEventMap[enchantId] = spe;

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} enchant proc data definitions in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellLinked()
{
    uint32 oldMSTime = getMSTime();

    mSpellLinkedMap.clear();    // need for reload case

    //                                                0              1             2
    QueryResult result = WorldDatabase.Query("SELECT spell_trigger, spell_effect, type FROM spell_linked_spell");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 linked spells. DB table `spell_linked_spell` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        int32 trigger = fields[0].GetInt32();
        int32 effect = fields[1].GetInt32();
        int32 type = fields[2].GetUInt8();

        SpellInfo const* spellInfo = GetSpellInfo(abs(trigger));
        if (!spellInfo)
        {
            TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_linked_spell` does not exist.", abs(trigger));
            continue;
        }

        if (effect >= 0)
            for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
            {
                if (spellEffectInfo.CalcValue() == abs(effect))
                    TC_LOG_ERROR("sql.sql", "The spell {} Effect: {} listed in `spell_linked_spell` has same bp{} like effect (possible hack).", abs(trigger), abs(effect), uint32(spellEffectInfo.EffectIndex));
            }

        spellInfo = GetSpellInfo(abs(effect));
        if (!spellInfo)
        {
            TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_linked_spell` does not exist.", abs(effect));
            continue;
        }

        if (type) //we will find a better way when more types are needed
        {
            if (trigger > 0)
                trigger += SPELL_LINKED_MAX_SPELLS * type;
            else
                trigger -= SPELL_LINKED_MAX_SPELLS * type;
        }
        mSpellLinkedMap[trigger].push_back(effect);

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} linked spells in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadPetLevelupSpellMap()
{
    uint32 oldMSTime = getMSTime();

    mPetLevelupSpellMap.clear();                                   // need for reload case

    uint32 count = 0;
    uint32 family_count = 0;

    for (uint32 i = 0; i < sCreatureFamilyStore.GetNumRows(); ++i)
    {
        CreatureFamilyEntry const* creatureFamily = sCreatureFamilyStore.LookupEntry(i);
        if (!creatureFamily)                                     // not exist
            continue;

        for (uint8 j = 0; j < 2; ++j)
        {
            if (!creatureFamily->SkillLine[j])
                continue;

            std::vector<SkillLineAbilityEntry const*> const* skillLineAbilities = GetSkillLineAbilitiesBySkill(creatureFamily->SkillLine[j]);
            if (!skillLineAbilities)
                continue;

            for (SkillLineAbilityEntry const* skillLine : *skillLineAbilities)
            {
                if (skillLine->AcquireMethod != SKILL_LINE_ABILITY_LEARNED_ON_SKILL_LEARN)
                    continue;

                SpellInfo const* spell = GetSpellInfo(skillLine->Spell);
                if (!spell) // not exist or triggered or talent
                    continue;

                if (!spell->SpellLevel)
                    continue;

                PetLevelupSpellSet& spellSet = mPetLevelupSpellMap[creatureFamily->ID];
                if (spellSet.empty())
                    ++family_count;

                spellSet.insert(PetLevelupSpellSet::value_type(spell->SpellLevel, spell->Id));
                ++count;
            }
        }
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} pet levelup and default spells for {} families in {} ms", count, family_count, GetMSTimeDiffToNow(oldMSTime));
}

bool LoadPetDefaultSpells_helper(CreatureTemplate const* cInfo, PetDefaultSpellsEntry& petDefSpells)
{
    // skip empty list;
    bool have_spell = false;
    for (uint8 j = 0; j < MAX_CREATURE_SPELL_DATA_SLOT; ++j)
    {
        if (petDefSpells.spellid[j])
        {
            have_spell = true;
            break;
        }
    }
    if (!have_spell)
        return false;

    // remove duplicates with levelupSpells if any
    if (PetLevelupSpellSet const* levelupSpells = cInfo->family ? sSpellMgr->GetPetLevelupSpellList(cInfo->family) : nullptr)
    {
        for (uint8 j = 0; j < MAX_CREATURE_SPELL_DATA_SLOT; ++j)
        {
            if (!petDefSpells.spellid[j])
                continue;

            for (PetLevelupSpellSet::const_iterator itr = levelupSpells->begin(); itr != levelupSpells->end(); ++itr)
            {
                if (itr->second == petDefSpells.spellid[j])
                {
                    petDefSpells.spellid[j] = 0;
                    break;
                }
            }
        }
    }

    // skip empty list;
    have_spell = false;
    for (uint8 j = 0; j < MAX_CREATURE_SPELL_DATA_SLOT; ++j)
    {
        if (petDefSpells.spellid[j])
        {
            have_spell = true;
            break;
        }
    }

    return have_spell;
}

void SpellMgr::LoadPetDefaultSpells()
{
    uint32 oldMSTime = getMSTime();

    mPetDefaultSpellsMap.clear();

    uint32 countCreature = 0;
    uint32 countData = 0;

    CreatureTemplateContainer const& ctc = sObjectMgr->GetCreatureTemplates();
    for (auto const& creatureTemplatePair : ctc)
    {
        if (!creatureTemplatePair.second.PetSpellDataId)
            continue;

        // for creature with PetSpellDataId get default pet spells from dbc
        CreatureSpellDataEntry const* spellDataEntry = sCreatureSpellDataStore.LookupEntry(creatureTemplatePair.second.PetSpellDataId);
        if (!spellDataEntry)
            continue;

        int32 petSpellsId = -int32(creatureTemplatePair.second.PetSpellDataId);
        PetDefaultSpellsEntry petDefSpells;
        for (uint8 j = 0; j < MAX_CREATURE_SPELL_DATA_SLOT; ++j)
            petDefSpells.spellid[j] = spellDataEntry->Spells[j];

        if (LoadPetDefaultSpells_helper(&creatureTemplatePair.second, petDefSpells))
        {
            mPetDefaultSpellsMap[petSpellsId] = petDefSpells;
            ++countData;
        }
    }

    TC_LOG_INFO("server.loading", ">> Loaded addition spells for {} pet spell data entries in {} ms", countData, GetMSTimeDiffToNow(oldMSTime));

    TC_LOG_INFO("server.loading", "Loading summonable creature templates...");
    oldMSTime = getMSTime();

    // different summon spells
    for (uint32 i = 0; i < GetSpellInfoStoreSize(); ++i)
    {
        SpellInfo const* spellEntry = GetSpellInfo(i);
        if (!spellEntry)
            continue;

        for (SpellEffectInfo const& spellEffectInfo : spellEntry->GetEffects())
        {
            if (spellEffectInfo.IsEffect(SPELL_EFFECT_SUMMON) || spellEffectInfo.IsEffect(SPELL_EFFECT_SUMMON_PET))
            {
                uint32 creature_id = spellEffectInfo.MiscValue;
                CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(creature_id);
                if (!cInfo)
                    continue;

                // already loaded
                if (cInfo->PetSpellDataId)
                    continue;

                // for creature without PetSpellDataId get default pet spells from creature_template
                int32 petSpellsId = cInfo->Entry;
                if (mPetDefaultSpellsMap.find(cInfo->Entry) != mPetDefaultSpellsMap.end())
                    continue;

                PetDefaultSpellsEntry petDefSpells;
                for (uint8 j = 0; j < MAX_CREATURE_SPELL_DATA_SLOT; ++j)
                    petDefSpells.spellid[j] = cInfo->spells[j];

                if (LoadPetDefaultSpells_helper(cInfo, petDefSpells))
                {
                    mPetDefaultSpellsMap[petSpellsId] = petDefSpells;
                    ++countCreature;
                }
            }
        }
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} summonable creature templates in {} ms", countCreature, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellAreas()
{
    uint32 oldMSTime = getMSTime();

    mSpellAreaMap.clear();                                  // need for reload case
    mSpellAreaForAreaMap.clear();
    mSpellAreaForQuestMap.clear();
    mSpellAreaForQuestEndMap.clear();
    mSpellAreaForAuraMap.clear();

    //                                                  0     1         2              3               4                 5          6          7       8         9
    QueryResult result = WorldDatabase.Query("SELECT spell, area, quest_start, quest_start_status, quest_end_status, quest_end, aura_spell, racemask, gender, autocast FROM spell_area");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 spell area requirements. DB table `spell_area` is empty.");

        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 spell = fields[0].GetUInt32();
        SpellArea spellArea;
        spellArea.spellId             = spell;
        spellArea.areaId              = fields[1].GetUInt32();
        spellArea.questStart          = fields[2].GetUInt32();
        spellArea.questStartStatus    = fields[3].GetUInt32();
        spellArea.questEndStatus      = fields[4].GetUInt32();
        spellArea.questEnd            = fields[5].GetUInt32();
        spellArea.auraSpell           = fields[6].GetInt32();
        spellArea.raceMask            = fields[7].GetUInt32();
        spellArea.gender              = Gender(fields[8].GetUInt8());
        spellArea.autocast            = fields[9].GetBool();

        if (SpellInfo const* spellInfo = GetSpellInfo(spell))
        {
            if (spellArea.autocast)
                const_cast<SpellInfo*>(spellInfo)->Attributes |= SPELL_ATTR0_CANT_CANCEL;
        }
        else
        {
            TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_area` does not exist", spell);
            continue;
        }

        {
            bool ok = true;
            SpellAreaMapBounds sa_bounds = GetSpellAreaMapBounds(spellArea.spellId);
            for (SpellAreaMap::const_iterator itr = sa_bounds.first; itr != sa_bounds.second; ++itr)
            {
                if (spellArea.spellId != itr->second.spellId)
                    continue;
                if (spellArea.areaId != itr->second.areaId)
                    continue;
                if (spellArea.questStart != itr->second.questStart)
                    continue;
                if (spellArea.auraSpell != itr->second.auraSpell)
                    continue;
                if ((spellArea.raceMask & itr->second.raceMask) == 0)
                    continue;
                if (spellArea.gender != itr->second.gender)
                    continue;

                // duplicate by requirements
                ok = false;
                break;
            }

            if (!ok)
            {
                TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_area` is already listed with similar requirements.", spell);
                continue;
            }
        }

        if (spellArea.areaId && !sAreaTableStore.LookupEntry(spellArea.areaId))
        {
            TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_area` has a wrong area ({}) requirement.", spell, spellArea.areaId);
            continue;
        }

        if (spellArea.questStart && !sObjectMgr->GetQuestTemplate(spellArea.questStart))
        {
            TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_area` has a wrong start quest ({}) requirement.", spell, spellArea.questStart);
            continue;
        }

        if (spellArea.questEnd)
        {
            if (!sObjectMgr->GetQuestTemplate(spellArea.questEnd))
            {
                TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_area` has a wrong ending quest ({}) requirement.", spell, spellArea.questEnd);
                continue;
            }
        }

        if (spellArea.auraSpell)
        {
            SpellInfo const* spellInfo = GetSpellInfo(abs(spellArea.auraSpell));
            if (!spellInfo)
            {
                TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_area` has wrong aura spell ({}) requirement", spell, abs(spellArea.auraSpell));
                continue;
            }

            if (uint32(abs(spellArea.auraSpell)) == spellArea.spellId)
            {
                TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_area` has aura spell ({}) requirement for itself", spell, abs(spellArea.auraSpell));
                continue;
            }

            // not allow autocast chains by auraSpell field (but allow use as alternative if not present)
            if (spellArea.autocast && spellArea.auraSpell > 0)
            {
                bool chain = false;
                SpellAreaForAuraMapBounds saBound = GetSpellAreaForAuraMapBounds(spellArea.spellId);
                for (SpellAreaForAuraMap::const_iterator itr = saBound.first; itr != saBound.second; ++itr)
                {
                    if (itr->second->autocast && itr->second->auraSpell > 0)
                    {
                        chain = true;
                        break;
                    }
                }

                if (chain)
                {
                    TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_area` has the aura spell ({}) requirement that it autocasts itself from the aura.", spell, spellArea.auraSpell);
                    continue;
                }

                SpellAreaMapBounds saBound2 = GetSpellAreaMapBounds(spellArea.auraSpell);
                for (SpellAreaMap::const_iterator itr2 = saBound2.first; itr2 != saBound2.second; ++itr2)
                {
                    if (itr2->second.autocast && itr2->second.auraSpell > 0)
                    {
                        chain = true;
                        break;
                    }
                }

                if (chain)
                {
                    TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_area` has the aura spell ({}) requirement that the spell itself autocasts from the aura.", spell, spellArea.auraSpell);
                    continue;
                }
            }
        }

        if (spellArea.raceMask && (spellArea.raceMask & RACEMASK_ALL_PLAYABLE) == 0)
        {
            TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_area` has wrong race mask ({}) requirement.", spell, spellArea.raceMask);
            continue;
        }

        if (spellArea.gender != GENDER_NONE && spellArea.gender != GENDER_FEMALE && spellArea.gender != GENDER_MALE)
        {
            TC_LOG_ERROR("sql.sql", "The spell {} listed in `spell_area` has wrong gender ({}) requirement.", spell, spellArea.gender);
            continue;
        }

        SpellArea const* sa = &mSpellAreaMap.insert(SpellAreaMap::value_type(spell, spellArea))->second;

        // for search by current zone/subzone at zone/subzone change
        if (spellArea.areaId)
            mSpellAreaForAreaMap.insert(SpellAreaForAreaMap::value_type(spellArea.areaId, sa));

        // for search at quest update checks
        if (spellArea.questStart || spellArea.questEnd)
        {
            if (spellArea.questStart == spellArea.questEnd)
                mSpellAreaForQuestMap.insert(SpellAreaForQuestMap::value_type(spellArea.questStart, sa));
            else
            {
                if (spellArea.questStart)
                    mSpellAreaForQuestMap.insert(SpellAreaForQuestMap::value_type(spellArea.questStart, sa));
                if (spellArea.questEnd)
                    mSpellAreaForQuestMap.insert(SpellAreaForQuestMap::value_type(spellArea.questEnd, sa));
            }
        }

        // for search at quest start/reward
        if (spellArea.questEnd)
            mSpellAreaForQuestEndMap.insert(SpellAreaForQuestMap::value_type(spellArea.questEnd, sa));

        // for search at aura apply
        if (spellArea.auraSpell)
            mSpellAreaForAuraMap.insert(SpellAreaForAuraMap::value_type(abs(spellArea.auraSpell), sa));

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} spell area requirements in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellInfoStore()
{
    uint32 oldMSTime = getMSTime();

    UnloadSpellInfoStore();
    mSpellInfoMap.resize(sSpellStore.GetNumRows(), nullptr);

    for (SpellEntry const* spellEntry : sSpellStore)
        mSpellInfoMap[spellEntry->ID] = new SpellInfo(spellEntry);

    for (uint32 spellIndex = 0; spellIndex < GetSpellInfoStoreSize(); ++spellIndex)
    {
        if (!mSpellInfoMap[spellIndex])
            continue;

        for (SpellEffectInfo const& spellEffectInfo : mSpellInfoMap[spellIndex]->GetEffects())
        {
            //ASSERT(effect.EffectIndex < MAX_SPELL_EFFECTS, "MAX_SPELL_EFFECTS must be at least %u", effect.EffectIndex + 1);
            ASSERT(spellEffectInfo.Effect < TOTAL_SPELL_EFFECTS, "TOTAL_SPELL_EFFECTS must be at least %u", spellEffectInfo.Effect + 1);
            ASSERT(spellEffectInfo.ApplyAuraName < TOTAL_AURAS, "TOTAL_AURAS must be at least %u", spellEffectInfo.ApplyAuraName + 1);
            ASSERT(spellEffectInfo.TargetA.GetTarget() < TOTAL_SPELL_TARGETS, "TOTAL_SPELL_TARGETS must be at least %u", spellEffectInfo.TargetA.GetTarget() + 1);
            ASSERT(spellEffectInfo.TargetB.GetTarget() < TOTAL_SPELL_TARGETS, "TOTAL_SPELL_TARGETS must be at least %u", spellEffectInfo.TargetB.GetTarget() + 1);
        }
    }

    TC_LOG_INFO("server.loading", ">> Loaded SpellInfo store in {} ms", GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::UnloadSpellInfoStore()
{
    for (uint32 i = 0; i < GetSpellInfoStoreSize(); ++i)
        delete mSpellInfoMap[i];

    mSpellInfoMap.clear();
}

void SpellMgr::UnloadSpellInfoImplicitTargetConditionLists()
{
    for (uint32 i = 0; i < GetSpellInfoStoreSize(); ++i)
        if (mSpellInfoMap[i])
            mSpellInfoMap[i]->_UnloadImplicitTargetConditionLists();
}

void SpellMgr::LoadSpellInfoCustomAttributes()
{
    uint32 oldMSTime = getMSTime();
    uint32 oldMSTime2 = oldMSTime;

    QueryResult result = WorldDatabase.Query("SELECT entry, attributes FROM spell_custom_attr");

    if (!result)
        TC_LOG_INFO("server.loading", ">> Loaded 0 spell custom attributes from DB. DB table `spell_custom_attr` is empty.");
    else
    {
        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            uint32 spellId = fields[0].GetUInt32();
            uint32 attributes = fields[1].GetUInt32();

            SpellInfo* spellInfo = _GetSpellInfo(spellId);
            if (!spellInfo)
            {
                TC_LOG_ERROR("sql.sql", "Table `spell_custom_attr` has wrong spell (entry: {}), ignored.", spellId);
                continue;
            }

            if ((attributes & SPELL_ATTR0_CU_NEGATIVE) != 0)
            {
                for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
                {
                    if (spellEffectInfo.IsEffect())
                        continue;

                    if ((attributes & (SPELL_ATTR0_CU_NEGATIVE_EFF0 << spellEffectInfo.EffectIndex)) != 0)
                    {
                        TC_LOG_ERROR("sql.sql", "Table `spell_custom_attr` has attribute SPELL_ATTR0_CU_NEGATIVE_EFF{} for spell {} with no EFFECT_{}", uint32(spellEffectInfo.EffectIndex), spellId, uint32(spellEffectInfo.EffectIndex));
                        continue;
                    }
                }
            }

            spellInfo->AttributesCu |= attributes;
            ++count;
        } while (result->NextRow());

        TC_LOG_INFO("server.loading", ">> Loaded {} spell custom attributes from DB in {} ms", count, GetMSTimeDiffToNow(oldMSTime2));
    }

    for (SpellInfo* spellInfo : mSpellInfoMap)
    {
        if (!spellInfo)
            continue;

        for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
        {
            // all bleed effects and spells ignore armor
            if (spellInfo->GetEffectMechanicMask(spellEffectInfo.EffectIndex) & (1 << MECHANIC_BLEED))
                spellInfo->AttributesCu |= SPELL_ATTR0_CU_IGNORE_ARMOR;

            switch (spellEffectInfo.ApplyAuraName)
            {
                case SPELL_AURA_MOD_POSSESS:
                case SPELL_AURA_MOD_CONFUSE:
                case SPELL_AURA_MOD_CHARM:
                case SPELL_AURA_AOE_CHARM:
                case SPELL_AURA_MOD_FEAR:
                case SPELL_AURA_MOD_STUN:
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_AURA_CC;
                    break;
                default:
                    break;
            }

            switch (spellEffectInfo.ApplyAuraName)
            {
                case SPELL_AURA_CONVERT_RUNE:   // Can't be saved - aura handler relies on calculated amount and changes it
                case SPELL_AURA_OPEN_STABLE:    // No point in saving this, since the stable dialog can't be open on aura load anyway.
                // Auras that require both caster & target to be in world cannot be saved
                case SPELL_AURA_CONTROL_VEHICLE:
                case SPELL_AURA_BIND_SIGHT:
                case SPELL_AURA_MOD_POSSESS:
                case SPELL_AURA_MOD_POSSESS_PET:
                case SPELL_AURA_MOD_CHARM:
                case SPELL_AURA_AOE_CHARM:
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_AURA_CANNOT_BE_SAVED;
                    break;
                default:
                    break;
            }

            switch (spellEffectInfo.Effect)
            {
                case SPELL_EFFECT_SCHOOL_DAMAGE:
                case SPELL_EFFECT_HEALTH_LEECH:
                case SPELL_EFFECT_HEAL:
                case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
                case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
                case SPELL_EFFECT_WEAPON_DAMAGE:
                case SPELL_EFFECT_POWER_BURN:
                case SPELL_EFFECT_HEAL_MECHANICAL:
                case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
                case SPELL_EFFECT_HEAL_PCT:
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_CAN_CRIT;
                    break;
                default:
                    break;
            }

            switch (spellEffectInfo.Effect)
            {
                case SPELL_EFFECT_SCHOOL_DAMAGE:
                case SPELL_EFFECT_WEAPON_DAMAGE:
                case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
                case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
                case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
                case SPELL_EFFECT_HEAL:
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_DIRECT_DAMAGE;
                    break;
                case SPELL_EFFECT_POWER_DRAIN:
                case SPELL_EFFECT_POWER_BURN:
                case SPELL_EFFECT_HEAL_MAX_HEALTH:
                case SPELL_EFFECT_HEALTH_LEECH:
                case SPELL_EFFECT_HEAL_PCT:
                case SPELL_EFFECT_ENERGIZE_PCT:
                case SPELL_EFFECT_ENERGIZE:
                case SPELL_EFFECT_HEAL_MECHANICAL:
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_NO_INITIAL_THREAT;
                    break;
                case SPELL_EFFECT_CHARGE:
                case SPELL_EFFECT_CHARGE_DEST:
                case SPELL_EFFECT_JUMP:
                case SPELL_EFFECT_JUMP_DEST:
                case SPELL_EFFECT_LEAP_BACK:
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_CHARGE;
                    break;
                case SPELL_EFFECT_PICKPOCKET:
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_PICKPOCKET;
                    break;
                case SPELL_EFFECT_ENCHANT_ITEM:
                case SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY:
                case SPELL_EFFECT_ENCHANT_ITEM_PRISMATIC:
                case SPELL_EFFECT_ENCHANT_HELD_ITEM:
                {
                    // only enchanting profession enchantments procs can stack
                    if (IsPartOfSkillLine(SKILL_ENCHANTING, spellInfo->Id))
                    {
                        uint32 enchantId = spellEffectInfo.MiscValue;
                        SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
                        if (!enchant)
                            break;

                        for (uint8 s = 0; s < MAX_ITEM_ENCHANTMENT_EFFECTS; ++s)
                        {
                            if (enchant->Effect[s] != ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL)
                                continue;

                            SpellInfo* procInfo = _GetSpellInfo(enchant->EffectArg[s]);
                            if (!procInfo)
                                continue;

                            // if proced directly from enchantment, not via proc aura
                            // NOTE: Enchant Weapon - Blade Ward also has proc aura spell and is proced directly
                            // however its not expected to stack so this check is good
                            if (procInfo->HasAura(SPELL_AURA_PROC_TRIGGER_SPELL))
                                continue;

                            procInfo->AttributesCu |= SPELL_ATTR0_CU_ENCHANT_PROC;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }

        // spells ignoring hit result should not be binary
        if (!spellInfo->HasAttribute(SPELL_ATTR3_IGNORE_HIT_RESULT))
        {
            bool setFlag = false;
            for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
            {
                if (spellEffectInfo.IsEffect())
                {
                    switch (spellEffectInfo.Effect)
                    {
                        case SPELL_EFFECT_SCHOOL_DAMAGE:
                        case SPELL_EFFECT_WEAPON_DAMAGE:
                        case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
                        case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
                        case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
                        case SPELL_EFFECT_TRIGGER_SPELL:
                        case SPELL_EFFECT_TRIGGER_SPELL_WITH_VALUE:
                            break;
                        case SPELL_EFFECT_PERSISTENT_AREA_AURA:
                        case SPELL_EFFECT_APPLY_AURA:
                        case SPELL_EFFECT_APPLY_AREA_AURA_PARTY:
                        case SPELL_EFFECT_APPLY_AREA_AURA_RAID:
                        case SPELL_EFFECT_APPLY_AREA_AURA_FRIEND:
                        case SPELL_EFFECT_APPLY_AREA_AURA_ENEMY:
                        case SPELL_EFFECT_APPLY_AREA_AURA_PET:
                        case SPELL_EFFECT_APPLY_AREA_AURA_OWNER:
                        {
                            if (spellEffectInfo.ApplyAuraName == SPELL_AURA_PERIODIC_DAMAGE ||
                                spellEffectInfo.ApplyAuraName == SPELL_AURA_PERIODIC_DAMAGE_PERCENT ||
                                spellEffectInfo.ApplyAuraName == SPELL_AURA_DUMMY ||
                                spellEffectInfo.ApplyAuraName == SPELL_AURA_PERIODIC_LEECH ||
                                spellEffectInfo.ApplyAuraName == SPELL_AURA_PERIODIC_HEALTH_FUNNEL ||
                                spellEffectInfo.ApplyAuraName == SPELL_AURA_PERIODIC_DUMMY)
                                break;
                            [[fallthrough]];
                        }
                        default:
                        {
                            // No value and not interrupt cast or crowd control without SPELL_ATTR0_UNAFFECTED_BY_INVULNERABILITY flag
                            if (!spellEffectInfo.CalcValue() && !((spellEffectInfo.Effect == SPELL_EFFECT_INTERRUPT_CAST || spellInfo->HasAttribute(SPELL_ATTR0_CU_AURA_CC)) && !spellInfo->HasAttribute(SPELL_ATTR0_UNAFFECTED_BY_INVULNERABILITY)))
                                break;

                            // Sindragosa Frost Breath
                            if (spellInfo->Id == 69649 || spellInfo->Id == 71056 || spellInfo->Id == 71057 || spellInfo->Id == 71058 || spellInfo->Id == 73061 || spellInfo->Id == 73062 || spellInfo->Id == 73063 || spellInfo->Id == 73064)
                                break;

                            // Frostbolt
                            if (spellInfo->SpellFamilyName == SPELLFAMILY_MAGE && (spellInfo->SpellFamilyFlags[0] & 0x20))
                                break;

                            // Frost Fever
                            if (spellInfo->Id == 55095)
                                break;

                            // Haunt
                            if (spellInfo->SpellFamilyName == SPELLFAMILY_WARLOCK && (spellInfo->SpellFamilyFlags[1] & 0x40000))
                                break;

                            setFlag = true;
                            break;
                        }
                    }

                    if (setFlag)
                    {
                        spellInfo->AttributesCu |= SPELL_ATTR0_CU_BINARY_SPELL;
                        break;
                    }
                }
            }
        }

        // Remove normal school mask to properly calculate damage
        if ((spellInfo->SchoolMask & SPELL_SCHOOL_MASK_NORMAL) && (spellInfo->SchoolMask & SPELL_SCHOOL_MASK_MAGIC))
        {
            spellInfo->SchoolMask &= ~SPELL_SCHOOL_MASK_NORMAL;
            spellInfo->AttributesCu |= SPELL_ATTR0_CU_SCHOOLMASK_NORMAL_WITH_MAGIC;
        }

        spellInfo->_InitializeSpellPositivity();

        if (spellInfo->SpellVisual[0] == 3879)
            spellInfo->AttributesCu |= SPELL_ATTR0_CU_CONE_BACK;

        switch (spellInfo->SpellFamilyName)
        {
            case SPELLFAMILY_WARRIOR:
                // Shout / Piercing Howl
                if (spellInfo->SpellFamilyFlags[0] & 0x20000/* || spellInfo->SpellFamilyFlags[1] & 0x20*/)
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_AURA_CC;
                break;
            case SPELLFAMILY_DRUID:
                // Roar
                if (spellInfo->SpellFamilyFlags[0] & 0x8)
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_AURA_CC;
                break;
            case SPELLFAMILY_GENERIC:
                // Stoneclaw Totem effect
                if (spellInfo->Id == 5729)
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_AURA_CC;
                break;
            default:
                break;
        }

        spellInfo->_InitializeExplicitTargetMask();

        if (spellInfo->Speed > 0.0f)
            if (SpellVisualEntry const* spellVisual = sSpellVisualStore.LookupEntry(spellInfo->SpellVisual[0]))
                if (spellVisual->HasMissile)
                    if (spellVisual->MissileModel == -4 || spellVisual->MissileModel == -5)
                        spellInfo->AttributesCu |= SPELL_ATTR0_CU_NEEDS_AMMO_DATA;

        // Saving to DB happens before removing from world - skip saving these auras
        if (spellInfo->HasAuraInterruptFlag(SpellAuraInterruptFlags::LeaveWorld))
            spellInfo->AttributesCu |= SPELL_ATTR0_CU_AURA_CANNOT_BE_SAVED;
    }

    // addition for binary spells, omit spells triggering other spells
    for (SpellInfo* spellInfo : mSpellInfoMap)
    {
        if (!spellInfo)
            continue;

        if (spellInfo->HasAttribute(SPELL_ATTR0_CU_BINARY_SPELL))
            continue;

        bool allNonBinary = true;
        bool overrideAttr = false;
        for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
        {
            if (spellEffectInfo.IsAura() && spellEffectInfo.TriggerSpell)
            {
                switch (spellEffectInfo.ApplyAuraName)
                {
                    case SPELL_AURA_PERIODIC_TRIGGER_SPELL:
                    case SPELL_AURA_PERIODIC_TRIGGER_SPELL_FROM_CLIENT:
                    case SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE:
                        if (SpellInfo const* triggerSpell = sSpellMgr->GetSpellInfo(spellEffectInfo.TriggerSpell))
                        {
                            overrideAttr = true;
                            if (triggerSpell->HasAttribute(SPELL_ATTR0_CU_BINARY_SPELL))
                                allNonBinary = false;
                        }
                        break;
                    default:
                        break;
                }
            }
        }

        if (overrideAttr && allNonBinary)
            spellInfo->AttributesCu &= ~SPELL_ATTR0_CU_BINARY_SPELL;
    }

    // remove attribute from spells that can't crit
    for (SpellInfo* spellInfo : mSpellInfoMap)
    {
        if (!spellInfo)
            continue;

        if (spellInfo->HasAttribute(SPELL_ATTR2_CANT_CRIT))
            spellInfo->AttributesCu &= ~SPELL_ATTR0_CU_CAN_CRIT;
    }

    // add custom attribute to liquid auras
    for (LiquidTypeEntry const* liquid : sLiquidTypeStore)
    {
        if (uint32 spellId = liquid->SpellID)
            if (SpellInfo* spellInfo = _GetSpellInfo(spellId))
                spellInfo->AttributesCu |= SPELL_ATTR0_CU_AURA_CANNOT_BE_SAVED;
    }

    TC_LOG_INFO("server.loading", ">> Loaded SpellInfo custom attributes in {} ms", GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellInfoSpellSpecificAndAuraState()
{
    uint32 oldMSTime = getMSTime();

    for (SpellInfo* spellInfo : mSpellInfoMap)
    {
        if (!spellInfo)
            continue;

        // AuraState depends on SpellSpecific
        spellInfo->_LoadSpellSpecific();
        spellInfo->_LoadAuraState();
    }

    TC_LOG_INFO("server.loading", ">> Loaded SpellInfo SpellSpecific and AuraState in {} ms", GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellInfoDiminishing()
{
    uint32 oldMSTime = getMSTime();

    for (SpellInfo* spellInfo : mSpellInfoMap)
    {
        if (!spellInfo)
            continue;

        spellInfo->_LoadSpellDiminishInfo();
    }

    TC_LOG_INFO("server.loading", ">> Loaded SpellInfo diminishing infos in {} ms", GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellInfoImmunities()
{
    uint32 oldMSTime = getMSTime();

    for (SpellInfo* spellInfo : mSpellInfoMap)
    {
        if (!spellInfo)
            continue;

        spellInfo->_LoadImmunityInfo();
    }

    TC_LOG_INFO("server.loading", ">> Loaded SpellInfo immunity infos in {} ms", GetMSTimeDiffToNow(oldMSTime));
}
