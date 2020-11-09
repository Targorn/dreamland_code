/* $Id: room.cpp,v 1.1.2.9.6.6 2009/09/19 00:53:17 rufina Exp $
 * 
 * ruffina, 2004
 */
#include "room.h"

#include "fenia/register-impl.h"

#include "skill.h"
#include "skillmanager.h"
#include "clanreference.h"
#include "profession.h"
#include "affect.h"
#include "object.h"
#include "character.h"
#include "mercdb.h"
#include "merc.h"
#include "def.h"

CLAN(none);
PROF(none);
LIQ(none);

RoomIndexData::RoomIndexData()
        : next(0), reset_first( 0 ), reset_last( 0 ),
          extra_descr(0), extra_exit(0),
          name(&str_empty[0]), description(&str_empty[0]), 
          vnum(0), room_flags(0),
          sector_type(0), heal_rate(100), mana_rate(100),
          clan( clan_none ),  guilds( professionManager ),
          liquid( liq_none ), area(0), room(0)
{
    for (int i = 0; i < DIR_SOMEWHERE; i++) 
        exit[i] = 0;
}

Room::Room( ) : 
                rnext( 0 ), aff_next( 0 ), 
                people( 0 ), contents( 0 ), extra_descr( 0 ),
                area( 0 ), extra_exit( 0 ),
                name(&str_empty[0]), description(&str_empty[0]), 
                owner(&str_empty[0]),
                vnum( 0 ), room_flags( 0 ), 
                light( 0 ), sector_type( 0 ),
                heal_rate( 100 ), mana_rate( 100 ), 
                affected( 0 ), affected_by( 0 ),
                behavior( RoomBehavior::NODE_NAME ),
                pIndexData(0)
{
    for (int i = 0; i < DIR_SOMEWHERE; i++) 
        exit[i] = 0;
}

bool Room::isOwner( Character *ch ) const
{
    if (owner == 0 || owner[0] == '\0')
        return false;

    return is_name( ch->getNameP( ), owner );
}

bool Room::isPrivate( ) const
{
    return getCapacity( ) == 0;
}

int Room::getCapacity( ) const
{
    Character *rch;
    int count;

    if (IS_SET( room_flags, ROOM_IMP_ONLY ))
        return 0;

    count = 0;
    for (rch = people; rch != 0; rch = rch->next_in_room)
        count++;

    if (IS_SET(room_flags, ROOM_PRIVATE))
        return max( 0, 2 - count );

    if (IS_SET(room_flags, ROOM_SOLITARY))
        return max( 0, 1 - count );

    return -1;
}


bool Room::isCommon( ) 
{
    if (IS_SET(room_flags, ROOM_IMP_ONLY | ROOM_GODS_ONLY | ROOM_HEROES_ONLY ))
        return false;

    if (IS_SET(room_flags,ROOM_NEWBIES_ONLY))
        return false;

    if (IS_SET(area->area_flag, AREA_WIZLOCK))
        return false;

    if (pIndexData->clan != clan_none)
        return false;
    
    if (!pIndexData->guilds.empty( ))
        return false;
    
    if (behavior && !behavior->isCommon( ))
        return false;

    return true;
}



/*
 * Room light
 */
bool Room::isDark( ) const
{
    if (light > 0)
        return false;

    if (IS_SET(room_flags, ROOM_DARK))
        return true;
    
    if (sector_type == SECT_INSIDE || sector_type == SECT_CITY)
        return false;

    if (weather_info.sunlight == SUN_LIGHT || weather_info.sunlight == SUN_RISE)
        return false;

    return true;
}

list<Character*> Room::getPeople ( )
{
        Character *tmp_vict;

        list<Character*> people;        

        for (tmp_vict = this->people; tmp_vict != 0; tmp_vict = tmp_vict->next_in_room)
        {
                people.push_back(tmp_vict);
        }

        return people;
}

