// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "cg_local.h"
#include "cg_raid_ui.h"

constexpr int32_t STAT_MINUS      = 10;  // num frame for '-' stats digit
constexpr const char *sb_nums[2][11] =
{
    {   "num_0", "num_1", "num_2", "num_3", "num_4", "num_5",
        "num_6", "num_7", "num_8", "num_9", "num_minus"
    },
    {   "anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5",
        "anum_6", "anum_7", "anum_8", "anum_9", "anum_minus"
    }
};

constexpr int32_t CHAR_WIDTH    = 16;
constexpr int32_t CONCHAR_WIDTH = 8;

static int32_t font_y_offset;

constexpr rgba_t alt_color { 112, 255, 52, 255 };

static cvar_t *scr_usekfont;

static cvar_t *scr_centertime;
static cvar_t *scr_printspeed;
static cvar_t *cl_notifytime;
static cvar_t *scr_maxlines;
static cvar_t *ui_acc_contrast;
static cvar_t* ui_acc_alttypeface;

// static temp data used for hud
static struct
{
    struct {
        struct {
            char    text[24];
        } table_cells[6];
    } table_rows[11]; // just enough to store 8 levels + header + total (+ one slack)

    size_t column_widths[6];
    int32_t num_rows = 0;
    int32_t num_columns = 0;
} hud_temp;

#include <vector>

// max number of centerprints in the rotating buffer
constexpr size_t MAX_CENTER_PRINTS = 4;

struct cl_bind_t {
    std::string bind;
    std::string purpose;
};

struct cl_centerprint_t {
    std::vector<cl_bind_t> binds; // binds

    std::vector<std::string> lines;
    bool        instant; // don't type out

    size_t      current_line; // current line we're typing out
    size_t      line_count; // byte count to draw on current line
    bool        finished; // done typing it out
    uint64_t    time_tick, time_off; // time to remove at
};

inline bool CG_ViewingLayout(const player_state_t *ps)
{
    return ps->stats[STAT_LAYOUTS] & (LAYOUTS_LAYOUT | LAYOUTS_INVENTORY);
}

inline bool CG_InIntermission(const player_state_t *ps)
{
    return ps->stats[STAT_LAYOUTS] & LAYOUTS_INTERMISSION;
}

inline bool CG_HudHidden(const player_state_t *ps)
{
    return ps->stats[STAT_LAYOUTS] & LAYOUTS_HIDE_HUD;
}

layout_flags_t CG_LayoutFlags(const player_state_t *ps)
{
    return (layout_flags_t) ps->stats[STAT_LAYOUTS];
}

#include <optional>
#include <array>

constexpr size_t MAX_NOTIFY = 8;

struct cl_notify_t {
    std::string     message; // utf8 message
    bool            is_active; // filled or not
    bool            is_chat; // green or not
    uint64_t        time; // rotate us when < CL_Time()
};

// per-splitscreen client hud storage
struct hud_data_t {
    std::array<cl_centerprint_t, MAX_CENTER_PRINTS> centers; // list of centers
    std::optional<size_t> center_index; // current index we're drawing, or unset if none left
    std::array<cl_notify_t, MAX_NOTIFY> notify; // list of notifies
};

static std::array<hud_data_t, MAX_SPLIT_PLAYERS> hud_data;

void CG_ClearCenterprint(int32_t isplit)
{
    hud_data[isplit].center_index = {};
}

void CG_ClearNotify(int32_t isplit)
{
    for (auto &msg : hud_data[isplit].notify)
        msg.is_active = false;
}

// if the top one is expired, cycle the ones ahead backwards (since
// the times are always increasing)
static void CG_Notify_CheckExpire(hud_data_t &data)
{
    while (data.notify[0].is_active && data.notify[0].time < cgi.CL_ClientTime())
    {
        data.notify[0].is_active = false;

        for (size_t i = 1; i < MAX_NOTIFY; i++)
            if (data.notify[i].is_active)
                std::swap(data.notify[i], data.notify[i - 1]);
    }
}

// add notify to list
static void CG_AddNotify(hud_data_t &data, const char *msg, bool is_chat)
{
    size_t i = 0;

    if (scr_maxlines->integer <= 0)
        return;

    const int max = min(MAX_NOTIFY, (size_t)scr_maxlines->integer);

    for (; i < max; i++)
        if (!data.notify[i].is_active)
            break;

    // none left, so expire the topmost one
    if (i == max)
    {
        data.notify[0].time = 0;
        CG_Notify_CheckExpire(data);
        i = max - 1;
    }
    
    data.notify[i].message.assign(msg);
    data.notify[i].is_active = true;
    data.notify[i].is_chat = is_chat;
    data.notify[i].time = cgi.CL_ClientTime() + (cl_notifytime->value * 1000);
}

// draw notifies
static void CG_DrawNotify(int32_t isplit, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale)
{
    auto &data = hud_data[isplit];

    CG_Notify_CheckExpire(data);

    int y;
    
    y = (hud_vrect.y * scale) + hud_safe.y;

    cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);

    if (ui_acc_contrast->integer)
    {
        for (auto& msg : data.notify)
        {
            if (!msg.is_active || !msg.message.length())
                break;

            vec2_t sz = cgi.SCR_MeasureFontString(msg.message.c_str(), scale);
            sz.x += 10; // extra padding for black bars
            cgi.SCR_DrawColorPic((hud_vrect.x * scale) + hud_safe.x - 5, y, sz.x, 15 * scale, "_white", rgba_black);
            y += 10 * scale;
        }
    }

    y = (hud_vrect.y * scale) + hud_safe.y;
    for (auto &msg : data.notify)
    {
        if (!msg.is_active)
            break;

        cgi.SCR_DrawFontString(msg.message.c_str(), (hud_vrect.x * scale) + hud_safe.x, y, scale, msg.is_chat ? alt_color : rgba_white, true, text_align_t::LEFT);
        y += 10 * scale;
    }

    cgi.SCR_SetAltTypeface(false);

    // draw text input (only the main player can really chat anyways...)
    if (isplit == 0)
    {
        const char *input_msg;
        bool input_team;

        if (cgi.CL_GetTextInput(&input_msg, &input_team))
            cgi.SCR_DrawFontString(G_Fmt("{}: {}", input_team ? "say_team" : "say", input_msg).data(), (hud_vrect.x * scale) + hud_safe.x, y, scale, rgba_white, true, text_align_t::LEFT);
    }
}

/*
==============
CG_DrawHUDString
==============
*/
static int CG_DrawHUDString (const char *string, int x, int y, int centerwidth, int _xor, int scale, bool shadow = true)
{
    int     margin;
    char    line[1024];
    int     width;
    int     i;

    margin = x;

    while (*string)
    {
        // scan out one line of text from the string
        width = 0;
        while (*string && *string != '\n')
            line[width++] = *string++;
        line[width] = 0;

        vec2_t size;
        
        if (scr_usekfont->integer)
            size = cgi.SCR_MeasureFontString(line, scale);

        if (centerwidth)
        {
            if (!scr_usekfont->integer)
                x = margin + ((centerwidth - width*CONCHAR_WIDTH*scale))/2;
            else
                x = margin + ((centerwidth - size.x))/2;
        }
        else
            x = margin;

        if (!scr_usekfont->integer)
        {
            for (i=0 ; i<width ; i++)
            {
                cgi.SCR_DrawChar (x, y, scale, line[i]^_xor, shadow);
                x += CONCHAR_WIDTH * scale;
            }
        }
        else
        {
            cgi.SCR_DrawFontString(line, x, y - (font_y_offset * scale), scale, _xor ? alt_color : rgba_white, true, text_align_t::LEFT);
            x += size.x;
        }

        if (*string)
        {
            string++;   // skip the \n
            x = margin;
            if (!scr_usekfont->integer)
                y += CONCHAR_WIDTH * scale;
            else
                // TODO
                y += 10 * scale;//size.y;
        }
    }

    return x;
}

// Shamefully stolen from Kex
size_t FindStartOfUTF8Codepoint(const std::string &str, size_t pos)
{
    if(pos >= str.size())
    {
        return std::string::npos;
    }

    for(ptrdiff_t i = pos; i >= 0; i--)
    {
        const char &ch = str[i];

        if((ch & 0x80) == 0)
        {
            // character is one byte
            return i;
        }
        else if((ch & 0xC0) == 0x80)
        {
            // character is part of a multi-byte sequence, keep going
            continue;
        }
        else
        {
            // character is the start of a multi-byte sequence, so stop now
            return i;
        }
    }

    return std::string::npos;
}

size_t FindEndOfUTF8Codepoint(const std::string &str, size_t pos)
{
    if(pos >= str.size())
    {
        return std::string::npos;
    }

    for(size_t i = pos; i < str.size(); i++)
    {
        const char &ch = str[i];

        if((ch & 0x80) == 0)
        {
            // character is one byte
            return i;
        }
        else if((ch & 0xC0) == 0x80)
        {
            // character is part of a multi-byte sequence, keep going
            continue;
        }
        else
        {
            // character is the start of a multi-byte sequence, so stop now
            return i;
        }
    }

    return std::string::npos;
}

void CG_NotifyMessage(int32_t isplit, const char *msg, bool is_chat)
{
    CG_AddNotify(hud_data[isplit], msg, is_chat);
}

// centerprint stuff
static cl_centerprint_t &CG_QueueCenterPrint(int isplit, bool instant)
{
    auto &icl = hud_data[isplit];

    // just use first index
    if (!icl.center_index.has_value() || instant)
    {
        icl.center_index = 0;

        for (size_t i = 1; i < MAX_CENTER_PRINTS; i++)
            icl.centers[i].lines.clear();

        return icl.centers[0];
    }

    // pick the next free index if we can find one
    for (size_t i = 1; i < MAX_CENTER_PRINTS; i++)
    {
        auto &center = icl.centers[(icl.center_index.value() + i) % MAX_CENTER_PRINTS];

        if (center.lines.empty())
            return center;
    }
    
    // none, so update the current one (the new end of buffer)
    // and skip ahead
    auto &center = icl.centers[icl.center_index.value()];
    icl.center_index = (icl.center_index.value() + 1) % MAX_CENTER_PRINTS;
    return center;
}

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void CG_ParseCenterPrint (const char *str, int isplit, bool instant) // [Sam-KEX] Made 1st param const
{
    const char    *s;
    char    line[64];
    int     i, j, l;

    // handle center queueing
    cl_centerprint_t &center = CG_QueueCenterPrint(isplit, instant);

    center.lines.clear();

    // split the string into lines
    size_t line_start = 0;

    std::string string(str);

    center.binds.clear();

    // [Paril-KEX] pull out bindings. they'll always be at the start
    while (string.compare(0, 6, "%bind:") == 0)
    {
        size_t end_of_bind = string.find_first_of('%', 1);

        if (end_of_bind == std::string::npos)
            break;

        std::string bind = string.substr(6, end_of_bind - 6);

        if (auto purpose_index = bind.find_first_of(':'); purpose_index != std::string::npos)
            center.binds.emplace_back(cl_bind_t { bind.substr(0, purpose_index), bind.substr(purpose_index + 1) });
        else
            center.binds.emplace_back(cl_bind_t { bind });

        string = string.substr(end_of_bind + 1);
    }

    // echo it to the console
    cgi.Com_Print("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");

    s = string.c_str();
    do
    {
        // scan the width of the line
        for (l=0 ; l<40 ; l++)
            if (s[l] == '\n' || !s[l])
                break;
        for (i=0 ; i<(40-l)/2 ; i++)
            line[i] = ' ';

        for (j=0 ; j<l ; j++)
        {
            line[i++] = s[j];
        }

        line[i] = '\n';
        line[i+1] = 0;

        cgi.Com_Print(line);

        while (*s && *s != '\n')
            s++;

        if (!*s)
            break;
        s++;        // skip the \n
    } while (1);
    cgi.Com_Print("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
    CG_ClearNotify (isplit);

    for (size_t line_end = 0; ; )
    {
        line_end = FindEndOfUTF8Codepoint(string, line_end);

        if (line_end == std::string::npos)
        {
            // final line
            if (line_start < string.size())
                center.lines.emplace_back(string.c_str() + line_start);
            break;
        }
        
        // char part of current line;
        // if newline, end line and cut off
        const char &ch = string[line_end];

        if (ch == '\n')
        {
            if (line_end > line_start)
                center.lines.emplace_back(string.c_str() + line_start, line_end - line_start);
            else
                center.lines.emplace_back();
            line_start = line_end + 1;
            line_end++;
            continue;
        }

         line_end++;
    }

    if (center.lines.empty())
    {
        center.finished = true;
        return;
    }

    center.time_tick = cgi.CL_ClientRealTime() + (scr_printspeed->value * 1000);
    center.instant = instant;
    center.finished = false;
    center.current_line = 0;
    center.line_count = 0;
}

static void CG_DrawCenterString( const player_state_t *ps, const vrect_t &hud_vrect, const vrect_t &hud_safe, int isplit, int scale, cl_centerprint_t &center)
{
    int32_t y = hud_vrect.y * scale;
    
    if (CG_ViewingLayout(ps))
        y += hud_safe.y;
    else if (center.lines.size() <= 4)
        y += (hud_vrect.height * 0.2f) * scale;
    else
        y += 48 * scale;

    int lineHeight = (scr_usekfont->integer ? 10 : 8) * scale;
    if (ui_acc_alttypeface->integer) lineHeight *= 1.5f;

    // easy!
    if (center.instant)
    {
        for (size_t i = 0; i < center.lines.size(); i++)
        {
            auto &line = center.lines[i];

            cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);

            if (ui_acc_contrast->integer && line.length())
            {
                vec2_t sz = cgi.SCR_MeasureFontString(line.c_str(), scale);
                sz.x += 10; // extra padding for black bars
                int barY = ui_acc_alttypeface->integer ? y - 8 : y;
                cgi.SCR_DrawColorPic((hud_vrect.x + hud_vrect.width / 2) * scale - (sz.x / 2), barY, sz.x, lineHeight, "_white", rgba_black);
            }
            CG_DrawHUDString(line.c_str(), (hud_vrect.x + hud_vrect.width/2 + -160) * scale, y, (320 / 2) * 2 * scale, 0, scale);

            cgi.SCR_SetAltTypeface(false);

            y += lineHeight;
        }

        for (auto &bind : center.binds)
        {
            y += lineHeight * 2;
            cgi.SCR_DrawBind(isplit, bind.bind.c_str(), bind.purpose.c_str(), (hud_vrect.x + (hud_vrect.width / 2)) * scale, y, scale);
        }

        if (!center.finished)
        {
            center.finished = true;
            center.time_off = cgi.CL_ClientRealTime() + (scr_centertime->value * 1000);
        }

        return;
    }

    // hard and annoying!
    // check if it's time to fetch a new char
    const uint64_t t = cgi.CL_ClientRealTime();

    if (!center.finished)
    {
        if (center.time_tick < t)
        {
            center.time_tick = t + (scr_printspeed->value * 1000);
            center.line_count = FindEndOfUTF8Codepoint(center.lines[center.current_line], center.line_count + 1);

            if (center.line_count == std::string::npos)
            {
                center.current_line++;
                center.line_count = 0;

                if (center.current_line == center.lines.size())
                {
                    center.current_line--;
                    center.finished = true;
                    center.time_off = t + (scr_centertime->value * 1000);
                }
            }
        }
    }

    // smallish byte buffer for single line of data...
    char buffer[256];

    for (size_t i = 0; i < center.lines.size(); i++)
    {
        cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);

        auto &line = center.lines[i];

        buffer[0] = 0;

        if (center.finished || i != center.current_line)
            Q_strlcpy(buffer, line.c_str(), sizeof(buffer));
        else
            Q_strlcpy(buffer, line.c_str(), min(center.line_count + 1, sizeof(buffer)));

        int blinky_x;

        if (ui_acc_contrast->integer && line.length())
        {
            vec2_t sz = cgi.SCR_MeasureFontString(line.c_str(), scale);
            sz.x += 10; // extra padding for black bars
            int barY = ui_acc_alttypeface->integer ? y - 8 : y;
            cgi.SCR_DrawColorPic((hud_vrect.x + hud_vrect.width / 2) * scale - (sz.x / 2), barY, sz.x, lineHeight, "_white", rgba_black);
        }
        
        if (buffer[0])
            blinky_x = CG_DrawHUDString(buffer, (hud_vrect.x + hud_vrect.width/2 + -160) * scale, y, (320 / 2) * 2 * scale, 0, scale);
        else
            blinky_x = (hud_vrect.width / 2) * scale;

        cgi.SCR_SetAltTypeface(false);

        if (i == center.current_line && !ui_acc_alttypeface->integer)
            cgi.SCR_DrawChar(blinky_x, y, scale, 10 + ((cgi.CL_ClientRealTime() >> 8) & 1), true);

        y += lineHeight;

        if (i == center.current_line)
            break;
    }
}

static void CG_CheckDrawCenterString( const player_state_t *ps, const vrect_t &hud_vrect, const vrect_t &hud_safe, int isplit, int scale )
{
    if (CG_InIntermission(ps))
        return;
    if (!hud_data[isplit].center_index.has_value())
        return;

    auto &data = hud_data[isplit];
    auto &center = data.centers[data.center_index.value()];

    // ran out of center time
    if (center.finished && center.time_off < cgi.CL_ClientRealTime())
    {
        center.lines.clear();

        size_t next_index = (data.center_index.value() + 1) % MAX_CENTER_PRINTS;
        auto &next_center = data.centers[next_index];

        // no more
        if (next_center.lines.empty())
        {
            data.center_index.reset();
            return;
        }

        // buffer rotated; start timer now
        data.center_index = next_index;
        next_center.current_line = next_center.line_count = 0;
    }

    if (!data.center_index.has_value())
        return;

    CG_DrawCenterString( ps, hud_vrect, hud_safe, isplit, scale, data.centers[data.center_index.value()] );
}

/*
==============
CG_DrawString
==============
*/
static void CG_DrawString (int x, int y, int scale, const char *s, bool alt = false, bool shadow = true)
{
    while (*s)
    {
        cgi.SCR_DrawChar (x, y, scale, *s ^ (alt ? 0x80 : 0), shadow);
        x+=8*scale;
        s++;
    }
}

#include <charconv>

/*
==============
CG_DrawField
==============
*/
static void CG_DrawField (int x, int y, int color, int width, int value, int scale)
{
    char    num[16], *ptr;
    int     l;
    int     frame;

    if (width < 1)
        return;

    // draw number string
    if (width > 5)
        width = 5;

    auto result = std::to_chars(num, num + sizeof(num) - 1, value);
    *(result.ptr) = '\0';

    l = (result.ptr - num);

    if (l > width)
        l = width;

    x += (2 + CHAR_WIDTH*(width - l)) * scale;

    ptr = num;
    while (*ptr && l)
    {
        if (*ptr == '-')
            frame = STAT_MINUS;
        else
            frame = *ptr -'0';
        int w, h;
        cgi.Draw_GetPicSize(&w, &h, sb_nums[color][frame]);
        cgi.SCR_DrawPic(x, y, w * scale, h * scale, sb_nums[color][frame]);
        x += CHAR_WIDTH * scale;
        ptr++;
        l--;
    }
}

// [Paril-KEX]
static void CG_DrawTable(int x, int y, uint32_t width, uint32_t height, int32_t scale)
{
    // half left
    int32_t width_pixels = width;
    x -= width_pixels / 2;
    y += CONCHAR_WIDTH * scale;
    // use Y as top though

    int32_t height_pixels = height;
    
    // draw border
    // KEX_FIXME method that requires less chars
    cgi.SCR_DrawChar(x - (CONCHAR_WIDTH * scale), y - (CONCHAR_WIDTH * scale), scale, 18, false);
    cgi.SCR_DrawChar((x + width_pixels), y - (CONCHAR_WIDTH * scale), scale, 20, false);
    cgi.SCR_DrawChar(x - (CONCHAR_WIDTH * scale), y + height_pixels, scale, 24, false);
    cgi.SCR_DrawChar((x + width_pixels), y + height_pixels, scale, 26, false);

    for (int cx = x; cx < x + width_pixels; cx += CONCHAR_WIDTH * scale)
    {
        cgi.SCR_DrawChar(cx, y - (CONCHAR_WIDTH * scale), scale, 19, false);
        cgi.SCR_DrawChar(cx, y + height_pixels, scale, 25, false);
    }

    for (int cy = y; cy < y + height_pixels; cy += CONCHAR_WIDTH * scale)
    {
        cgi.SCR_DrawChar(x - (CONCHAR_WIDTH * scale), cy, scale, 21, false);
        cgi.SCR_DrawChar((x + width_pixels), cy, scale, 23, false);
    }

    cgi.SCR_DrawColorPic(x, y, width_pixels, height_pixels, "_white", { 0, 0, 0, 255 });

    // draw in columns
    for (int i = 0; i < hud_temp.num_columns; i++)
    {
        for (int r = 0, ry = y; r < hud_temp.num_rows; r++, ry += (CONCHAR_WIDTH + font_y_offset) * scale)
        {
            int x_offset = 0;

            // center 
            if (r == 0)
            {
                x_offset = ((hud_temp.column_widths[i]) / 2) -
                    ((cgi.SCR_MeasureFontString(hud_temp.table_rows[r].table_cells[i].text, scale).x) / 2);
            }
            // right align
            else if (i != 0)
            {
                x_offset = (hud_temp.column_widths[i] - cgi.SCR_MeasureFontString(hud_temp.table_rows[r].table_cells[i].text, scale).x);
            }

            //CG_DrawString(x + x_offset, ry, scale, hud_temp.table_rows[r].table_cells[i].text, r == 0, true);
            cgi.SCR_DrawFontString(hud_temp.table_rows[r].table_cells[i].text, x + x_offset, ry - (font_y_offset * scale), scale, r == 0 ? alt_color : rgba_white, true, text_align_t::LEFT);
        }

        x += (hud_temp.column_widths[i] + cgi.SCR_MeasureFontString(" ", 1).x);
    }
}

/*
================
CG_ExecuteLayoutString

================
*/
static void CG_ExecuteLayoutString (const char *s, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale, int32_t playernum, const player_state_t *ps)
{
    int     x, y;
    int     w, h;
    int     hx, hy;
    int     value;
    const char *token;
    int     width;
    int     index;

    if (!s[0])
        return;

    x = hud_vrect.x;
    y = hud_vrect.y;
    width = 3;

    hx = 320 / 2;
    hy = 240 / 2;

    bool flash_frame = (cgi.CL_ClientTime() % 1000) < 500;

    // if non-zero, parse but don't affect state
    int32_t if_depth = 0; // current if statement depth
    int32_t endif_depth = 0; // at this depth, toggle skip_depth
    bool skip_depth = false; // whether we're in a dead stmt or not

    while (s)
    {
        token = COM_Parse (&s);
        if (!strcmp(token, "xl"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
                x = ((hud_vrect.x + atoi(token)) * scale) + hud_safe.x;
            continue;
        }
        if (!strcmp(token, "xr"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
                x = ((hud_vrect.x + hud_vrect.width + atoi(token)) * scale) - hud_safe.x;
            continue;
        }
        if (!strcmp(token, "xv"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
                x = (hud_vrect.x + hud_vrect.width/2 + (atoi(token) - hx)) * scale;
            continue;
        }

        if (!strcmp(token, "yt"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
                y = ((hud_vrect.y + atoi(token)) * scale) + hud_safe.y;
            continue;
        }
        if (!strcmp(token, "yb"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
                y = ((hud_vrect.y + hud_vrect.height + atoi(token)) * scale) - hud_safe.y;
            continue;
        }
        if (!strcmp(token, "yv"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
                y = (hud_vrect.y + hud_vrect.height/2 + (atoi(token) - hy)) * scale;
            continue;
        }

        if (!strcmp(token, "pic"))
        {   // draw a pic from a stat number
            token = COM_Parse (&s);
            if (!skip_depth)
            {
                value = ps->stats[atoi(token)];
                if (value >= MAX_IMAGES)
                    cgi.Com_Error("Pic >= MAX_IMAGES");

                const char *const pic = cgi.get_configstring(CS_IMAGES + value);

                if (pic && *pic)
                {
                    cgi.Draw_GetPicSize (&w, &h, pic);
                    cgi.SCR_DrawPic (x, y, w * scale, h * scale, pic);
                }
            }

            continue;
        }

        if (!strcmp(token, "client"))
        {   // draw a deathmatch client block
            token = COM_Parse (&s);
            if (!skip_depth)
            {
                x = (hud_vrect.x + hud_vrect.width/2 + (atoi(token) - hx)) * scale;
                x += 8 * scale;
            }
            token = COM_Parse (&s);
            if (!skip_depth)
            {
                y = (hud_vrect.y + hud_vrect.height/2 + (atoi(token) - hy)) * scale;
                y += 7 * scale;
            }

            token = COM_Parse (&s);

            if (!skip_depth)
            {
                value = atoi(token);
                if (value >= MAX_CLIENTS || value < 0)
                    cgi.Com_Error("client >= MAX_CLIENTS");
            }

            int score, ping;

            token = COM_Parse (&s);
            if (!skip_depth)
                score = atoi(token);

            token = COM_Parse (&s);
            if (!skip_depth)
            {
                ping = atoi(token);

                if (!scr_usekfont->integer)
                    CG_DrawString (x + 32 * scale, y, scale, cgi.CL_GetClientName(value));
                else
                    cgi.SCR_DrawFontString(cgi.CL_GetClientName(value), x + 32 * scale, y - (font_y_offset * scale), scale, rgba_white, true, text_align_t::LEFT);
                
                if (!scr_usekfont->integer)
                    CG_DrawString (x + 32 * scale, y + 10 * scale, scale, G_Fmt("{}", score).data(), true);
                else
                    cgi.SCR_DrawFontString(G_Fmt("{}", score).data(), x + 32 * scale, y + (10 - font_y_offset) * scale, scale, rgba_white, true, text_align_t::LEFT);

                cgi.SCR_DrawPic(x + 96 * scale, y + 10 * scale, 9 * scale, 9 * scale, "ping");
                
                if (!scr_usekfont->integer)
                    CG_DrawString (x + 73 * scale + 32 * scale, y + 10 * scale, scale, G_Fmt("{}", ping).data());
                else
                    cgi.SCR_DrawFontString (G_Fmt("{}", ping).data(), x + 107 * scale, y + (10 - font_y_offset) * scale, scale, rgba_white, true, text_align_t::LEFT);
            }
            continue;
        }

        if (!strcmp(token, "ctf"))
        {   // draw a ctf client block
            int     score, ping;

            token = COM_Parse (&s);
            if (!skip_depth)
                x = (hud_vrect.x + hud_vrect.width/2 - hx + atoi(token)) * scale;
            token = COM_Parse (&s);
            if (!skip_depth)
                y = (hud_vrect.y + hud_vrect.height/2 - hy + atoi(token)) * scale;

            token = COM_Parse (&s);
            if (!skip_depth)
            {
                value = atoi(token);
                if (value >= MAX_CLIENTS || value < 0)
                    cgi.Com_Error("client >= MAX_CLIENTS");
            }

            token = COM_Parse (&s);
            if (!skip_depth)
                score = atoi(token);

            token = COM_Parse (&s);
            if (!skip_depth)
            {
                ping = atoi(token);
                if (ping > 999)
                    ping = 999;
            }

            token = COM_Parse (&s);

            if (!skip_depth)
            {

                cgi.SCR_DrawFontString (G_Fmt("{}", score).data(), x, y - (font_y_offset * scale), scale, value == playernum ? alt_color : rgba_white, true, text_align_t::LEFT);
                x += 3 * 9 * scale;
                cgi.SCR_DrawFontString (G_Fmt("{}", ping).data(), x, y - (font_y_offset * scale), scale, value == playernum ? alt_color : rgba_white, true, text_align_t::LEFT);
                x += 3 * 9 * scale;
                cgi.SCR_DrawFontString (cgi.CL_GetClientName(value), x, y - (font_y_offset * scale), scale, value == playernum ? alt_color : rgba_white, true, text_align_t::LEFT);

                if (*token)
                {
                    cgi.Draw_GetPicSize(&w, &h, token);
                    cgi.SCR_DrawPic(x - ((w + 2) * scale), y, w * scale, h * scale, token);
                }
            }
            continue;
        }

        if (!strcmp(token, "picn"))
        {   // draw a pic from a name
            token = COM_Parse (&s);
            if (!skip_depth)
            {
                cgi.Draw_GetPicSize(&w, &h, token);
                cgi.SCR_DrawPic(x, y, w * scale, h * scale, token);
            }
            continue;
        }

        if (!strcmp(token, "num"))
        {   // draw a number
            token = COM_Parse (&s);
            if (!skip_depth)
                width = atoi(token);
            token = COM_Parse (&s);
            if (!skip_depth)
            {
                value = ps->stats[atoi(token)];
                CG_DrawField (x, y, 0, width, value, scale);
            }
            continue;
        }
        // [Paril-KEX] special handling for the lives number
        else if (!strcmp(token, "lives_num"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
            {
                value = ps->stats[atoi(token)];
                CG_DrawField(x, y, value <= 2 ? flash_frame : 0, 1, max(0, value - 2), scale);
            }
        }

        if (!strcmp(token, "hnum"))
        {
            // health number
            if (!skip_depth)
            {
                int     color;

                width = 3;
                value = ps->stats[STAT_HEALTH];
                if (value > 25)
                    color = 0;  // green
                else if (value > 0)
                    color = flash_frame;      // flash
                else
                    color = 1;
                if (ps->stats[STAT_FLASHES] & 1)
                {
                    cgi.Draw_GetPicSize(&w, &h, "field_3");
                    cgi.SCR_DrawPic(x, y, w * scale, h * scale, "field_3");
                }

                CG_DrawField (x, y, color, width, value, scale);
            }
            continue;
        }

        if (!strcmp(token, "anum"))
        {
            // ammo number
            if (!skip_depth)
            {
                int     color;

                width = 3;
                value = ps->stats[STAT_AMMO];

                int32_t min_ammo = cgi.CL_GetWarnAmmoCount(ps->stats[STAT_ACTIVE_WEAPON]);

                if (!min_ammo)
                    min_ammo = 5; // back compat

                if (value > min_ammo)
                    color = 0;  // green
                else if (value >= 0)
                    color = flash_frame;      // flash
                else
                    continue;   // negative number = don't show
                if (ps->stats[STAT_FLASHES] & 4)
                {
                    cgi.Draw_GetPicSize(&w, &h, "field_3");
                    cgi.SCR_DrawPic(x, y, w * scale, h * scale, "field_3");
                }

                CG_DrawField (x, y, color, width, value, scale);
            }
            continue;
        }

        if (!strcmp(token, "rnum"))
        {
            // armor number
            if (!skip_depth)
            {
                int     color;

                width = 3;
                value = ps->stats[STAT_ARMOR];
                if (value < 0)
                    continue;

                color = 0;  // green
                if (ps->stats[STAT_FLASHES] & 2)
                {
                    cgi.Draw_GetPicSize(&w, &h, "field_3");
                    cgi.SCR_DrawPic(x, y, w * scale, h * scale, "field_3");
                }

                CG_DrawField (x, y, color, width, value, scale);
            }
            continue;
        }

        if (!strcmp(token, "stat_string"))
        {
            token = COM_Parse (&s);

            if (!skip_depth)
            {
                index = atoi(token);
                if (index < 0 || index >= MAX_STATS)
                    cgi.Com_Error("Bad stat_string index");
                index = ps->stats[index];

                if (cgi.CL_ServerProtocol() <= PROTOCOL_VERSION_3XX)
                    index = CS_REMAP(index).start / CS_MAX_STRING_LENGTH;

                if (index < 0 || index >= MAX_CONFIGSTRINGS)
                    cgi.Com_Error("Bad stat_string index");
                if (!scr_usekfont->integer)
                    CG_DrawString (x, y, scale, cgi.get_configstring(index));
                else
                    cgi.SCR_DrawFontString(cgi.get_configstring(index), x, y - (font_y_offset * scale), scale, rgba_white, true, text_align_t::LEFT);
            }
            continue;
        }

        if (!strcmp(token, "cstring"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
                CG_DrawHUDString (token, x, y, hx*2*scale, 0, scale);
            continue;
        }

        if (!strcmp(token, "string"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
            {
                if (!scr_usekfont->integer)
                    CG_DrawString (x, y, scale, token);
                else
                    cgi.SCR_DrawFontString(token, x, y - (font_y_offset * scale), scale, rgba_white, true, text_align_t::LEFT);
            }
            continue;
        }

        if (!strcmp(token, "cstring2"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
                CG_DrawHUDString (token, x, y, hx*2*scale, 0x80, scale);
            continue;
        }

        if (!strcmp(token, "string2"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
            {
                if (!scr_usekfont->integer)
                    CG_DrawString (x, y, scale, token, true);
                else
                    cgi.SCR_DrawFontString(token, x, y - (font_y_offset * scale), scale, alt_color, true, text_align_t::LEFT);
            }
            continue;
        }

        if (!strcmp(token, "if"))
        {
            // if stmt
            token = COM_Parse (&s);

            if_depth++;

            // skip to endif
            if (!skip_depth && !ps->stats[atoi(token)])
            {
                skip_depth = true;
                endif_depth = if_depth;
            }

            continue;
        }

        if (!strcmp(token, "ifgef"))
        {
            // if stmt
            token = COM_Parse (&s);

            if_depth++;

            // skip to endif
            if (!skip_depth && cgi.CL_ServerFrame() < atoi(token))
            {
                skip_depth =#]:ó¾í¢G§²ÚîÆ­yÕvel.level_name);

	if (level.is_n64)
	{
		helpString += G_Fmt("xv 0 yv 54 loc_cstring 1 \"{{}}\" \"{}\" ",  // help 1
			game.helpmessage1);
	}
	else 
	{
		int y = 54;
		if (strlen(game.helpmessage1))
		{
			helpString += G_Fmt("xv 0 yv {} loc_cstring2 0 \"$g_pc_primary_objective\" "  // title
				"xv 0 yv {} loc_cstring 0 \"{}\" ",
				y,
				y + 11,
				game.helpmessage1);

			y += 58;
		}

		if (strlen(game.helpmessage2))
		{
			helpString += G_Fmt("xv 0 yv {} loc_cstring2 0 \"$g_pc_secondary_objective\" "  // title
				"xv 0 yv {} loc_cstring 0 \"{}\" ",
				y,
				y + 11,
				game.helpmessage2);
		}

	}

	helpString += G_Fmt("xv 55 yv 164 loc_string2 0 \"{}\" "
		"xv 265 yv 164 loc_rstring2 1 \"{{}}: {}/{}\" \"$g_pc_goals\" "
		"xv 55 yv 172 loc_string2 1 \"{{}}: {}/{}\" \"$g_pc_kills\" "
		"xv 265 yv 172 loc_rstring2 1 \"{{}}: {}/{}\" \"$g_pc_secrets\" ",
		sk,
		level.found_goals, level.total_goals,
		level.killed_monsters, level.total_monsters,
		level.found_secrets, level.total_secrets);

	gi.WriteByte(svc_layout);
	gi.WriteString(helpString.c_str());
	gi.unicast(ent, true);
}

/*
==================
Cmd_Help_f

Display the current help message
==================
*/
void Cmd_Help_f(edict_t *ent)
{
	// this is for backwards compatability
	if (deathmatch->integer)
	{
		Cmd_Score_f(ent);
		return;
	}

	if (level.intermissiontime)
		return;

	ent->client->showinventory = false;
	ent->client->showscores = false;

	if (ent->client->showhelp &&
			(ent->client->pers.game_help1changed == game.help1changed ||
			ent->client->pers.game_help2changed == game.help2changed))
	{
		ent->client->showhelp = false;
		globals.server_flags &= ~SERVER_FLAG_SLOW_TIME;
		return;
	}

	ent->client->showhelp = true;
	ent->client->pers.helpchanged = 0;
	globals.server_flags |= SERVER_FLAG_SLOW_TIME;
	HelpComputer(ent);
}

//=======================================================================

// [Paril-KEX] for stats we want to always be set in coop
// even if we're spectating
void G_SetCoopStats(edict_t *ent)
{
	if (coop->integer && g_coop_enable_lives->integer)
		ent->client->ps.stats[STAT_LIVES] = ent->client->pers.lives + 1;
	else
		ent->client->ps.stats[STAT_LIVES] = 0;

	// stat for text on what we're doing for respawn
	if (ent->client->coop_respawn_state)
		ent->client->ps.stats[STAT_COOP_RESPAWN] = CONFIG_COOP_RESPAWN_STRING + (ent->client->coop_respawn_state - COOP_RESPAWN_IN_COMBAT);
	else
		ent->client->ps.stats[STAT_COOP_RESPAWN] = 0;
}

struct powerup_info_t
{
	item_id_t item;
	gtime_t gclient_t::*time_ptr = nullptr;
	int32_t gclient_t::*count_ptr = nullptr;
} powerup_table[] = {
	{ IT_ITEM_QUAD, &gclient_t::quad_time },
	{ IT_ITEM_QUADFIRE, &gclient_t::quadfire_time },
	{ IT_ITEM_DOUBLE, &gclient_t::double_time },
	{ IT_ITEM_INVULNERABILITY, &gclient_t::invincible_time },
	{ IT_ITEM_INVISIBILITY, &gclient_t::invisible_time },
	{ IT_ITEM_ENVIROSUIT, &gclient_t::enviro_time },
	{ IT_ITEM_REBREATHER, &gclient_t::breather_time },
	{ IT_ITEM_IR_GOGGLES, &gclient_t::ir_time },
	{ IT_ITEM_SILENCER, nullptr, &gclient_t::silencer_shots }
};

/*
===============
G_SetStats
===============
*/
void G_SetStats(edict_t *ent)
{
	gitem_t	*item;
	item_id_t index;
	int		  cells = 0;
	item_id_t power_armor_type;
	unsigned int invIndex;

	//
	// health
	//
	if (ent->s.renderfx & RF_USE_DISGUISE)
		ent->client->ps.stats[STAT_HEALTH_ICON] = level.disguise_icon;
	else
		ent->client->ps.stats[STAT_HEALTH_ICON] = level.pic_health;
	ent->client->ps.stats[STAT_HEALTH] = ent->health;
	if (RaidDowned_IsDown(ent))
		ent->client->ps.stats[STAT_HEALTH] = 0;

	//
	// weapons
	//
	uint32_t weaponbits = 0;

	for (invIndex = IT_WEAPON_GRAPPLE; invIndex <= IT_WEAPON_DISRUPTOR; invIndex++)
	{
		if (ent->client->pers.inventory[invIndex])
		{
			weaponbits |= 1 << GetItemByIndex((item_id_t) invIndex)->weapon_wheel_index;
		}
	}

	ent->client->ps.stats[STAT_WEAPONS_OWNED_1] = (weaponbits & 0xFFFF);
	ent->client->ps.stats[STAT_WEAPONS_OWNED_2] = (weaponbits >> 16);

	ent->client->ps.stats[STAT_ACTIVE_WHEEL_WEAPON] = (ent->client->newweapon ? ent->client->newweapon->weapon_wheel_index :
		ent->client->pers.weapon ? ent->client->pers.weapon->weapon_wheel_index :
		-1);
	ent->client->ps.stats[STAT_ACTIVE_WEAPON] = ent->client->pers.weapon ? ent->client->pers.weapon->weapon_wheel_index : -1;

	//
	// ammo
	//
	ent->client->ps.stats[STAT_AMMO_ICON] = 0;
	ent->client->ps.stats[STAT_AMMO] = 0;

	if (ent->client->pers.weapon && ent->client->pers.weapon->ammo)
	{
		item = GetItemByIndex(ent->client->pers.weapon->ammo);

		if (!G_CheckInfiniteAmmo(item))
		{
			ent->client->ps.stats[STAT_AMMO_ICON] = gi.imageindex(item->icon);
			ent->client->ps.stats[STAT_AMMO] = ent->client->pers.inventory[ent->client->pers.weapon->ammo];
		}
	}
	
	memset(&ent->client->ps.stats[STAT_AMMO_INFO_START], 0, sizeof(uint16_t) * NUM_AMMO_STATS);
	for (unsigned int ammoIndex = AMMO_BULLETS; ammoIndex < AMMO_MAX; ++ammoIndex)
	{
		gitem_t *ammo = GetItemByAmmo((ammo_t) ammoIndex);
		uint16_t val = G_CheckInfiniteAmmo(ammo) ? AMMO_VALUE_INFINITE : clamp(ent->client->pers.inventory[ammo->id], 0, AMMO_VALUE_INFINITE - 1);
		G_SetAmmoStat((uint16_t *) &ent->client->ps.stats[STAT_AMMO_INFO_START], ammo->ammo_wheel_index, val);
	}

	//
	// armor
	//
	power_armor_type = PowerArmorType(ent);
	if (power_armor_type)
		cells = ent->client->pers.inventory[IT_AMMO_CELLS];

	index = ArmorIndex(ent);
	if (power_armor_type && (!index || (level.time.milliseconds() % 3000) < 1500))
	{ // flash between power armor and other armor icon
		ent->client->ps.stats[STAT_ARMOR_ICON] = power_armor_type == IT_ITEM_POWER_SHIELD ? gi.imageindex("i_powershield") : gi.imageindex("i_powerscreen");
		ent->client->ps.stats[STAT_ARMOR] = cells;
	}
	else if (index)
	{
		item = GetItemByIndex(index);
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex(item->icon);
		ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.inventory[index];
	}
	else
	{
		ent->client->ps.stats[STAT_ARMOR_ICON] = 0;
		ent->client->ps.stats[STAT_ARMOR] = 0;
	}

	//
	// pickup message
	//
	if (level.time > ent->client->pickup_msg_time)
	{
		ent->client->ps.stats[STAT_PICKUP_ICON] = 0;
		ent->client->ps.stats[STAT_PICKUP_STRING] = 0;
	}

	// owned powerups
	memset(&ent->client->ps.stats[STAT_POWERUP_INFO_START], 0, sizeof(uint16_t) * NUM_POWERUP_STATS);
	for (unsigned int powerupIndex = POWERUP_SCREEN; powerupIndex < POWERUP_MAX; ++powerupIndex)
	{
		gitem_t *powerup = GetItemByPowerup((powerup_t) powerupIndex);
		uint16_t val;

		switch (powerup->id)
		{
		case IT_ITEM_POWER_SCREEN:
		case IT_ITEM_POWER_SHIELD:
			if (!ent->client->pers.inventory[powerup->id])
				val = 0;
			else if (ent->flags & FL_POWER_ARMOR)
				val = 2;
			else
				val = 1;
			break;
		case IT_ITEM_FLASHLIGHT:
			if (!ent->client->pers.inventory[powerup->id])
				val = 0;
			else if (ent->flags & FL_FLASHLIGHT)
				val = 2;
			else
				val = 1;
			break;
		default:
			val = clamp(ent->client->pers.inventory[powerup->id], 0, 3);
			break;
		}

		G_SetPowerupStat((uint16_t *) &ent->client->ps.stats[STAT_POWERUP_INFO_START], powerup->powerup_wheel_index, val);
	}

	ent->client->ps.stats[STAT_TIMER_ICON] = 0;
	ent->client->ps.stats[STAT_TIMER] = 0;

	//
	// timers
	//
	// PGM
	if (ent->client->owned_sphere)
	{
		if (ent->client->owned_sphere->spawnflags == SPHERE_DEFENDER) // defender
			ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_defender");
		else if (ent->client->owned_sphere->spawnflags == SPHERE_HUNTER) // hunter
			ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_hunter");
		else if (ent->client->owned_sphere->spawnflags == SPHERE_VENGEANCE) // vengeance
			ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_vengeance");
		else // error case
			ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("i_fixme");

		ent->client->ps.stats[STAT_TIMER] = ceil(ent->client->owned_sphere->wait - level.time.seconds());
	}
	else
	{
		powerup_info_t *best_powerup = nullptr;

		for (auto &powerup : powerup_table)
		{
			auto *powerup_time = powerup.time_ptr ? &(ent->client->*powerup.time_ptr) : nullptr;
			auto *powerup_count = powerup.count_ptr ? &(ent->client->*powerup.count_ptr) : nullptr;

			if (powerup_time && *powerup_time <= level.time)
				continue;
			else if (powerup_count && !*powerup_count)
				continue;

			if (!best_powerup)
			{
				best_powerup = &powerup;
				continue;
			}
			
			if (powerup_time && *powerup_time < ent->client->*best_powerup->time_ptr)
			{
				best_powerup = &powerup;
				continue;
			}
			else if (powerup_count && !best_powerup->time_ptr)
			{
				best_powerup = &powerup;
				continue;
			}
		}

		if (best_powerup)
		{
			int16_t value;

			if (best_powerup->count_ptr)
				value = (ent->client->*best_powerup->count_ptr);
			else
				value = ceil((ent->client->*best_powerup->time_ptr - level.time).seconds());

			ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex(GetItemByIndex(best_powerup->item)->icon);
			ent->client->ps.stats[STAT_TIMER] = value;
		}
	}
	// PGM

	//
	// selected item
	//
	ent->client->ps.stats[STAT_SELECTED_ITEM] = ent->client->pers.selected_item;

	if (ent->client->pers.selected_item == IT_NULL)
		ent->client->ps.stats[STAT_SELECTED_ICON] = 0;
	else
	{
		ent->client->ps.stats[STAT_SELECTED_ICON] = gi.imageindex(itemlist[ent->client->pers.selected_item].icon);

		if (ent->client->pers.selected_item_time < level.time)
			ent->client->ps.stats[STAT_SELECTED_ITEM_NAME] = 0;
	}

	//
	// layouts
	//
	ent->client->ps.stats[STAT_LAYOUTS] = 0;

	if (deathmatch->integer)
	{
		if (ent->client->pers.health <= 0 || level.intermissiontime || ent->client->showscores)
			ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;
		if (ent->client->showinventory && ent->client->pers.health > 0)
			ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_INVENTORY;
	}
	else
	{
		if (ent->client->showscores || ent->client->showhelp || ent->client->showeou)
			ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;
		if (ent->client->showinventory && ent->client->pers.health > 0)
			ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_INVENTORY;

		if (ent->client->showhelp)
			ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_HELP;
	}

	if (level.intermissiontime || ent->client->awaiting_respawn)
	{
		if (ent->client->awaiting_respawn || (level.intermission_eou || level.is_n64 || (deathmatch->integer && level.intermissiontime)))
			ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_HIDE_HUD;

		// N64 always merges into one screen on level ends
		if (level.intermission_eou || level.is_n64 || (deathmatch->integer && level.intermissiontime))
			ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_INTERMISSION;
	}
	
	if (level.story_active)
		ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_HIDE_CROSSHAIR;
	else
		ent->client->ps.stats[STAT_LAYOUTS] &= ~LAYOUTS_HIDE_CROSSHAIR;

	// [Paril-KEX] key display
	if (!deathmatch->integer)
	{
		int32_t key_offset = 0;
		player_stat_t stat = STAT_KEY_A;
		
		ent->client->ps.stats[STAT_KEY_A] = 
		ent->client->ps.stats[STAT_KEY_B] = 
		ent->client->ps.stats[STAT_KEY_C] = 0;

		// there's probably a way to do this in one pass but
		// I'm lazy
		std::array<item_id_t, IT_TOTAL> keys_held;
		size_t num_keys_held = 0;

		for (auto &item : itemlist)
		{
			if (!(item.flags & IF_KEY))
				continue;
			else if (!ent->client->pers.inventory[item.id])
				continue;

			keys_held[num_keys_held++] = item.id;
		}

		if (num_keys_held > 3)
			key_offset = (int32_t) (level.time.seconds() / 5);

		for (int32_t i = 0; i < min(num_keys_held, (size_t) 3); i++, stat = (player_stat_t) (stat + 1))
			ent->client->ps.stats[stat] = gi.imageindex(GetItemByIndex(keys_held[(i + key_offset) % num_keys_held])->icon);
	}

	//
	// frags
	//
	ent->client->ps.stats[STAT_FRAGS] = ent->client->resp.score;

	//
	// help icon / current weapon if not shown
	//
	if (ent->client->pers.helpchanged >= 1 && ent->client->pers.helpchanged <= 2 && (level.time.milliseconds() % 1000) < 500) // haleyjd: time-limited
		ent->client->ps.stats[STAT_HELPICON] = gi.imageindex("i_help");
	else if ((ent->client->pers.hand == CENTER_HANDED) && ent->client->pers.weapon)
		ent->client->ps.stats[STAT_HELPICON] = gi.imageindex(ent->client->pers.weapon->icon);
	else
		ent->client->ps.stats[STAT_HELPICON] = 0;

	ent->client->ps.stats[STAT_SPECTATOR] = 0;

	// set & run the health bar stuff
	for (size_t i = 0; i < MAX_HEALTH_BARS; i++)
	{
		byte *health_byte = reinterpret_cast<byte *>(&ent->client->ps.stats[STAT_HEALTH_BARS]) + i;

		if (!level.health_bar_entities[i])
			*health_byte = 0;
		else if (level.health_bar_entities[i]->timestamp)
		{
			if (level.health_bar_entities[i]->timestamp < level.time)
			{
				level.health_bar_entities[i] = nullptr;
				*health_byte = 0;
				continue;
			}

			*health_byte = 0b10000000;
		}
		else
		{
			// enemy dead
			if (!level.health_bar_entities[i]->enemy->inuse || level.health_bar_entities[i]->enemy->health <= 0)
			{
				// hack for Makron
				if (level.health_bar_entities[i]->enemy->monsterinfo.aiflags & AI_DOUBLE_TROUBLE)
				{
					*health_byte = 0b10000000;
					continue;
				}

				if (level.health_bar_entities[i]->delay)
				{
					level.health_bar_entities[i]->timestamp = level.time + gtime_t::from_sec(level.health_bar_entities[i]->delay);
					*health_byte = 0b10000000;
				}
				else
				{
					level.health_bar_entities[i] = nullptr;
					*health_byte = 0;
				}
				
				continue;
			}
			else if (level.health_bar_entities[i]->spawnflags.has(SPAWNFLAG_HEALTHBAR_PVS_ONLY) && !gi.inPVS(ent->s.origin, level.health_bar_entities[i]->enemy->s.origin, true))
			{
				*health_byte = 0;
				continue;
			}

			float health_remaining = ((float) level.health_bar_entities[i]->enemy->health) / level.health_bar_entities[i]->enemy->max_health;
			*health_byte = ((byte) (health_remaining * 0b01111111)) | 0b10000000;
		}
	}

	// ZOID
	SetCTFStats(ent);
	// ZOID
	// Raid presentation deliberately owns its aliased stat slots after CTF has
	// finished clearing/updating them.
	RaidHats_UpdateHUD(ent);
	RaidUI_UpdateHUD(ent);
}

/*
===============
G_CheckChaseStats
===============
*/
void G_CheckChaseStats(edict_t *ent)
{
	gclient_t *cl;

	for (uint32_t i = 1; i <= game.maxclients; i++)
	{
		cl = g_edicts[i].client;
		if (!g_edicts[i].inuse || cl->chase_target != ent)
			continue;
		cl->ps.stats = ent->client->ps.stats;
		G_SetSpectatorStats(g_edicts + i);
	}
}

/*
===============
G_SetSpectatorStats
===============
*/
void G_SetSpectatorStats(edict_t *ent)
{
	gclient_t *cl = ent->client;

	if (!cl->chase_target)
		G_SetStats(ent);

	cl->ps.stats[STAT_SPECTATOR] = 1;

	// layouts are independant in spectator
	cl->ps.stats[STAT_LAYOUTS] = 0;
	if (cl->pers.health <= 0 || level.intermissiontime || cl->showscores)
		cl->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;
	if (cl->showinventory && cl->pers.health > 0)
		cl->ps.stats[STAT_LAYOUTS] |= LAYOUTS_INVENTORY;

	if (cl->chase_target && cl->chase_target->inuse)
		cl->ps.stats[STAT_CHASE] = CS_PLAYERSKINS +
								   (cl->chase_target - g_edicts) - 1;
	else
		cl->ps.stats[STAT_CHASE] = 0;
}
