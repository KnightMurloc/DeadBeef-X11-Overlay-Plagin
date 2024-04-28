#include <deadbeef/deadbeef.h>
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xft/Xft.h>
#include <iconv.h>

#define MAX_LEN 256

static DB_functions_t *deadbeef;

Display* display;
Window root;
XSetWindowAttributes attrs;
XVisualInfo vinfo;
int width;
int height = 50;
char title_format[MAX_LEN];
char title_font[MAX_LEN];
int font_size = 20;
int wait_time;
static XftDraw* xft_drawable;

static Window window;
static XRenderColor color;
static XftColor font_color;
static XftFont *default_font;
static volatile int wait = 0;
static volatile int is_viseble = 0;
static Picture buffer_picture;
static intptr_t mutex;
static intptr_t anim_thread = 0;
static unsigned int thread_count = 0;

void get_primary_monitor_info(
        Display *display,
        int screen,
        int* x_offset, int* y_offset,
        unsigned int* width, unsigned int* height) {
    XRRScreenResources *resources = XRRGetScreenResources(display, RootWindow(display, screen));
    if (resources == NULL) {
        fprintf(stderr, "Error: Failed to get screen resources\n");
        return;
    }
    RROutput primary_output = XRRGetOutputPrimary(display, RootWindow(display, screen));
    if (primary_output == None) {
        fprintf(stderr, "Error: Failed to get primary output\n");
        XRRFreeScreenResources(resources);
        return;
    }
    XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display, resources, XRRGetOutputInfo(display, resources, primary_output)->crtc);
    if (crtc_info == NULL) {
        fprintf(stderr, "Error: Failed to get crtc info\n");
        XRRFreeScreenResources(resources);
        return;
    }
    *width = crtc_info->width;
    *height = crtc_info->height;
    *x_offset = crtc_info->x;
    *y_offset = crtc_info->y;
    XRRFreeCrtcInfo(crtc_info);
    XRRFreeScreenResources(resources);
}

XftFont* find_fallback_font(FcChar32 c) {
    FcCharSet* charset;
    XftFont* font = NULL;
    XftPattern* pattern;
    FcResult result;

    charset = FcCharSetCreate();
    FcCharSetAddChar(charset,c);

    pattern = XftNameParse("");
    FcPatternAddCharSet(pattern, "charset", charset);
    FcPatternAddInteger(pattern, "size", font_size);
    pattern = XftFontMatch(display, DefaultScreen(display), pattern, &result);

    font = XftFontOpenPattern(display, pattern);

    FcCharSetDestroy(charset);
    return font;
}

int utf8_to_utf32(const char *src, size_t src_len, FcChar32 *dst, size_t dst_len)
{
    iconv_t cd = iconv_open("UTF-32LE", "UTF-8");
    if (cd == (iconv_t) -1) {
        perror("iconv_open");
        exit(EXIT_FAILURE);
    }

    char *inbuf = (char *) src;
    char *outbuf = (char *) dst;
    size_t inbytesleft = src_len;
    size_t outbytesleft = dst_len;

    size_t ret = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    if (ret == -1) {
        perror("iconv");
        return 0;
    }

    iconv_close(cd);
    return 1;
}

size_t utf32_strlen(const FcChar32* str)
{
    size_t len = 0;
    while (*str != 0 && len < MAX_LEN) {
        ++len;
        ++str;
    }
    return len;
}

static int font_check(XftFont* font,FcChar32* str, size_t len){
    for(size_t i = 0; i < len; i++){
        if(!XftCharExists(display,font,str[i])){
            return 0;
        }
    }
    return 1;
}

static size_t font_str_len(XftFont* font, FcChar32* str, size_t len){
    size_t i;
    for(i = 0; i < len; i++){
        if(!XftCharExists(display,font,str[i])){
            return i;
        }
    }
    return i;
}

static int text_width(XftFont* main_font, FcChar32* str, size_t str_len){
    XftFont* fallback_font = NULL;

    XGlyphInfo glyph_info;
    size_t i = 0;

    int x_offset = 0;

    while (i < str_len){
        XftFont* font;

        if (XftCharExists(display, main_font, str[i])){
            font = main_font;
        }else{
            if (!fallback_font || !XftCharExists(display,fallback_font,str[i])){
                if (fallback_font){
                    XftFontClose(display,fallback_font);
                }
                fallback_font = find_fallback_font(str[i]);
            }
            font = fallback_font;
        }

        size_t len = font_str_len(font, str + i, str_len - i);
        if(len == 0){
            len = 1;
        }
        XftTextExtents32(display, font, str + i, len, &glyph_info);

        x_offset += glyph_info.width;

        i += len;
    }
    if(fallback_font){
        XftFontClose(display,fallback_font);
    }
    return x_offset;
}

static void draw_text_center(XftColor font_color, XftFont* main_font, const char* text){
    XGlyphInfo glyph_info;
    size_t str_len = strlen(text);
    XftFont* fallback_font = NULL;
    FcChar32* utf32_text = malloc(str_len * sizeof(FcChar32));
    memset(utf32_text,0,str_len * sizeof(FcChar32));
    if(!utf8_to_utf32(text,strlen(text),utf32_text,str_len * sizeof(FcChar32))){
        goto ffree;
    }
    str_len = utf32_strlen(utf32_text);

    //if we can draw string with single font
    if(font_check(main_font, utf32_text, str_len)){
        XGlyphInfo extents;
        XftTextExtentsUtf8(display, main_font, (XftChar8*) text, strlen(text), &extents);
        int x = (int) (width - extents.width) / 2;
        int y = (int) (height + extents.height) / 2;

        XftDrawStringUtf8(xft_drawable, &font_color, main_font, x, y, (XftChar8*) text, strlen(text));
        goto ffree;
    }

    size_t i = 0;

    int x_offset = width / 2 - text_width(main_font, utf32_text, str_len) / 2;

    while (i < str_len){
        XftFont* font;

        if (XftCharExists(display, main_font, utf32_text[i])){
            printf("draw with main font\n");
            font = main_font;
        }else{
            printf("find fallback font\n");
            if(!fallback_font || !XftCharExists(display,fallback_font,utf32_text[i])){
                if(fallback_font){
                    XftFontClose(display,fallback_font);
                }
                fallback_font = find_fallback_font(utf32_text[i]);
            }
            font = fallback_font;
        }

        int y_offset = (int) (height - font->ascent - font->descent) / 2 + font->ascent;
        size_t len = font_str_len(font, utf32_text + i, str_len - i);
        if(len == 0){
            len = 1;
        }
        XftDrawString32(xft_drawable, &font_color, font, x_offset, y_offset, utf32_text + i, len);

        XftTextExtents32(display, font, utf32_text + i, len, &glyph_info);

        x_offset += glyph_info.width;

        i += len;
    }
    if(fallback_font){
        XftFontClose(display,fallback_font);
    }
    ffree:
    free(utf32_text);
}

char* format_string(DB_playItem_t* it){
    ddb_playlist_t * nowplaying_plt = deadbeef->plt_get_curr ();
    char* code_script = deadbeef->tf_compile(title_format);

    ddb_tf_context_t context;
    context._size = sizeof(ddb_tf_context_t);
    context.flags = 0;
    context.it = it;
    context.plt = nowplaying_plt;
    context.idx = 0;
    context.id = 0;
    context.iter = PL_MAIN;
    context.update = 0;
    context.dimmed = 0;

    char * out = malloc(MAX_LEN);

    deadbeef->tf_eval (&context, code_script, out, MAX_LEN);
    deadbeef->tf_free (code_script);
    deadbeef->plt_unref (nowplaying_plt);


    return out;
}

static void sendExposeEvent(){
    XEvent event;
    event.type = Expose;
    XLockDisplay(display);
    XSendEvent(display,window,False,ExposureMask,&event);
    XFlush(display);
    XUnlockDisplay(display);
}

static void animation(void* args){
    thread_count++;
    XLockDisplay(display);
    XMapWindow(display, window);
    XUnlockDisplay(display);
    is_viseble = 1;
    for(float i = 0; i < M_PI / 2.f; i+=0.1f) {
        color.alpha = (short) (65535.f * sinf(i));
        sendExposeEvent();
        usleep(25000);
    }
    color.alpha = 65535;
    sendExposeEvent();
    wait = 1;
    do{
        wait--;
        sleep(wait_time);
    }while(wait);

    for(float i = M_PI / 2.f; i < M_PI; i+=0.1f) {
        color.alpha = (short) (65535.f * sinf(i));
        sendExposeEvent();
        usleep(25000);
    }
    color.alpha = 0;
    sendExposeEvent();

    is_viseble = 0;
    XLockDisplay(display);
    XUnmapWindow(display, window);
    XUnlockDisplay(display);
    thread_count--;
}

static void show_overlay(){
    if(is_viseble){
        return;
    }

    DB_playItem_t* nowplaying = deadbeef->streamer_get_playing_track();

    char* title = format_string(nowplaying);

    deadbeef->pl_item_unref(nowplaying);

    XRenderColor clear_color;
    memset(&clear_color,0,sizeof(clear_color));
    clear_color.alpha = 43690;
    XLockDisplay(display);
    XRenderFillRectangle(display,PictOpSrc,buffer_picture,&clear_color,0,0,width,height);

    draw_text_center(font_color, default_font, title);
    XUnlockDisplay(display);
    deadbeef->thread_start(animation,NULL);

    free(title);
}

static int action_show_overlay(struct DB_plugin_action_s *action, void* ctx){
    show_overlay();
    return 0;
}

static int message(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2){
    if(id == DB_EV_SONGCHANGED){
        show_overlay();
    }
    if (id == DB_EV_CONFIGCHANGED)
    {
        height = deadbeef->conf_get_int("overlay.height",50);
        deadbeef->conf_get_str("overlay.title","%artist% :: %title% $if(%album%,. from )%album% :: %length%",title_format,MAX_LEN);
        font_size = deadbeef->conf_get_int("overlay.font_size",20);
        deadbeef->conf_get_str("overlay.font_name","Sans Regular",title_font,MAX_LEN);
        wait_time = deadbeef->conf_get_int("overlay.wait_time",3);
    }
	return 0;
}

_Noreturn static void window_thread(void* arg){

    XInitThreads();

    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Failed to open X display\n");
        return;
    }
    Window root = RootWindow(display, DefaultScreen(display));

    int x_offset;
    int y_offset;
    XVisualInfo vinfo;
    Colormap colormap;

    get_primary_monitor_info(display,DefaultScreen(display),&x_offset,&y_offset,&width,&height);
    height = 50;

    XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &vinfo);

    colormap = XCreateColormap(display, root, vinfo.visual, AllocNone);
    XSetWindowAttributes attributes;
    attributes.colormap = colormap;
    attributes.border_pixel = 0;
    attributes.background_pixel = 0;

    window = XCreateWindow(display, root, x_offset, y_offset, width, height, 0, vinfo.depth, InputOutput, vinfo.visual,
                           CWColormap | CWBorderPixel | CWBackPixel, &attributes);

    XSelectInput(display,window,ExposureMask);
    XSetWindowAttributes windowAttributes;
    windowAttributes.override_redirect = True; // this removes decoration
    XChangeWindowAttributes(display, window, CWOverrideRedirect, &windowAttributes);

    //XMapWindow(display, window);

    memset(&color, 0, sizeof(color));
    color.alpha = 43690;
    Picture picture = XRenderCreatePicture(display, window, XRenderFindVisualFormat(display, vinfo.visual), 0, NULL);
    XRenderPictureAttributes pa;
    pa.subwindow_mode = IncludeInferiors;

    XRenderChangePicture(display, picture, CPSubwindowMode, &pa);
    XRenderFillRectangle(display, PictOpSrc, picture, &color, 0, 0, width, height);

    Pixmap buffer = XCreatePixmap(display,window,width,height,vinfo.depth);

    buffer_picture = XRenderCreatePicture(display, buffer, XRenderFindVisualFormat(display, vinfo.visual), 0, NULL);
    XRenderFillRectangle(display, PictOpSrc, buffer_picture, &color, 0, 0, width, height);

    {
        FcPattern* pattern = XftNameParse("");
        XftPatternAddDouble(pattern, XFT_SIZE, (double) 20);
        FcResult result;
        pattern = XftFontMatch(display, DefaultScreen(display), pattern, &result);
		default_font = XftFontOpenPattern(display, pattern);
    }

    xft_drawable = XftDrawCreate(display, buffer, vinfo.visual, colormap);
    XftColorAllocName(display, vinfo.visual, colormap, "white", &font_color);

    Picture opacityPicture;
    {

        XRenderPictureAttributes attrs;
        attrs.repeat = 1;
        attrs.component_alpha = True;
        Pixmap opacityBuffer = XCreatePixmap(display,window,width,height,vinfo.depth);

        opacityPicture = XRenderCreatePicture(display, opacityBuffer, XRenderFindVisualFormat(display, vinfo.visual), 0, &attrs);
    }


    XRectangle rect;
    XserverRegion region = XFixesCreateRegion(display, &rect, 1);
    XFixesSetWindowShapeRegion(display, window, ShapeInput, 0, 0, region);
    XFixesDestroyRegion(display, region);

    XEvent event;

    color.alpha = 0;

    while (1){
        XNextEvent(display, &event);
        XLockDisplay(display);
        if (event.type == Expose) {
            XRenderFillRectangle(display, PictOpSrc, opacityPicture, &color, 0, 0, width, height);
            XRenderComposite(display, PictOpSrc, buffer_picture,opacityPicture , picture ,0,0,0,0,0,0,width,height);
        }
        XUnlockDisplay(display);
    }
}

static int start(){

    mutex = deadbeef->mutex_create();

    display = XOpenDisplay(NULL);
    root = DefaultRootWindow(display);

    attrs.override_redirect = 1;


    XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &vinfo);

    attrs.colormap = XCreateColormap(display, root, vinfo.visual, AllocNone);
    attrs.background_pixel = 0;
    attrs.border_pixel = 0;
    attrs.event_mask = EnterWindowMask;

    Screen* screen;
    screen = ScreenOfDisplay(display, 0);
    width = screen->width;

    deadbeef->thread_start(window_thread,NULL);

    return 0;
}

static int stop(){
    deadbeef->mutex_free(mutex);
    return 0;
}

static DB_plugin_action_t show_action = {
        .title = "show overlay",
        .name = "overlay",
        .flags = DB_ACTION_COMMON,
        .callback = action_show_overlay,
        .next = NULL
};

static DB_plugin_action_t* get_actions(DB_playItem_t *it){
    return &show_action;
}

static const char settings_dlg[] =
        "property \"height\" entry overlay.height \"50\";\n"
        "property \"title\" entry overlay.title \"%artist% :: %title% $if(%album%,. from )%album% :: %length%\";\n"
        "property \"Font size\" entry overlay.font_size \"20\";\n"
        "property \"Font\" entry overlay.font_name \"Sans Regular\";\n"
        "property \"show time\" entry overlay.wait_time \"3\";\n";

static DB_misc_t plugin = {
        .plugin = {
                .api_vmajor = 1,
                .api_vminor = 10,
                .id = NULL,
                .name = "X11 overlay 3",
                .descr = "show overlay song info\ntitle format help:https://github.com/DeaDBeeF-Player/deadbeef/wiki/Title-formatting-2.0",
                .copyright = "Murloc Knight",
                .website = "https://github.com/KnightMurloc/DeadBeef-X11-Overlay-Plugin-",

                .command = NULL,
                .start = start,
                .stop = stop,
                .connect = NULL,
                .disconnect = NULL,
                .exec_cmdline = NULL,
                .get_actions = get_actions,
                .message = message,
                .configdialog = settings_dlg
        }
};

extern DB_plugin_t *overlay_load(DB_functions_t *api) {
    deadbeef = api;
    return &plugin.plugin;
}
