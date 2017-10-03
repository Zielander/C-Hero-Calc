#include "battleLogic.h"

int totalFightsSimulated = 0;

// Function determining if a monster is strictly better than another
bool isBetter(Monster * a, Monster * b, bool considerAbilities) {
    if (a->element == b->element) {
        return (a->damage >= b->damage) && (a->hp >= b->hp);
    } else { // a needs to be better than b even when b has elemental advantage, or a is at disadvantage
        return !considerAbilities && (a->damage >= b->damage * elementalBoost) && (a->hp >= b->hp * elementalBoost);
    }
}

// TODO: Implement MAX AOE Damage to make sure nothing gets revived
// Simulates One fight between 2 Armies and writes results into left's LastFightData
void simulateFight(Army & left, Army & right, bool verbose) {
    // left[0] and right[0] are the first to fight
    // Damage Application Order:
    //  1. Base Damage of creature
    //  2. Multiplicators of self       (friends, berserk)
    //  3. Buffs from heroes            (Hunter, Rei, etc.)
    //  4. Elemental Advantage          (f.e. Fire vs. Earth)
    //  5. Protection of enemy Side     (Nimue, Athos, etc.)
    //  6. AOE of friendly Side         (Tiny, Alpha, etc.)
    //  7. Healing of enemy Side        (Auri, Aeris, etc.)
    
    totalFightsSimulated++;
    
    size_t i;
    int8_t turncounter = 0;
    
    size_t leftLost = 0;
    size_t leftArmySize = left.monsterAmount;
    Monster * leftLineup[6];
    
    int16_t leftFrontDamageTaken = 0;
    int16_t leftHealing = 0;
    int16_t leftCumAoeDamageTaken = 0;
    int8_t leftBerserkProcs = 0;
    
    size_t rightLost = 0;
    size_t rightArmySize = right.monsterAmount;
    Monster * rightLineup[6];
    
    int16_t rightFrontDamageTaken = 0;
    int16_t rightHealing = 0;
    int16_t rightCumAoeDamageTaken = 0;
    int8_t rightBerserkProcs = 0;
    
    // If no heroes are in the army the result from the smaller army is still valid
    if (left.lastFightData.valid && !verbose) { 
        // Set pre-computed values to pick up where we left off
        leftLost                = leftArmySize-1; // All monsters of left died last fight only the new one counts
        leftFrontDamageTaken    = left.lastFightData.leftAoeDamage;
        leftCumAoeDamageTaken   = left.lastFightData.leftAoeDamage;
        rightLost               = left.lastFightData.monstersLost;
        rightFrontDamageTaken   = left.lastFightData.damage;
        rightCumAoeDamageTaken  = left.lastFightData.rightAoeDamage;
        rightBerserkProcs       = left.lastFightData.berserk;
        turncounter             = left.lastFightData.turncounter;
    }
    
    // Values for skills  
    int16_t damageLeft, damageRight;
    int16_t damageBuffLeft, damageBuffRight;
    int16_t protectionLeft, protectionRight;
    int16_t aoeDamageLeft, aoeDamageRight;
    int16_t paoeDamageLeft, paoeDamageRight;
    int16_t healingLeft, healingRight;
    int8_t pureMonstersLeft, pureMonstersRight;
    int8_t rainbowConditionLeft, rainbowConditionRight;
    int8_t elementalDifference;
    
    // hero temp Variables
    HeroSkill * skill;
    SkillType skillTypeLeft[6];
    Element skillTargetLeft[6];
    float skillAmountLeft[6];
    SkillType skillTypeRight[6];
    Element skillTargetRight[6];
    float skillAmountRight[6];
    
    for (i = leftLost; i < leftArmySize; i++) {
        leftLineup[i] = &monsterReference[left.monsters[i]];
        skill = &leftLineup[i]->skill;
        rainbowConditionLeft |= 1 << leftLineup[i]->element;
        skillTypeLeft[i] = skill->type;
        if (skill->type == rainbow) {
            rainbowConditionLeft = 0;
        }
        skillTargetLeft[i] = skill->target;
        skillAmountLeft[i] = skill->amount;
    }
    
    for (i = rightLost; i < rightArmySize; i++) {
        rightLineup[i] = &monsterReference[right.monsters[i]];
        skill = &rightLineup[i]->skill;
        rainbowConditionRight |= 1 << rightLineup[i]->element;
        skillTypeRight[i] = skill->type;
        if (skill->type == rainbow) {
            rainbowConditionRight = 0;
        }
        skillTargetRight[i] = skill->target;
        skillAmountRight[i] = skill->amount;
    }
    
    while (true) {
        // Get all hero influences
        damageBuffLeft = 0;
        protectionLeft = 0;
        aoeDamageLeft = 0;
        paoeDamageLeft = 0;
        healingLeft = 0;
        pureMonstersLeft = 0;
        for (i = leftLost; i < leftArmySize; i++) {
            if (leftCumAoeDamageTaken >= leftLineup[i]->hp) { // Check for Backline Deaths
                leftLost += (leftLost == i);
            } else {
                if (skillTypeLeft[i] == nothing) {
                    pureMonstersLeft++; // count for friends ability
                } else if (skillTypeLeft[i] == protect && (skillTargetLeft[i] == all || skillTargetLeft[i] == leftLineup[leftLost]->element)) {
                    protectionLeft += skillAmountLeft[i];
                } else if (skillTypeLeft[i] == buff && (skillTargetLeft[i] == all || skillTargetLeft[i] == leftLineup[leftLost]->element)) {
                    damageBuffLeft += skillAmountLeft[i];
                } else if (skillTypeLeft[i] == champion && (skillTargetLeft[i] == all || skillTargetLeft[i] == leftLineup[leftLost]->element)) {
                    damageBuffLeft += skillAmountLeft[i];
                    protectionLeft += skillAmountLeft[i];
                } else if (skillTypeLeft[i] == heal) {
                    healingLeft += skillAmountLeft[i];
                } else if (skillTypeLeft[i] == aoe) {
                    aoeDamageLeft += skillAmountLeft[i];
                } else if (skillTypeLeft[i] == pAoe && i == leftLost) {
                    paoeDamageLeft += leftLineup[i]->damage;
                }
            }
        }
        
        damageBuffRight = 0;
        protectionRight = 0;
        aoeDamageRight = 0;
        paoeDamageRight = 0;
        healingRight = 0;
        pureMonstersRight = 0;
        for (i = rightLost; i < rightArmySize; i++) {
            if (rightCumAoeDamageTaken >= rightLineup[i]->hp) { // Check for Backline Deaths
                rightLost += (i == rightLost);
            } else {
                if (skillTypeRight[i] == nothing) {
                    pureMonstersRight++;  // count for friends ability
                } else if (skillTypeRight[i] == protect && (skillTargetRight[i] == all || skillTargetRight[i] == rightLineup[rightLost]->element)) {
                    protectionRight += skillAmountRight[i];
                } else if (skillTypeRight[i] == buff && (skillTargetRight[i] == all || skillTargetRight[i] == rightLineup[rightLost]->element)) {
                    damageBuffRight += skillAmountRight[i];
                } else if (skillTypeRight[i] == champion && (skillTargetRight[i] == all || skillTargetRight[i] == rightLineup[rightLost]->element)) {
                    damageBuffRight += skillAmountRight[i];
                    protectionRight += skillAmountRight[i];
                } else if (skillTypeRight[i] == heal) {
                    healingRight += skillAmountRight[i];
                } else if (skillTypeRight[i] == aoe) {
                    aoeDamageRight += skillAmountRight[i];
                } else if (skillTypeRight[i] == pAoe && i == rightLost) {
                    paoeDamageRight += rightLineup[i]->damage;
                }
            }
        }
        
        // Heal everything that hasnt died
        leftFrontDamageTaken -= leftHealing; // these values are from the last iteration
        leftCumAoeDamageTaken -= leftHealing;
        rightFrontDamageTaken -= rightHealing;
        rightCumAoeDamageTaken -= rightHealing;
        if (leftFrontDamageTaken < 0) {
            leftFrontDamageTaken = 0;
        }
        if (leftCumAoeDamageTaken < 0) {
            leftCumAoeDamageTaken = 0;
        }
        if (rightFrontDamageTaken < 0) {
            rightFrontDamageTaken = 0;
        }
        if (rightCumAoeDamageTaken < 0) {
            rightCumAoeDamageTaken = 0;
        }
        
        // Add last effects of abilities and start resolving the turn
        if (leftLost >= leftArmySize || rightLost >= rightArmySize) {
            break; // At least One army was beaten
        }
        
        // Get Base Damage for this Turn
        damageLeft = leftLineup[leftLost]->damage;
        damageRight = rightLineup[rightLost]->damage;
        
        // Handle Monsters with skills berserk or friends or training etc.
        if (skillTypeLeft[leftLost] == friends) {
            damageLeft *= pow(skillAmountLeft[leftLost], pureMonstersLeft);
        } else if (skillTypeLeft[leftLost] == adapt && leftLineup[leftLost]->element == rightLineup[rightLost]->element) {
            damageLeft *= skillAmountLeft[leftLost];
        } else if (skillTypeLeft[leftLost] == training) {
            damageLeft += skillAmountLeft[leftLost] * turncounter;
        } else if (skillTypeLeft[leftLost] == rainbow && rainbowConditionLeft == 15) {
            damageLeft += skillAmountLeft[leftLost];
        } else if (skillTypeLeft[leftLost] == berserk) {
            damageLeft *= pow(skillAmountLeft[leftLost], leftBerserkProcs);
            leftBerserkProcs++;
        }
        
        if (skillTypeRight[rightLost] == friends) {
            damageRight *= pow(skillAmountRight[rightLost], pureMonstersRight);
        } else if (skillTypeRight[rightLost] == adapt && rightLineup[rightLost]->element == leftLineup[leftLost]->element) {
            damageRight *= skillAmountRight[rightLost];
        } else if (skillTypeRight[rightLost] == training) {
            damageRight += skillAmountRight[rightLost] * turncounter;
        } else if (skillTypeRight[rightLost] == rainbow && rainbowConditionRight == 15) {
            damageRight += skillAmountRight[rightLost];
        } else if (skillTypeRight[rightLost] == berserk) {
            damageRight *= pow(skillAmountRight[rightLost], rightBerserkProcs);
            rightBerserkProcs++; 
        }
        
        // Add Buff Damage
        damageLeft += damageBuffLeft;
        damageRight += damageBuffRight;
        
        // Handle Elemental advantage
        elementalDifference = (leftLineup[leftLost]->element - rightLineup[rightLost]->element);
        if (elementalDifference == -1 || elementalDifference == 3) {
            damageLeft *= elementalBoost;
        } else if (elementalDifference == 1 || elementalDifference == -3) {
            damageRight *= elementalBoost;
        }
        
        // Handle Protection
        if (damageLeft > protectionRight) {
            damageLeft -= protectionRight;
        } else {
            damageLeft = 0;
        }
        
        if (damageRight > protectionLeft) {
            damageRight -= protectionLeft;
        } else {
            damageRight = 0; 
        }
        
        // Write values into permanent Variables for the next iteration
        rightFrontDamageTaken += damageLeft + aoeDamageLeft;
        rightCumAoeDamageTaken += aoeDamageLeft + paoeDamageLeft;
        rightHealing = healingRight;
        leftFrontDamageTaken += damageRight + aoeDamageRight;
        leftCumAoeDamageTaken += aoeDamageRight + paoeDamageRight;
        leftHealing = healingLeft;
        
        // Check if the first Monster died (otherwise it will be revived next turn)
        if (leftLineup[leftLost]->hp <= leftFrontDamageTaken) {
            leftLost++;
            leftBerserkProcs = 0;
            leftFrontDamageTaken = leftCumAoeDamageTaken;
        } else if (skillTypeLeft[leftLost] == wither) {
            leftFrontDamageTaken += (leftLineup[leftLost]->hp - leftFrontDamageTaken) * skillAmountLeft[leftLost];
        }
        if (rightLineup[rightLost]->hp <= rightFrontDamageTaken) {
            rightLost++;
            rightBerserkProcs = 0;
            rightFrontDamageTaken = rightCumAoeDamageTaken;
        } else if (skillTypeRight[rightLost] == wither) {
            rightFrontDamageTaken += (rightLineup[rightLost]->hp - rightFrontDamageTaken) * skillAmountRight[rightLost];
        }
        
        turncounter++;
        
        // Output detailed fight Data for debugging
        if (verbose) {
            cout << setw(3) << leftLost << " " << setw(3) << leftFrontDamageTaken << " " << setw(3) << rightLost << " " << setw(3) << rightFrontDamageTaken << endl;
        }
    }
    
    // write all the results into a FightResult
    left.lastFightData.dominated = false;
    left.lastFightData.turncounter = turncounter;
    left.lastFightData.leftAoeDamage = leftCumAoeDamageTaken;
    left.lastFightData.rightAoeDamage = rightCumAoeDamageTaken;
    
    if (leftLost >= leftArmySize) { //draws count as right wins. 
        left.lastFightData.rightWon = true;
        left.lastFightData.monstersLost = rightLost; 
        left.lastFightData.damage = rightFrontDamageTaken;
        left.lastFightData.berserk = rightBerserkProcs;
    } else {
        left.lastFightData.rightWon = false;
        left.lastFightData.monstersLost = leftLost; 
        left.lastFightData.damage = leftFrontDamageTaken;
        left.lastFightData.berserk = leftBerserkProcs;
    }
}