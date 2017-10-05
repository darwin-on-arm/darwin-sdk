/* xcode-select - clone of apple's xcode-select utility
 *
 * Copyright (c) 2013-2017, Brian McKenzie <mckenzba@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the organization nor the names of its contributors may
 *     be used to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#define TOOL_VERSION "1.0.1"
#define SDK_CFG ".xcdev.dat"

/**
 * @func usage -- Print helpful information about this tool.
 * @arg prog - name of this tool
 */
static void usage(void)
{

	fprintf(stderr,
			"Usage: xcode-select -print-path\n"
			"   or: xcode-select -switch <sdk_folder_path>\n"
			"   or: xcode-select -version\n"
			"Arguments:\n"
			"   -print-path                     Prints the path of the current SDK folder\n"
			"   -switch <xcode_folder_path>     Sets the path for the current SDK folder\n"
			"   -version                        Prints xcode-select version information\n\n");

	exit(1);
}

/**
 * @func usage -- Print the tool version.
 */
static void version(void)
{
	fprintf(stdout, "xcode-select version %s\n", TOOL_VERSION);

	exit(0);
}

/**
 * @func validate_directory_path -- validate if requested directory path exists
 * @arg dir - directory to validate
 * @return: 0 on success, -1 on failure
 */
static int validate_directory_path(const char *dir)
{
	int status = -1;
	struct stat fstat;

	if (stat(dir, &fstat) != 0)
		fprintf(stderr, "xcode-select: error: unable to validate directory \'%s\' (%s)\n", dir, strerror(errno));
	else {
		if (S_ISDIR(fstat.st_mode) == 0)
			fprintf(stderr, "xcode-select: error: \'%s\' is not a directory, please try a different path\n", dir);
		else
			status = 0;
	}

	return status;
}

/**
 * @func get_developer_path -- retrieve current developer path
 * @return: number of bytes read
 */
static int get_developer_path(char *path)
{
	FILE *fp;
	int len = 0;
	char *home_path, *cfg_path, *dev_path;

	if ((dev_path = getenv("DEVELOPER_DIR")) != NULL) {
		len = strlen(dev_path);
		strncpy(path, dev_path, len);
		return len;
	}

	if ((home_path = getenv("HOME")) == NULL) {
		fprintf(stderr, "xcode-select: error: failed to read HOME environment variable.\n");
		return len;
	}

	cfg_path = (char *)calloc((strlen(home_path) + strlen(SDK_CFG) + 2), sizeof(char));

	strcat(home_path, "/");
	strcat(cfg_path, strcat(home_path, SDK_CFG));

	if ((fp = fopen(cfg_path, "r")) != NULL) {
		fseek(fp, 0, SEEK_END);
		int fsize = ftell(fp);
		fseek(fp, SEEK_SET, 0);
		len = fread(path, fsize, 1, fp);
		fclose(fp);
	} else {
		fprintf(stderr, "xcode-select: error: unable to read configuration file. (%s)\n", strerror(errno));
	}

	free(cfg_path);

	return len;
}

/**
 * @func set_developer_path -- set the current developer path
 * @arg path - path to set
 * @return: 0 on success, -1 on failure
 */
static int set_developer_path(const char *path)
{
	FILE *fp;
	int status = -1;
	char *cfg_path, *home_path;

	if (validate_directory_path(path) < 0)
		return status;

	if ((home_path = getenv("HOME")) == NULL) {
		fprintf(stderr, "xcode-select: error: failed to read HOME variable.\n");
		return status;
	}

        cfg_path = (char *)calloc((strlen(home_path) + strlen(SDK_CFG) + 2), sizeof(char));

        strcat(home_path, "/");
	strcat(cfg_path, strcat(home_path, SDK_CFG));

	if ((fp = fopen(cfg_path, "w+")) != NULL) {
		fwrite(path, 1, strlen(path), fp);
		fclose(fp);
		status = 0;
	} else {
		fprintf(stderr, "xcode-select: error: unable to open configuration file. (%s)\n", strerror(errno));
	}

	free(cfg_path);

	return status;
}

int main(int argc, char *argv[])
{
	int ch;
	int status = -1;
	char path[PATH_MAX] = { 0 };

	if (argc < 2)
		usage();

	struct option options[] = {
		{ "help", no_argument, 0, 'h' },
		{ "version", no_argument, 0, 'v' },
		{ "switch", required_argument, 0, 's' },
		{ "print-path", no_argument, 0, 'p' },
		{ NULL, 0, 0, 0 }
	};

	while ((ch = getopt_long_only(argc, argv, "hvs:p", options, NULL)) != (-1)) {
		switch (ch) {
			case 'h':
				usage();
				break;
			case 'v':
				version();
				break;
			case 's':
				strncpy(path, optarg, strlen(optarg));
				status = set_developer_path(path);
				break;
			case 'p':
				if (get_developer_path(path) > 0) {
					fprintf(stdout, "%s\n", path);
					status = 0;
				}
				break;
			case '?':
			default:
				usage();
		}
	}

	return status;
}
