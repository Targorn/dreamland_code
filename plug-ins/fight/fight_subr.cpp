/* $Id: fight_subr.cpp,v 1.1.2.4 2008/05/27 21:30:02 rufina Exp $
 *
 * ruffina, 2004
 */
/***************************************************************************
 * Все права на этот код 'Dream Land' пренадлежат Igor {Leo} и Olga {Varda}*
 * Некоторую помощь в написании этого кода, а также своими идеями помогали:*
 *    Igor S. Petrenko            {NoFate, Demogorgon}                           *
 *    Koval Nazar            {Nazar, Redrum}                                    *
 *    Doropey Vladimir            {Reorx}                                           *
 *    Kulgeyko Denis            {Burzum}                                           *
 *    Andreyanov Aleksandr  {Manwe}                                           *
 *    и все остальные, кто советовал и играл в этот MUD                           *
 ***************************************************************************/

#include "skill.h"
#include "affect.h"
#include "room.h"
#include "pcharacter.h"
#include "dreamland.h"
#include "npcharacter.h"
#include "object.h"
#include "race.h"
#include "gsn_plugin.h"
#include "merc.h"
#include "mercdb.h"
#include "act.h"
#include "handler.h"
#include "interp.h"
#include "save.h"
#include "vnum.h"

#include "fight.h"
#include "def.h"

bool check_stun(Character *ch, Character *victim)
{
    if (IS_AFFECTED(ch, AFF_STUN)) {
        set_violent(ch, victim, false);

        // paralysis lasts up to level/30 ticks
        // chance to shrug paralysis increases by 15% each tick + with higher CON
        Affect *paf = ch->affected.find(gsn_paralysis);
        if (paf) {
            float chance, stat_mod, level_mod;
            //////////////// BASE MODIFIERS //////////////// TODO: add this to XML
            stat_mod = 0.03;
            level_mod = 0.02;
            chance = 0;
            //////////////// PROBABILITY ////////////////
            if (paf->duration >= 0)
                chance += (paf->level / 30 - paf->duration) * 15;                          // 15% per each tick, up to ~45%
            chance += (ch->getCurrStat(STAT_CON) - 20) * stat_mod * 100;                   // 3% per each CON point, up to 24%
            chance += (ch->getModifyLevel() - victim->getModifyLevel()) * level_mod * 100; // 2% per each lvl diff, up to ~20%
            chance = URANGE(5, chance, 95);

            int roll = number_percent( );
            // critical success: shrug the stun entirely
            if (roll <= (int)chance / 2) {
                affect_strip(ch, gsn_paralysis);
                ch->pecho("{1{GТвой паралич проходит, и ты снова можешь двигаться!{2");
                ch->recho("{1{GПаралич %1$C2 проходит, и %1$P1 снова начинает двигаться!{2", ch);
                REMOVE_BIT(ch->affected_by, AFF_STUN);
                return false; // can attack
            }
            // success: replace stun with weak stun
            else if (roll <= (int)chance) {
                affect_strip(ch, gsn_paralysis);
                ch->pecho("{1{MТвой паралич проходит, но ты все еще оглуше%1$Gно|н|на|ны!{2", ch);
                ch->recho("{1{MПаралич %1$C2 проходит, но %1$P1, похоже, все еще оглуше%1$Gно|н|на|ны!!{2", ch);
                REMOVE_BIT(ch->affected_by, AFF_STUN);
                SET_BIT(ch->affected_by, AFF_WEAK_STUN);
            } else {
                oldact_p("{WТы парализова$gно|н|на и не можешь реагировать на атаки $C2.{x",
                         ch, 0, victim, TO_CHAR, POS_FIGHTING);
                oldact_p("{W$c1 парализова$gно|н|на и не может реагировать на твои атаки.{x",
                         ch, 0, victim, TO_VICT, POS_FIGHTING);
                oldact_p("{W$c1 парализова$gно|н|на и не может реагировать на атаки.{x",
                         ch, 0, victim, TO_NOTVICT, POS_FIGHTING);
            }
            return true;
        }
        // other stuns -- not AP's paralysis
        else {
            ch->pecho("{1{MТвой паралич проходит, но ты все еще оглуше%1$Gно|н|на|ны!{2", ch);
            ch->recho("{1{MПаралич %1$C2 проходит, но %1$P1, похоже, все еще оглуше%1$Gно|н|на|ны!!{2", ch);
            REMOVE_BIT(ch->affected_by, AFF_STUN);
            SET_BIT(ch->affected_by, AFF_WEAK_STUN);
            return true;
        }
    }

    if (IS_AFFECTED(ch, AFF_WEAK_STUN)) {
        oldact_p("{MТы оглуше$gно|н|на и не можешь реагировать на атаки $C2.{x",
                 ch, 0, victim, TO_CHAR, POS_FIGHTING);
        oldact_p("{M$c1 оглуше$gно|н|на и не может реагировать на твои атаки.{x",
                 ch, 0, victim, TO_VICT, POS_FIGHTING);
        oldact_p("{M$c1 оглуше$gно|н|на и не может реагировать на атаки.{x",
                 ch, 0, victim, TO_NOTVICT, POS_FIGHTING);

        REMOVE_BIT(ch->affected_by, AFF_WEAK_STUN);

        set_violent(ch, victim, false);
        return true;
    }

    return false;
}

void check_assist(Character *ch, Character *victim)
{
    Character *rch, *rch_next;

    for (rch = ch->in_room->people; rch != 0; rch = rch_next) {
        rch_next = rch->next_in_room;
        
        if (!IS_AWAKE(rch) || rch->fighting || rch == ch || rch == victim)
            continue;
        
        /* mobile assistance */
        if (rch->is_npc( )) {
            if (rch->getNPC( )->behavior)
                rch->getNPC( )->behavior->assist( ch, victim );

            continue;
        }
        
        /* PC-master assist his charmices */
        if (ch->is_npc( )
            && IS_CHARMED(ch)
            && ch->master == rch)
        {
            oldact("Ты вступаешь в битву на стороне $C2.", rch, 0, ch, TO_CHAR);            
            one_hit( rch, victim );
            continue;
        }
        
        /* PC assist characters from his group */
        if (IS_SET(rch->act, PLR_AUTOASSIST)
            && is_same_group( ch, rch ))
        {
            oldact("Ты вступаешь в битву на стороне $C2.", rch, 0, ch, TO_CHAR);
            one_hit( rch, victim );
            continue;
        }
    }
}


bool check_bare_hands( Character *ch )
{
    Object *obj;

    for (obj = ch->carrying; obj; obj = obj->next_content)
        if (obj->wear_loc == wear_wield
            || obj->wear_loc == wear_second_wield
            || obj->wear_loc == wear_shield
            || obj->wear_loc == wear_hold)
            return false;
    
    return true;
}

void check_bloodthirst( Character *ch )
{
    Character *vch, *vch_next;

    if (dreamland->hasOption( DL_BUILDPLOT ))
        return;
    if (!IS_AFFECTED(ch, AFF_BLOODTHIRST))
        return;
    if (IS_AFFECTED(ch,AFF_CALM))
        return;        
    if (!IS_AWAKE(ch))
        return;
    if (ch->fighting)
        return;

    // Prevent bloodfirsty and stunned mobs from spam-attacking.
    if (ch->is_npc() && ch->wait > 0)
        return;
    
    for (vch = ch->in_room->people; vch && !ch->fighting; vch = vch_next)
    {
        vch_next = vch->next_in_room;

        if (ch != vch && ch->can_see(vch) && !is_safe_nomessage(ch, vch))
        {
            ch->pecho( "{RБОЛЬШЕ КРОВИ! БОЛЬШЕ КРОВИ! БОЛЬШЕ КРОВИ!!!{x" );
            REMOVE_BIT(ch->affected_by, AFF_CHARM);

            if (ch->is_npc( ) && ch->in_room) 
                save_mobs( ch->in_room );

            interpret_raw( ch, "murder",  vch->getDoppel( ch )->getNameC() );
        }
    }
}

