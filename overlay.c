#include <deadbeef/deadbeef.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>

static DB_functions_t *deadbeef;

Display* display;
Window root;
int default_screen;
XSetWindowAttributes attrs;
XVisualInfo vinfo;
int c; 
int width;

static int wait = 0;
static int is_viseble = 0;

void draw(cairo_t *cr,const char* title) {
	if(title == NULL){
		return;
	}
	cairo_select_font_face(cr, "Sans Regular",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 20);
	cairo_text_extents_t extents;
	cairo_text_extents(cr, title, &extents);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	for(float i = 0; i < 1.57f; i+= 0.1f){
		cairo_set_source_rgba(cr, 0, 0, 0, 0.4 * sin(i));
		cairo_move_to(cr,0,0);
    		cairo_rectangle(cr, 0, 0, width, 50);
		
    		cairo_fill(cr);
		cairo_set_source_rgba(cr, 1, 1, 1, sin(i));
		cairo_move_to(cr,width/2 - extents.width/2,25);
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
    	cairo_rectangle(cr, 0, 0, width, 50);
		
    	cairo_fill(cr);
		cairo_set_source_rgba(cr, 1, 1, 1, sin(i));
		cairo_move_to(cr,width/2 - extents.width/2,25);
		cairo_show_text(cr,title);
		XFlush(display);
		usleep(10000);
	}

}

char* format_string(DB_playItem_t* it){
	ddb_playlist_t * nowplaying_plt = deadbeef->plt_get_curr ();
	char* code_script = NULL;
	if(deadbeef->pl_find_meta(it,"title") == 0){
		code_script = deadbeef->tf_compile("%title%");
	}else{
		code_script = deadbeef->tf_compile("%artist% - %title%");
	}
	
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

	char * out = malloc(256);

	deadbeef->tf_eval (&context, code_script, out, 256);
	deadbeef->tf_free (code_script);
	deadbeef->plt_unref (nowplaying_plt);


	return out;
}

static void window_thread(void* arg){
	DB_playItem_t* nowplaying = deadbeef->streamer_get_playing_track ();

	char* title = format_string(nowplaying);

	Window overlay = XCreateWindow(
	        display, root,
	        0, 0, width, 50, 0,
	        vinfo.depth, InputOutput, 
	        vinfo.visual,
	        CWOverrideRedirect | CWColormap | CWBackPixel | CWBorderPixel, &attrs
	);
	XMapWindow(display, overlay);
	cairo_surface_t* surf = cairo_xlib_surface_create(display, overlay,
                                  vinfo.visual,
                                  width, 50);
	cairo_t* cr = cairo_create(surf);
	draw(cr,title);
	free(title);
	XFlush(display);

	cairo_destroy(cr);
	cairo_surface_destroy(surf);
	XUnmapWindow(display, overlay);
	XDestroyWindow(display,overlay);
	XFlush(display);
	deadbeef->pl_item_unref(nowplaying);
	is_viseble = 0;
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

static DB_misc_t plugin = {
    .plugin = {
        .api_vmajor = 1,
        .api_vminor = 10,
        .id = NULL,
        .name = "X11 overlay",
        .descr = "show overlay song info",
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
        .configdialog = NULL
    }
};

extern DB_plugin_t *overlay_load(DB_functions_t *api) {
    deadbeef = api;
    return &plugin.plugin;
}
