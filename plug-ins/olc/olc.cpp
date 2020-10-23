/* $Id$
 *
 * ruffina, 2004
 */
#include <algorithm>
#include "grammar_entities_impl.h"
#include "dlfilestream.h"
#include "regexp.h"
#include <xmldocument.h>
#include "stringset.h"
#include "wrapperbase.h"
#include "dlscheduler.h"
#include "schedulertaskroundplugin.h"
#include "plugininitializer.h"

#include <skill.h>
#include <spell.h>
#include <skillmanager.h>
#include "feniamanager.h"
#include "areabehaviormanager.h"
#include <affect.h>
#include <object.h>
#include <pcharacter.h>
#include <npcharacter.h>
#include <commandmanager.h>
#include "race.h"
#include "clanreference.h"
#include "room.h"

#include "olc.h"
#include "olcflags.h"
#include "olcstate.h"
#include "security.h"
#include "areahelp.h"

#include "websocketrpc.h"
#include "interp.h"
#include "merc.h"
#include "handler.h"
#include "act.h"
#include "save.h"
#include "mudtags.h"
#include "act_move.h"
#include "vnum.h"
#include "mercdb.h"
#include "comm.h"
#include "def.h"


DLString format_longdescr_to_char(const char *descr, Character *ch);
GSN(enchant_weapon);
GSN(enchant_armor);
GSN(none);
CLAN(none);

using namespace std;


enum {
    NDX_ROOM,
    NDX_OBJ,
    NDX_MOB,
};

static int next_index_data( Character *ch, Room *r, int ndx_type )
{
    AREA_DATA *pArea;
    
    if (!r)
        return -1;

    pArea = r->area;
    if (!pArea)
        return -1;

    for (int i = pArea->min_vnum; i <= pArea->max_vnum; i++) {
        if (!OLCState::can_edit( ch, i ))
            continue;

        switch (ndx_type) {
        case NDX_ROOM:
            if (!get_room_index( i ))
                return i;
            break;
        case NDX_OBJ:
            if (!get_obj_index( i ))
                return i;
            break;
        case NDX_MOB:
            if (!get_mob_index( i ))
                return i;
            break;
        }
    }

    return -1;
}
    
int next_room( Character *ch, Room *r )
{
    return next_index_data( ch, r, NDX_ROOM );
}

int next_obj_index( Character *ch, Room *r )
{
    return next_index_data( ch, r, NDX_OBJ );
}

int next_mob_index( Character *ch, Room *r )
{
    return next_index_data( ch, r, NDX_MOB );
}

const char *
get_skill_name( int sn, bool verbose )
{
    Skill *skill = SkillManager::getThis( )->find( sn );

    if (skill)
        return skill->getName( ).c_str( );
    else if (verbose)
        return "none";
    else
        return "";
}

void
ptc(Character *ch, const char *fmt, ...)
{
    va_list av;

    va_start(av, fmt);

    DLString rc = vfmt(ch, fmt, av);
    stc(rc.c_str( ), ch);

    va_end(av);
}

void show_fenia_triggers(Character *ch, Scripting::Object *wrapper)
{
    WrapperBase *base = get_wrapper(wrapper);
    if (base) {
        StringSet triggers, misc;

        base->collectTriggers(triggers, misc);
        if (!triggers.empty()) 
            ptc(ch, "{gFenia triggers{x:           %s\r\n", triggers.toString().c_str());
        
        if (!misc.empty()) 
            ptc(ch, "{gFenia fields and methods{x: %s\r\n", misc.toString().c_str());
    }
}

/** Find area with given vnum. */
AREA_DATA *get_area_data(int vnum)
{
    AREA_DATA *pArea;

    for (pArea = area_first; pArea; pArea = pArea->next) {
        if (pArea->vnum == vnum)
            return pArea;
    }
    return 0;
}

void display_resets(Character * ch)
{
    Room *pRoom;
    RESET_DATA *pReset;
    MOB_INDEX_DATA *pMob = NULL;
    char buf[MAX_STRING_LENGTH];
    char final[MAX_STRING_LENGTH];
    int iReset = 0;

    pRoom = ch->in_room;

    final[0] = '\0';

    stc(
           " No.  Loads       Description          Location   Vnum   Mx Mn Description"
           "\n\r"
           "==== ======== ======================== ======== ======== ===== ==========="
           "\n\r", ch);

    for (pReset = pRoom->reset_first; pReset; pReset = pReset->next) {
        OBJ_INDEX_DATA *pObj;
        MOB_INDEX_DATA *pMobIndex;
        OBJ_INDEX_DATA *pObjIndex;
        OBJ_INDEX_DATA *pObjToIndex;
        Room *pRoomIndex;

        final[0] = '\0';
        sprintf(final, "[%2d] ", ++iReset);

        switch (pReset->command) {
        default:
            sprintf(buf, "Bad reset command: %c.", pReset->command);
            strcat(final, buf);
            break;
        case 'M':
            if (!(pMobIndex = get_mob_index(pReset->arg1))) {
                sprintf(buf, "Load Mobile - Bad Mob %u\n\r", pReset->arg1);
                strcat(final, buf);
                continue;
            }
            if (!(pRoomIndex = get_room_index(pReset->arg3))) {
                sprintf(buf, "Load Mobile - Bad Room %u\n\r", pReset->arg3);
                strcat(final, buf);
                continue;
            }

            pMob = pMobIndex;
            sprintf(buf, "M[%5u] %-24.24s %-8s R[%5u] %2d-%2d %-15.15s{x\n\r",
                      pReset->arg1, 
                      russian_case(pMob->short_descr, '1').colourStrip( ).c_str( ),
                      "in room", 
                      pReset->arg3,
                      pReset->arg2, 
                      pReset->arg4, 
                      pRoomIndex->name);
            strcat(final, buf);
            break;

        case 'O':
            if (!(pObjIndex = get_obj_index(pReset->arg1))) {
                sprintf(buf, "Load Object - Bad Object %u\n\r", pReset->arg1);
                strcat(final, buf);
                continue;
            }
            pObj = pObjIndex;
            if (!(pRoomIndex = get_room_index(pReset->arg3))) {
                sprintf(buf, "Load Object - Bad Room %u\n\r", pReset->arg3);
                strcat(final, buf);
                continue;
            }
            sprintf(buf, "O[%5u] %-24.24s %-8s R[%5u]       %-15.15s{x\n\r",
                      pReset->arg1, 
                      russian_case(pObj->short_descr, '1').colourStrip( ).c_str( ),
                      "in room",
                      pReset->arg3, 
                      pRoomIndex->name);
            strcat(final, buf);
            break;

        case 'P':
            if (!(pObjIndex = get_obj_index(pReset->arg1))) {
                sprintf(buf, "Put Object - Bad Object %u\n\r", pReset->arg1);
                strcat(final, buf);
                continue;
            }

            pObj = pObjIndex;

            if (!(pObjToIndex = get_obj_index(pReset->arg3))) {
                sprintf(buf, "Put Object - Bad To Object %u\n\r", pReset->arg3);
                strcat(final, buf);
                continue;
            }

            sprintf(buf, "O[%5u] %-24.24s %-8s O[%5u] %2d-%2d %-15.15s{x\n\r",
                      pReset->arg1,
                      russian_case(pObj->short_descr, '1').colourStrip( ).c_str( ),
                      "inside",
                      pReset->arg3,
                      pReset->arg2,
                      pReset->arg4,
                      russian_case(pObjToIndex->short_descr, '1').colourStrip( ).c_str( ));
            strcat(final, buf);
            break;

        case 'G':
        case 'E':
            if (!(pObjIndex = get_obj_index(pReset->arg1))) {
                sprintf(buf, "Give/Equip Object - Bad Object %u\n\r", pReset->arg1);
                strcat(final, buf);
                continue;
            }

            pObj = pObjIndex;

            if (!pMob) {
                sprintf(buf, "Give/Equip Object - No Previous Mobile\n\r");
                strcat(final, buf);
                break;
            }

            sprintf(buf,
                      "O[%5u] %-24.24s %-8.8s M[%5u]       %-15.15s{x\n\r",
                      pReset->arg1,
                      russian_case(pObj->short_descr, '1').colourStrip( ).c_str( ),
                      (pReset->command == 'G') ?
                          wear_none.getName( ).c_str( )
                          : wearlocationManager->find( pReset->arg3 )->getName( ).c_str( ),
                      pMob->vnum,
                      russian_case(pMob->short_descr, '1').colourStrip( ).c_str( ));
            strcat(final, buf);
            break;

        case 'D':
            pRoomIndex = get_room_index(pReset->arg1);
            sprintf(buf, "R[%5u] %s door of %-19s reset to %s{x\n\r",
                      pReset->arg1,
                      DLString(dirs[pReset->arg2].name).capitalize( ).c_str( ),
                      pRoomIndex->name,
                      door_resets_table.name(pReset->arg3).c_str());
            strcat(final, buf);

            break;
        case 'R':
            if (!(pRoomIndex = get_room_index(pReset->arg1))) {
                sprintf(buf, "Randomize Exits - Bad Room %u\n\r", pReset->arg1);
                strcat(final, buf);
                continue;
            }

            sprintf(buf, "R[%5u] Exits are randomized in %s{x\n\r",
                      pReset->arg1, pRoomIndex->name);
            strcat(final, buf);
            break;
        }
        stc(final, ch);
    }
}

struct editor_table_entry {
    const char *arg, *cmd;
} editor_table[] = {
    {"area",   "aedit"},
    {"room",   "redit"},
    {"object", "oedit"},
    {"mobile", "medit"},
    {"help",   "hmedit"},
    {NULL, 0,}
};

CMD(edit, 50, "", POS_DEAD, 103, LOG_ALWAYS, 
        "Online editor.")
{
    char command[MAX_INPUT_LENGTH];
    int cmd;

    argument = one_argument(argument, command);

    if (command[0] == '\0') {
//        do_help(ch, "olc");
        return;
    }

    for (cmd = 0; editor_table[cmd].arg != NULL; cmd++) {
        if (!str_prefix(command, editor_table[cmd].arg)) {
            interpret_raw(ch, editor_table[cmd].cmd, "%s", argument);
            return;
        }
    }

//    do_help(ch, "olc");
}

void add_reset(Room * room, RESET_DATA * pReset, int index)
{
    RESET_DATA *reset;
    int iReset = 0;

    if (!room->reset_first) {
        room->reset_first = pReset;
        room->reset_last = pReset;
        pReset->next = NULL;
        return;
    }
    index--;

    if (index == 0) {                /* First slot (1) selected. */
        pReset->next = room->reset_first;
        room->reset_first = pReset;
        return;
    }

    // If negative slot( <= 0 selected) then this will find the last.
    for (reset = room->reset_first; reset->next; reset = reset->next) {
        if (++iReset == index)
            break;
    }

    pReset->next = reset->next;
    reset->next = pReset;
    if (!pReset->next)
        room->reset_last = pReset;
}

CMD(resets, 50, "", POS_DEAD, 103, LOG_ALWAYS, 
        "Online resets editor.")
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char arg4[MAX_INPUT_LENGTH];
    char arg5[MAX_INPUT_LENGTH];
    char arg6[MAX_INPUT_LENGTH];
    char arg7[MAX_INPUT_LENGTH];
    RESET_DATA *pReset = NULL;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);
    argument = one_argument(argument, arg4);
    argument = one_argument(argument, arg5);
    argument = one_argument(argument, arg6);
    argument = one_argument(argument, arg7);
    
    if (!OLCState::can_edit( ch, ch->in_room->vnum )) {
        stc("Resets: Invalid security for editing this area.\n\r", ch);
        return;
    }

    if (arg1[0] == '\0') {
        if (ch->in_room->reset_first) {
            stc("Resets: M = mobile, R = room, O = object\n\r", ch);
            display_resets(ch);
        }
        else
            stc("No resets in this room.\n\r", ch);
    }

    if (is_number(arg1)) {
        Room *pRoom = ch->in_room;

        if (!str_cmp(arg2, "delete")) {
            int insert_loc = atoi(arg1);

            if (!ch->in_room->reset_first) {
                stc("No resets in this area.\n\r", ch);
                return;
            }
            if (insert_loc - 1 <= 0) {
                pReset = pRoom->reset_first;
                pRoom->reset_first = pRoom->reset_first->next;
                if (!pRoom->reset_first)
                    pRoom->reset_last = NULL;
            }
            else {
                int iReset = 0;
                RESET_DATA *prev = NULL;

                for (pReset = pRoom->reset_first; pReset; pReset = pReset->next) {
                    if (++iReset == insert_loc)
                        break;
                    prev = pReset;
                }
                if (!pReset) {
                    stc("Reset not found.\n\r", ch);
                    return;
                }
                if (prev)
                    prev->next = prev->next->next;
                else
                    pRoom->reset_first = pRoom->reset_first->next;

                for (pRoom->reset_last = pRoom->reset_first;
                     pRoom->reset_last->next;
                     pRoom->reset_last = pRoom->reset_last->next);
            }
            free_reset_data(pReset);
            SET_BIT(ch->in_room->area->area_flag, AREA_CHANGED);
            stc("Reset deleted.\n\r", ch);
        }
        else if ((!str_cmp(arg2, "mob") && is_number(arg3))
                 || (!str_cmp(arg2, "obj") && is_number(arg3))) {
            if (!str_cmp(arg2, "mob")) {
                pReset = new_reset_data();
                pReset->command = 'M';
                if (get_mob_index(is_number(arg3) ? atoi(arg3) : 1) == NULL) {
                    stc("Монстр не существует.\n\r", ch);
                    return;
                }
                pReset->arg1 = atoi(arg3);
                pReset->arg2 = is_number(arg4) ? atoi(arg4) : 1;        /* Max # */
                pReset->arg3 = ch->in_room->vnum;
                pReset->arg4 = is_number(arg5) ? atoi(arg5) : 1;        /* Min # */
            }
            else if (!str_cmp(arg2, "obj")) {
                pReset = new_reset_data();
                pReset->arg1 = atoi(arg3);
                if (!str_prefix(arg4, "inside")) {
                    pReset->command = 'P';
                    pReset->arg2 = 0;
                    if ((get_obj_index(is_number(arg5) ? atoi(arg5) : 1))->item_type != ITEM_CONTAINER) {
                        stc("Предмет не является контейнером.\n\r", ch);
                        return;
                    }
                    pReset->arg2 = is_number(arg6) ? atoi(arg6) : 1;
                    pReset->arg3 = is_number(arg5) ? atoi(arg5) : 1;
                    pReset->arg4 = is_number(arg7) ? atoi(arg7) : 1;
                }
                else if (!str_cmp(arg4, "room")) {
                    pReset = new_reset_data();
                    pReset->command = 'O';
                    if (get_obj_index(atoi(arg3)) == NULL) {
                        stc("Предметов с таким номером не существует.\n\r", ch);
                        return;
                    }
                    pReset->arg1 = atoi(arg3);
                    pReset->arg2 = 0;
                    pReset->arg3 = ch->in_room->vnum;
                    pReset->arg4 = 0;
                }
                else {
                    Wearlocation *wl;
                    
                    if (!( wl = wearlocationManager->findExisting( arg4 ) )) {
                        stc("Resets: '? wear-loc'\n\r", ch);
                        return;
                    }
                    pReset = new_reset_data();
                    if (get_obj_index(atoi(arg3)) == NULL) {
                        stc("Предметов с таким номером не существует.\n\r", ch);
                        return;
                    }
                    pReset->arg1 = atoi(arg3);
                    pReset->arg3 = wl->getIndex( );
                    if (pReset->arg3 == wear_none)
                        pReset->command = 'G';
                    else
                        pReset->command = 'E';
                }
            }
            add_reset(ch->in_room, pReset, atoi(arg1));
            SET_BIT(ch->in_room->area->area_flag, AREA_CHANGED);
            stc("Reset added.\n\r", ch);
        }
        else {
            stc("Syntax: RESET <number> OBJ <vnum> <wear_loc>\n\r", ch);
            stc("        RESET <number> OBJ <vnum> inside <vnum> [limit] [count]\n\r", ch);
            stc("        RESET <number> OBJ <vnum> room\n\r", ch);
            stc("        RESET <number> MOB <vnum> [max # area] [max # room]\n\r", ch);
            stc("        RESET <number> DELETE\n\r", ch);
        }
    }
}

CMD(alist, 50, "", POS_DEAD, 103, LOG_ALWAYS, 
        "List areas.")
{
    AREA_DATA *pArea;

    const DLString lineFormat = 
            "[" + web_cmd(ch, "aedit $1", "%3d") 
            + "] {%s%-29s {%s(%5u-%5u) %17s %s{w\n\r";

    ptc(ch, "[%3s] %-29s   (%5s-%5s) %-17s %s\n\r",
      "Num", "Area Name", "lvnum", "uvnum", "Filename", "Help");

    for (pArea = area_first; pArea; pArea = pArea->next) {
        DLString hedit = "";
        AreaHelp *ahelp = area_selfhelp(pArea);
        if (ahelp && ahelp->getID() > 0) {
            DLString id(ahelp->getID());
            // Mark zones without meaningful help articles with red asterix.
            DLString color = help_is_empty(*ahelp) ? " {R*{x": "";
            hedit = web_cmd(ch, "hedit " + id, "hedit " + id) + color;
        }

        // System areas are shown in grey colors.           
        const char *colorAreaName = IS_SET(pArea->area_flag, AREA_SYSTEM) ? "D" : "W";
        const char *colorAreaVnums = IS_SET(pArea->area_flag, AREA_SYSTEM) ? "D" : "w";

        ch->send_to(
            dlprintf(lineFormat.c_str(), 
                pArea->vnum, 
                colorAreaName,
                DLString(pArea->name).colourStrip().cutSize(29).c_str(),
                colorAreaVnums,
                pArea->min_vnum, pArea->max_vnum,
                pArea->area_file->file_name,
                hedit.c_str()));
    }
}

static DLString trim(const DLString& str, const string& chars = "\t\n\v\f\r ")
{
    DLString line = str;
    line.erase(line.find_last_not_of(chars) + 1);
    line.erase(0, line.find_first_not_of(chars));
    return line;
}


static DLString find_word_mention(const char *text, const list<RussianString> &words)
{
    DLString t = text;
    t.colourStrip();
    t.toLower();

    for (const auto &word: words)
        for (int c = Grammar::Case::NONE; c < Grammar::Case::MAX; c++) {
            DLString myword = word.decline(c);
            if (t.isName(myword))
                return myword;
        }

    return DLString::emptyString;
}

static DLString eraseNewLine(const DLString &original)
{
    DLString s = original;
    s.erase(s.find_last_not_of('\r') + 1);
    s.erase(s.find_last_not_of('\n') + 1);
    return s;
}

static DLString eraseExtraSpaces(const DLString &original)
{
    DLString s = original;

    // TODO retain only one space between characters, numbers or punctuation marks.
    return s;
}

static list<DLString> splitToList(const DLString &s)
{
    list<DLString> result;

    char buf[1024];
    istringstream is(s);

    while (is.getline(buf, sizeof(buf)))
        result.push_back(
            eraseNewLine(buf));

    return result;
}

static list<DLString> convertForMe(const list<DLString> &lines, Character *looker)
{
    list<DLString> result;

    for (auto &line: lines) {
        ostringstream out;
        mudtags_convert(line.c_str(), out, TAGS_CONVERT_VIS|TAGS_ENFORCE_NOCOLOR|TAGS_ENFORCE_WEB, looker);
        result.push_back(out.str());
    }

    return result;
}

static bool stringIsBlank(const DLString &s)
{
    for (DLString::size_type i = 0; i < s.size(); i++)
        if (!dl_isspace(s.at(i)))
            return false;

    return true;
}

struct TextFlattener {
    TextFlattener(const DLString &source, Character *looker) 
    {
        lines = splitToList(source);
        visLines = convertForMe(lines, looker);
        this->looker = looker;
    }

    void process() 
    {
        buf.clear();

        list<DLString>::const_iterator l, v;

        for (l = lines.begin(), v = visLines.begin(); l != lines.end(); ) {
            line = *l;
            visLine = *v;
            if (++l == lines.end()) {
                nextLine = LAST_LINE;
                nextVisLine = LAST_LINE;
            } else {
                nextLine = *l;
                nextVisLine = *(++v);
            }

            looker->printf("Processing line: [%s]\r\n", line.c_str());
            buf << eraseExtraSpaces(line);

            if (needsLineBreak()) {
                buf << endl;
                looker->println(" >>> BREAK");
            } 
            else {
                buf << " ";
                looker->println(" >>> SPACE");
            }
        }
    }

    DLString getResult() 
    {
        return buf.str();
    }

private:
    bool needsLineBreak()
    {
        if (endsInPunctuationMark())
            return true;
        if (nextLineBeginsWithSpace())
            return true;
        if (stringIsBlank(visLine))
            return true;

        return false;
    }

    bool nextLineBeginsWithSpace() 
    {
        if (nextVisLine == LAST_LINE)
            return false;

        int s;
        for (s = 0; s < nextVisLine.size(); s++)
            if (!dl_isspace(nextVisLine.at(s)))
                break;

        // Any spaces in the beginning of the line: assume it's a new paragraph.
        if (s > 0)
            return true;

        return false;
    }


    bool endsInPunctuationMark()
    {
        if (visLine.empty())
            return false;
        
        int s;
        for (s = visLine.size() - 1; s >= 0; s--) {
            LogStream::sendNotice() << "::" << " s=" << s << endl;
            if (!dl_isspace(visLine.at(s)))
                break;
        }

        if (s < 0)
            return false;

        char c = visLine.at(s);
        return c == '.' || c == '?' || c == '!' || c == ':';
    }

    static DLString LAST_LINE;

    list<DLString> lines;    
    list<DLString> visLines;
    ostringstream buf;

    DLString line;
    DLString nextLine;
    DLString visLine;
    DLString nextVisLine;

    Character *looker;
};

DLString TextFlattener::LAST_LINE = DLString::emptyString;

CMD(abc, 50, "", POS_DEAD, 106, LOG_ALWAYS, "")
{
    DLString args = argument;
    DLString arg = args.getOneArgument();

    if (arg == "water") {
        ostringstream buf;
        const DLString lineFormat = "[" + web_cmd(ch, "goto $1", "%5d") + "] {W%-29s{x ({C%s{x)";

        list<RussianString> water, air;
        water.push_back(RussianString("вод|а|ы|е|у|ой|е"));
        water.push_back(RussianString("водоем||а|у||ом|е" ));
        water.push_back(RussianString("водопад||а|у||ом|е" ));
        water.push_back(RussianString("озер|о|а|у|о|ом|е"));
        water.push_back(RussianString("болот|о|а|у|о|ом|е"));
        water.push_back(RussianString("мор|е|я|ю|е|ем|е" ));
        water.push_back(RussianString("рек|а|и|е|у|ой|е" ));
        water.push_back(RussianString("причал||а|у||ом|е" ));
        water.push_back(RussianString("лодк|а|и|е|у|ой|е" ));
        water.push_back(RussianString("трясин|а|ы|е|у|ой|е" ));
        water.push_back(RussianString("верф|ь|и|и|ь|ью|и" ));
        water.push_back(RussianString("набережн|ая|ой|ой|ую|ой|ой"));
        water.push_back(RussianString("корабл|ь|я|ю|ь|ем|я"));

        air.push_back(RussianString("облак|о|а|у|о|ом|е"));
        air.push_back(RussianString("облак|а|ов|ам|а|ами|ов"));
        air.push_back(RussianString("туч|а|и|е|у|ей|е" ));
        air.push_back(RussianString("туч|и||ам|и|ами|ах" ));

        buf << "Всех неводные комнаты с упоминанием воды, без флагов indoors и near_water:" << endl;

        for (Room *room = room_list; room; room = room->rnext) {
            if (IS_SET(room->room_flags, ROOM_INDOORS|ROOM_NEAR_WATER|ROOM_MANSION))
                continue;
            if (room->sector_type == SECT_UNDERWATER)
                continue;
            if (IS_WATER(room))
                continue;

            if (!room->isCommon() && room->clan == clan_none)
                continue;

            if (!str_cmp(room->area->area_file->file_name, "galeon.are"))
                continue;

            DLString myword;

            if (room->sector_type == SECT_AIR)
                myword = find_word_mention(room->description, air);
            else
                myword = find_word_mention(room->description, water);

            if (!myword.empty()) {
                buf << dlprintf(lineFormat.c_str(), room->vnum, room->name, myword.c_str()) << endl;
                continue;
            }

            if (room->sector_type == SECT_AIR)
                myword = find_word_mention(room->name, air);
            else
                myword = find_word_mention(room->name, water);

            if (!myword.empty())
                buf << dlprintf(lineFormat.c_str(), room->vnum, room->name, myword.c_str()) << endl;
        }

        

        page_to_char( buf.str( ).c_str( ), ch );
        return;
    }

    if (arg == "eexit") {
        ostringstream abuf, cbuf, mbuf;
        abuf << endl << "Экстравыходы везде:" << endl;
        mbuf << endl << "Экстравыходы в особняках и пригородах:" << endl;
        cbuf << endl << "Экстравыходы в кланах:" << endl;

        const DLString lineFormat = "[" + web_cmd(ch, "goto $1", "%5d") + "] %-35s{x [{C%s{x]";
        for (Room *room = room_list; room; room = room->rnext) {
            ostringstream *buf;
            if (IS_SET(room->room_flags, ROOM_MANSION) || !str_prefix("ht", room->area->area_file->file_name))
                buf = &mbuf;
            else if (room->clan != clan_none)
                buf = &cbuf;
            else
                buf = &abuf;
            for (EXTRA_EXIT_DATA *eexit = room->extra_exit; eexit; eexit = eexit->next) {
                (*buf) << dlprintf(lineFormat.c_str(), room->vnum, room->name, eexit->keyword) << endl;
            }
        }
        
        page_to_char( mbuf.str( ).c_str( ), ch );
        page_to_char( cbuf.str( ).c_str( ), ch );
        page_to_char( abuf.str( ).c_str( ), ch );

        return;
    }

    if (!ch->isCoder( ))
        return;

    if (arg == "objname") {
        int cnt = 0, hcnt = 0, rcnt = 0;
        ostringstream buf, hbuf, rbuf;

        for (int i = 0; i < MAX_KEY_HASH; i++)
        for (OBJ_INDEX_DATA *pObj = obj_index_hash[i]; pObj; pObj = pObj->next) {
            DLString longd = pObj->description;
            longd.colourstrip( );

            static RegExp pattern_rus("[а-я]");
            static RegExp pattern_longd("^.*\\(([-a-z ]+)\\).*$");
            
            if (!pattern_rus.match( longd )) 
                continue;

            if (IS_SET(pObj->area->area_flag, AREA_NOQUEST|AREA_WIZLOCK|AREA_HIDDEN))
                continue;
            
            {
                DLString names = DLString( pObj->name );
                RussianString rshortd(pObj->short_descr, pObj->gram_gender );
                DLString shortd = rshortd.decline( '7' ).colourStrip( );
                if (!arg_contains_someof( shortd, pObj->name )) {
                    rcnt ++;
                    rbuf << pObj->vnum << ": [" << rshortd.decline( '1' ) << "] [" << pObj->name << "]" <<  endl;
                }
            }
            if (!pattern_longd.match( longd )) {
                buf << pObj->vnum << ": [" << longd << "] [" << pObj->name << "]" <<  endl;
                cnt++;
            } else {
                RegExp::MatchVector matches = pattern_longd.subexpr( longd.c_str( ) );
                if (matches.size( ) < 1) {
                    buf << pObj->vnum << ": [" << longd << "] [" << pObj->short_descr << "]" <<  endl;
                    cnt++;
                } else {
                    
                    DLString hint = matches.front( );
                    if (!is_name( hint.c_str( ), pObj->name )) {
                        hbuf << dlprintf( "%6d : [%35s] hint [{G%10s{x] [{W%s{x]\r\n",
                                pObj->vnum, longd.c_str( ), hint.c_str( ), pObj->name );
                        hcnt++;
                    }
                }
            }
        }

//        ch->printf("Найдено %d длинных имен предметов без подсказок (пустых).\r\n", cnt);
//        page_to_char( buf.str( ).c_str( ), ch );
//        ch->printf("Найдено %d несоответствий подсказок в длинном имени предметов.\r\n", hcnt);
//        page_to_char( hbuf.str( ).c_str( ), ch );
        ch->printf("Найдено %d предметов где в short descr не встречаются имена.\r\n", rcnt);
        page_to_char( rbuf.str( ).c_str( ), ch );
        return;
    }

    if (arg == "rlim") {
        
        if (args.empty( )) {
            for (Object *obj = object_list; obj; obj = obj->next) {
                    if (obj->pIndexData->limit < 0)
                        continue;
                    if (obj->in_room == NULL)
                        continue;
                    if (obj->pIndexData->area == obj->in_room->area)
                        continue;
                    if (obj->timestamp > 0)
                        continue;
                    
                    ch->printf("Found %s [%d] in [%d] %s\r\n", 
                            obj->getShortDescr('1').c_str( ), obj->pIndexData->vnum,
                            obj->in_room->vnum, obj->in_room->area->name);
            }

            return;
        }

        if (!is_number( args.c_str( )))
            return;

        Room *r = get_room_index( atoi( args.c_str( ) ) );
        if (!r) {
            ch->println("Room vnum not found.");
            return;
        }

        for (Object *obj = r->contents; obj; obj = obj->next_content) {
                if (obj->pIndexData->limit < 0)
                    continue;
                if (obj->in_room == NULL)
                    continue;
                if (obj->pIndexData->area == obj->in_room->area)
                    continue;
                if (obj->timestamp > 0)
                    continue;

                obj->timestamp = 1531034011;
                save_items( obj->in_room );
                ch->printf("Marking %s [%d] in [%d] %s\r\n", 
                        obj->getShortDescr('1').c_str( ), obj->pIndexData->vnum,
                            obj->in_room->vnum, obj->in_room->area->name);
        }
        ch->println("Done marking limits.");
        return;
    }

    if (arg == "mobname") {
        int cnt = 0, hcnt = 0, rcnt = 0;
        ostringstream buf, hbuf, rbuf;

        for (int i = 0; i < MAX_KEY_HASH; i++)
        for (MOB_INDEX_DATA *pMob = mob_index_hash[i]; pMob; pMob = pMob->next) {
            DLString names = DLString(pMob->player_name).toLower();
            DLString longd = trim(pMob->long_descr).toLower();
            longd.colourstrip( );

            static RegExp pattern_rus("[а-я]");
            static RegExp pattern_longd("^.*\\(([-a-z ]+)\\).*$");
            
            if (!pattern_rus.match( longd )) 
                continue;

            if (IS_SET(pMob->area->area_flag, AREA_WIZLOCK|AREA_HIDDEN))
                continue;
            
            {
                RussianString rshortd(pMob->short_descr, MultiGender(pMob->sex, pMob->gram_number));
                DLString shortd = rshortd.decline( '7' ).colourStrip( ).toLower();
                if (!arg_contains_someof( shortd, pMob->player_name )) {
                    rcnt ++;
                    rbuf << pMob->vnum << ": [" << rshortd.decline( '1' ) << "] [" << pMob->player_name << "]" <<  endl;
                }
            }

            RegExp::MatchVector matches = pattern_longd.subexpr( longd.c_str( ) );
            if (matches.size( ) < 1) {
                buf << pMob->vnum << ": [" << longd << "] [" << pMob->short_descr << "]" <<  endl;
                cnt++;
            } else {
                
                DLString hint = matches.front( );
                if (!is_name(hint.c_str(), names.c_str())) {
                    hbuf << dlprintf( "%6d : [%35s] hint [{G%10s{x] [{W%s{x]\r\n",
                            pMob->vnum, longd.c_str( ), hint.c_str( ), pMob->player_name );
                    hcnt++;
                }
            }
        }

        ch->printf("\r\n{RНайдено %d несоответствий подсказок в длинном имени моба.{x\r\n", hcnt);
        page_to_char( hbuf.str( ).c_str( ), ch );
        return;
    }

    if (arg == "maxhelp") {
        ch->printf("Max help ID is %d.\r\n", helpManager->getLastID());
        return;
    }

    if (arg == "readroom") {
        Integer vnum;
        Room *room;

        if (args.empty() || !Integer::tryParse(vnum, args)) {
            ch->println("abc readroom <vnum>");
            return;
        }

        room = get_room_index(vnum);
        if (!room) {
            ch->printf("Room vnum [%d] not found.\r\n", vnum.getValue());
            return;
        }
        
        ch->printf("Loading room objects for '%s' [%d], check logs for details.\r\n", 
                    room->name, room->vnum);
        load_room_objects(room, const_cast<char *>("/tmp"), false);
        return;
    }

    if (arg == "line") {
        TextFlattener flat(ch->in_room->description, ch);
        flat.process();
        ch->println("Результат обработки текста:");
        ch->send_to(flat.getResult());
        ch->println("----------");
        return;
    }
}



