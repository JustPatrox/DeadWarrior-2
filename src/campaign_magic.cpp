#include "campaign_tutorial.hpp"
#include <algorithm>
#include <cmath>

namespace sw2 {
bool CampaignTutorial::startSpell(Unit& u,int spell,Point at) {
    const char* names[]={"nuke","electricWall","poisonSpray"};
    const char* poses[]={"attack_1","electricAttack","poisonAttack"};
    const std::string prefix=names[spell-1];
    if (u.spell || u.spellCooldown[spell-1] || u.garrisoned ||
        std::hypot(at.x-u.position.x,at.y-u.position.y)>=stat(u,(prefix+".range").c_str())) return false;
    const int cost=int(stat(u,(prefix+".mana").c_str()));
    if (!u.enemy && mana_<cost) return false;
    if (!u.enemy) mana_-=cost;
    u.spell=spell; u.spellPoint=at; u.facing=at.x<u.position.x ? -1 : 1;
    u.spellCooldown[spell-1]=int(stat(u,(prefix+".cooldown").c_str()));
    animate(u,poses[spell-1]); u.hit=false;
    sound(spell==1 ? "fulminateSound" : spell==2 ? "electricWallSound" : "wizardPoisonSound",u.position);
    ++spells_[spell-1];
    return true;
}

void CampaignTutorial::castAt(Point at) {
    for (auto& u:units_) if (u.selected && !u.enemy && u.hp>0 && u.symbol==wizard) startSpell(u,aimedSpell_,at);
    aimedSpell_=0;
}

void CampaignTutorial::applyMagicDamage(Unit& target,double damage,bool enemy,bool hasInflictor) {
    if (target.hp<=0 || target.cloakTicks) return;
    const bool statue=target.symbol==1904;
    if(target.inBlock) {
        const double remaining=1-rules_.at("spearton.shieldWall.damageReduction");
        if(!hasInflictor)damage=int(int(damage)*remaining);
        damage*=remaining;
    }
    if (!statue && difficulty_==Difficulty::Normal) {
        if (target.enemy) damage/=rules_.at("normalHealthScale");
        if (enemy && rules_.contains("normalDamageScale")) damage*=rules_.at("normalDamageScale");
    }
    target.hp=std::max(0.,target.hp-damage);
}

bool CampaignTutorial::updateCaster(Unit& u) {
    for (auto& cooldown:u.spellCooldown) if (cooldown>0) --cooldown;
    if (!u.spell && !u.garrisoned) {
        if (u.symbol==wizard && u.enemy && u.target!=-1) {
            const auto& target=combatTarget(u.target);
            for (int i=0;i<3;++i) {
                if (!u.spellCooldown[i]) {
                    startSpell(u,i+1,target.position);
                    break;
                }
            }
        } else if (u.symbol==healer && !u.enemy && (u.order==Order::Stand || u.engageOnMove) &&
                   u.autoCure && upgrades_[8] && !u.spellCooldown[1] && mana_>=stat(u,"cure.mana")) {
            // Current EnemyGoodTeamAi grants Cloak and Magikill spells, not
            // MONK_CURE; the player's campaign upgrade never grants enemy tech.
            for (int i=0;i<int(units_.size());++i) {
                const auto& other=units_[i];
                if (other.enemy!=u.enemy || other.hp<=0 || other.poison<=0 ||
                    std::hypot(other.position.x-u.position.x,other.position.y-u.position.y)>=stat(u,"cure.range")) continue;
                u.spell=5; u.spellTarget=i; u.spellCooldown[1]=int(stat(u,"cure.cooldown"));
                mana_-=int(stat(u,"cure.mana")); animate(u,"attack_2"); u.hit=false;
                sound("PoisonCureSpellStart",u.position);
                break;
            }
        }
        // MonkAi auto-heals only when mayAttack is true. A plain Move has
        // priority, whereas Stand/Hold, AttackMove and an explicit target may heal.
        if (!u.spell && u.symbol==healer && (u.order!=Order::Move || u.engageOnMove) && u.autoHeal && !u.spellCooldown[0]) {
            int candidate=-1;
            for (int i=0;i<int(units_.size());++i) {
                const auto& other=units_[i];
                if (other.enemy!=u.enemy || other.hp<=0 || other.hp>=other.maxHp ||
                    !spatialAreaContains(u.position,100,other.position)) continue;
                if (candidate<0 || other.hp<units_[candidate].hp) candidate=i;
            }
            if (candidate>=0) {
                u.spell=4; u.spellTarget=candidate; u.spellCooldown[0]=int(stat(u,"heal.cooldown"));
                animate(u,"attack_1"); u.hit=false;
                sound("HealSpellFinish",u.position);
            }
        }
    }
    if (!u.spell) return false;
    const int trigger=u.spell==1 ? 36 : u.spell==2 ? 47 : u.spell==3 ? 44 : 25;
    if (u.frame==trigger && !u.hit) {
        u.hit=true;
        if (u.spell==4 || u.spell==5) {
            auto& target=units_.at(u.spellTarget);
            if (target.hp>0) {
                if (u.spell==5) {target.poison=0;++cures_;}
                else {
                    target.healTicks=int(stat(u,"heal.duration"));
                    target.healAmount=stat(u,"heal.amount");
                }
                ++heals_; sound(u.spell==5 ? "PoisonCureSpellFinish" : "HealSpellFinish",target.position);
            }
        } else {
            const int symbol=u.spell==1 ? 3172 : u.spell==2 ? 3120 : 3968;
            Point origin=u.position;
            if (u.spell==3) {
                origin.x+=u.facing*rules_.at("poison.originX")*scale(u);
                origin.y+=rules_.at("poison.originY")*scale(u);
            }
            magic_.push_back({symbol,1,origin,u.spellPoint,u.enemy,u.position.y});
            sound(u.spell==1 ? "mediumExplosion"+std::to_string(1+soundSequence_++%3) :
                u.spell==2 ? "ElectricWallSoundEffect" : "AcidSpraySound",u.spellPoint);
        }
    }
    if (u.frame>=animations_.at({u.symbol,u.pose}).length) {u.spell=0; animate(u,"run");}
    return true;
}

void CampaignTutorial::updateMagic() {
    for (auto& effect:magic_) {
        const auto point=effect.end;
        auto affect=[&](Unit& target) {
            if (target.hp<=0 || target.enemy==effect.enemy || target.cloakTicks ||
                (target.garrisoned && (target.enemy ? target.position.x>=length_-legacyWidth : target.position.x<=legacyWidth))) return;
            bool hit=false;
            double damage=0;
            if (effect.symbol==3172 && effect.frame==3) {
                hit=std::hypot(target.position.x-point.x,target.position.y-point.y)<rules_.at("nuke.radius");
                damage=effect.damage>=0 ? effect.damage : stat(wizard,"nuke.damage");
            } else if (effect.symbol==3120 && ticks_%int(stat(wizard,"electricWall.frequency"))==0) {
                hit=std::abs(target.position.x-point.x)<stat(wizard,"electricWall.area");
                damage=stat(wizard,"electricWall.damage");
            } else if (effect.symbol==3968 && effect.frame<=20 && target.symbol!=1904 && target.symbol!=1174) {
                const double fraction=effect.frame/20.;
                const Point current{effect.start.x+(point.x-effect.start.x)*fraction,
                                    effect.start.y+(point.y-effect.start.y)*fraction};
                if (std::hypot(target.position.x-current.x,target.position.y-current.y)<stat(wizard,"poisonSpray.area")) {
                    if (target.poison<stat(wizard,"poisonSpray.damage")) sound("PoisonedSound",target.position);
                    target.poison=std::max(target.poison,stat(wizard,"poisonSpray.damage")); ++magicHits_;
                }
            }
            if (hit) {applyMagicDamage(target,damage,effect.enemy); ++magicHits_;}
        };
        for (auto& target:units_) affect(target);
        for (auto& target:statues_) affect(target);
        if (wall_.hp>0) affect(wall_);
        ++effect.frame;
    }
    std::erase_if(magic_,[&](const MagicEffect& e){return e.frame>rules_.at("effect."+std::to_string(e.symbol)+".frames");});
}

void CampaignTutorial::drawMagic(std::vector<std::pair<double,Draw>>& layers) const {
    for (const auto& effect:magic_) {
        const Point at=effect.symbol==3968 ? effect.start : effect.end;
        const double size=rules_.at("backScale")+effect.depth/depth_*(rules_.at("frontScale")-rules_.at("backScale"));
        auto image=sprite(effect.symbol,effect.frame,{at.x-camera_,battlefieldY_+at.y},size);
        if (effect.symbol==3968) {
            const double angle=std::atan2(effect.end.y-effect.depth,effect.end.x-effect.start.x)*180/3.141592653589793;
            image=rotatedSprite(effect.symbol,effect.frame,{at.x-camera_,battlefieldY_+at.y},size,angle);
        }
        if (effect.symbol==3120) {
            image.target.y=battlefieldY_; image.target.h=depth_;
        }
        layers.emplace_back(effect.symbol==3120 ? depth_ : at.y,std::move(image));
    }
}
}
