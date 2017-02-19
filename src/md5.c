#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#if defined(__linux__)
	#define MD5 "md5sum"
#elif defined(__APPLE__)
	#define MD5 "md5"
#endif		


/* Fonction calculant le hash MD5 des fichiers
** pour l'option -v
*/
char *md5(char *toarchive){
	int fds[2];
	pipe(fds);
	pid_t pid = fork();
	if(pid==0){
		close(fds[0]);
		int arc=open(toarchive,O_RDONLY);
		dup2(arc,0);
    	dup2(fds[1],1);
    	close(arc);
    	close(fds[1]);	
	
		char *arguments[] = { MD5, NULL };
		execvp(MD5,arguments);
		
	}else{
		close(fds[1]);
		waitpid(pid,NULL,WUNTRACED);
		char *md=(char *)malloc(33);
		read(fds[0],md,32);
		md[32]='\0';
		return md;
	}
	return NULL;
}

/* Fonction calculant le hash MD5 du mot de
** passe pour l'option -p
*/
char *md52(){
	char pass[255];
	printf("Mot de passe?\n");
	fgets(pass,255,stdin);
	int arc=open("/tmp/pass",O_RDWR|O_CREAT,0644);
	int fds[2];
	pipe(fds);
	pid_t pid = fork();
	if(pid==0){
		close(fds[0]);
		lseek(arc,0,SEEK_SET);
		dup2(arc,0);
    	dup2(fds[1],1);
    	close(arc);
    	close(fds[1]);	
		char *arguments[] = { MD5,NULL };
		execvp(MD5,arguments);
	}else{
		close(fds[1]);
		close(arc);
		waitpid(pid,NULL,WUNTRACED);
		char *md=(char *)malloc(32);
		read(fds[0],md,32);
		unlink("/tmp/pass");
		return md;
	}
	return NULL;
}
