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

#define MAX_STR_LEN 256

static void screen_changed(GtkWidget *widget, GdkScreen *old_screen, gpointer user_data);
static gboolean expose_draw(GtkWidget *widget, GdkEventExpose *event, gpointer userdata);

GtkWidget* lable;
GtkWidget* window;

pthread_t p_config_thread;

float opacity = 0;
int width = 1920;

int config_fd;
sem_t* config_sem;

void* config_thread(void* arg){
	/*
    config_fd = shm_open("deadbeef_config",O_RDWR,0644);


	
    if(config_fd < 0){
        fprintf(stderr,"open config shared memory error\n");
        return NULL;
    }

    config = mmap(NULL, sizeof(Config),PROT_READ | PROT_WRITE,MAP_SHARED,config_fd,0);
    if((caddr_t) -1  == config){
        fprintf(stderr,"map config shared memory error\n");
        return NULL;
    }
    config_sem = sem_open("deadbeef_config_sem",O_CREAT,0644,0);
    if(config_sem == (void*) -1){
        fprintf(stderr,"config semaphore open error\n");
        return NULL;
    }

    while(1){
        sem_wait(config_sem);
        gtk_window_set_default_size(GTK_WINDOW(window),width,config->height);
        gtk_widget_modify_font(lable, pango_font_description_from_string( config->font));
//        gtk_window_set_defa
    }
	*/
}

void* update_thread(void* arg){
	printf("test\n");
/*
    int fd = shm_open("deadbeef_overlay",O_RDWR,0644);
    if(fd < 0){
        fprintf(stderr,"open title shared memory error\n");
        return NULL;
    }

    char* mem = mmap(NULL,MAX_STR_LEN,PROT_READ | PROT_WRITE,MAP_SHARED,fd,0);
    if((caddr_t) -1  == mem){
        fprintf(stderr,"map title shared memory error\n");
        return NULL;
    }

    sem_t* sem = sem_open("deadbeef_overlay_sem",O_CREAT,0644,0);
    if(sem == (void*) -1){
        fprintf(stderr,"title semaphore open error\n");
        return NULL;
    }
*/

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

        //sem_wait(sem);
        if(strcmp("q",msg) == 0){
	//sem_close(sem);
			//mq_close(qq);
			//mq_unlink("deadbeef");
		//sem_close(config_sem);
            break;
        }
		//printf("type %d\n", msg[0]);
		if(msg[0] == 0){
			//printf("overlay: %d\n", msg[1]);
			int height = -1;
			int show_time_tmp = -1;
			sscanf(msg+1,"%d %d", &height, &show_time_tmp);
			//printf("%d %d\n", height, show_time_tmp);
			if(height == -1 || show_time_tmp == -1){
				continue;
			}
			gdk_threads_enter();
			gtk_window_set_default_size(GTK_WINDOW(window),width,height);
			gdk_threads_leave();
			show_time = show_time_tmp;
			continue;
		}
		//printf("%d\n", show_time);


	//printf("%s\n",msg);
	//continue;
	char* clear_msg[200];
	if(u8_strlen(msg+1) > 100){
		//msg[100] = '\0';
		u8_cpy(clear_msg, msg+1, 100);
	}else{
		u8_cpy(clear_msg, msg+1, u8_strlen(msg+1));
	}
GError* error;
//char* clear_msg = g_locale_to_utf8(msg+1,-1,NULL,NULL,&error);
//snprintf(clear_msg, 200,"%s",msg+1);
char* markup = g_markup_printf_escaped("<span foreground='white' font='30'>\%s</span>",clear_msg);
gdk_threads_enter();
gtk_label_set_markup(GTK_LABEL(lable),markup);
        //gtk_label_set_text(GTK_LABEL(lable),mem);
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
	//printf("%d\n",config->show_time);
        //sleep(config->show_time);
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
    //munmap(config, sizeof(Config));
    //close(config_fd);
    //sem_close(config_sem);
    //shm_unlink("deadbeef_config");

    //munmap(mem,MAX_STR_LEN);
    //close(fd);
    //sem_close(sem);
    //shm_unlink("deadbeef_overlay");

    //pthread_cancel(p_config_thread);
    gtk_main_quit();
    return NULL;
}

void on_mouse_enter(){
    gtk_widget_hide(window);
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
    //gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_widget_set_app_paintable(window, TRUE);

    g_signal_connect(G_OBJECT(window), "draw", G_CALLBACK(expose_draw), NULL);
    g_signal_connect(G_OBJECT(window), "screen-changed", G_CALLBACK(screen_changed), NULL);
    //g_signal_connect(G_OBJECT(window),"enter-notify-event",G_CALLBACK(on_mouse_enter),NULL);
    screen_changed(window, NULL, NULL);

    //char* markup = g_strdup_printf("<span foreground='green'>%s</span>","none");

	char* markup = g_markup_printf_escaped("<span foreground='white' font='30'>\%s</span>","none");

    //gtk_label_set_markup

    lable = gtk_label_new(NULL);
    //gtk_label_set_markup(lable,"<span foreground='green'>none</span>");
	gtk_label_set_markup(GTK_LABEL(lable),markup);
	g_free(markup);
    //gtk_widget_modify_font(lable, pango_font_description_from_string("30"));

    gtk_container_add(GTK_CONTAINER(window),lable);

    pthread_t thread;
    pthread_create(&thread,NULL,update_thread,NULL);
    //pthread_create(&p_config_thread,NULL,config_thread,NULL);

    gtk_widget_show_all(window);
    gtk_widget_hide(window);

    //pthread_t thread2;
    //pthread_create(&thread,NULL,update_thread,NULL);
    //pthread_create(&p_config_thread,NULL,config_thread,NULL);
    //config->show_time = 3;
printf("test\n");


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
//        printf("setting transparent window\n");
        cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.4 * opacity);
    } else {
//        printf("setting opaque window\n");
        cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
    }

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
    cairo_destroy(cr);

    return FALSE;
}
