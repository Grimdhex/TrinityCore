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

#include "DBCStores.h"
#include "MovementDefines.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

inline void ApplySpellFix(std::initializer_list<uint32> spellIds, void(*fix)(SpellInfo*))
{
    for (uint32 spellId : spellIds)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
        {
            TC_LOG_ERROR("server.loading", "Spell info correction specified for non-existing spell {}", spellId);
            continue;
        }

        fix(const_cast<SpellInfo*>(spellInfo));
    }
}

void SpellMgr::LoadSpellInfoCorrections()
{
    uint32 oldMSTime = getMSTime();

    // Some spells have no amplitude set
    {
        ApplySpellFix({
            6727,  // Poison Mushroom
            7288,  // Immolate Cumulative (TEST) (Rank 1)
            7291,  // Food (TEST)
            7331,  // Healing Aura (TEST) (Rank 1)
            /*
            30400, // Nether Beam - Perseverance
                Blizzlike to have it disabled? DBC says:
                "This is currently turned off to increase performance. Enable this to make it fire more frequently."
            */
            34589, // Dangerous Water
            52562, // Arthas Zombie Catcher
            57550, // Tirion Aggro
            65755
        }, [](SpellInfo* spellInfo)
        {
            spellInfo->_GetEffect(EFFECT_0).Amplitude = 1 * IN_MILLISECONDS;
        });

        ApplySpellFix({
            24707, // Food
            26263, // Dim Sum
            29055, // Refreshing Red Apple
            37504  // Karazhan - Chess NPC AI, action timer
        }, [](SpellInfo* spellInfo)
        {
            // first effect has correct amplitude
            spellInfo->_GetEffect(EFFECT_1).Amplitude = spellInfo->GetEffect(EFFECT_0).Amplitude;
        });

        // Vomit
        ApplySpellFix({ 43327 }, [](SpellInfo* spellInfo)
        {
            spellInfo->_GetEffect(EFFECT_1).Amplitude = 1 * IN_MILLISECONDS;
        });

        // Strider Presence
        ApplySpellFix({ 4312 }, [](SpellInfo* spellInfo)
        {
            spellInfo->_GetEffect(EFFECT_0).Amplitude = 1 * IN_MILLISECONDS;
            spellInfo->_GetEffect(EFFECT_1).Amplitude = 1 * IN_MILLISECONDS;
        });

        // Food
        ApplySpellFix({ 64345 }, [](SpellInfo* spellInfo)
        {
            spellInfo->_GetEffect(EFFECT_0).Amplitude = 1 * IN_MILLISECONDS;
            spellInfo->_GetEffect(EFFECT_2).Amplitude = 1 * IN_MILLISECONDS;
        });
    }

    // specific code for cases with no trigger spell provided in field
    {
        // Brood Affliction: Bronze
        ApplySpellFix({ 23170 }, [](SpellInfo* spellInfo)
        {
            spellInfo->_GetEffect(EFFECT_0).TriggerSpell = 23171;
        });

        // Feed Captured Animal
        ApplySpellFix({ 29917 }, [](SpellInfo* spellInfo)
        {
            spellInfo->_GetEffect(EFFECT_0).TriggerSpell = 29916;
        });

        // Remote Toy
        ApplySpellFix({ 37027 }, [](SpellInfo* spellInfo)
        {
            spellInfo->_GetEffect(EFFECT_0).TriggerSpell = 37029;
        });

        // Eye of Grillok
        ApplySpellFix({ 38495 }, [](SpellInfo* spellInfo)
        {
            spellInfo->_GetEffect(EFFECT_0).TriggerSpell = 38530;
        });

        // Tear of Azzinoth Summon Channel - it's not really supposed to do anything, and this only prevents the console spam
        ApplySpellFix({ 39857 }, [](SpellInfo* spellInfo)
        {
            spellInfo->_GetEffect(EFFECT_0).TriggerSpell = 39856;
        });

        // Personalized Weather
        ApplySpellFix({ 46736 }, [](SpellInfo* spellInfo)
        {
            spellInfo->_GetEffect(EFFECT_1).TriggerSpell = 46737;
        });
    }

    // this one is here because we have no SP bonus for dmgclass none spell
    // but this one should since it's DBC data
    ApplySpellFix({
        52042, // Healing Stream Totem
    }, [](SpellInfo* spellInfo)
    {
        // We need more spells to find a general way (if there is any)
        spellInfo->DmgClass = SPELL_DAMAGE_CLASS_MAGIC;
    });

    // Spell Reflection
    ApplySpellFix({ 57643 }, [](SpellInfo* spellInfo)
    {
        spellInfo->EquippedItemClass = -1;
    });

    ApplySpellFix({
        63026, // Force Cast (HACK: Target shouldn't be changed)
        63137  // Force Cast (HACK: Target shouldn't be changed; summon position should be untied from spell destination)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_DEST_DB);
    });

    // Immolate
    ApplySpellFix({
        348,
        707,
        1094,
        2941,
        11665,
        11667,
        11668,
        25309,
        27215,
        47810,
        47811
        }, [](SpellInfo* spellInfo)
    {
        // copy SP scaling data from direct damage to DoT
        spellInfo->_GetEffect(EFFECT_0).BonusMultiplier = spellInfo->GetEffect(EFFECT_1).BonusMultiplier;
    });

    // Detect Undead
    ApplySpellFix({ 11389 }, [](SpellInfo* spellInfo)
    {
        spellInfo->PowerType = POWER_MANA;
        spellInfo->ManaCost = 0;
        spellInfo->ManaPerSecond = 0;
    });

    // Drink! (Brewfest)
    ApplySpellFix({ 42436 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_UNIT_TARGET_ANY);
    });

    // Warsong Gulch Anti-Stall Debuffs
    ApplySpellFix({
        46392, // Focused Assault
        46393, // Brutal Assault
    }, [](SpellInfo* spellInfo)
    {
        // due to discrepancies between ranks
        spellInfo->Attributes |= SPELL_ATTR0_UNAFFECTED_BY_INVULNERABILITY;
    });

    // Summon Skeletons
    ApplySpellFix({ 52611, 52612 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).MiscValueB = 64;
    });

    // Battlegear of Eternal Justice
    ApplySpellFix({
        26135, // Battlegear of Eternal Justice
        37557  // Mark of Light
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->SpellFamilyFlags = flag96();
    });

    ApplySpellFix({
        40244, // Simon Game Visual
        40245, // Simon Game Visual
        40246, // Simon Game Visual
        40247, // Simon Game Visual
        42835  // Spout, remove damage effect, only anim is needed
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).Effect = SPELL_EFFECT_NONE;
    });

    ApplySpellFix({
        63665, // Charge (Argent Tournament emote on riders)
        31298, // Sleep (needs target selection script)
        51904, // Summon Ghouls On Scarlet Crusade (this should use conditions table, script for this spell needs to be fixed)
        2895,  // Wrath of Air Totem rank 1 (Aura)
        68933, // Wrath of Air Totem rank 2 (Aura)
        29200, // Purify Helboar Meat
        10872, // Abolish Disease Effect
        3137   // Abolish Poison Effect
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
        spellInfo->_GetEffect(EFFECT_0).TargetB = SpellImplicitTargetInfo();
    });

    ApplySpellFix({
        56690, // Thrust Spear
        60586, // Mighty Spear Thrust
        60776, // Claw Swipe
        60881, // Fatal Strike
        60864  // Jaws of Death
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx4 |= SPELL_ATTR4_FIXED_DAMAGE;
    });

    // Missile Barrage
    ApplySpellFix({ 44401 }, [](SpellInfo* spellInfo)
    {
        // should be consumed before Clearcasting
        spellInfo->Priority = 100;
    });

    // Howl of Azgalor
    ApplySpellFix({ 31344 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_100_YARDS); // 100yards instead of 50000?!
    });

    ApplySpellFix({
        42818, // Headless Horseman - Wisp Flight Port
        42821, // Headless Horseman - Wisp Flight Missile
        17678  // Despawn Spectral Combatants
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(6); // 100 yards
    });

    // They Must Burn Bomb Aura (self)
    ApplySpellFix({ 36350 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TriggerSpell = 36325; // They Must Burn Bomb Drop (DND)
    });

    ApplySpellFix({
        61407, // Energize Cores
        62136, // Energize Cores
        54069, // Energize Cores
        56251  // Energize Cores
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_UNIT_SRC_AREA_ENTRY);
    });

    ApplySpellFix({
        50785, // Energize Cores
        59372  // Energize Cores
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_UNIT_SRC_AREA_ENEMY);
    });

    // Mana Shield (rank 2)
    ApplySpellFix({ 8494 }, [](SpellInfo* spellInfo)
    {
        // because of bug in dbc
        spellInfo->ProcChance = 0;
    });

    // Maelstrom Weapon
    ApplySpellFix({
        51528, // (Rank 1)
        51529, // (Rank 2)
        51530, // (Rank 3)
        51531, // (Rank 4)
        51532  // (Rank 5)
    }, [](SpellInfo* spellInfo)
    {
        // due to discrepancies between ranks
        spellInfo->EquippedItemSubClassMask = 0x0000FC33;
        spellInfo->AttributesEx3 |= SPELL_ATTR3_CAN_PROC_WITH_TRIGGERED;
    });

    ApplySpellFix({
        20335, // Heart of the Crusader
        20336,
        20337,
        53228, // Rapid Killing (Rank 1)
        53232, // Rapid Killing (Rank 2)
        63320  // Glyph of Life Tap
    }, [](SpellInfo* spellInfo)
    {
        // Entries were not updated after spell effect change, we have to do that manually :/
        spellInfo->AttributesEx3 |= SPELL_ATTR3_CAN_PROC_WITH_TRIGGERED;
    });

    ApplySpellFix({
        51627, // Turn the Tables (Rank 1)
        51628, // Turn the Tables (Rank 2)
        51629  // Turn the Tables (Rank 3)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 |= SPELL_ATTR3_STACK_FOR_DIFF_CASTERS;
    });

    ApplySpellFix({
        52910, // Turn the Tables
        52914, // Turn the Tables
        52915  // Turn the Tables
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
    });

    // Magic Absorption
    ApplySpellFix({
        29441, // (Rank 1)
        29444  // (Rank 2)
    }, [](SpellInfo* spellInfo)
    {
        // Caused off by 1 calculation (ie 79 resistance at level 80)
        spellInfo->SpellLevel = 0;
    });

    // Execute
    ApplySpellFix({
        5308,  // (Rank 1)
        20658, // (Rank 2)
        20660, // (Rank 3)
        20661, // (Rank 4)
        20662, // (Rank 5)
        25234, // (Rank 6)
        25236, // (Rank 7)
        47470, // (Rank 8)
        47471  // (Rank 9)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 |= SPELL_ATTR3_CANT_TRIGGER_PROC;
    });

    // Improved Spell Reflection - aoe aura
    ApplySpellFix({ 59725 }, [](SpellInfo* spellInfo)
    {
        // Target entry seems to be wrong for this spell :/
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER_AREA_PARTY);
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_20_YARDS);
    });

    ApplySpellFix({
        44978, // Wild Magic
        45001, // Wild Magic
        45002, // Wild Magic
        45004, // Wild Magic
        45006, // Wild Magic
        45010, // Wild Magic
        31347, // Doom
        41635, // Prayer of Mending
        44869, // Spectral Blast
        45027, // Revitalize
        45976, // Muru Portal Channel
        39365, // Thundering Storm
        41071, // Raise Dead (HACK)
        52124, // Sky Darkener Assault
        42442, // Vengeance Landing Cannonfire
        45863, // Cosmetic - Incinerate to Random Target
        25425, // Shoot
        45761, // Shoot
        42611, // Shoot
        61588, // Blazing Harpoon
        52479, // Gift of the Harvester
        48246, // Ball of Flame
        36327, // Shoot Arcane Explosion Arrow
        55479, // Force Obedience
        28560, // Summon Blizzard (Sapphiron)
        53096, // Quetz'lun's Judgment
        70743, // AoD Special
        70614, // AoD Special - Vegard
        4020,  // Safirdrang's Chill
        52438, // Summon Skittering Swarmer (Force Cast)
        52449, // Summon Skittering Infector (Force Cast)
        53609, // Summon Anub'ar Assassin (Force Cast)
        53457, // Summon Impale Trigger (AoE)
        45907, // Torch Target Picker
        52953, // Torch
        58121, // Torch
        43109, // Throw Torch
        58552, // Return to Orgrimmar
        58533, // Return to Stormwind
        21855, // Challenge Flag
        38762, // Force of Neltharaku
        51122, // Fierce Lightning Stike
        71848, // Toxic Wasteling Find Target
        36146, // Chains of Naberius
        33711, // Murmur's Touch
        38794  // Murmur's Touch
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->MaxAffectedTargets = 1;
    });

    ApplySpellFix({
        36384, // Skartax Purple Beam
        47731  // Critter
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->MaxAffectedTargets = 2;
    });

    ApplySpellFix({
        41376, // Spite
        39992, // Needle Spine
        29576, // Multi-Shot
        40816, // Saber Lash
        37790, // Spread Shot
        46771, // Flame Sear
        45248, // Shadow Blades
        41303, // Soul Drain
        54172, // Divine Storm (heal)
        29213, // Curse of the Plaguebringer - Noth
        28542, // Life Drain - Sapphiron
        66588, // Flaming Spear
        54171  // Divine Storm
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->MaxAffectedTargets = 3;
    });

    ApplySpellFix({
        38310, // Multi-Shot
        53385  // Divine Storm (Damage)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->MaxAffectedTargets = 4;
    });

    ApplySpellFix({
        42005, // Bloodboil
        38296, // Spitfire Totem
        37676, // Insidious Whisper
        46008, // Negative Energy
        45641, // Fire Bloom
        55665, // Life Drain - Sapphiron (H)
        28796, // Poison Bolt Volly - Faerlina
        37135  // Domination
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->MaxAffectedTargets = 5;
    });

    ApplySpellFix({
        40827, // Sinful Beam
        40859, // Sinister Beam
        40860, // Vile Beam
        40861, // Wicked Beam
        54098, // Poison Bolt Volly - Faerlina (H)
        54835  // Curse of the Plaguebringer - Noth (H)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->MaxAffectedTargets = 10;
    });

    ApplySpellFix({
        50312  // Unholy Frenzy
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->MaxAffectedTargets = 15;
    });

    ApplySpellFix({
        47977, // Magic Broom
        48025, // Headless Horseman's Mount
        54729, // Winged Steed of the Ebon Blade
        58983, // Big Blizzard Bear
        65917, // Magic Rooster
        71342, // Big Love Rocket
        72286, // Invincible
        74856, // Blazing Hippogryph
        75614, // Celestial Steed
        75973  // X-53 Touring Rocket
    }, [](SpellInfo* spellInfo)
    {
        // First two effects apply auras, which shouldn't be there
        // due to NO_TARGET applying aura on current caster (core bug)
        // Just wipe effect data, to mimic blizz-behavior
        spellInfo->_GetEffect(EFFECT_0).Effect = SPELL_EFFECT_NONE;
        spellInfo->_GetEffect(EFFECT_1).Effect = SPELL_EFFECT_NONE;
    });

    // Lock and Load (Rank 1)
    ApplySpellFix({ 56342 }, [](SpellInfo* spellInfo)
    {
        // @workaround: Delete dummy effect from rank 1
        // effect apply aura has NO_TARGET but core still applies it to caster (same as above)
        spellInfo->_GetEffect(EFFECT_2).Effect = SPELL_EFFECT_NONE;
    });

    // Roar of Sacrifice
    ApplySpellFix({ 53480 }, [](SpellInfo* spellInfo)
    {
        // missing spell effect 2 data, taken from 4.3.4
        spellInfo->_GetEffect(EFFECT_1).Effect = SPELL_EFFECT_APPLY_AURA;
        spellInfo->_GetEffect(EFFECT_1).ApplyAuraName = SPELL_AURA_DUMMY;
        spellInfo->_GetEffect(EFFECT_1).MiscValue = 127;
        spellInfo->_GetEffect(EFFECT_1).TargetA = SpellImplicitTargetInfo(TARGET_UNIT_TARGET_ALLY);
    });

    // Fingers of Frost
    ApplySpellFix({ 44544 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).SpellClassMask = flag96(685904631, 1151048, 0);
    });

    // Magic Suppression - DK
    ApplySpellFix({ 49224, 49610, 49611 }, [](SpellInfo* spellInfo)
    {
        spellInfo->ProcCharges = 0;
    });

    // Death and Decay
    ApplySpellFix({ 52212 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx6 |= SPELL_ATTR6_CAN_TARGET_INVISIBLE;
    });

    // Oscillation Field
    ApplySpellFix({ 37408 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 |= SPELL_ATTR3_STACK_FOR_DIFF_CASTERS;
    });

    // Everlasting Affliction
    ApplySpellFix({ 47201, 47202, 47203, 47204, 47205 }, [](SpellInfo* spellInfo)
    {
        // add corruption to affected spells
        spellInfo->_GetEffect(EFFECT_1).SpellClassMask[0] |= 2;
    });

    // Renewed Hope
    ApplySpellFix({
        57470, // (Rank 1)
        57472  // (Rank 2)
    }, [](SpellInfo* spellInfo)
    {
        // should also affect Flash Heal
        spellInfo->_GetEffect(EFFECT_0).SpellClassMask[0] |= 0x800;
    });

    // Crafty's Ultra-Advanced Proto-Typical Shortening Blaster
    ApplySpellFix({ 51912 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).Amplitude = 3000;
    });

    // Desecration Arm - 36 instead of 37 - typo? :/
    ApplySpellFix({ 29809 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_7_YARDS);
    });

    // In sniff caster hits multiple targets
    ApplySpellFix({
        73725, // [DND] Test Cheer
        73835, // [DND] Test Salute
        73836  // [DND] Test Roar
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_50_YARDS); // 50yd
    });

    // In sniff caster hits multiple targets
    ApplySpellFix({
        73837, // [DND] Test Dance
        73886  // [DND] Test Stop Dance
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_150_YARDS); // 150yd
    });

    // Master Shapeshifter: missing stance data for forms other than bear - bear version has correct data
    // To prevent aura staying on target after talent unlearned
    ApplySpellFix({ 48420 }, [](SpellInfo* spellInfo)
    {
        spellInfo->Stances = UI64LIT(1) << (FORM_CAT - 1);
    });

    ApplySpellFix({ 48421 }, [](SpellInfo* spellInfo)
    {
        spellInfo->Stances = UI64LIT(1) << (FORM_MOONKIN - 1);
    });

    ApplySpellFix({ 48422 }, [](SpellInfo* spellInfo)
    {
        spellInfo->Stances = UI64LIT(1) << (FORM_TREE - 1);
    });

    // Improved Shadowform (Rank 1)
    ApplySpellFix({ 47569 }, [](SpellInfo* spellInfo)
    {
        // with this spell atrribute aura can be stacked several times
        spellInfo->Attributes &= ~SPELL_ATTR0_NOT_SHAPESHIFT;
    });

    // Hymn of Hope
    ApplySpellFix({ 64904 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_1).ApplyAuraName = SPELL_AURA_MOD_INCREASE_ENERGY_PERCENT;
    });

    // Improved Stings (Rank 2)
    ApplySpellFix({ 19465 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_2).TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
    });

    // Nether Portal - Perseverence
    ApplySpellFix({ 30421 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_2).BasePoints += 30000;
    });

    // Natural shapeshifter
    ApplySpellFix({ 16834, 16835 }, [](SpellInfo* spellInfo)
    {
        spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(21);
    });

    // Ebon Plague
    ApplySpellFix({ 65142 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 &= ~SPELL_ATTR3_STACK_FOR_DIFF_CASTERS;
    });

    // Ebon Plague
    ApplySpellFix({ 51735, 51734, 51726 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 |= SPELL_ATTR3_STACK_FOR_DIFF_CASTERS;
        spellInfo->SpellFamilyFlags[2] = 0x10;
        spellInfo->_GetEffect(EFFECT_1).ApplyAuraName = SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN;
    });

    // Parasitic Shadowfiend Passive
    ApplySpellFix({ 41913 }, [](SpellInfo* spellInfo)
    {
        // proc debuff, and summon infinite fiends
        spellInfo->_GetEffect(EFFECT_0).ApplyAuraName = SPELL_AURA_DUMMY;
    });

    ApplySpellFix({
        27892, // To Anchor 1
        27928, // To Anchor 1
        27935, // To Anchor 1
        27915, // Anchor to Skulls
        27931, // Anchor to Skulls
        27937, // Anchor to Skulls
        16177, // Ancestral Fortitude (Rank 1)
        16236, // Ancestral Fortitude (Rank 2)
        16237, // Ancestral Fortitude (Rank 3)
        47930, // Grace
        45145, // Snake Trap Effect (Rank 1)
        13812, // Explosive Trap Effect (Rank 1)
        14314, // Explosive Trap Effect (Rank 2)
        14315, // Explosive Trap Effect (Rank 3)
        27026, // Explosive Trap Effect (Rank 4)
        49064, // Explosive Trap Effect (Rank 5)
        49065, // Explosive Trap Effect (Rank 6)
        43446, // Explosive Trap Effect (Hexlord Malacrass)
        50661, // Weakened Resolve
        68979, // Unleashed Souls
        48714, // Compelled
        7853   // The Art of Being a Water Terror: Force Cast on Player
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(13);
    });

    // Wrath of the Plaguebringer
    ApplySpellFix({ 29214, 54836 }, [](SpellInfo* spellInfo)
    {
        // target allys instead of enemies, target A is src_caster, spells with effect like that have ally target
        // this is the only known exception, probably just wrong data
        spellInfo->_GetEffect(EFFECT_0).TargetB = SpellImplicitTargetInfo(TARGET_UNIT_SRC_AREA_ALLY);
        spellInfo->_GetEffect(EFFECT_1).TargetB = SpellImplicitTargetInfo(TARGET_UNIT_SRC_AREA_ALLY);
    });

    // Wind Shear
    ApplySpellFix({ 57994 }, [](SpellInfo* spellInfo)
    {
        // improper data for EFFECT_1 in 3.3.5 DBC, but is correct in 4.x
        spellInfo->_GetEffect(EFFECT_1).Effect = SPELL_EFFECT_MODIFY_THREAT_PERCENT;
        spellInfo->_GetEffect(EFFECT_1).BasePoints = -6; // -5%
    });

    ApplySpellFix({
        50526, // Wandering Plague
        15290  // Vampiric Embrace
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 |= SPELL_ATTR3_NO_INITIAL_AGGRO;
    });

    // Vampiric Touch (dispel effect)
    ApplySpellFix({ 64085 }, [](SpellInfo* spellInfo)
    {
        // copy from similar effect of Unstable Affliction (31117)
        spellInfo->AttributesEx4 |= SPELL_ATTR4_FIXED_DAMAGE;
        spellInfo->AttributesEx6 |= SPELL_ATTR6_LIMIT_PCT_DAMAGE_MODS;
    });

    // Improved Devouring Plague
    ApplySpellFix({ 63675 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 |= SPELL_ATTR3_NO_DONE_BONUS;
    });

    // Deep Wounds
    ApplySpellFix({ 12721 }, [](SpellInfo* spellInfo)
    {
        // shouldnt ignore resillience or damage taken auras because its damage is not based off a spell.
        spellInfo->AttributesEx4 &= ~SPELL_ATTR4_FIXED_DAMAGE;
    });

    // Tremor Totem (instant pulse)
    ApplySpellFix({ 8145 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx2 |= SPELL_ATTR2_CAN_TARGET_NOT_IN_LOS;
        spellInfo->AttributesEx5 |= SPELL_ATTR5_START_PERIODIC_AT_APPLY;
    });

    // Earthbind Totem (instant pulse)
    ApplySpellFix({ 6474 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx5 |= SPELL_ATTR5_START_PERIODIC_AT_APPLY;
    });

    // Flametongue Totem (Aura)
    ApplySpellFix({
        52109, // rank 1
        52110, // rank 2
        52111, // rank 3
        52112, // rank 4
        52113, // rank 5
        58651, // rank 6
        58654, // rank 7
        58655  // rank 8
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
        spellInfo->_GetEffect(EFFECT_1).TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
        spellInfo->_GetEffect(EFFECT_0).TargetB = SpellImplicitTargetInfo();
        spellInfo->_GetEffect(EFFECT_1).TargetB = SpellImplicitTargetInfo();
    });

    // Marked for Death
    ApplySpellFix({
        53241, // (Rank 1)
        53243, // (Rank 2)
        53244, // (Rank 3)
        53245, // (Rank 4)
        53246  // (Rank 5)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).SpellClassMask = flag96(0x00067801, 0x10820001, 0x00000801);
    });

    ApplySpellFix({
        70728, // Exploit Weakness (needs target selection script)
        70840  // Devious Minds (needs target selection script)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
        spellInfo->_GetEffect(EFFECT_0).TargetB = SpellImplicitTargetInfo(TARGET_UNIT_PET);
    });

    // Culling The Herd (needs target selection script)
    ApplySpellFix({ 70893 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
        spellInfo->_GetEffect(EFFECT_0).TargetB = SpellImplicitTargetInfo(TARGET_UNIT_MASTER);
    });

    // Sigil of the Frozen Conscience
    ApplySpellFix({ 54800 }, [](SpellInfo* spellInfo)
    {
        // change class mask to custom extended flags of Icy Touch
        // this is done because another spell also uses the same SpellFamilyFlags as Icy Touch
        // SpellFamilyFlags[0] & 0x00000040 in SPELLFAMILY_DEATHKNIGHT is currently unused (3.3.5a)
        // this needs research on modifier applying rules, does not seem to be in Attributes fields
        spellInfo->_GetEffect(EFFECT_0).SpellClassMask = flag96(0x00000040, 0x00000000, 0x00000000);
    });

    // Idol of the Flourishing Life
    ApplySpellFix({ 64949 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).SpellClassMask = flag96(0x00000000, 0x02000000, 0x00000000);
        spellInfo->_GetEffect(EFFECT_0).ApplyAuraName = SPELL_AURA_ADD_FLAT_MODIFIER;
    });

    ApplySpellFix({
        34231, // Libram of the Lightbringer
        60792, // Libram of Tolerance
        64956  // Libram of the Resolute
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).SpellClassMask = flag96(0x80000000, 0x00000000, 0x00000000);
        spellInfo->_GetEffect(EFFECT_0).ApplyAuraName = SPELL_AURA_ADD_FLAT_MODIFIER;
    });

    ApplySpellFix({
        28851, // Libram of Light
        28853, // Libram of Divinity
        32403  // Blessed Book of Nagrand
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).SpellClassMask = flag96(0x40000000, 0x00000000, 0x00000000);
        spellInfo->_GetEffect(EFFECT_0).ApplyAuraName = SPELL_AURA_ADD_FLAT_MODIFIER;
    });

    // Ride Carpet
    ApplySpellFix({ 45602 }, [](SpellInfo* spellInfo)
    {
        // force seat 0, vehicle doesn't have the required seat flags for "no seat specified (-1)"
        spellInfo->_GetEffect(EFFECT_0).BasePoints = 0;
    });

    ApplySpellFix({
        64745, // Item - Death Knight T8 Tank 4P Bonus
        64936  // Item - Warrior T8 Protection 4P Bonus
    }, [](SpellInfo* spellInfo)
    {
        // 100% chance of procc'ing, not -10% (chance calculated in PrepareTriggersExecutedOnHit)
        spellInfo->_GetEffect(EFFECT_0).BasePoints = 100;
    });

    // Entangling Roots -- Nature's Grasp Proc
    ApplySpellFix({
        19970, // (Rank 6)
        19971, // (Rank 5)
        19972, // (Rank 4)
        19973, // (Rank 3)
        19974, // (Rank 2)
        19975, // (Rank 1)
        27010, // (Rank 7)
        53313  // (Rank 8)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->CastTimeEntry = sSpellCastTimesStore.LookupEntry(1);
    });

    // Easter Lay Noblegarden Egg Aura
    ApplySpellFix({ 61719 }, [](SpellInfo* spellInfo)
    {
        // Interrupt flags copied from aura which this aura is linked with
        spellInfo->AuraInterruptFlags = SpellAuraInterruptFlags::HostileActionReceived | SpellAuraInterruptFlags::Damage;
    });

    // Death Knight T10 Tank 2P Bonus
    ApplySpellFix({ 70650 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).ApplyAuraName = SPELL_AURA_ADD_PCT_MODIFIER;
    });

    ApplySpellFix({
        6789,  // Warlock - Death Coil (Rank 1)
        17925, // Warlock - Death Coil (Rank 2)
        17926, // Warlock - Death Coil (Rank 3)
        27223, // Warlock - Death Coil (Rank 4)
        47859, // Warlock - Death Coil (Rank 5)
        47860, // Warlock - Death Coil (Rank 6)
        71838, // Drain Life - Bryntroll Normal
        71839  // Drain Life - Bryntroll Heroic
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx2 |= SPELL_ATTR2_CANT_CRIT;
    });

    ApplySpellFix({
        51597, // Summon Scourged Captive
        56606, // Ride Jokkum
        61791  // Ride Vehicle (Yogg-Saron)
    }, [](SpellInfo* spellInfo)
    {
        /// @todo: remove this when basepoints of all Ride Vehicle auras are calculated correctly
        spellInfo->_GetEffect(EFFECT_0).BasePoints = 1;
    });

    // Summon Scourged Captive
    ApplySpellFix({ 51597 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).DieSides = 0;
    });

    // Black Magic
    ApplySpellFix({ 59630 }, [](SpellInfo* spellInfo)
    {
        spellInfo->Attributes |= SPELL_ATTR0_PASSIVE;
    });

    ApplySpellFix({
        17364, // Stormstrike
        48278, // Paralyze
        53651  // Light's Beacon
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 |= SPELL_ATTR3_STACK_FOR_DIFF_CASTERS;
    });

    ApplySpellFix({
        51798, // Brewfest - Relay Race - Intro - Quest Complete
        47134  // Quest Complete
    }, [](SpellInfo* spellInfo)
    {
        //! HACK: This spell break quest complete for alliance and on retail not used
        spellInfo->_GetEffect(EFFECT_0).Effect = SPELL_EFFECT_NONE;
    });

    ApplySpellFix({
        47476, // Deathknight - Strangulate
        15487, // Priest - Silence
        5211,  // Druid - Bash  - R1
        6798,  // Druid - Bash  - R2
        8983   // Druid - Bash  - R3
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx7 |= SPELL_ATTR7_INTERRUPT_ONLY_NONPLAYER;
    });

    // Guardian Spirit
    ApplySpellFix({ 47788 }, [](SpellInfo* spellInfo)
    {
        spellInfo->ExcludeTargetAuraSpell = 72232; // Weakened Spirit
    });

    ApplySpellFix({
        15538, // Gout of Flame
        42490, // Energized!
        42492, // Cast Energized
        43115  // Plague Vial
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx |= SPELL_ATTR1_NO_THREAT;
    });

    ApplySpellFix({
        46842, // Flame Ring
        46836  // Flame Patch
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo();
    });

    // Test Ribbon Pole Channel
    ApplySpellFix({ 29726 }, [](SpellInfo* spellInfo)
    {
        spellInfo->ChannelInterruptFlags &= ~SpellAuraInterruptFlags::Action;
    });

    ApplySpellFix({
        42767, // Sic'em
        43092  // Stop the Ascension!: Halfdan's Soul Destruction
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_UNIT_NEARBY_ENTRY);
    });

    // Polymorph (Six Demon Bag)
    ApplySpellFix({ 14621 }, [](SpellInfo* spellInfo)
    {
        spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(4); // Medium Range
    });

    // Concussive Barrage
    ApplySpellFix({ 35101 }, [](SpellInfo* spellInfo)
    {
        spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(155); // Hunter Range (Long)
    });

    ApplySpellFix({
        44327, // Trained Rock Falcon/Hawk Hunting
        44408  // Trained Rock Falcon/Hawk Hunting
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->Speed = 0.f;
    });

    ApplySpellFix({
        51675,  // Rogue - Unfair Advantage (Rank 1)
        51677   // Rogue - Unfair Advantage (Rank 2)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(2); // 5 yards
    });

    ApplySpellFix({
        55741, // Desecration (Rank 1)
        68766, // Desecration (Rank 2)
        57842  // Killing Spree (Off hand damage)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(2); // Melee Range
    });

    // Safeguard
    ApplySpellFix({
        46946, // (Rank 1)
        46947  // (Rank 2)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(34); // Twenty-Five yards
    });

    // Summon Corpse Scarabs
    ApplySpellFix({ 28864, 29105 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_10_YARDS);
    });

    ApplySpellFix({
        37851, // Tag Greater Felfire Diemetradon
        37918  // Arcano-pince
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->RecoveryTime = 3000;
    });

    // Jormungar Strike
    ApplySpellFix({ 56513 }, [](SpellInfo* spellInfo)
    {
        spellInfo->RecoveryTime = 2000;
    });

    ApplySpellFix({
        54997, // Cast Net (tooltip says 10s but sniffs say 6s)
        56524  // Acid Breath
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->RecoveryTime = 6000;
    });

    ApplySpellFix({
        47911, // EMP
        48620, // Wing Buffet
        51752  // Stampy's Stompy-Stomp
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->RecoveryTime = 10000;
    });

    ApplySpellFix({
        37727, // Touch of Darkness
        54996  // Ice Slick (tooltip says 20s but sniffs say 12s)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->RecoveryTime = 12000;
    });

    // Signal Helmet to Attack
    ApplySpellFix({ 51748 }, [](SpellInfo* spellInfo)
    {
        spellInfo->RecoveryTime = 15000;
    });

    ApplySpellFix({
        51756, // Charge
        37919, //Arcano-dismantle
        37917  //Arcano-Cloak
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->RecoveryTime = 20000;
    });

    // Summon Frigid Bones
    ApplySpellFix({ 53525 }, [](SpellInfo* spellInfo)
    {
        spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(4); // 2 minutes
    });

    // Dark Conclave Ritualist Channel
    ApplySpellFix({ 38469 }, [](SpellInfo* spellInfo)
    {
        spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(6);  // 100yd
    });

    //
    // VIOLET HOLD SPELLS
    //
    // Water Globule (Ichoron)
    ApplySpellFix({ 54258, 54264, 54265, 54266, 54267 }, [](SpellInfo* spellInfo)
    {
        // in 3.3.5 there is only one radius in dbc which is 0 yards in this
        // use max radius from 4.3.4
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_25_YARDS);
    });
    // ENDOF VIOLET HOLD

    //
    // ULDUAR SPELLS
    //
    // Pursued (Flame Leviathan)
    ApplySpellFix({ 62374 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_50000_YARDS);   // 50000yd
    });

    // Focused Eyebeam Summon Trigger (Kologarn)
    ApplySpellFix({ 63342 }, [](SpellInfo* spellInfo)
    {
        spellInfo->MaxAffectedTargets = 1;
    });

    ApplySpellFix({
        62716, // Growth of Nature (Freya)
        65584, // Growth of Nature (Freya)
        64381  // Strength of the Pack (Auriaya)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 |= SPELL_ATTR3_STACK_FOR_DIFF_CASTERS;
    });

    ApplySpellFix({
        63018, // Searing Light (XT-002)
        65121, // Searing Light (25m) (XT-002)
        63024, // Gravity Bomb (XT-002)
        64234  // Gravity Bomb (25m) (XT-002)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->MaxAffectedTargets = 1;
    });

    ApplySpellFix({
        64386, // Terrifying Screech (Auriaya)
        64389, // Sentinel Blast (Auriaya)
        64678  // Sentinel Blast (Auriaya)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(28); // 5 seconds, wrong DBC data?
    });

    // Summon Swarming Guardian (Auriaya)
    ApplySpellFix({ 64397 }, [](SpellInfo* spellInfo)
    {
        spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(137); // 8y, Based in BFA effect radius
    });

    // Potent Pheromones (Freya)
    ApplySpellFix({ 64321 }, [](SpellInfo* spellInfo)
    {
        // spell should dispel area aura, but doesn't have the attribute
        // may be db data bug, or blizz may keep reapplying area auras every update with checking immunity
        // that will be clear if we get more spells with problem like this
        spellInfo->AttributesEx |= SPELL_ATTR1_DISPEL_AURAS_ON_IMMUNITY;
    });

    // Blizzard (Thorim)
    ApplySpellFix({ 62576, 62602 }, [](SpellInfo* spellInfo)
    {
        // DBC data is wrong for EFFECT_0, it's a different dynobject target than EFFECT_1
        // Both effects should be shared by the same DynObject
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_DEST_CASTER_LEFT);
    });

    // Spinning Up (Mimiron)
    ApplySpellFix({ 63414 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetB = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
        spellInfo->ChannelInterruptFlags = SpellAuraInterruptFlags::None;
    });

    // Rocket Strike (Mimiron)
    ApplySpellFix({ 63036 }, [](SpellInfo* spellInfo)
    {
        spellInfo->Speed = 0;
    });

    // Magnetic Field (Mimiron)
    ApplySpellFix({ 64668 }, [](SpellInfo* spellInfo)
    {
        spellInfo->Mechanic = MECHANIC_NONE;
    });

    // Empowering Shadows (Yogg-Saron)
    ApplySpellFix({ 64468, 64486 }, [](SpellInfo* spellInfo)
    {
        spellInfo->MaxAffectedTargets = 3;  // same for both modes?
    });

    // Cosmic Smash (Algalon the Observer)
    ApplySpellFix({ 62301 }, [](SpellInfo* spellInfo)
    {
        spellInfo->MaxAffectedTargets = 1;
    });

    // Cosmic Smash (Algalon the Observer)
    ApplySpellFix({ 64598 }, [](SpellInfo* spellInfo)
    {
        spellInfo->MaxAffectedTargets = 3;
    });

    // Cosmic Smash (Algalon the Observer)
    ApplySpellFix({ 62293 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetB = SpellImplicitTargetInfo(TARGET_DEST_CASTER);
    });

    // Cosmic Smash (Algalon the Observer)
    ApplySpellFix({ 62311, 64596 }, [](SpellInfo* spellInfo)
    {
        spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(6);  // 100yd
    });

    ApplySpellFix({
        64014, // Expedition Base Camp Teleport
        64024, // Conservatory Teleport
        64025, // Halls of Invention Teleport
        64028, // Colossal Forge Teleport
        64029, // Shattered Walkway Teleport
        64030, // Antechamber Teleport
        64031, // Scrapyard Teleport
        64032, // Formation Grounds Teleport
        65042  // Prison of Yogg-Saron Teleport
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_DEST_DB);
    });
    // ENDOF ULDUAR SPELLS

    //
    // TRIAL OF THE CRUSADER SPELLS
    //
    // Infernal Eruption
    ApplySpellFix({ 66258, 67901 }, [](SpellInfo* spellInfo)
    {
        // increase duration from 15 to 18 seconds because caster is already
        // unsummoned when spell missile hits the ground so nothing happen in result
        spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(85);
    });
    // ENDOF TRIAL OF THE CRUSADER SPELLS

    //
    // HALLS OF REFLECTION SPELLS
    //
    ApplySpellFix({
        72435, // Defiling Horror
        72452  // Defiling Horror
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = spellInfo->_GetEffect(EFFECT_1).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_60_YARDS); // 60yd
    });

    // Achievement Check
    ApplySpellFix({ 72830 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_50000_YARDS); // 50000yd
    });

    // Start Halls of Reflection Quest AE
    ApplySpellFix({ 72900 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_200_YARDS); // 200yd
    });
    // ENDOF HALLS OF REFLECTION SPELLS

    //
    // ICECROWN CITADEL SPELLS
    //
    ApplySpellFix({
        // THESE SPELLS ARE WORKING CORRECTLY EVEN WITHOUT THIS HACK
        // THE ONLY REASON ITS HERE IS THAT CURRENT GRID SYSTEM
        // DOES NOT ALLOW FAR OBJECT SELECTION (dist > 333)
        70781, // Light's Hammer Teleport
        70856, // Oratory of the Damned Teleport
        70857, // Rampart of Skulls Teleport
        70858, // Deathbringer's Rise Teleport
        70859, // Upper Spire Teleport
        70860, // Frozen Throne Teleport
        70861  // Sindragosa's Lair Teleport
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_DEST_DB);
    });

    // Bone Slice (Lord Marrowgar)
    ApplySpellFix({ 69055, 70814 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_5_YARDS); // 5yd
    });

    ApplySpellFix({
        69075, // Bone Storm (Lord Marrowgar)
        70834, // Bone Storm (Lord Marrowgar)
        70835, // Bone Storm (Lord Marrowgar)
        70836, // Bone Storm (Lord Marrowgar)
        71160, // Plague Stench (Stinky)
        71161, // Plague Stench (Stinky)
        71123  // Decimate (Stinky & Precious)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_100_YARDS); // 100yd
    });

    // Coldflame (Lord Marrowgar)
    ApplySpellFix({ 69146, 70823, 70824, 70825 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx4 &= ~SPELL_ATTR4_IGNORE_RESISTANCES;
    });

    // Shadow's Fate
    ApplySpellFix({ 71169 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 |= SPELL_ATTR3_STACK_FOR_DIFF_CASTERS;
    });

    // Lock Players and Tap Chest
    ApplySpellFix({ 72347 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 &= ~SPELL_ATTR3_NO_INITIAL_AGGRO;
    });

    // Award Reputation - Boss Kill
    ApplySpellFix({ 73843, 73844, 73845, 73846 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_50000_YARDS); // 50000yd
    });

    ApplySpellFix({
        72378, // Blood Nova (Deathbringer Saurfang)
        73058, // Blood Nova (Deathbringer Saurfang)
        72769  // Scent of Blood (Deathbringer Saurfang)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = spellInfo->_GetEffect(EFFECT_1).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_200_YARDS);
    });

    // Scent of Blood (Deathbringer Saurfang)
    ApplySpellFix({ 72771 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_1).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_200_YARDS);
    });

    // Resistant Skin (Deathbringer Saurfang adds)
    ApplySpellFix({ 72723 }, [](SpellInfo* spellInfo)
    {
        // this spell initially granted Shadow damage immunity, however it was removed but the data was left in client
        spellInfo->_GetEffect(EFFECT_2).Effect = SPELL_EFFECT_NONE;
    });

    // Coldflame Jets (Traps after Saurfang)
    ApplySpellFix({ 70460 }, [](SpellInfo* spellInfo)
    {
        spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(1); // 10 seconds
    });

    ApplySpellFix({
        71412, // Green Ooze Summon (Professor Putricide)
        71415  // Orange Ooze Summon (Professor Putricide)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_UNIT_TARGET_ANY);
    });

    // Ooze flood
    ApplySpellFix({ 69783, 69797, 69799, 69802 }, [](SpellInfo* spellInfo)
    {
        // Those spells are cast on creatures with same entry as caster while they have TARGET_UNIT_NEARBY_ENTRY.
        spellInfo->AttributesEx |= SPELL_ATTR1_CANT_TARGET_SELF;
    });

    // Awaken Plagued Zombies
    ApplySpellFix({ 71159 }, [](SpellInfo* spellInfo)
    {
        spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(21);
    });

    // Volatile Ooze Beam Protection (Professor Putricide)
    ApplySpellFix({ 70530 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).Effect = SPELL_EFFECT_APPLY_AURA; // for an unknown reason this was SPELL_EFFECT_APPLY_AREA_AURA_RAID
    });

    // Mutated Strength (Professor Putricide)
    ApplySpellFix({ 71604, 72673, 72674, 72675 }, [](SpellInfo* spellInfo)
    {
        // THIS IS HERE BECAUSE COOLDOWN ON CREATURE PROCS WERE NOT IMPLEMENTED WHEN THE SCRIPT WAS WRITTEN
        spellInfo->_GetEffect(EFFECT_1).Effect = SPELL_EFFECT_NONE;
    });

    // Mutated Plague (Professor Putricide)
    ApplySpellFix({ 72454, 72464, 72506, 72507 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_50000_YARDS); // 50000yd
    });

    // Unbound Plague (Professor Putricide) (needs target selection script)
    ApplySpellFix({ 70911, 72854, 72855, 72856 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetB = SpellImplicitTargetInfo(TARGET_UNIT_TARGET_ENEMY);
    });

    ApplySpellFix({
        71518, // Unholy Infusion Quest Credit (Professor Putricide)
        72934, // Blood Infusion Quest Credit (Blood-Queen Lana'thel)
        72289  // Frost Infusion Quest Credit (Sindragosa)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_200_YARDS); // another missing radius
    });

    // Empowered Flare (Blood Prince Council)
    ApplySpellFix({ 71708, 72785, 72786, 72787 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 |= SPELL_ATTR3_NO_DONE_BONUS;
    });

    // Swarming Shadows
    ApplySpellFix({ 71266, 72890 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AreaGroupId = 0; // originally, these require area 4522, which is... outside of Icecrown Citadel
    });

    // Corruption
    ApplySpellFix({ 70602 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 |= SPELL_ATTR3_STACK_FOR_DIFF_CASTERS;
    });

    // Column of Frost (visual marker)
    ApplySpellFix({ 70715 }, [](SpellInfo* spellInfo)
    {
        spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(32); // 6 seconds (missing)
    });

    // Mana Void (periodic aura)
    ApplySpellFix({ 71085 }, [](SpellInfo* spellInfo)
    {
        spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(9); // 30 seconds (missing)
    });

    // Frostbolt Volley (only heroic)
    ApplySpellFix({ 72015, 72016 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_2).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_40_YARDS);
    });

    // Summon Suppressor (needs target selection script)
    ApplySpellFix({ 70936 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_UNIT_TARGET_ANY);
        spellInfo->_GetEffect(EFFECT_0).TargetB = SpellImplicitTargetInfo();
        spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(157); // 90yd
    });

    ApplySpellFix({
        72706, // Achievement Check (Valithria Dreamwalker)
        71357  // Order Whelp
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_200_YARDS);   // 200yd
    });

    // Sindragosa's Fury
    ApplySpellFix({ 70598 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).TargetA = SpellImplicitTargetInfo(TARGET_DEST_DEST);
    });

    // Frost Bomb
    ApplySpellFix({ 69846 }, [](SpellInfo* spellInfo)
    {
        spellInfo->Speed = 0.0f;    // This spell's summon happens instantly
    });

    // Chilled to the Bone
    ApplySpellFix({ 70106 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 |= SPELL_ATTR3_NO_DONE_BONUS;
        spellInfo->AttributesEx6 |= SPELL_ATTR6_LIMIT_PCT_DAMAGE_MODS;
    });

    // Ice Lock
    ApplySpellFix({ 71614 }, [](SpellInfo* spellInfo)
    {
        spellInfo->Mechanic = MECHANIC_STUN;
    });

    // Defile
    ApplySpellFix({ 72762 }, [](SpellInfo* spellInfo)
    {
        spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(559); // 53 seconds
    });

    // Defile
    ApplySpellFix({ 72743 }, [](SpellInfo* spellInfo)
    {
        spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(22); // 45 seconds
    });

    // Defile
    ApplySpellFix({ 72754, 73708, 73709, 73710 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = spellInfo->_GetEffect(EFFECT_1).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_200_YARDS); // 200yd
    });

    // Val'kyr Target Search
    ApplySpellFix({ 69030 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = spellInfo->_GetEffect(EFFECT_1).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_200_YARDS); // 200yd
        spellInfo->Attributes |= SPELL_ATTR0_UNAFFECTED_BY_INVULNERABILITY;
    });

    // Raging Spirit Visual
    ApplySpellFix({ 69198 }, [](SpellInfo* spellInfo)
    {
        spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(13); // 50000yd
    });

    // Harvest Souls
    ApplySpellFix({ 73654, 74295, 74296, 74297 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_50000_YARDS); // 50000yd
        spellInfo->_GetEffect(EFFECT_1).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_50000_YARDS); // 50000yd
        spellInfo->_GetEffect(EFFECT_2).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_50000_YARDS); // 50000yd
    });

    // Harvest Soul
    ApplySpellFix({ 73655 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 |= SPELL_ATTR3_NO_DONE_BONUS;
    });

    // Summon Shadow Trap
    ApplySpellFix({ 73540 }, [](SpellInfo* spellInfo)
    {
        spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(3); // 60 seconds
    });

    // Shadow Trap (visual)
    ApplySpellFix({ 73530 }, [](SpellInfo* spellInfo)
    {
        spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(27); // 3 seconds
    });

    // Shadow Trap
    ApplySpellFix({ 73529 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_1).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_10_YARDS); // 10yd
    });

    // Shadow Trap (searcher)
    ApplySpellFix({ 74282 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_5_YARDS); // 5yd
    });

    // Restore Soul
    ApplySpellFix({ 72595, 73650 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_200_YARDS); // 200yd
    });

    // Destroy Soul
    ApplySpellFix({ 74086 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_200_YARDS); // 200yd
    });

    // Summon Spirit Bomb
    ApplySpellFix({ 74302, 74342 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_200_YARDS); // 200yd
        spellInfo->MaxAffectedTargets = 1;
    });

    // Summon Spirit Bomb
    ApplySpellFix({ 74341, 74343 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_200_YARDS); // 200yd
        spellInfo->MaxAffectedTargets = 3;
    });

    // Summon Spirit Bomb
    ApplySpellFix({ 73579 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_25_YARDS); // 25yd
    });

    // Fury of Frostmourne
    ApplySpellFix({ 72350 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = spellInfo->_GetEffect(EFFECT_1).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_50000_YARDS); // 50000yd
    });

    ApplySpellFix(
    {
        75127, // Kill Frostmourne Players
        72351, // Fury of Frostmourne
        72431, // Jump (removes Fury of Frostmourne debuff)
        72429, // Mass Resurrection
        73159, // Play Movie
        73582  // Trigger Vile Spirit (Inside, Heroic)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_50000_YARDS); // 50000yd
    });

    // Raise Dead
    ApplySpellFix({ 72376 }, [](SpellInfo* spellInfo)
    {
        spellInfo->MaxAffectedTargets = 3;
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_50000_YARDS); // 50000yd
    });

    // Jump
    ApplySpellFix({ 71809 }, [](SpellInfo* spellInfo)
    {
        spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(5); // 40yd
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_10_YARDS); // 10yd
        spellInfo->_GetEffect(EFFECT_0).MiscValue = 190;
    });

    // Broken Frostmourne
    ApplySpellFix({ 72405 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_1).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_20_YARDS); // 20yd
        spellInfo->AttributesEx |= SPELL_ATTR1_NO_THREAT;
    });
    // ENDOF ICECROWN CITADEL SPELLS

    //
    // RUBY SANCTUM SPELLS
    //
    // Soul Consumption
    ApplySpellFix({ 74799 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_1).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_12_YARDS);
    });

    // Twilight Cutter
    ApplySpellFix({ 74769, 77844, 77845, 77846 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_100_YARDS); // 100yd
    });

    // Twilight Mending
    ApplySpellFix({ 75509 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx6 |= SPELL_ATTR6_CAN_TARGET_INVISIBLE;
        spellInfo->AttributesEx2 |= SPELL_ATTR2_CAN_TARGET_NOT_IN_LOS;
    });

    // Combustion and Consumption Heroic versions lacks radius data
    ApplySpellFix({ 75875 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).Mechanic = MECHANIC_NONE;
        spellInfo->_GetEffect(EFFECT_1).Mechanic = MECHANIC_SNARE;
        spellInfo->_GetEffect(EFFECT_1).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_6_YARDS);
    });

    ApplySpellFix({ 75884 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_0).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_6_YARDS);
        spellInfo->_GetEffect(EFFECT_1).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_6_YARDS);
    });

    ApplySpellFix({ 75883, 75876 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_1).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_6_YARDS);
    });
    // ENDOF RUBY SANCTUM SPELLS

    //
    // EYE OF ETERNITY SPELLS
    //
    ApplySpellFix({
        // All spells below work even without these changes. The LOS attribute is due to problem
        // from collision between maps & gos with active destroyed state.
        57473, // Arcane Storm bonus explicit visual spell
        57431, // Summon Static Field
        56091, // Flame Spike (Wyrmrest Skytalon)
        56092, // Engulf in Flames (Wyrmrest Skytalon)
        57090, // Revivify (Wyrmrest Skytalon)
        57143  // Life Burst (Wyrmrest Skytalon)
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx2 |= SPELL_ATTR2_CAN_TARGET_NOT_IN_LOS;
    });

    // Arcane Barrage (cast by players and NONMELEEDAMAGELOG with caster Scion of Eternity (original caster)).
    ApplySpellFix({ 63934 }, [](SpellInfo* spellInfo)
    {
        // This would never crit on retail and it has attribute for SPELL_ATTR3_NO_DONE_BONUS because is handled from player,
        // until someone figures how to make scions not critting without hack and without making them main casters this should stay here.
        spellInfo->AttributesEx2 |= SPELL_ATTR2_CANT_CRIT;
    });
    // ENDOF EYE OF ETERNITY SPELLS

    //
    // OCULUS SPELLS
    //
    ApplySpellFix({
        // The spells below are here because their effect 1 is giving warning due to
        // triggered spell not found in any dbc and is missing from encounter source* of data.
        // Even judged as clientside these spells can't be guessed for* now.
        49462, // Call Ruby Drake
        49461, // Call Amber Drake
        49345  // Call Emerald Drake
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_1).Effect = SPELL_EFFECT_NONE;
    });
    // ENDOF OCULUS SPELLS

    // Introspection
    ApplySpellFix({ 40055, 40165, 40166, 40167 }, [](SpellInfo* spellInfo)
    {
        spellInfo->Attributes |= SPELL_ATTR0_NEGATIVE_1;
    });

    // Chains of Ice
    ApplySpellFix({ 45524 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_2).TargetA = SpellImplicitTargetInfo();
    });

    // Minor Fortitude
    ApplySpellFix({ 2378 }, [](SpellInfo* spellInfo)
    {
        spellInfo->ManaCost = 0;
        spellInfo->ManaPerSecond = 0;
    });

    // Threatening Gaze
    ApplySpellFix({ 24314 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AuraInterruptFlags |= SpellAuraInterruptFlags::Action | SpellAuraInterruptFlags::Moving | SpellAuraInterruptFlags::Anim;
    });

    //
    // ISLE OF CONQUEST SPELLS
    //
    // Teleport
    ApplySpellFix({ 66551 }, [](SpellInfo* spellInfo)
    {
        spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(13); // 50000yd
    });
    // ENDOF ISLE OF CONQUEST SPELLS

    // Aura of Fear
    ApplySpellFix({ 40453 }, [](SpellInfo* spellInfo)
    {
        // Bad DBC data? Copying 25820 here due to spell description
        // either is a periodic with chance on tick, or a proc

        spellInfo->_GetEffect(EFFECT_0).ApplyAuraName = SPELL_AURA_PROC_TRIGGER_SPELL;
        spellInfo->_GetEffect(EFFECT_0).Amplitude = 0;
        spellInfo->ProcChance = 10;
    });

    // Survey Sinkholes
    ApplySpellFix({ 45853 }, [](SpellInfo* spellInfo)
    {
        spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(5); // 40 yards
    });

    ApplySpellFix({
        41485, // Deadly Poison - Black Temple
        41487  // Envenom - Black Temple
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx6 |= SPELL_ATTR6_CAN_TARGET_INVISIBLE;
    });

    ApplySpellFix({
        // Proc attribute correction
        // Remove procflags from test/debug/deprecated spells to avoid DB Errors
        2479,  // Honorless Target
        3232,  // Gouge Stun Test
        3409,  // Crippling Poison
        4312,  // Strider Presence
        5707,  // Lifestone Regeneration
        5760,  // Mind-numbing Poison
        6727,  // Poison Mushroom
        6940,  // Hand of Sacrifice (handled remove in split hook)
        6984,  // Frost Shot (Rank 2)
        7164,  // Defensive Stance
        7288,  // Immolate Cumulative (TEST) (Rank 1)
        7291,  // Food (TEST)
        7331,  // Healing Aura (TEST) (Rank 1)
        7366,  // Berserker Stance
        7824,  // Blacksmithing Skill +10
        12551, // Frost Shot
        13218, // Wound Poison (Rank 1)
        13222, // Wound Poison II (Rank 2)
        13223, // Wound Poison III (Rank 3)
        13224, // Wound Poison IV (Rank 4)
        14795, // Venomhide Poison
        16610, // Razorhide
        18099, // Chill Nova
        18499, // Berserker Rage (extra rage implemented in Unit::RewardRage)
        18802, // Frost Shot
        20000, // Alexander's Test Periodic Aura
        21163, // Polished Armor (Rank 1)
        22818, // Mol'dar's Moxie
        22820, // Slip'kik's Savvy
        23333, // Warsong Flag
        23335, // Silverwing Flag
        25160, // Sand Storm
        27189, // Wound Poison V (Rank 5)
        28313, // Aura of Fear
        28726, // Nightmare Seed
        28754, // Fury of the Ashbringer
        30802, // Unleashed Rage (Rank 1)
        31481, // Lung Burst
        32430, // Battle Standard
        32431, // Battle Standard
        32447, // Travel Form
        33370, // Spell Haste
        33807, // Abacus of Violent Odds
        33891, // Tree of Life (Shapeshift)
        34132, // Gladiator's Totem of the Third Wind
        34135, // Libram of Justice
        34666, // Tamed Pet Passive 08 (DND)
        34667, // Tamed Pet Passive 09 (DND)
        34775, // Dragonspine Flurry
        34889, // Fire Breath (Rank 1)
        34976, // Netherstorm Flag
        35131, // Bladestorm
        35244, // Choking Vines
        35323, // Fire Breath (Rank 2)
        35336, // Energizing Spores
        36148, // Chill Nova
        36613, // Aspect of the Spirit Hunter
        36786, // Soul Chill
        37174, // Perceived Weakness
        37482, // Exploited Weakness
        37526, // Battle Rush
        37588, // Dive
        37985, // Fire Breath
        38317, // Forgotten Knowledge
        38843, // Soul Chill
        39015, // Atrophic Blow
        40396, // Fel Infusion
        40603, // Taunt Gurtogg
        40803, // Ron's Test Buff
        40879, // Prismatic Shield (no longer used since patch 2.2/adaptive prismatic shield)
        41341, // Balance of Power (implemented by hooking absorb)
        41435, // The Twin Blades of Azzinoth
        42369, // Merciless Libram of Justice
        42371, // Merciless Gladiator's Totem of the Third Wind
        42636, // Birmingham Tools Test 3
        43727, // Vengeful Libram of Justice
        43729, // Vengeful Gladiator's Totem of the Third Wind
        43817, // Focused Assault
        44305, // You're a ...! (Effects2)
        44586, // Prayer of Mending (unknown, unused aura type)
        45384, // Birmingham Tools Test 4
        45433, // Birmingham Tools Test 5
        46093, // Brutal Libram of Justice
        46099, // Brutal Gladiator's Totem of the Third Wind
        46705, // Honorless Target
        49145, // Spell Deflection (Rank 1) (implemented by hooking absorb)
        49883, // Flames
        50365, // Improved Blood Presence (Rank 1)
        50371, // Improved Blood Presence (Rank 2)
        50462, // Anti-Magic Zone (implemented by hooking absorb)

        50498, // Savage Rend (Rank 1) - proc from Savage Rend moved from attack itself to autolearn aura 50871
        53578, // Savage Rend (Rank 2)
        53579, // Savage Rend (Rank 3)
        53580, // Savage Rend (Rank 4)
        53581, // Savage Rend (Rank 5)
        53582, // Savage Rend (Rank 6)

        50655, // Frost Cut
        50995, // Empowered Blood Presence (Rank 1)
        51809, // First Aid
        53032, // Flurry of Claws
        55482, // Fire Breath (Rank 3)
        55483, // Fire Breath (Rank 4)
        55484, // Fire Breath (Rank 5)
        55485, // Fire Breath (Rank 6)
        57974, // Wound Poison VI (Rank 6)
        57975, // Wound Poison VII (Rank 7)
        60062, // Essence of Life
        60302, // Meteorite Whetstone
        60437, // Grim Toll
        60492, // Embrace of the Spider
        62142, // Improved Chains of Ice (Rank 3)
        63024, // Gravity Bomb
        64205, // Divine Sacrifice (handled remove in split hook)
        64772, // Comet's Trail
        65004, // Alacrity of the Elements
        65019, // Mjolnir Runestone
        65024, // Implosion

        66334, // Mistress' Kiss - currently not used in script, need implement?
        67905, // Mistress' Kiss
        67906, // Mistress' Kiss
        67907, // Mistress' Kiss

        71003, // Vegard's Touch

        72151, // Frenzied Bloodthirst - currently not used in script, need implement?
        72648, // Frenzied Bloodthirst
        72649, // Frenzied Bloodthirst
        72650, // Frenzied Bloodthirst

        72559, // Birmingham Tools Test 3
        72560, // Birmingham Tools Test 3
        72561, // Birmingham Tools Test 5
        72980  // Shadow Resonance
    }, [](SpellInfo* spellInfo)
    {
        spellInfo->ProcFlags = 0;
    });

    // Feral Charge - Cat
    ApplySpellFix({ 49376 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_1).RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_3_YARDS); // 3yd
    });

    // Baron Rivendare (Stratholme) - Unholy Aura
    ApplySpellFix({ 17466, 17467 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 |= SPELL_ATTR3_NO_INITIAL_AGGRO;
    });

    // Spore - Spore Visual
    ApplySpellFix({ 42525 }, [](SpellInfo* spellInfo)
    {
        spellInfo->AttributesEx3 |= SPELL_ATTR3_DEATH_PERSISTENT;
        spellInfo->AttributesEx2 |= SPELL_ATTR2_CAN_TARGET_DEAD;
    });

    // Death's Embrace
    ApplySpellFix({ 47198, 47199, 47200 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_1).SpellClassMask[0] |= 0x00004000; // Drain soul
    });

    // Soul Sickness (Forge of Souls)
    ApplySpellFix({ 69131 }, [](SpellInfo* spellInfo)
    {
        spellInfo->_GetEffect(EFFECT_1).ApplyAuraName = SPELL_AURA_MOD_DECREASE_SPEED;
    });

    // Headless Horseman Climax - Return Head (Hallow End)
    // Headless Horseman Climax - Body Regen (confuse only - removed on death)
    // Headless Horseman Climax - Head Is Dead
    ApplySpellFix({ 42401, 43105, 42428 }, [](SpellInfo* spellInfo)
    {
        spellInfo->Attributes |= SPELL_ATTR0_UNAFFECTED_BY_INVULNERABILITY;
    });

    // Sacred Cleansing
    ApplySpellFix({ 53659 }, [](SpellInfo* spellInfo)
    {
        spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(5); // 40yd
    });

    for (uint32 i = 0; i < GetSpellInfoStoreSize(); ++i)
    {
        SpellInfo* spellInfo = mSpellInfoMap[i];
        if (!spellInfo)
            continue;

        // Fix range for trajectory triggered spell
        for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
        {
            if (spellEffectInfo.IsEffect() && (spellEffectInfo.TargetA.GetTarget() == TARGET_DEST_TRAJ || spellEffectInfo.TargetB.GetTarget() == TARGET_DEST_TRAJ))
            {
                // Get triggered spell if any
                if (SpellInfo* spellInfoTrigger = const_cast<SpellInfo*>(GetSpellInfo(spellEffectInfo.TriggerSpell)))
                {
                    float maxRangeMain = spellInfo->RangeEntry ? spellInfo->RangeEntry->RangeMax[0] : 0.0f;
                    float maxRangeTrigger = spellInfoTrigger->RangeEntry ? spellInfoTrigger->RangeEntry->RangeMax[0] : 0.0f;

                    // check if triggered spell has enough max range to cover trajectory
                    if (maxRangeTrigger < maxRangeMain)
                        spellInfoTrigger->RangeEntry = spellInfo->RangeEntry;
                }
            }
        }

        for (SpellEffectInfo& spellEffectInfo : spellInfo->_GetEffects())
        {
            switch (spellEffectInfo.Effect)
            {
                case SPELL_EFFECT_CHARGE:
                case SPELL_EFFECT_CHARGE_DEST:
                case SPELL_EFFECT_JUMP:
                case SPELL_EFFECT_JUMP_DEST:
                case SPELL_EFFECT_LEAP_BACK:
                    if (!spellInfo->Speed && !spellInfo->SpellFamilyName)
                        spellInfo->Speed = SPEED_CHARGE;
                    break;
                case SPELL_EFFECT_APPLY_AURA:
                    // special aura updates each 30 seconds
                    if (spellEffectInfo.ApplyAuraName == SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR)
                        spellEffectInfo.Amplitude = 30 * IN_MILLISECONDS;
                    break;
                default:
                    break;
            }

            // Passive talent auras cannot target pets
            if (spellInfo->IsPassive() && GetTalentSpellCost(i))
                if (spellEffectInfo.TargetA.GetTarget() == TARGET_UNIT_PET)
                    spellEffectInfo.TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);

            // Area auras may not target area (they're self cast)
            if (spellEffectInfo.IsAreaAuraEffect() && spellEffectInfo.IsTargetingArea())
            {
                spellEffectInfo.TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
                spellEffectInfo.TargetB = SpellImplicitTargetInfo(0);
            }
        }

        // disable proc for magnet auras, they're handled differently
        if (spellInfo->HasAura(SPELL_AURA_SPELL_MAGNET))
            spellInfo->ProcFlags = 0;

        // due to the way spell system works, unit would change orientation in Spell::_cast
        if (spellInfo->HasAura(SPELL_AURA_CONTROL_VEHICLE))
            spellInfo->AttributesEx5 |= SPELL_ATTR5_DONT_TURN_DURING_CAST;

        if (spellInfo->ActiveIconID == 2158)  // flight
            spellInfo->Attributes |= SPELL_ATTR0_PASSIVE;

        switch (spellInfo->SpellFamilyName)
        {
            case SPELLFAMILY_PALADIN:
                // Seals of the Pure should affect Seal of Righteousness
                if (spellInfo->SpellIconID == 25 && spellInfo->HasAttribute(SPELL_ATTR0_PASSIVE))
                    spellInfo->_GetEffect(EFFECT_0).SpellClassMask[1] |= 0x20000000;
                break;
            case SPELLFAMILY_DEATHKNIGHT:
                // Icy Touch - extend FamilyFlags (unused value) for Sigil of the Frozen Conscience to use
                if (spellInfo->SpellIconID == 2721 && spellInfo->SpellFamilyFlags[0] & 0x2)
                    spellInfo->SpellFamilyFlags[0] |= 0x40;
                break;
        }
    }

    if (SummonPropertiesEntry* properties = const_cast<SummonPropertiesEntry*>(sSummonPropertiesStore.LookupEntry(121)))
        properties->Title = SUMMON_TYPE_TOTEM;
    if (SummonPropertiesEntry* properties = const_cast<SummonPropertiesEntry*>(sSummonPropertiesStore.LookupEntry(647))) // 52893
        properties->Title = SUMMON_TYPE_TOTEM;
    if (SummonPropertiesEntry* properties = const_cast<SummonPropertiesEntry*>(sSummonPropertiesStore.LookupEntry(628))) // Hungry Plaguehound
        properties->Control = SUMMON_CATEGORY_PET;

    if (LockEntry* entry = const_cast<LockEntry*>(sLockStore.LookupEntry(36))) // 3366 Opening, allows to open without proper key
        entry->Type[2] = LOCK_KEY_NONE;

    TC_LOG_INFO("server.loading", ">> Loaded SpellInfo corrections in {} ms", GetMSTimeDiffToNow(oldMSTime));
}
