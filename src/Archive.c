#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include "Archive.h"
#if defined(__linux__)
	#define MD5 "md5sum"
#elif defined(__APPLE__)
	#define MD5 "md5"
#endif

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

/* Fonction servant a ecrire le contenu
** d'une st_archive dans l'archive
*/
int ecrire(int archive, int type, st_archive *a){
	write(archive, &(a->path_length), sizeof(size_t));
	if(type){
		write(archive, &(a->file_length), sizeof(off_t));
	}else{
		off_t tmp = 0;
		write(archive, &tmp, sizeof(off_t));
	}
	write(archive, &(a->mode), sizeof(mode_t));
	write(archive, &(a->a_time), sizeof(time_t));
	write(archive, &(a->m_time), sizeof(time_t));
  	write(archive, (a->checksum), 32);
	write(archive, (a->path), a->path_length);
	return 0;
}

/* Fonction servant a lire le contenu
** d'une archive et l'enregistrer dans une struc
** archive
*/
st_archive *lire(int archive){
	st_archive *a;
	size_t pl;
	off_t fl;
	read(archive, &pl, sizeof(size_t));
	read(archive, &fl, sizeof(off_t));
	if(!pl || pl>10000){
		return NULL;
	}
	if(fl){
		a = (st_archive *)malloc(fl+sizeof(st_archive));
		a->file_length = fl;
	}else{
		a = (st_archive *)malloc(sizeof(st_archive));
	}
	a->path_length = pl;
	read(archive, &(a->mode), sizeof(mode_t));
	read(archive, &(a->a_time), sizeof(time_t));
	read(archive, &(a->m_time), sizeof(time_t));
  	read(archive, (a->checksum), 32);
	//a->path = (char *)malloc(a->path_length);
	read(archive, a->path, a->path_length);
	/*if(fl){
		a->file = (char *)malloc(fl);
		read(archive, (a->file), a->file_length);
	}*/
	return a;
}
