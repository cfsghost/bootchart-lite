/*
 * Copyright (C) 2008 Fred Chien <fred@openmoko.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>

#define LOGTMP         "/etc/bootchart-lite"
#define PROC           "/proc"
#define PROC_UPTIME    "/proc/uptime"
#define PROC_STAT      "/proc/stat"
#define PROC_DISKSTATS "/proc/diskstats"
#define PROC_CMDLINE   "/proc/cmdline"
#define PROC_VERSION   "/proc/version"
#define BUFSIZE        1024
#define EXIT_PROC      "quicklauncher"

static char exit_proc[] = { "(" EXIT_PROC ")" };
static char buffer[BUFSIZE];
static char uptime[10];
static int  uptime_count;
/* proc fd */
static int  p_uptime_fd = 0;
static int  p_stat_fd = 0;
static int  p_diskstats_fd = 0;
/* log fd */
static int  header_fd;
static int  stat_fd;
static int  diskstats_fd;
static int  ps_fd;
static int  running = 1;
static int  exit_proc_count = sizeof(exit_proc) - 1;
static int  check_maxlen = sizeof(exit_proc) + 5;

void USR1(int i) {
	running = 0;
	signal(SIGUSR1, USR1);
}

int
check_exitproc(const char *source)
{
	int i;
	int j;

	for (i=0;i<check_maxlen;i++) {
		if (*(++source)==' ') {
			for (j=0;j<exit_proc_count;j++) {
				if (*(++source)!=exit_proc[j])
					return 1;
			}

			break;
		}
	}

	return 0;
}

int get_uptime()
{
	int i;
	int j;

	if (!p_uptime_fd) {
		if ((p_uptime_fd=open(PROC_UPTIME, O_RDONLY))==-1) {
			p_uptime_fd = 0;
			return 0;
		}
	}

	lseek(p_uptime_fd, SEEK_SET, 0);

	/* read uptime */
	read(p_uptime_fd, buffer, 9);
	for (i=0,j=0;i<10;i++, j++) {
		if (buffer[i]==' ')
			break;

		if (buffer[i]=='.') {
			j--;
			continue;
		}

		uptime[j] = buffer[i];
	}

	uptime[j] = '\0';
	uptime_count = j;

	return 1;
}

void fetch_data_with_uptime(int fd_src, int fd_dest, const char *source)
{
	/* write uptime */
	write(fd_dest, uptime, uptime_count);
	write(fd_dest, "\n", 1);
	fetch_data(fd_src, fd_dest, source);
	write(fd_dest, "\n", 1);
}

int
fetch_data(int fd_source, int fd_dest, const char *source)
{
	int count;
	int fd_src;

	if (!fd_source) {
		/* open the source file */
		if ((fd_src=open(source, O_RDONLY))==-1)
			return -1;
	} else {
		fd_src = fd_source;
	}

	lseek(fd_src, SEEK_SET, 0);

	/* copy information to destination */
	while((count=read(fd_src, buffer, BUFSIZE-1))>0) {
		buffer[count] = '\0';
		write(fd_dest, buffer, count);
	}

	return 0;
}

void
fetch_data_ps(int fd_dest)
{
	DIR *dir;
	struct dirent *file;
	int fd_src;
	int count;
	char path[17];

	/* write uptime */
	write(fd_dest, uptime, uptime_count);
	write(fd_dest, "\n", 1);

	/* open directory */
	if ((dir=opendir(PROC))==NULL)
		exit(1);

	sprintf(path, PROC "/self/stat");
	fd_src = open(path, O_RDONLY);
	while((count=read(fd_src, buffer, BUFSIZE-1))>0) {
		buffer[count] = '\0';
		write(fd_dest, buffer, count);
	}

	close(fd_src);

	while((file=readdir(dir))) {
		if (*file->d_name>'9'||*file->d_name<'1')
			continue;

		sprintf(path, PROC "/%s/stat", file->d_name);

		if ((fd_src=open(path, O_RDONLY))==-1)
			continue;

		while((count=read(fd_src, buffer, BUFSIZE-1))>0) {
			buffer[count] = '\0';

			running = running ? check_exitproc(&buffer) : 0;

			write(fd_dest, buffer, count);
		}

		close(fd_src);
	}

	write(fd_dest, "\n", 1);

	closedir(dir);
}

int
main(int argc, char **argv)
{
	pid_t pid;

	/* Run daemon in the background */
    pid = fork();
    if (pid>0) {
		/* execute init */
		if (argc>1) {
			char *new_argv[] = { "/sbin/init", argv[1], (char *)0 };
			execv("/sbin/init", new_argv);
		} else {
			char *new_argv[] = { "/sbin/init", (char *)0 };
			execv("/sbin/init", new_argv);
		}

        return 0;
    }

	/* signal handler */
	signal(SIGUSR1, USR1);

	while(!get_uptime()) {
		/* 0.1 second */
		usleep(100000);
	}

	/* open destination file first */
	if ((stat_fd=open(LOGTMP "/proc_stat.log", O_WRONLY | O_TRUNC | O_CREAT, 0755))==-1) {
		printf("fbootchart: cannot create %s\n", LOGTMP "/proc_stat.log");
		exit(1);
	}

	if ((diskstats_fd=open(LOGTMP "/proc_diskstats.log", O_WRONLY | O_TRUNC | O_CREAT, 0755))==-1) {
		printf("fbootchart: cannot create %s\n", LOGTMP "/proc_diskstats.log");
		exit(1);
	}

	if ((ps_fd=open(LOGTMP "/proc_ps.log", O_WRONLY | O_TRUNC | O_CREAT, 0755))==-1) {
		printf("fbootchart: cannot create %s\n", LOGTMP "/proc_ps.log");
		exit(1);
	}

	while(running) {
		get_uptime();
		fetch_data_ps(ps_fd);
		fetch_data_with_uptime(p_stat_fd, stat_fd, PROC_STAT);
		fetch_data_with_uptime(p_diskstats_fd, diskstats_fd, PROC_DISKSTATS);

		/* 0.1 second */
		usleep(100000);
	}

	close(p_uptime_fd);
	close(p_stat_fd);
	close(p_diskstats_fd);
	close(ps_fd);
	close(diskstats_fd);
	close(stat_fd);

	/* create header file */
	if ((header_fd=open(LOGTMP "/header", O_WRONLY | O_TRUNC | O_CREAT))==-1) {
		printf("fbootchart: cannot create %s\n", LOGTMP "/header");
		exit(1);
	}

	write(header_fd, "version = 0.8\n", 14);
	write(header_fd, "title = Boot Chart by fbootchart\n", 33);
	write(header_fd, "system.uname = ", 15);
	fetch_data(0, header_fd, PROC_VERSION);
	write(header_fd, "system.release = not support yet\n", 33);
	write(header_fd, "system.cpu = not support yet\n", 29);
	write(header_fd, "system.kernel.options = ", 24);
	fetch_data(0, header_fd, PROC_CMDLINE);
	close(header_fd);

    return 0;
}
