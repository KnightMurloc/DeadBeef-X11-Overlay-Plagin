#include "deadbeef/deadbeef.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <string.h>
#include <spawn.h>
#include <pwd.h>
#include <mqueue.h>

#define MAX_LEN 256
#define TITLE 1
#define CONFIG 0

typedef struct Config{
    int height;
    int show_time;
}Config;

static DB_functions_t *deadbeef;

int title_fd;
int config_fd;

char title_format[MAX_LEN];
char* title;
//sem_t* title_sem;

Config* config;
//sem_t* config_sem;
mqd_t qq;

extern char **environ;

void format_string(DB_playItem_t* it){
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


	deadbeef->tf_eval (&context, code_script, title+1, MAX_LEN);
	deadbeef->tf_free (code_script);
	deadbeef->plt_unref (nowplaying_plt);

}

static void send_msg(int type){
	title[0] = type;
	mq_send(qq, title, strlen(title+1) + 2, 0);
}

static void show_overlay(){
	DB_playItem_t* nowplaying = deadbeef->streamer_get_playing_track ();
	format_string(nowplaying);
	deadbeef->pl_item_unref(nowplaying);
	//sem_post(title_sem);
	send_msg(TITLE);
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
		int height = deadbeef->conf_get_int("overlay.height",50);
		deadbeef->conf_get_str("overlay.title","%artist% :: %title% $if(%album%,. from )%album% :: %length%",title_format,MAX_LEN);
		int show_time = config->show_time = deadbeef->conf_get_int("overlay.wait_time",3);
		memset(title, 0, MAX_LEN);
		sprintf(title+1, "%d %d", height, show_time);
		//printf("config: %s\n", title+1);
		send_msg(CONFIG);
		//sem_post(config_sem);
	}
}

static const char *getUserName()
{
  uid_t uid = geteuid();
  struct passwd *pw = getpwuid(uid);
  if (pw)
  {
    return pw->pw_name;
  }

  return "";
}

static int start(){
	/*
	printf("test MURLOC\n");
	title_fd = shm_open("deadbeef_overlay",O_RDWR | O_CREAT,0644);
	if (title_fd < 0)
	{
		fprintf(stderr,"create title shared memory error\n");
	}
	ftruncate(title_fd,MAX_LEN);
	title = mmap(NULL,MAX_LEN,PROT_READ | PROT_WRITE,MAP_SHARED,title_fd,0);
	if ((caddr_t) -1  == title)
	{
		fprintf(stderr,"map title shared memory error\n");
	}
	memset(title,0,MAX_LEN);

	
	config_fd = shm_open("deadbeef_config",O_RDWR | O_CREAT,0644);
	if (config_fd < 0)
	{
		fprintf(stderr,"create config shared memory error\n");
	}
	ftruncate(config_fd,sizeof(Config));
	config = mmap(NULL,sizeof(Config),PROT_READ | PROT_WRITE,MAP_SHARED,config_fd,0);
	if ((caddr_t) -1  == config)
	{
		fprintf(stderr,"map config shared memory error\n");
	}
	memset(config,0,sizeof(Config));

	title_sem = sem_open("deadbeef_overlay_sem",O_CREAT,0644,0);
	if (title_sem == (void*) -1)
	{
		fprintf(stderr,"title semaphore open error\n");
	}

	config_sem = sem_open("deadbeef_config_sem",O_CREAT,0644,0);
	if (config_sem == (void*) -1)
	{
		fprintf(stderr,"config semaphore open error\n");
	}

	//char username[33] = {0};
	//getlogin_r(username,33);
	*/

	config = malloc(sizeof(config));

	title = malloc(MAX_LEN);

	memset(title, 0, MAX_LEN);

	struct mq_attr attr;

	attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_LEN;
    attr.mq_curmsgs = 0;

	qq = mq_open("/deadbeef", O_RDWR | O_CREAT, 0660, &attr); 

	const char* username = getUserName();

	char path[75];
	sprintf(path,"/home/%s/.local/lib/deadbeef/overlay/overlay",username);
	pid_t pid;
	char *argv[] = {"overlay", (char *) 0};
	int ret = posix_spawn(&pid,path,NULL,NULL,argv,environ);
	printf("test: %s \n", path);
	
	return 0;
}

static int stop(){
	/*
	title[0] = 'q';
	title[1] = 0;
	sem_post(title_sem);

	munmap(config,sizeof(Config));
	close(config_fd);
	sem_close(config_sem);
	sem_destroy(config_sem);
	shm_unlink("deadbeef_config");

	munmap(title,MAX_LEN);
	close(title_fd);
	sem_close(title_sem);
	sem_destroy(title_sem);
	shm_unlink("deadbeef_overlay");
*/
	

	title[0] = 'q';
	title[1] = 0;

	send_msg('q');

	mq_close(qq);

	mq_unlink("/deadbeef");

	free(config);
	free(title);

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
"property \"show time\" entry overlay.wait_time \"3\";\n";

static DB_misc_t plugin = {
    .plugin = {
        .api_vmajor = 1,
        .api_vminor = 10,
        .id = NULL,
        .name = "X11 overlay2",
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

extern DB_plugin_t *overlay2_load(DB_functions_t *api) {
    deadbeef = api;
    return &plugin.plugin;
}
