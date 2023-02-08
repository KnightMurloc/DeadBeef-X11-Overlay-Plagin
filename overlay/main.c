#include <stdio.h>
#include <gtk/gtk.h>
#include "cairo/cairo.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <math.h>
#include <stdlib.h>
#include <mqueue.h>
#include <unistr.h>
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xatom.h>
#include "utils.h"

#define MAX_STR_LEN 256

static void screen_changed(GtkWidget *widget, GdkScreen *old_screen, gpointer user_data);
static gboolean expose_draw(GtkWidget *widget, GdkEventExpose *event, gpointer userdata);

GtkWidget* lable;
GtkWidget* window;

pthread_t p_config_thread;

float opacity = 0;
int offset_x = 0;
int offset_y = 0;
int width = 1920;

int config_fd;

void* config_thread(void* arg){

}

size_t count_utf8_code_points(const char *s) {
    size_t count = 0;
    while (*s) {
        count += (*s++ & 0xC0) != 0x80;
    }
    return count;
}

void* update_thread(void* arg){
    struct mq_attr attr;

    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_STR_LEN;
    attr.mq_curmsgs = 0;

    mqd_t qq = mq_open("/deadbeef", O_RDWR | O_CREAT, 0660, &attr);
    //char msg[MAX_STR_LEN];
    char* msg = malloc(MAX_STR_LEN);
    int show_time = 3;


    while(1){
        mq_receive(qq, msg, MAX_STR_LEN, NULL);
				printf("%s\n", msg+1);
        if(strcmp("q",msg) == 0){
            break;
        }
        if(msg[0] == 0){
            int height = -1;
            int show_time_tmp = -1;
            sscanf(msg+1,"%d %d", &height, &show_time_tmp);
            if(height == -1 || show_time_tmp == -1){
                continue;
            }
            gdk_threads_enter();
            gtk_window_set_default_size(GTK_WINDOW(window),width,height);
            gdk_threads_leave();
            show_time = show_time_tmp;
            continue;
        }
        char clear_msg[200];
				memset(clear_msg,0, sizeof(clear_msg));
        if(g_utf8_strlen(msg+1, -1) > 100){
            g_utf8_strncpy(clear_msg, msg+1, 100);
        }else{
            g_utf8_strncpy(clear_msg, msg+1, g_utf8_strlen(msg+1, -1));
        }
        GError* error;
        char* markup = g_markup_printf_escaped("<span foreground='white' font='30'>\%s</span>",clear_msg);
        gdk_threads_enter();
        gtk_label_set_markup(GTK_LABEL(lable),markup);
        gtk_widget_show(window);
        gdk_threads_leave();
        g_free(markup);
        for (float i = 0; i < 1.57f; i+=0.1f) {
            opacity = sinf(i);
            gdk_threads_enter();
            gtk_widget_set_opacity(lable, opacity);
            gdk_threads_leave();
            usleep(50000);
        }
        sleep(3);
        for (float i = 1.57f; i < 3.14f; i+= 0.1f) {
            opacity = sinf(i);
            gdk_threads_enter();
            gtk_widget_set_opacity(lable, opacity);
            gdk_threads_leave();
            usleep(50000);
        }
        gdk_threads_enter();
        gtk_widget_hide(window);
        gdk_threads_leave();
        printf("TEST\n");
    }

    free(msg);

    mq_close(qq);
    mq_unlink("deadbeef");
    gtk_main_quit();
    return NULL;
}

void on_mouse_enter(){
    gtk_widget_hide(window);
}

void init_xrandr(Display* dpy_){
    dpy = dpy_;
    root = XDefaultRootWindow(dpy);
    int		event_base, error_base;
    int		major, minor;
    if (!XRRQueryExtension (dpy, &event_base, &error_base) ||
        !XRRQueryVersion (dpy, &major, &minor))
    {
        fprintf (stderr, "RandR extension missing\n");
        return;
    }
    if (major > 1 || (major == 1 && minor >= 2))
        has_1_2 = True;
    if (major > 1 || (major == 1 && minor >= 3))
        has_1_3 = True;

    get_screen (True);
    get_crtcs();
    get_outputs();

    output_t *output;

    for (output = all_outputs; output; output = output->next)
    {

        crtc_t* cur_crtc = output->crtc_info;
        XRRCrtcInfo	*crtc_info = cur_crtc ? cur_crtc->crtc_info : NULL;
        if(cur_crtc && output->primary){
            offset_x = crtc_info->x;
            offset_y = crtc_info->y;
            break;
        }

    }
}

int main(int argc,char* args[]) {

    gdk_threads_init();

    gtk_init(&argc,&args);

    window = gtk_window_new(GTK_WINDOW_POPUP);
    GdkScreen* screen = gtk_window_get_screen(GTK_WINDOW(window));
    GdkRectangle rectangle;
    gdk_monitor_get_geometry(gdk_display_get_primary_monitor(gdk_screen_get_display(screen)),&rectangle);

    width = rectangle.width;
    gtk_window_set_default_size(GTK_WINDOW(window),width,50);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_widget_set_app_paintable(window, TRUE);

    g_signal_connect(G_OBJECT(window), "draw", G_CALLBACK(expose_draw), NULL);
    g_signal_connect(G_OBJECT(window), "screen-changed", G_CALLBACK(screen_changed), NULL);
    screen_changed(window, NULL, NULL);

    char* markup = g_markup_printf_escaped("<span foreground='white' font='30'>\%s</span>","none");


    lable = gtk_label_new(NULL);
		gtk_label_set_line_wrap(GTK_LABEL(lable), TRUE);
		gtk_label_set_lines(GTK_LABEL(lable), 1);
    gtk_label_set_markup(GTK_LABEL(lable),markup);
    g_free(markup);

    gtk_container_add(GTK_CONTAINER(window),lable);

    pthread_t thread;
    pthread_create(&thread,NULL,update_thread,NULL);

    gtk_widget_show_all(window);

    GdkDisplay* disp = gdk_display_get_default();

    Display* xdisp = gdk_x11_display_get_xdisplay(disp);
    Window win = gdk_x11_window_get_xid(gtk_widget_get_window(window));

    XRectangle rect;
    XserverRegion region = XFixesCreateRegion(xdisp, &rect, 1);
    XFixesSetWindowShapeRegion(xdisp, win, ShapeInput, 0, 0, region);
    XFixesDestroyRegion(xdisp, region);
		Atom blur = XInternAtom(xdisp, "_KDE_NET_WM_BLUR_BEHIND_REGION", 0);
		int data = 0;
		XChangeProperty(xdisp,win,blur,XA_CARDINAL,32,PropModeReplace,&data,1);
		//Atom type;
		//int di;
		//unsigned long size, dul;
		//unsigned char *prop_ret = NULL;
		//XGetWindowProperty(xdisp,win,blur,0,0,False,AnyPropertyType,&type,&di,&dul,&size,&prop_ret);
		//printf("%d %d\n", di,size);
		//printf("%d\n", win);
		//printf("%d\n",win);
		XChangeProperty(xdisp,win,blur,XA_CARDINAL,32,PropModeReplace,&data,1);

		//printf("%d\n",XInternAtom(xdisp, "_KDE_NET_WM_BLUR_BEHIND_REGION", 0));
    init_xrandr(xdisp);
    gtk_window_move(GTK_WINDOW(window),offset_x,offset_y);
    gtk_widget_hide(window);
    gtk_main();

    return 0;
}

gboolean supports_alpha = FALSE;
static void screen_changed(GtkWidget *widget, GdkScreen *old_screen, gpointer userdata) {
    GdkScreen *screen = gtk_widget_get_screen(widget);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);

    if (!visual) {
        fprintf(stderr,"Your screen does not support alpha channels!\n");
        visual = gdk_screen_get_system_visual(screen);
        supports_alpha = FALSE;
    } else {
        supports_alpha = TRUE;
    }

    gtk_widget_set_visual(widget, visual);
}

static gboolean expose_draw(GtkWidget *widget, GdkEventExpose *event, gpointer userdata) {
    cairo_t *cr = gdk_cairo_create(gtk_widget_get_window(widget));

    if (supports_alpha) {
        cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.4 * opacity);
    } else {
        cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
    }

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
    cairo_destroy(cr);

    return FALSE;
}
