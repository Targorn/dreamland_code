/* $Id: room.h,v 1.1.2.7.6.9 2014-09-19 11:44:46 rufina Exp $
 * 
 * ruffina, 2004
 */
#ifndef __ROOM_H__
#define __ROOM_H__

#include <list>
#include <map>
#include <stdarg.h>

#include <stdarg.h>

#include "xmlmap.h"
#include "xmlboolean.h"
#include "globalbitvector.h"
#include "xmlstreamable.h"

#include "core/fenia/wrappertarget.h"
#include "clanreference.h"
#include "liquid.h"
#include "roombehavior.h"

struct AreaInstance;
struct extra_descr_data;
struct exit_data;
struct extra_exit_data;
struct reset_data;
class Affect;
typedef map<DLString, DLString> Properties;

struct RoomHistoryEntry {
    RoomHistoryEntry( DLString n, int w )
            : name( n ), went( w )
    {
    }
    DLString name;
    int went;
};

struct RoomHistory : public list<RoomHistoryEntry> {
    void record( Character *, int );
    void erase( );
    bool traverse( Room *, Character * ) const;
    int  went( Character * ) const;
    int  went( DLString &, bool ) const;
    void toStream( ostringstream & ) const;
    static const unsigned int MAX_SIZE;
};

struct RoomIndexData : public virtual DLObject, public WrapperTarget {
    RoomIndexData();
    
    reset_data *reset_first;
    reset_data *reset_last;
    extra_descr_data *        extra_descr;
    exit_data *        exit        [6];
    extra_exit_data * extra_exit;

    char *        name;
    char *        description;
    int         vnum;
    int         room_flags;
    int         sector_type;
    int         heal_rate;
    int         mana_rate;
    ClanReference clan;
    GlobalBitvector guilds;
    LiquidReference liquid;
    Properties properties;
    XMLDocumentPointer behavior;

    Scripting::Register init;

    AREA_DATA *area;
    RoomVector instances;
};

class Room : public virtual DLObject, public WrapperTarget {
public:
    Room( );

    bool isOwner( Character *ch ) const;
    bool isPrivate( ) const;
    int  getCapacity( ) const;
    bool isCommon( );

    // room affects
    void affectModify( Affect *paf, bool fAdd );
    void affectTo( Affect *paf );
    void affectJoin( Affect *paf );
    void affectCheck( int where, int vector );
    void affectRemove( Affect *paf );
    void affectStrip( int sn );
    bool isAffected( int sn ) const;
    
    bool isDark( ) const;
    /** Recalculate 'light' field for current room content. */
    void updateLight();

    void vecho( int, const char *, va_list ) const;
    void echo( int, const char *, ... ) const;
    void echoAround( int, const char *, ... ) const;

public:
    Character *        people;
    Object *        contents;
    extra_descr_data *        extra_descr;
    exit_data *        exit        [6];
    extra_exit_data * extra_exit;

    char *        name;
    char *        description;
    char *        owner;
    int                vnum;
    int        room_flags;
    int                light;
    int                sector_type;
    int                heal_rate;
    int         mana_rate;
    RoomHistory history;
    Affect        *affected;
    int        affected_by;

    XMLPersistentStreamable<RoomBehavior> behavior;
    Scripting::Register init;

    AreaInstance *areaInstance;

    RoomIndexData *pIndexData;
};


/*
 * room macros
 */

#define IS_ROOM_AFFECTED(room, sn)         (IS_SET((room)->affected_by, (sn)))
#define IS_RAFFECTED(room, sn)         (IS_SET((room)->affected_by, (sn)))
#define IS_WATER( var )                (((var)->sector_type == SECT_WATER_SWIM) || \
                                 ((var)->sector_type == SECT_WATER_NOSWIM) )
#define IS_NATURE(var)          ((var)->sector_type == SECT_FIELD || \
                                  (var)->sector_type == SECT_FOREST || \
                                  (var)->sector_type == SECT_HILLS || \
                                  (var)->sector_type == SECT_MOUNTAIN)

#endif
