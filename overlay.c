#include <deadbeef/deadbeef.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>

#define MAX_LEN 256

static DB_functions_t *deadbeef;

Display* display;
Window root;
int default_screen;
XSetWindowAttributes attrs;
XVisualInfo vinfo;
int width;
int height = 50;
char title_format[MAX_LEN];
char title_font[MAX_LEN];
int font_size = 20;
Window overlay;

static int wait = 0;
static int is_viseble = 0;

void draw(void** args) {
	cairo_t *cr = args[0];
	const char* title = args[1];
	if(title == NULL){
		return;
	}
	cairo_select_font_face(cr, title_font,CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, font_size);
	cairo_text_extents_t extents;
	cairo_text_extents(cr, title, &extents);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	for(float i = 0; i < 1.57f; i+= 0.1f){
		cairo_set_source_rgba(cr, 0, 0, 0, 0.4 * sin(i));
		cairo_move_to(cr,0,0);
    		cairo_rectangle(cr, 0, 0, width, height);
		
    		cairo_fill(cr);
		cairo_set_source_rgba(cr, 1, 1, 1, sin(i));
		cairo_move_to(cr,width/2 - extents.width/2,height/2);
		cairo_show_text(cr,title);
		XFlush(display);
		usleep(10000);
	}
	
	wait = 1;
	do{
		wait--;
		sleep(3);
	}while(wait);
	
	for(float i = 1.57f; i < 3.14f; i+= 0.1f){
		cairo_set_source_rgba(cr, 0, 0, 0, 0.4 * sin(i));
		cairo_move_to(cr,0,0);
    	cairo_rectangle(cr, 0, 0, width, height);
		
    	cairo_fill(cr);
		cairo_set_source_rgba(cr, 1, 1, 1, sin(i));
		cairo_move_to(cr,width/2 - extents.width/2,height/2);
		cairo_show_text(cr,title);
		XFlush(display);
		usleep(10000);
	}
	XEvent event = {0};
	event.type = EnterNotify;
	XSendEvent(display,overlay,0,EnterWindowMask,&event);
	XFlush(display);
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

static void window_thread(void* arg){
	DB_playItem_t* nowplaying = deadbeef->streamer_get_playing_track ();

	char* title = format_string(nowplaying);

	overlay = XCreateWindow(
	        display, root,
	        0, 0, width, height, 0,
	        vinfo.depth, InputOutput, 
	        vinfo.visual,
	        CWOverrideRedirect | CWColormap | CWBackPixel | CWBorderPixel, &attrs
	);
	XSelectInput(display, overlay, EnterWindowMask);
	XMapWindow(display, overlay);
	cairo_surface_t* surf = cairo_xlib_surface_create(display, overlay,
                                  vinfo.visual,
                                  width, height);
	cairo_t* cr = cairo_create(surf);
	//draw(cr,title);
	void* args[] = {cr,title};
	intptr_t thread = deadbeef->thread_start(draw,args);
	XEvent event;
	while(1){
		XNextEvent(display,&event);
		if(event.type == EnterNotify){
			break;
		}
	}
	deadbeef->thread_detach(thread);
	XFlush(display);
	
	cairo_destroy(cr);
	cairo_surface_destroy(surf);
	XUnmapWindow(display, overlay);
	XDestroyWindow(display,overlay);
	XFlush(display);
	deadbeef->pl_item_unref(nowplaying);
	is_viseble = 0;
	free(title);
}

static void show_overlay(){
	if(is_viseble){
		wait = 1;
	}else{
		is_viseble = 1;
		deadbeef->thread_start(window_thread,NULL);
	}

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
		deadbeef->conf_get_str("overlay.title","%artist% :: %title% $if(%album%,. from )%album% :: %length%\"",title_format,MAX_LEN);
		font_size = deadbeef->conf_get_int("overlay.font_size",20);
		deadbeef->conf_get_str("overlay.font_name","Sans Regular",title_font,MAX_LEN);
	}
}

static int start(){
	display = XOpenDisplay(NULL);
	root = DefaultRootWindow(display);
	default_screen = XDefaultScreen(display);

	attrs.override_redirect = 1;


	XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &vinfo);

	attrs.colormap = XCreateColormap(display, root, vinfo.visual, AllocNone);
	attrs.background_pixel = 0;
	attrs.border_pixel = 0;
	attrs.event_mask = EnterWindowMask;

	Screen* screen;
	screen = ScreenOfDisplay(display, 0);
	width = screen->width;

	return 0;
}

static int stop(){
	XCloseDisplay(display);
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
"property \"Font\" entry overlay.font_name \"Sans Regular\";\n";

static DB_misc_t plugin = {
    .plugin = {
        .api_vmajor = 1,
        .api_vminor = 10,
        .id = NULL,
        .name = "X11 overlay",
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
