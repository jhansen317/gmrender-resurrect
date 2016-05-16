/* logging.c - Logging facility
 *
 * Copyright (C) 2013 Henner Zeller
 *
 * This file is part of GMediaRender.
 *
 * GMediaRender is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GMediaRender is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GMediaRender; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
 * MA 02110-1301, USA.
 *
 */

#define _GNU_SOURCE

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include "logging.h"
#include "config.h"
#include "git-version.h"

static int log_fd = -1;
static int last_played_fd = -1;
static int playback_fd = -1;
static int enable_color = 0;
static const char *const kInfoHighlight  = "\033[1mINFO  ";
static const char *const kErrorHighlight = "\033[1m\033[31mERROR ";
static const char *const kTermReset      = "\033[0m";

static const char *info_markup_start_  = "INFO  ";
static const char *error_markup_start_ = "ERROR ";
static const char *markup_end_ = "";




void Log_init(const char *filename, const char *last_played_file, const char *playback_time_file) {
	if (filename != NULL)
	{
		log_fd = open(filename, O_CREAT|O_APPEND|O_WRONLY, 0644);
		if (log_fd < 0) {
			perror("Cannot open logfile");
			return;
		}
	}
	if (last_played_file != NULL)
	{
		last_played_fd = open(last_played_file, O_CREAT|O_APPEND|O_WRONLY, 0644);
		if (last_played_fd < 0) {
			perror("Cannot open last played file");
			return;
		}
	}
	if (playback_time_file != NULL)
	{
		playback_fd = open(playback_time_file, O_CREAT|O_APPEND|O_WRONLY, 0644);
		if (playback_fd < 0) {
			perror("Cannot open playback time file");
			return;
		}
	}
	enable_color = isatty(log_fd);
	if (enable_color) {
		info_markup_start_ = kInfoHighlight;
		error_markup_start_ = kErrorHighlight;
		markup_end_ = kTermReset;
	}
}

int Log_color_allowed(void) { return enable_color; }
int Log_info_enabled(void) { return log_fd >= 0; }
int Log_error_enabled(void) { return 1; }

static void Log_internal(int fd, const char *markup_start,
			 const char *category, const char *format,
			 va_list ap) {
	struct timeval now;
	gettimeofday(&now, NULL);
	struct tm time_breakdown;
	localtime_r(&now.tv_sec, &time_breakdown);
	char fmt_buf[128];
	strftime(fmt_buf, sizeof(fmt_buf), "%F %T", &time_breakdown);
	struct iovec parts[3];
	parts[0].iov_len = asprintf((char**) &parts[0].iov_base,
				    "%s[%s | %s]%s ",
				    markup_start, fmt_buf, 
				    category, markup_end_);
	parts[1].iov_len = vasprintf((char**) &parts[1].iov_base, format, ap);
	parts[2].iov_base = (void*) "\n";
	parts[2].iov_len = 1;
	int already_newline 
		= (parts[1].iov_len > 0 &&
		   ((const char*)parts[1].iov_base)[parts[1].iov_len-1] == '\n');
	if (writev(fd, parts, already_newline ? 2 : 3) < 0) {
		// Logging trouble. Ignore.
	}

	free(parts[0].iov_base);
	free(parts[1].iov_base);
}

void Log_info(const char *category, const char *format, ...) {
	if (log_fd < 0) return;
	va_list ap;
	va_start(ap, format);
	Log_internal(log_fd, info_markup_start_, category, format, ap);
	va_end(ap);
}

void Log_error(const char *category, const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	Log_internal(log_fd < 0 ? STDERR_FILENO : log_fd,
		     error_markup_start_, category, format, ap);
	va_end(ap);
}

void print_log(int level, const char *category, const char *format, ...) {
	if (level > debug_level) return;
	va_list ap;
        va_start(ap, format);
        Log_internal(log_fd < 0 ? STDERR_FILENO : log_fd,
                     info_markup_start_, category, format, ap);
        va_end(ap);
}

void log_last_playback(time_t playstart){
	struct tm *timeinfo;
	char timestring[80];
	char buffer[80];
	int buflength;

	timeinfo = gmtime(&playstart);
	strftime(timestring, 80, "%F %T", timeinfo);
	buflength = sprintf(buffer, "UPNP_LAST_PLAYED=\'%s\'\n", timestring);
	ftruncate(last_played_fd, 0);
	write(last_played_fd, buffer, buflength);
}



void log_playback_duration(time_t playstart, time_t playend){
	int elapsedHours, elapsedMin, elapsedSec, rawSeconds;
	char buffer[80];
	int buflength;

	rawSeconds = (int)difftime(playend, playstart);
	elapsedHours = rawSeconds/3600;
	elapsedMin = (rawSeconds/60)%60;
	elapsedSec = (rawSeconds)%60;
	buflength = sprintf(buffer, "UPNP_TOTAL=%d\n", rawSeconds);
	ftruncate(playback_fd, 0);
	write(playback_fd, buffer, buflength);
	print_log(0, "transport", "Total playing time %02d:%02d:%02d\n", elapsedHours, elapsedMin, elapsedSec);

}


