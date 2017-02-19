#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/dir.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <libgen.h>
#include <errno.h>
#include <signal.h>
#include "md5.h"
#include "Archive.h"
#if defined(__linux__)
	#define MD5 "md5sum"
#elif defined(__APPLE__)
	#define MD5 "md5"
#endif		

#define BUFF_SIZE 1024
#define ARG_SIZE 13

/*Structure pour les elements de l'archive
**sans le contenu des fichiers
*/
struct archive{
	size_t path_length;
	off_t file_length;
	mode_t mode;
	time_t m_time;
	time_t a_time;
	char checksum[33];
	char path[255];
};	


/* Fonction utilisee pour le nom des paths 
** lorsqu'on utilise l'option -C
*/
int diff(char *s1, char *s2){
	int i=0;
	while(s1[i]==s2[i]){
		i++;
	}
	return i;
}


/* Fonction compressant l'archive avec gunzip
** pour l'option -z
*/
int gzip(char *archive,int n){
	if(!n){
		int pid=fork();
		if(!pid){
			execlp("gzip","gzip",archive,NULL);
			exit(EXIT_FAILURE);
		}else{
			wait(NULL);
			return 0;
		}
	}else{
		int fds[2];
		pipe(fds);
		int pid=fork();
		if(!pid){
			close(fds[0]);
			dup2(fds[1],1);
			close(fds[1]);
			execlp("gunzip","gunzip","-c",archive,NULL);
			exit(EXIT_FAILURE);
		}else{
			close(fds[1]);
			char path[6+strlen(archive)-3];
			char test[strlen(archive)-3];
			int i;
			for(i=0; i<strlen(archive)-3;i++){
				test[i]=archive[i];
			}
			sprintf(path,"/tmp/%s%c",test,'\0');
			int lu,decomp=open(path,O_CREAT|O_RDWR,0700);
			char buff[BUFF_SIZE];
			while((lu=read(fds[0],buff,BUFF_SIZE))){
				write(decomp,buff,lu);
			}
			close(fds[0]);
			close(decomp);
		}
	}
	return 0;
}


/* Fonction principale du programme
** elle gere toutes les options pour l'archivage,
** desarchivage, ajout, suppression, affichage du contenu
*/
int mytar(int options, int archive, char *toarchive, char *newrep){
	if(options == 0 || options == 10 || options == 12 || options == 9 || options == 15 || options == 17 || options == 22 || options == 23 || options == 24 || options== 25){
		struct stat st;
		if(lstat(toarchive, &st)){
			perror("stat");
			exit(EXIT_FAILURE);
		}
		
		if(S_ISDIR(st.st_mode)){
			printf("%s\n", toarchive);
			DIR *rep = opendir(toarchive);
			struct dirent *ent;
			st_archive *a =(st_archive *)malloc(sizeof(st_archive));
			a->mode = st.st_mode;
			a->a_time = st.st_atime;
			a->m_time = st.st_mtime;
			a->path_length = 1+strlen(toarchive);
			if(options!=17 && options!=22 && options !=23 && options !=25){
				// a->path = (char *)malloc((sizeof(char))*a->path_length);
				sprintf(a->path, "%s%c", toarchive, '\0');
			}else{
				a->path_length+=1+strlen(newrep);
				// a->path = (char *)malloc((sizeof(char))*a->path_length);
				sprintf(a->path, "%s/%s%c", newrep,toarchive, '\0');
			}
			sprintf(a->checksum,"0");
			ecrire(archive, 0, a);
			//write(archive,a,sizeof(st_archive));
			while((ent = readdir(rep))){
				if(ent->d_name[0] != '.'){
					char *pth =
					(char *)malloc(strlen(ent->d_name)+strlen(toarchive)+2);
					sprintf(pth, "%s/%s", toarchive, ent->d_name);
					mytar(options, archive, pth,newrep);
					free(pth);
				}
			}			
			free(a);
			closedir(rep);
			free(ent);
		}
		if(S_ISREG(st.st_mode)){
			printf("%s\n", toarchive);
			int file = open(toarchive, O_RDWR);
			struct flock fl, fl1;
			fl.l_type = F_WRLCK;
			fl.l_whence = SEEK_SET;
			fl.l_start = 0;
			fl.l_len = 0;
			fl1 = fl;
			fcntl(file, F_GETLK, &fl1);
			if(fl1.l_type != F_UNLCK || fcntl(file, F_SETLK, &fl) == -1){
				perror("fcntl SETLCK");
				exit(EXIT_FAILURE);
			}else{
				st_archive *a = (st_archive *)malloc(sizeof(st_archive));
				a->mode = st.st_mode;
				a->a_time = st.st_atime;
				a->m_time = st.st_mtime;
				a->path_length = 1+strlen(toarchive);
				if(options!=17 && options!=22 && options!=23 && options!=25){
					// a->path = (char *)malloc((sizeof(char))*a->path_length);
					sprintf(a->path, "%s%c", toarchive, '\0');
				}else{
					a->path_length+=1+strlen(newrep);
					// a->path = (char *)malloc((sizeof(char))*a->path_length);
					sprintf(a->path, "%s/%s%c", newrep,toarchive, '\0');
				}
				a->file_length = st.st_size;
				if(options==15 || options==23 || options==24 || options==24){
					char *md=md5(toarchive);
					char backup[a->path_length];
	 				sprintf(backup,"%s",a->path);
					sprintf(a->checksum,"%s",md);
					sprintf(a->path,"%s",backup);
					free(md);
				}else{
					sprintf(a->checksum,"0");
				}
				a->mode = st.st_mode;
				//write(archive, a, sizeof(st_archive));
				ecrire(archive, 1, a);
				int lu;
				char buff[BUFF_SIZE];
				while((lu = read(file, buff, BUFF_SIZE))){
					write(archive,buff,lu);
				}
				close(file);
				free(a);
			}
		}
		if((options==12 || options==9 || options==22 || options==23 || options==24) && S_ISLNK(st.st_mode)){
			printf("%s\n", toarchive);
			st_archive *a = (st_archive *)malloc(sizeof(st_archive));
			a->mode = st.st_mode;
			a->a_time = st.st_atime;
			a->m_time = st.st_mtime;
			a->file_length = st.st_size;

			sprintf(a->checksum,"0");
			if(options!=22 && options!=23){
				a->path_length = 1+strlen(toarchive);
				// a->path = (char *)malloc((sizeof(char))*a->path_length);
				sprintf(a->path, "%s%c", toarchive, '\0');	
				char *tmp = (char *)malloc(a->file_length);
				readlink(toarchive,tmp,a->file_length);
				//write(archive,a,sizeof(st_archive));
				ecrire(archive, 1, a);
				write(archive,tmp,a->file_length);
			}else{
				a->path_length = 2+strlen(toarchive)+strlen(newrep);
				// a->path = (char *)malloc((sizeof(char))*a->path_length);
				sprintf(a->path, "%s/%s%c", newrep, toarchive, '\0');
				char *tmp1 = (char *)malloc(a->file_length);
				readlink(toarchive,tmp1,a->file_length);
				int d = diff(tmp1,a->path);
				a->file_length-=d;
				char *tmp = (char *)malloc(a->file_length);
				sprintf(tmp,"%s",tmp1+d);
				//write(archive,a,sizeof(st_archive));
				ecrire(archive, 1, a);
				write(archive,tmp,a->file_length);
			}

			// ecrire(archive, 1, a);

			free(a);

		}
	}else if(options == 11 || options == 20 || options == 21 || options == 14 || options == 13 || options == 16 || options == 18 || options == 26 || options == 27 || options == 28 || options == 29 ||
			 options == 212 || options == 142 || options == 162 || options == 182 || options == 262 || options == 272 || options == 282 || options == 292){
		st_archive *a = lire(archive);
		if(options == 18 || options == 27 || options == 28 || options == 29 || options == 182 || options == 272 || options == 282 || options == 292){
			mkdir(newrep,0766);
		}
		//int lu;
		while(a!=NULL){
			printf("%s\n", a->path);
			if(S_ISDIR(a->mode)){
				struct stat st;
				if(options != 18 && options != 27 && options != 28 && options != 29 && options != 182 && options != 272 && options != 282 && options != 292){
					if(stat(a->path, &st)){
						mkdir(a->path, a->mode);
						stat(a->path, &st);
						st.st_mtime = a->m_time;
						st.st_atime = a->a_time;
					}
				}else{
					char pat[a->path_length+strlen(newrep)+1];
					sprintf(pat,"%s/%s%c",newrep,a->path,'\0');
					if(stat(pat, &st)){
						mkdir(pat, a->mode);
						stat(pat, &st);
						st.st_mtime = a->m_time;
						st.st_atime = a->a_time;
					}
				}
			}

			if((options==13 || options==14 || options==26 || options==27 || options==29 || options==142 || options==262 || options==272 || options==292) && S_ISLNK(a->mode)){
				struct stat st;
				if(options!=27 && options!=29 && options!=272 && options!=292){
					/*if(stat(a->path, &st)){
						char file[a->file_length];
						read(archive,file,a->file_length);
						int tmp1 = open(file, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IXUSR);
						close(tmp1);
						symlink(file,a->path);	
						stat(a->path, &st);
						st.st_mtime = a->m_time;
						st.st_atime = a->a_time;
					}else{*/
						if(options==142 || options==262){
							if(a->m_time > st.st_mtime){
								remove(a->path);
								char file[a->file_length];
								read(archive,file,a->file_length);
								int tmp1 = open(file, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IXUSR);
								close(tmp1);
								symlink(file,a->path);	
								stat(a->path, &st);
								st.st_mtime = a->m_time;
								st.st_atime = a->a_time;
							}
						}
						else{
							char file[a->file_length];
						read(archive,file,a->file_length);
						int tmp1 = open(file, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IXUSR);
						close(tmp1);
						symlink(file,a->path);	
						stat(a->path, &st);
						st.st_mtime = a->m_time;
						st.st_atime = a->a_time;
							//lseek(archive,a->file_length,SEEK_CUR);
						}
					//}
				}else{
					char pat[a->path_length+strlen(newrep)+1];
					sprintf(pat,"%s/%s%c",newrep,a->path,'\0');
					/*if(stat(pat,&st)){
						char file[a->file_length];
						read(archive,file,a->file_length);
						int tmp1 = open(file, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IXUSR);
						close(tmp1);
						symlink(file,pat);	
						stat(a->path, &st);
						st.st_mtime = a->m_time;
						st.st_atime = a->a_time;
					}else{*/
						if(options == 272 || options == 292){
							if(a->m_time > st.st_mtime){
								remove(a->path);
								char file[a->file_length];
								read(archive,file,a->file_length);
								int tmp1 = open(file, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IXUSR);
								close(tmp1);
								symlink(file,pat);	
								stat(a->path, &st);
								st.st_mtime = a->m_time;
								st.st_atime = a->a_time;
							}
						}
						else{
							char file[a->file_length];
						read(archive,file,a->file_length);
						int tmp1 = open(file, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IXUSR);
						close(tmp1);
						symlink(file,pat);	
						stat(a->path, &st);
						st.st_mtime = a->m_time;
						st.st_atime = a->a_time;
							//lseek(archive,a->file_length,SEEK_CUR);
						}
					//}
				}
			}else if((options!=13 && options!=14 && options!=26 && options!=27 && options!=29 && options!=142 && options!=262 && options!=272 && options!=292) && S_ISLNK(a->mode)){
				lseek(archive,a->file_length,SEEK_CUR);
			}

			if(S_ISREG(a->mode)){
				struct stat st;
				char *str = NULL;
				if((options == 20 || options == 21 || options == 212) &&
					a->path_length >= strlen(toarchive)){
					str = (char *)malloc(strlen(toarchive));
					int i;
					for(i = 0; i < strlen(toarchive); i++){
						str[i] = a->path[i];
					}
				}
				if(((((options == 20 || options == 21 || options == 212) &&
					str!=NULL && !strcmp(str,toarchive)) ||
					options==1 || options==11) &&
					stat(a->path,&st)) || (options==13 || options==14 || options==16 || options==18 || options==26 || options==27 || options==28 ||options==29)){
					int tmp,status;
					if(options!=18 && options!=27 && options!=28 && options!=29){
						//if(stat(a->path,&st)){
							status=1;
							tmp = open(a->path, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IXUSR);
							stat(a->path, &st);
							chmod(a->path, a->mode);
						/*}else{
							status=0;
						}*/
					}else{
						char pat[a->path_length+strlen(newrep)+1];
						sprintf(pat,"%s/%s%c",newrep,a->path,'\0');
						//if(stat(pat,&st)){
							status=1;
							tmp = open(pat, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IXUSR);
							stat(pat, &st);
							chmod(pat, a->mode);
						/*}else{
							status=0;
						}*/
					}
					//if(status){
					if(((options == 212 || options == 142 || options == 162 || options == 182 || options == 262 || options == 272 || options == 282 || options == 292)  && a->m_time > st.st_mtime) || !(options == 212 || options == 142 || options == 162 || options == 182 || options == 262 || options == 272 || options == 282 || options == 292)){
						char *file=(char *) malloc(a->file_length);
						read(archive,file,a->file_length);
						write(tmp, file, a->file_length);
						close(tmp);
						free(file);

						if(options==16 || options==26 || options==27 || options==28){
							char *md;
							if(options==28){
								char pat[a->path_length+strlen(newrep)+1];
								sprintf(pat,"%s/%s%c",newrep,a->path,'\0');
								md=md5(pat);
							}else{
								md=md5(a->path);
							}
							printf("Verification MD5: %d\n", !strcmp(md,a->checksum));
							free(md);
						}
						
						st.st_mtime = a->m_time;
						st.st_atime = a->a_time;
					}
				}else{
					lseek(archive,a->file_length,SEEK_CUR);
				}
				if(str != NULL){
					free(str);
				}
			}
			free(a);
			a=lire(archive);
		}
	}else if(options == 3){
		int newarchive = open(".tmp.mtr", O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IXUSR);
		lseek(newarchive,32,SEEK_CUR);
		st_archive *a = lire(archive);
		while(a!=NULL){
			printf("%s\n", a->path);
			char *str = NULL;
			if(a->path_length >= strlen(toarchive)){
				str = (char *)malloc(strlen(toarchive)+1);
				int i;
				for(i = 0; i < strlen(toarchive); i++){
					str[i] = a->path[i];
				}
				str[i]='\0';
			}
			if(S_ISDIR(a->mode)){
				// struct stat st;
				if(((str != NULL && strcmp(str, toarchive)) ||
				(str == NULL && a->path_length < strlen(toarchive)))){
					//write(newarchive,a,sizeof(st_archive));
					 ecrire(newarchive, 0, a);
				}
			}
			if(S_ISREG(a->mode) || S_ISLNK(a->mode)){
				// struct stat st;
				if(((str != NULL && strcmp(str, toarchive)) ||
					(str == NULL && a->path_length < strlen(toarchive)))){
					ecrire(newarchive, 1, a);
					//write(newarchive,a,sizeof(st_archive));
					char buff[a->file_length];
					read(archive, buff, a->file_length);
					write(newarchive,buff,a->file_length);
				}else{
					lseek(archive,a->file_length,SEEK_CUR);
				}
			}
			if(str != NULL){
				free(str);
			}
			free(a);
			a=lire(archive);
		}
	}else if(options == 4 || options == 5 || options == 50 || options == 1 || options == 2){
		struct stat st;
		if(stat(toarchive, &st)){
			perror("stat");
			exit(EXIT_FAILURE);
		}
		if(options == 1 || options == 2){
			st_archive *a2 = lire(archive);
			while(a2 != NULL){
				if(a2->mode == st.st_mode){
					if(a2->m_time > st.st_mtime){
						return 0;
					}
				}
				free(a2);
				a2 = lire(archive);
			}
		}
		lseek(archive,0,SEEK_END);
		printf("%s\n", toarchive);
		if(S_ISDIR(st.st_mode)){
			DIR *rep = opendir(toarchive);
			struct dirent *ent;
			st_archive *a =(st_archive *)malloc(sizeof(st_archive));
			a->mode = st.st_mode;
			a->a_time = st.st_atime;
			a->m_time = st.st_mtime;
			a->path_length = 1+strlen(toarchive);
			// a->path = (char *)malloc((sizeof(char))*a->path_length);
			sprintf(a->path, "%s%c", toarchive, '\0');
			ecrire(archive, 0, a);
			//write(archive,a,sizeof(struct  archive));
			while((ent = readdir(rep))){
				if(ent->d_name[0] != '.'){
					char *pth = (char *)malloc(strlen(ent->d_name)+strlen(toarchive)+2);
					sprintf(pth, "%s/%s", toarchive, ent->d_name);
					mytar(options, archive, pth,newrep);
				}
			}
			free(a);
			free(ent);
			closedir(rep);
		}
		if(S_ISREG(st.st_mode)){
			int file = open(toarchive, O_RDWR);
			struct flock fl, fl1;
			fl.l_type = F_WRLCK;
			fl.l_whence = SEEK_SET;
			fl.l_start = 0;
			fl.l_len = 0;
			fl1 = fl;
			fcntl(file, F_GETLK, &fl1);
			if(fl1.l_type != F_UNLCK || fcntl(file, F_SETLK, &fl) == -1){
				perror("fcntl SETLCK");
				exit(EXIT_FAILURE);
			}else{
				st_archive *a =(st_archive *)malloc(sizeof(st_archive));
				a->mode = st.st_mode;
				a->a_time = st.st_atime;
				a->m_time = st.st_mtime;
				a->path_length = 1+strlen(toarchive);
				// a->path = (char *)malloc((sizeof(char))*a->path_length);
				sprintf(a->path, "%s%c", toarchive, '\0');
				a->file_length = st.st_size;
				if(options == 50 || options == 1){
					char *md=md5(toarchive);
					char backup[a->path_length];
	 				sprintf(backup,"%s",a->path);
					sprintf(a->checksum,"%s",md);
					sprintf(a->path,"%s",backup);
					free(md);
				}else{
					sprintf(a->checksum,"0");
				}
				ecrire(archive, 1, a);
				//write(archive,a,sizeof(st_archive));
				int lu;
				char buff[BUFF_SIZE];
				while((lu = read(file, buff, BUFF_SIZE))){
					write(archive,buff,lu);
				}
				close(file);
				free(a);
			}
		}
		if(S_ISLNK(st.st_mode)){
			st_archive *a = (st_archive *)malloc(sizeof(st_archive));
			a->mode = st.st_mode;
			a->a_time = st.st_atime;
			a->m_time = st.st_mtime;
			a->file_length = st.st_size;

			sprintf(a->checksum,"0");
			if(options!=22 && options!=23){
				a->path_length = 1+strlen(toarchive);
				// a->path = (char *)malloc((sizeof(char))*a->path_length);
				sprintf(a->path, "%s%c", toarchive, '\0');	
				char *tmp = (char *)malloc(a->file_length);
				readlink(toarchive,tmp,a->file_length);
				ecrire(archive, 1, a);
				//write(archive,a,sizeof(st_archive));
				write(archive,tmp,a->file_length);
			}else{
				a->path_length = 2+strlen(toarchive)+strlen(newrep);
				// a->path = (char *)malloc((sizeof(char))*a->path_length);
				sprintf(a->path, "%s/%s%c", newrep, toarchive, '\0');
				char *tmp1 = (char *)malloc(a->file_length);
				readlink(toarchive,tmp1,a->file_length);
				printf("%s %s\n", a->path,tmp1);
				int d = diff(tmp1,a->path);
				a->file_length-=d;
				char *tmp = (char *)malloc(a->file_length);
				sprintf(tmp,"%s",tmp1+d);
				ecrire(archive, 1, a);
				//write(archive,a,sizeof(st_archive));
				write(archive,tmp,a->file_length);
			}

			// ecrire(archive, 1, a);

			free(a);
		}
	}else if(options == 6){
		st_archive *a = lire(archive);
		//int lu;
		while(a!=NULL){
			char time[13];
			strftime(time,13,"%d %b %H:%M",localtime(&(a->a_time)));
			printf("%o %d %s %s\n", a->mode, (int)a->file_length, time, a->path);
			if(!S_ISDIR(a->mode)){
				lseek(archive,a->file_length,SEEK_CUR);
				
			}
			free(a);
			a=lire(archive);
		}
	}
	else if(options == 7 || options == 8 || options == 30 || options == 31 || options == 32 || options == 33 || options == 34 || options == 35){
		struct archive *a =lire(archive);
		if(options==31 || options==34 || options==35){
			mkdir(newrep,0766);
		}
		//int lu;
		while(a!=NULL){
			printf("%s\n", a->path);
			if(S_ISDIR(a->mode)){
				struct stat st;
				if(options!=31){
					if(stat(a->path, &st)){
						mkdir(a->path, a->mode);
						stat(a->path, &st);
						st.st_mtime = a->m_time;
						st.st_atime = a->a_time;
					}else{
						printf("%s deja present\n", a->path);
					}
				}else{
					char pat[a->path_length+strlen(newrep)+1];
					sprintf(pat,"%s/%s%c",newrep,a->path,'\0');
					if(stat(pat, &st)){
						mkdir(pat, a->mode);
						stat(pat, &st);
						st.st_mtime = a->m_time;
						st.st_atime = a->a_time;
					}else{
						printf("%s deja present\n", pat);
					}
				}
			}
			if(S_ISREG(a->mode)){
				struct stat st;
				int tmp;
				if(options!=31){
						
					if(stat(a->path, &st)){
						tmp = open(a->path, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IXUSR);
						chmod(a->path, a->mode);
						char *file=(char *) malloc(a->file_length);
						read(archive,file,a->file_length);
						write(tmp, file, a->file_length);
						close(tmp);
						free(file);
						if(options==30 || options==33){
							char *md;
							md=md5(a->path);
							sprintf(a->checksum,"%s",md);
							printf("Verification MD5: %d\n", !strcmp(md,a->checksum));
						}
						st.st_mtime = a->m_time;
						st.st_atime = a->a_time;
					}else{
						printf("%s deja present\n", a->path);
						lseek(archive,a->file_length,SEEK_CUR);	
					}
						
				}else{
					char pat[a->path_length+strlen(newrep)+1];
					sprintf(pat,"%s/%s%c",newrep,a->path,'\0');
					if(stat(pat, &st)){
						tmp = open(pat, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IXUSR);
						stat(pat, &st);
						chmod(pat, a->mode);
						char *file=(char *) malloc(a->file_length);
						read(archive,file,a->file_length);
						write(tmp, file, a->file_length);
						close(tmp);
						free(file);
						char *md=md5(pat);
						sprintf(a->checksum,"%s",md);
						printf("Verification MD5: %d\n", !strcmp(md,a->checksum));
						st.st_mtime = a->m_time;
						st.st_atime = a->a_time;
					}else{
						printf("%s deja present\n", a->path);	
						lseek(archive,a->file_length,SEEK_CUR);	
					}
				}
			}

			if((options==32 || options==33 || options==35) && S_ISLNK(a->mode)){
				struct stat st;
				if(options!=27 && options!=29){
					if(stat(a->path, &st)){
						char *file=(char *) malloc(a->file_length);
						read(archive,file,a->file_length);
						int tmp1 = open(file, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IXUSR);
						close(tmp1);
						symlink(file,a->path);	
						stat(a->path, &st);
						free(file);
						st.st_mtime = a->m_time;
						st.st_atime = a->a_time;
					}else{
						printf("%s deja present\n", a->path);
						lseek(archive,a->file_length,SEEK_CUR);	
					}
				}else{
					char pat[a->path_length+strlen(newrep)+1];
					sprintf(pat,"%s/%s%c",newrep,a->path,'\0');
					if(stat(pat, &st)){
						char *file=(char *) malloc(a->file_length);
						read(archive,file,a->file_length);
						int tmp1 = open(file, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IXUSR);
						close(tmp1);
						symlink(file,pat);	
						free(file);
						stat(a->path, &st);
						st.st_mtime = a->m_time;
						st.st_atime = a->a_time;
					}else{
						printf("%s deja present\n",pat);
						lseek(archive,a->file_length,SEEK_CUR);	
					}
				}
			}else if(S_ISLNK(a->mode)){
				lseek(archive,a->file_length,SEEK_CUR);	
			}
			free(a);
			a=lire(archive);
		}
	}
	return 0;
}

int main(int argc, char *argv[]){
	int archive, i, c = 1;
	// fonctionnement de argt :
	//  0  1  2  3  4  5  6  7  8  9 10 11 12
	// -a -c -d -l -x -f -k -s -v -C -p -z -u
	// argt[i] == 0 : argument manquant
	// argt[i] == 1 : argument present
	int argt[ARG_SIZE];
	for(i = 0; i < ARG_SIZE; i++){
		argt[i] = 0;
	}
	// fonctionnement de path :
	// path[0] : archive
	// path[i] pour i allant de 1 a c : chemins vers des fichiers ordinaires, des repertoires ou des liens symboliques
	char *path[argc];
	path[0] = "", path[1] = "", path[2] = "";
	for(i = 1; i < argc; i++){
		if((!strcmp(argv[i], "-a") && (argt[1] || argt[2] || argt[3] || argt[4])) ||
		   (!strcmp(argv[i], "-c") && (argt[0] || argt[2] || argt[3] || argt[4])) ||
		   (!strcmp(argv[i], "-d") && (argt[0] || argt[1] || argt[3] || argt[4])) ||
		   (!strcmp(argv[i], "-l") && (argt[0] || argt[1] || argt[2] || argt[4])) ||
		   (!strcmp(argv[i], "-x") && (argt[0] || argt[1] || argt[2] || argt[3])) ||
		   /*(!strcmp(argv[i], "-f") && (i == argc-1 || (strcmp(argv[i+1]+strlen(argv[i+1])-4, ".mtr") && strcmp(argv[i+1]+strlen(argv[i+1])-7, ".mtr.gz")) || argt[5])) ||*/
		   (!strcmp(argv[i], "-k") && argt[6]) ||
		   (!strcmp(argv[i], "-s") && argt[7]) ||
		   (!strcmp(argv[i], "-v") && argt[8]) ||
		   (!strcmp(argv[i], "-C") && argt[9]) ||
		   (!strcmp(argv[i], "-p") && argt[10])||
		   (!strcmp(argv[i], "-z") && argt[11])||
		   (!strcmp(argv[i], "-u") && argt[12])){
			write(2, "Erreur: parametre invalide\n", 27);
			return -1;
		}
		if(!strcmp(argv[i], "-a")){
			argt[0] = 1;
		}
		else if(!strcmp(argv[i], "-c")){
			argt[1] = 1;
		}
		else if(!strcmp(argv[i], "-d")){
			argt[2] = 1;
		}
		else if(!strcmp(argv[i], "-l")){
			argt[3] = 1;
		}
		else if(!strcmp(argv[i], "-x")){
			argt[4] = 1;
		}
		else if(!strcmp(argv[i], "-f")){
			argt[5] = 1;
		}
		else if(!strcmp(argv[i], "-k")){
			argt[6] = 1;
		}
		else if(!strcmp(argv[i], "-s")){
			argt[7] = 1;
		}
		else if(!strcmp(argv[i], "-v")){
			argt[8] = 1;
		}
		else if(!strcmp(argv[i], "-C")){
			argt[9] = 1;
            path[1]=argv[++i];
		}
		else if(!strcmp(argv[i], "-p")){
			argt[10] = 1;
		}
		else if(!strcmp(argv[i], "-z")){
			argt[11] = 1;
		}
		else if(!strcmp(argv[i], "-u")){
			argt[12] = 1;
		}
		else if(!strcmp(argv[i]+strlen(argv[i])-7, ".mtr.gz") || !strcmp(argv[i]+strlen(argv[i])-4, ".mtr")){
			if(!strcmp(argv[i-1], "-f")){
				path[0] = argv[i];
			}
			else{
				path[++c] = argv[i];
			}
		}	
		else{
			path[++c] = argv[i];
		}
	}
	if((!argt[0] && !argt[1] && !argt[2] && !argt[3] && !argt[4] && !argt[5]) || (c == 1 && (argt[0] || argt[1] || argt[2]))){
		write(2, "Erreur: parametre invalide\n", 27);
		return -1;
	}
	struct flock fl, fl1;
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;
	fl1 = fl;

	if(argt[1]){
		if(argt[5]){
			archive = open(path[0], O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IXUSR);
		}else{
			archive = 1;
		}
	}
	else{
		if(argt[5]){
			archive = open(path[0], O_RDWR);
		}else{
			archive = 0;
		}		
	}
	if(archive){	
		fcntl(archive, F_GETLK, &fl1);
	}
	if(archive && (fl1.l_type != F_UNLCK || fcntl(archive, F_SETLK, &fl) == -1)){
		perror("fcntl SETLCK");
		exit(EXIT_FAILURE);
	}

	if(argt[0]){
		char pass[33];
		char check;
		read(archive,pass,32);		
		if(!strcmp(pass,"0")){
			check='1';
		}else{
			char *verif=md52();
			printf("%s\n", verif);
			pass[32]='\0';
			if(!strcmp(verif,pass)){
				check=1;
			}else{
				check=0;
			}
		}if(check){
			if(argt[8]){
				if(argt[12]){
					// -a -v -u
					for(i = 2; i <= c; i++){
						mytar(1, archive, path[i],"");
					}
				}
				else{
					// -a -v
					for(i = 2; i <= c; i++){
						mytar(50, archive, path[i],"");
					}
				}
			}
			else{
				if(argt[12]){
					// -a -u
					for(i = 2; i <= c; i++){
						mytar(2, archive, path[i],"");
					}
				}
				else{
					// -a
					for(i = 2; i <= c; i++){
						mytar(5, archive, path[i],"");
					}
				}
			}
		}else{
			printf("Mauvais mot de passe ¯\\(ツ)/¯\n");	
		}
	}
	else if(argt[1]){
		if(argt[10]){
			char *pass=md52();
			write(archive,pass,32);
		}else{
			char *pass="0\0";
			write(archive,pass,32);
		}
		if(argt[7]){
			if(argt[8]){
				if(argt[9]){
					// -c -s -v -C
					st_archive *a =(st_archive *)malloc(sizeof(st_archive));
					mkdir(path[1],0766);
					struct stat st;
					stat(path[1],&st);
					a->mode = st.st_mode;
					a->a_time = st.st_atime;
					a->m_time = st.st_mtime;
					a->path_length = 1+strlen(path[1]);
					// a->path = (char *)malloc((sizeof(char))*a->path_length);
					sprintf(a->path, "%s%c", path[1], '\0');
					 ecrire(archive, 0, a);
					//write(archive,a,sizeof(st_archive));
					free(a);
					rmdir(path[1]);
					for(i = 2; i <= c; i++){
						mytar(23, archive, path[i],path[1]);
					}
				}
				else{
					// -c -s -v
					for(i = 2; i <= c; i++){
						mytar(24, archive, path[i],"");
					}
				}
			}
			else if(argt[9]){
				// -c -s -C
				st_archive *a =(st_archive *)malloc(sizeof(st_archive));
				mkdir(path[1],0766);
				struct stat st;
				stat(path[1],&st);
				a->mode = st.st_mode;
				a->a_time = st.st_atime;
				a->m_time = st.st_mtime;
				a->path_length = 1+strlen(path[1]);
				// a->path = (char *)malloc((sizeof(char))*a->path_length);
				sprintf(a->path, "%s%c", path[1], '\0');
				 ecrire(archive, 0, a);
				//write(archive,a,sizeof(st_archive));
				free(a);
				rmdir(path[1]);
				for(i = 2; i <= c; i++){
					mytar(22, archive, path[i],path[1]);
				}

			}
			else{
				// -c -s
				for(i = 2; i <= c; i++){
					mytar(12, archive, path[i],"");
				}
			}
		}
		else if(argt[8]){
			if(argt[9]){
				// -c -v -C
				st_archive *a =(st_archive *)malloc(sizeof(st_archive));
				mkdir(path[1],0766);
				struct stat st;
				stat(path[1],&st);
				a->mode = st.st_mode;
				a->a_time = st.st_atime;
				a->m_time = st.st_mtime;
				a->path_length = 1+strlen(path[1]);
				// a->path = (char *)malloc((sizeof(char))*a->path_length);
				sprintf(a->path, "%s%c", path[1], '\0');
				 ecrire(archive, 0, a);
				//write(archive,a,sizeof(st_archive));
				free(a);
				rmdir(path[1]);
				for(i = 2; i <= c; i++){
					mytar(25, archive, path[i],path[1]);
				}
			}
			else{

				// -c -v
				for(i = 2; i <= c; i++){
					mytar(15, archive, path[i],"");
				}
			}
		}
		else if(argt[9]){
			// -c -C
			st_archive *a =(st_archive *)malloc(sizeof(st_archive));
			mkdir(path[1],0766);
			struct stat st;
			stat(path[1],&st);
			a->mode = st.st_mode;
			a->a_time = st.st_atime;
			a->m_time = st.st_mtime;
			a->path_length = 1+strlen(path[1]);
			// a->path = (char *)malloc((sizeof(char))*a->path_length);
			sprintf(a->path, "%s%c", path[1], '\0');
			 ecrire(archive, 0, a);
			//write(archive,a,sizeof(st_archive));
			free(a);
			rmdir(path[1]);
            for(i = 2; i <= c; i++){
                mytar(17, archive, path[i],path[1]);
            }
		}
		else{
			// -c
			for(i = 2; i <= c; i++){
				mytar(10, archive, path[i],"");
			}
		}
		if(argt[11]){
			gzip(path[0],0);
		}
	}
	else if(argt[2]){
		// -d
		char pass[33];
		char check;
		read(archive,pass,32);
		if(!strcmp(pass,"0")){
			check='1';
		}else{
			char *verif=md52();
			printf("%s\n", verif);
			pass[32]='\0';
			if(!strcmp(verif,pass)){
				check=1;
			}else{
				check=0;
			}
			free(verif);
		}
		if(check){
			
			
			for(i = 2; i <= c; i++){
				int newarchive = open(".tmp.mtr", O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IXUSR);
				write(newarchive,pass,32);
				mytar(3, archive, path[i],"");
				unlink(path[0]);
				rename(".tmp.mtr", path[0]);
				struct flock fl2, fl3;
				fl2.l_type = F_WRLCK;
				fl2.l_whence = SEEK_SET;
				fl2.l_start = 0;
				fl2.l_len = 0;
				fl3 = fl;
				//close(archive);
				//archive = open(path[0], O_RDWR);
				archive=dup(newarchive);
				fcntl(archive, F_GETLK, &fl3);
				if(fl3.l_type != F_UNLCK || fcntl(archive, F_SETLK, &fl2) == -1){
					perror("fcntl SETLCK");
					exit(EXIT_FAILURE);
				}
			}
		}else{
			printf("Mauvais mot de passe ¯\\(ツ)/¯\n");
		}
		
	}
	else if(argt[3]){
		// -l
		if(argt[11]){
			gzip(path[0],1);
			char pa[3+strlen(path[0])];
			char test[strlen(path[0])-3];
			for(i=0; i<strlen(path[0])-3;i++){
				test[i]=path[0][i];
			}
			sprintf(pa,"/tmp/%s%c",test,'\0');
			archive = open(pa, O_RDWR);

		}
		char pass[33];
		char check;
		read(archive,pass,32);
		if(!strcmp(pass,"0")){
			check='1';
		}else{
			char *verif=md52();
			printf("%s\n", verif);
			pass[32]='\0';
			if(!strcmp(verif,pass)){
				check=1;
			}else{
				check=0;
			}
			free(verif);
		}
		if(check){
			mytar(6, archive, "","");
		}else{
			printf("Mauvais mot de passe ¯\\(ツ)/¯\n");
		}
		if(argt[11]){
			char pa[3+strlen(path[0])];
			char test[strlen(path[0])-3];
			for(i=0; i<strlen(path[0])-3;i++){
				test[i]=path[0][i];
			}
			sprintf(pa,"/tmp/%s%c",test,'\0');
			unlink(pa);

		}
	}
	else if(argt[4]){
		if(argt[11]){
			gzip(path[0],1);
			char pa[3+strlen(path[0])];
			char test[strlen(path[0])-3];
			for(i=0; i<strlen(path[0])-3;i++){
				test[i]=path[0][i];
			}
			sprintf(pa,"/tmp/%s%c",test,'\0');
			archive = open(pa, O_RDWR);

		}
		char pass[33];
		char check;
		read(archive,pass,32);
		if(!strcmp(pass,"0")){
			check='1';
		}else{
			char *verif=md52();
			printf("%s\n", verif);
			pass[32]='\0';
			if(!strcmp(verif,pass)){
				check=1;
			}else{
				check=0;
			}
			free(verif);
		}
		if(check){
			if(argt[6]){
				if(argt[7]){
					if(argt[8]){
						if(argt[9]){
							// -x -k -s -v -C (-u)
							mytar(34, archive, path[2],path[1]);
						}
						else{
							// -x -k -s -v (-u)
							mytar(33, archive, path[2],"");
						}
					}
					else if(argt[9]){
						// -x -k -s -C (-u)
						mytar(35, archive, path[2],path[1]);
					}
					else{
						// -x -k -s (-u)
						mytar(32, archive, path[2],"");
					}
				}
				else if(argt[8]){
					if(argt[9]){
						// -x -k -v -C (-u)
						mytar(31, archive, path[2],path[1]);
					}
					else{
						// -x -k -v (-u)
						mytar(30, archive, path[2],"");
					}
				}
				else{
					// -x -k (-u)
					mytar(8, archive, path[2],"");
				}
			}
			else if(argt[7]){
				if(argt[8]){
					if(argt[9]){
						if(argt[12]){
							// -x -s -v -C -u
							mytar(272, archive, path[2],path[1]);
						}
						else{
							// -x -s -v -C
							mytar(27, archive, path[2],path[1]);
						}
					}
					else{
						if(argt[12]){
							// -x -s -v -u
							mytar(262, archive, path[2],"");
						}
						else{
							// -x -s -v
							mytar(26, archive, path[2],"");
						}
					}
				}
				else if(argt[9]){
					if(argt[12]){
						// -x -s -C -u
						mytar(292, archive, path[2],path[1]);
					}
					else{
						// -x -s -C
						mytar(29, archive, path[2],path[1]);
					}
				}
				else{
					if(argt[12]){
						// -x -s -u
						mytar(142, archive, path[2],"");
					}
					else{
						// -x -s
						mytar(14, archive, path[2],"");
					}
				}
			}
			else if(argt[8]){
				if(argt[9]){
					if(argt[12]){
						// -x -v -C -u
						mytar(282, archive, path[2],path[1]);
					}
					else{
						// -x -v -C
						mytar(28, archive, path[2],path[1]);
					}
				}
				else{
					if(argt[12]){
						// -x -v -u
						mytar(162, archive, "","");
					}
					else{
						// -x -v
						mytar(16, archive, "","");
					}
				}
			}
			else if(argt[9]){
				if(argt[12]){
					// -x -C -u
					mytar(182, archive, path[2],path[1]);
				}
				else{
					// -x -C
					mytar(18, archive, path[2],path[1]);
				}
			}
			else{
				if(argt[12]){
					// -x -u
					mytar(212, archive, path[2],"");
				}
				else{
					// -x
					mytar(21, archive, path[2],"");
				}
			}
		}else{
			printf("Mauvais mot de passe ¯\\(ツ)/¯\n");
		}
		if(argt[11]){
			char pa[3+strlen(path[0])];
			char test[strlen(path[0])-3];
			for(i=0; i<strlen(path[0])-3;i++){
				test[i]=path[0][i];
			}
			sprintf(pa,"/tmp/%s%c",test,'\0');
			unlink(pa);

		}
	}
	
	/*
	Concept pour ameliorer le code :
	int i, n = 1, s = 0;
	for(i = 0; i < 10; i++){
		if(argt[i]){
			s += n;
		}
		n *= 2;
	}
	mytar(s, archive, path[1]);
	*/
	
	close(archive);
	return 0;
}
