/*
 *  Copyright (C) 2009 Sourcefire, Inc.
 *
 *  Author: aCaB
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#if HAVE_CONFIG_H
#include "clamav-config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <errno.h>
#include <libgen.h>

#include "shared/optparser.h"
#include "shared/output.h"
#include "shared/misc.h"

void (*action)(const char *) = NULL;
unsigned int notmoved = 0, notremoved = 0;

static char *actarget;
static int targlen;



int getdest(const char *fullpath, char **newname) {
    char *tmps, *filename;
    int fd, i;

    tmps = strdup(fullpath);
    if(!tmps) return -1;
    filename = basename(tmps);

    if(!(*newname = (char *)malloc(targlen + strlen(filename) + 6))) {
	free(tmps);
	return -1;
    }
    sprintf(*newname, "%s/%s", actarget, filename);
    for(i=1; i<1000; i++) {
	fd = open(*newname, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if(fd >= 0) {
	    free(tmps);
	    return fd;
	}
	if(errno != EEXIST) break;
	sprintf(*newname, "%s/%s.%03u", actarget, filename, i);
    }
    free(tmps);
    free(*newname);
    *newname = NULL;
    return -1;
}

static void action_move(const char *filename) {
    char *nuname;
    int fd = getdest(filename, &nuname);

    if(fd<0 || rename(filename, nuname) || filecopy(filename, nuname)) {
	logg("!Can't move file %s\n", filename);
	notmoved++;
	if(nuname) unlink(nuname);
    } else {
	if(unlink(filename))
	    logg("!Can't unlink '%s': %s\n", filename, strerror(errno));
	else
	    logg("~%s: moved to '%s'\n", filename, nuname);
    }

    if(fd>=0) close(fd);
    if(nuname) free(nuname);
}

static void action_copy(const char *filename) {
    char *nuname;
    int fd = getdest(filename, &nuname);

    if(fd < 0 || filecopy(filename, nuname)) {
	logg("!Can't copy file '%s'\n", filename);
	notmoved++;
	if(nuname) unlink(nuname);
    } else
	logg("~%s: copied to '%s'\n", filename, nuname);

    if(fd>=0) close(fd);
    if(nuname) free(nuname);
}

static void action_remove(const char *filename) {
    if(unlink(filename)) {
	logg("!Can't remove.\n", filename);
	notremoved++;
    } else {
	logg("~%s: Removed.\n", filename);
    }
}

int isdir() {
    struct stat sb;
    if(stat(actarget, &sb) || !S_ISDIR(sb.st_mode)) {
	logg("!'%s' doesn't exist or is not a directory\n", actarget);
	return 1;
    }
    return 0;
}

int actsetup(const struct optstruct *opts) {
    int move = optget(opts, "move")->enabled;
    if(move || optget(opts, "copy")->enabled) {
	actarget = optget(opts, move ? "move" : "copy")->strarg;
	if(!isdir()) return 1;
	action = move ? action_move : action_copy;
	targlen = strlen(actarget);
    } else if(optget(opts, "remove")->enabled)
	action = action_remove;
    return 0;
}
