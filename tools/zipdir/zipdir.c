/*
** zipdir.c
** Copyright (C) 2008-2009 Randy Heit
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
****************************************************************************
**
** Usage: zipdir [-dfuq] <zip file> <directory> ...
**
** Given one or more directories, their contents are scanned recursively.
** If any files are newer than the zip file or the zip file does not exist,
** then everything in the specified directories is stored in the zip. The
** base directory names are not stored in the zip file, but subdirectories
** recursed into are stored.
*/

// HEADER FILES ------------------------------------------------------------

#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <io.h>
#define stat _stat
#else
#include <dirent.h>
#if __has_include(<fts.h>)
#define HAVE_FTS_H 1
#include <fts.h>
#endif
#endif
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include "zlib.h"
#include "bzlib.h"
#include "LzmaEnc.h"
#include "7zVersion.h"
#ifdef PPMD
#include "../../ppmd/PPMd.h"
#endif

// MACROS ------------------------------------------------------------------

#ifdef __GNUC__
// With versions of GCC newer than 4.2, it appears it was determined that the
// cost of an unaligned pointer on PPC was high enough to add padding to the
// end of packed structs.  For whatever reason __packed__ and pragma pack are
// handled differently in this regard. Note that this only needs to be applied
// to types which are used in arrays.
#define FORCE_PACKED __attribute__((__packed__))
#else
#define FORCE_PACKED
#endif

#ifndef __BIG_ENDIAN__
#define MAKE_ID(a,b,c,d)	((a)|((b)<<8)|((c)<<16)|((d)<<24))
#define LittleShort(x)		(x)
#define LittleLong(x)		(x)
#else
#define MAKE_ID(a,b,c,d)	((d)|((c)<<8)|((b)<<16)|((a)<<24))
static unsigned short LittleShort(unsigned short x)
{
	return (x>>8) | (x<<8);
}

static unsigned int LittleLong(unsigned int x)
{
	return (x>>24) | ((x>>8) & 0xff00) | ((x<<8) & 0xff0000) | (x<<24);
}
#endif

#define ZIP_LOCALFILE	MAKE_ID('P','K',3,4)
#define ZIP_CENTRALFILE	MAKE_ID('P','K',1,2)
#define ZIP_ENDOFDIR	MAKE_ID('P','K',5,6)

#define METHOD_STORED	0
#define METHOD_DEFLATE	8
#define METHOD_BZIP2	12
#define METHOD_LZMA		14
#define METHOD_PPMD		98

// Buffer size for central directory search
#define BUFREADCOMMENT	(0x400)

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// TYPES -------------------------------------------------------------------

typedef struct file_entry_s
{
	struct file_entry_s *next;
	time_t time_write;
	unsigned int uncompressed_size;
	unsigned int compressed_size;
	unsigned int crc32;
	unsigned int zip_offset;
	short date, time;
	short method;
	char path[];
} file_entry_t;

typedef struct dir_tree_s
{
	struct dir_tree_s *next;
	file_entry_t *files;
	size_t path_size;
	char path[];
} dir_tree_t;

typedef struct file_sorted_s
{
	file_entry_t *file;
	char *path_in_zip;
} file_sorted_t;

typedef struct compressor_s
{
	int (*compress)(Byte *out, unsigned int *outlen, const Byte *in, unsigned int inlen);
	int method;
} compressor_t;

typedef unsigned int UINT32;
typedef unsigned short WORD;
typedef unsigned char BYTE;

// [BL] Solaris (well GCC on Solaris) doesn't seem to support pack(push/pop, 1) so we'll need to use use it
//      on the whole file.
#pragma pack(1)
//#pragma pack(push,1)
typedef struct
{
	UINT32	Magic;						// 0
	BYTE	VersionToExtract[2];		// 4
	WORD	Flags;						// 6
	WORD	Method;						// 8
	WORD	ModTime;					// 10
	WORD	ModDate;					// 12
	UINT32	CRC32;						// 14
	UINT32	CompressedSize;				// 18
	UINT32	UncompressedSize;			// 22
	WORD	NameLength;					// 26
	WORD	ExtraLength;				// 28
} FORCE_PACKED LocalFileHeader;

typedef struct
{
	UINT32	Magic;
	BYTE	VersionMadeBy[2];
	BYTE	VersionToExtract[2];
	WORD	Flags;
	WORD	Method;
	WORD	ModTime;
	WORD	ModDate;
	UINT32	CRC32;
	UINT32	CompressedSize;
	UINT32	UncompressedSize;
	WORD	NameLength;
	WORD	ExtraLength;
	WORD	CommentLength;
	WORD	StartingDiskNumber;
	WORD	InternalAttributes;
	UINT32	ExternalAttributes;
	UINT32	LocalHeaderOffset;
} FORCE_PACKED CentralDirectoryEntry;

typedef struct
{
	UINT32	Magic;
	WORD	DiskNumber;
	WORD	FirstDisk;
	WORD	NumEntries;
	WORD	NumEntriesOnAllDisks;
	UINT32	DirectorySize;
	UINT32	DirectoryOffset;
	WORD	ZipCommentLength;
} FORCE_PACKED EndOfCentralDirectory;
//#pragma pack(pop)

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void print_usage(const char *cmdname);
dir_tree_t *alloc_dir_tree(const char *dir);
file_entry_t *alloc_file_entry(const char *prefix, const char *path, time_t last_written);
void free_dir_tree(dir_tree_t *tree);
void free_dir_trees(dir_tree_t *tree);
#ifdef _WIN32
void recurse_dir(dir_tree_t *tree, const char *dirpath);
dir_tree_t *add_dir(const char *dirpath);
#endif
dir_tree_t *add_dirs(char **argv);
int count_files(dir_tree_t *trees);
int sort_cmp(const void *a, const void *b);
file_sorted_t *sort_files(dir_tree_t *trees, int num_files);
void write_zip(const char *zipname, dir_tree_t *trees, int update);
int append_to_zip(FILE *zip_file, file_sorted_t *file, FILE *ozip, BYTE *odir);
int write_central_dir(FILE *zip, file_sorted_t *file);
void time_to_dos(struct tm *time, short *dosdate, short *dostime);
int method_to_version(int method);
const char *method_name(int method);
int compress_lzma(Byte *out, unsigned int *outlen, const Byte *in, unsigned int inlen);
int compress_bzip2(Byte *out, unsigned int *outlen, const Byte *in, unsigned int inlen);
int compress_ppmd(Byte *out, unsigned int *outlen, const Byte *in, unsigned int inlen);
int compress_deflate(Byte *out, unsigned int *outlen, const Byte *in, unsigned int inlen);
BYTE *find_central_dir(FILE *fin);
CentralDirectoryEntry *find_file_in_zip(BYTE *dir, const char *path, unsigned int len, unsigned int crc, short date, short time);
int copy_zip_file(FILE *zip, file_entry_t *file, FILE *ozip, CentralDirectoryEntry *dirent);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void *SzAlloc(ISzAllocPtr p, size_t size) { p = p; return malloc(size); }
static void SzFree(ISzAllocPtr p, void *address) { p = p; free(address); }

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int DeflateOnly;
int UpdateCount;
int Quiet;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const UINT32 centralfile = ZIP_CENTRALFILE;
static const UINT32 endofdir = ZIP_ENDOFDIR;

static int no_mem;

static ISzAlloc Alloc = { SzAlloc, SzFree };
static compressor_t Compressors[] =
{
	{ compress_lzma,	METHOD_LZMA },
	{ compress_bzip2,	METHOD_BZIP2 },
#ifdef PPMD
	{ compress_ppmd,	METHOD_PPMD },
#endif
	{ compress_deflate,	METHOD_DEFLATE },
	{ NULL, 0 }
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// print_usage
//
//==========================================================================

void print_usage(const char *cmdname)
{
#ifdef _WIN32
	const char *rchar = strrchr(cmdname, '\\');
	if (rchar != NULL)
	{
		cmdname = rchar+1;
	}
#endif
	fprintf(stderr, "Usage: %s [options] <zip file> <directory> ...\n"
					"Options: -d  Use deflate compression only\n"
					"         -f  Force creation of archive\n"
					"         -u  Only update changed files\n"
					"         -q  Do not list files\n", cmdname);
}

//==========================================================================
//
// alloc_dir_tree
//
//==========================================================================

dir_tree_t *alloc_dir_tree(const char *dir)
{
	dir_tree_t *tree;
	size_t dirlen;

	dirlen = strlen(dir);
	tree = malloc(sizeof(dir_tree_t) + dirlen + 2);
	if (tree != NULL)
	{
		strcpy(tree->path, dir);
		tree->path_size = dirlen;
		if (dir[dirlen - 1] != '/')
		{
			tree->path_size++;
			tree->path[dirlen] = '/';
			tree->path[dirlen + 1] = '\0';
		}
		tree->files = NULL;
		tree->next = NULL;
	}
	return tree;
}

//==========================================================================
//
// alloc_file_entry
//
//==========================================================================

file_entry_t *alloc_file_entry(const char *prefix, const char *path, time_t last_written)
{
	file_entry_t *entry;

	entry = malloc(sizeof(file_entry_t) + strlen(prefix) + strlen(path) + 1);
	if (entry != NULL)
	{
		strcpy(entry->path, prefix);
		strcat(entry->path, path);
		entry->next = NULL;
		entry->time_write = last_written;
	}
	return entry;
}

//==========================================================================
//
// free_dir_tree
//
//==========================================================================

void free_dir_tree(dir_tree_t *tree)
{
	file_entry_t *entry, *next;

	if (tree != NULL)
	{
		for (entry = tree->files; entry != NULL; entry = next)
		{
			next = entry->next;
			free(entry);
		}
		free(tree);
	}
}

//==========================================================================
//
// free_dir_trees
//
//==========================================================================

void free_dir_trees(dir_tree_t *tree)
{
	dir_tree_t *next;

	for (; tree != NULL; tree = next)
	{
		next = tree->next;
		free_dir_tree(tree);
	}
}

#ifdef _WIN32

//==========================================================================
//
// recurse_dir
//
//==========================================================================

void recurse_dir(dir_tree_t *tree, const char *dirpath)
{
	struct _finddata_t fileinfo;
	intptr_t handle;
	char *dirmatch;

	dirmatch = malloc(strlen(dirpath) + 2);
	if (dirmatch == NULL)
	{
		no_mem = 1;
		return;
	}
	strcpy(dirmatch, dirpath);
	strcat(dirmatch, "*");
	if ((handle = _findfirst(dirmatch, &fileinfo)) == -1)
	{
		fprintf(stderr, "Could not scan '%s': %s\n", dirpath, strerror(errno));
	}
	else
	{
		do
		{
			if (fileinfo.attrib & _A_HIDDEN)
			{
				// Skip hidden files and directories. (Prevents SVN bookkeeping
				// info from being included.)
				continue;
			}
			if (fileinfo.attrib & _A_SUBDIR)
			{
				char *newdir;

				if (fileinfo.name[0] == '.' &&
					(fileinfo.name[1] == '\0' ||
					 (fileinfo.name[1] == '.' && fileinfo.name[2] == '\0')))
				{
					// Do not record . and .. directories.
					continue;
				}
				newdir = malloc(strlen(dirpath) + strlen(fileinfo.name) + 2);
				strcpy(newdir, dirpath);
				strcat(newdir, fileinfo.name);
				strcat(newdir, "/");
				recurse_dir(tree, newdir);
			}
			else
			{
				file_entry_t *entry;

				if (strstr(fileinfo.name, ".orig"))
				{
					// .orig files are left behind by patch.exe and should never be
					// added to zdoom.pk3
					continue;
				}

				entry = alloc_file_entry(dirpath, fileinfo.name, fileinfo.time_write);
				if (entry == NULL)
				{
					no_mem = 1;
					break;
				}
				entry->next = tree->files;
				tree->files = entry;
			}
		} while (_findnext(handle, &fileinfo) == 0);
		_findclose(handle);
	}
	free(dirmatch);
}

//==========================================================================
//
// add_dir
//
//==========================================================================

dir_tree_t *add_dir(const char *dirpath)
{
	dir_tree_t *tree = alloc_dir_tree(dirpath);

	if (tree != NULL)
	{
		recurse_dir(tree, tree->path);
	}
	return tree;
}

//==========================================================================
//
// add_dirs
// Windows version
//
// Given NULL-terminated array of directory paths, create trees for them.
//
//==========================================================================

dir_tree_t *add_dirs(char **argv)
{
	dir_tree_t *tree, *trees = NULL;
	char *s;

	while (*argv != NULL)
	{
		for (s = *argv; *s != '\0'; ++s)
		{
			if (*s == '\\')
			{
				*s = '/';
			}
		}
		tree = add_dir(*argv);
		tree->next = trees;
		trees = tree;
		if (no_mem)
		{
			break;
		}
		argv++;
	}
	return trees;
}

#elif !defined(HAVE_FTS_H)

//==========================================================================
//
// add_dirs
// Solaris version
//
// Given NULL-terminated array of directory paths, create trees for them.
//
//==========================================================================

void add_dir(dir_tree_t *tree, char* dirpath)
{
	DIR *directory = opendir(dirpath);
	if(directory == NULL)
		return;

	struct dirent *file;
	while((file = readdir(directory)) != NULL)
	{
		if(file->d_name[0] == '.') //File is hidden or ./.. directory so ignore it.
			continue;

		int isDirectory = 0;
		int time = 0;

		char* fullFileName = malloc(strlen(dirpath) + strlen(file->d_name) + 1);
		strcpy(fullFileName, dirpath);
		strcat(fullFileName, file->d_name);

		struct stat *fileStat;
		fileStat = malloc(sizeof(struct stat));
		stat(fullFileName, fileStat);
		isDirectory = S_ISDIR(fileStat->st_mode);
		time = fileStat->st_mtime;
		free(stat);

		free(fullFileName);

		if(isDirectory)
		{
			char* newdir;
			newdir = malloc(strlen(dirpath) + strlen(file->d_name) + 2);
			strcpy(newdir, dirpath);
			strcat(newdir, file->d_name);
			strcat(newdir, "/");
			add_dir(tree, newdir);
			free(newdir);
			continue;
		}

		file_entry_t *entry;
		entry = alloc_file_entry(dirpath, file->d_name, time);
		if (entry == NULL)
		{
			//no_mem = 1;
			break;
		}
		entry->next = tree->files;
		tree->files = entry;
	}

	closedir(directory);
}

dir_tree_t *add_dirs(char **argv)
{
	dir_tree_t *tree, *trees = NULL;

	int i = 0;
	while(argv[i] != NULL)
	{
		tree = alloc_dir_tree(argv[i]);
		tree->next = trees;
		trees = tree;

		if(tree != NULL)
		{
			char* dirpath = malloc(sizeof(argv[i]) + 2);
			strcpy(dirpath, argv[i]);
			if(dirpath[strlen(dirpath)] != '/')
				strcat(dirpath, "/");
			add_dir(tree, dirpath);
			free(dirpath);
		}

		i++;
	}
	return trees;
}

#else

//==========================================================================
//
// add_dirs
// 4.4BSD version
//
// Given NULL-terminated array of directory paths, create trees for them.
//
//==========================================================================

dir_tree_t *add_dirs(char **argv)
{
	FTS *fts;
	FTSENT *ent;
	dir_tree_t *tree, *trees = NULL;
	file_entry_t *file;

	fts = fts_open(argv, FTS_LOGICAL, NULL);
	if (fts == NULL)
	{
		fprintf(stderr, "Failed to start directory traversal: %s\n", strerror(errno));
		return NULL;
	}
	while ((ent = fts_read(fts)) != NULL)
	{
		if (ent->fts_info == FTS_D && ent->fts_name[0] == '.')
		{
			// Skip hidden directories. (Prevents SVN bookkeeping
			// info from being included.)
			// [BL] Also skip backup files.
			fts_set(fts, ent, FTS_SKIP);
		}
		if (ent->fts_info == FTS_D && ent->fts_level == 0)
		{
			tree = alloc_dir_tree(ent->fts_path);
			if (tree == NULL)
			{
				no_mem = 1;
				break;
			}
			tree->next = trees;
			trees = tree;
		}
		if (ent->fts_info != FTS_F)
		{
			// We're only interested in remembering files.
			continue;
		}
		else if(ent->fts_name[strlen(ent->fts_name)-1] == '~')
		{
			// Don't remember backup files.
			continue;
		}
		file = alloc_file_entry("", ent->fts_path, ent->fts_statp->st_mtime);
		if (file == NULL)
		{
			no_mem = 1;
			break;
		}
		file->next = tree->files;
		tree->files = file;
	}
	fts_close(fts);
	return trees;
}
#endif

//==========================================================================
//
// count_files
//
//==========================================================================

int count_files(dir_tree_t *trees)
{
	dir_tree_t *tree;
	file_entry_t *file;
	int count;

	for (count = 0, tree = trees; tree != NULL; tree = tree->next)
	{
		for (file = tree->files; file != NULL; file = file->next)
		{
			count++;
		}
	}
	return count;
}

//==========================================================================
//
// sort_cmp
//
// Arbitrarily-selected sorting for the zip files: Files in the root
// directory sort after files in subdirectories. Otherwise, everything
// sorts by name.
//
//==========================================================================

int sort_cmp(const void *a, const void *b)
{
	const file_sorted_t *sort1 = (const file_sorted_t *)a;
	const file_sorted_t *sort2 = (const file_sorted_t *)b;
	int in_dir1, in_dir2;

	in_dir1 = (strchr(sort1->path_in_zip, '/') != NULL);
	in_dir2 = (strchr(sort2->path_in_zip, '/') != NULL);
	if (in_dir1 == 1 && in_dir2 == 0)
	{
		return -1;
	}
	if (in_dir1 == 0 && in_dir2 == 1)
	{
		return 1;
	}
	return strcmp(((const file_sorted_t *)a)->path_in_zip,
		((const file_sorted_t *)b)->path_in_zip);
}

//==========================================================================
//
// sort_files
//
//==========================================================================

file_sorted_t *sort_files(dir_tree_t *trees, int num_files)
{
	file_sorted_t *sorter;
	dir_tree_t *tree;
	file_entry_t *file;
	int i;

	sorter = malloc(sizeof(*sorter) * num_files);
	if (sorter != NULL)
	{
		for (i = 0, tree = trees; tree != NULL; tree = tree->next)
		{
			for (file = tree->files; file != NULL; file = file->next)
			{
				sorter[i].file = file;
				sorter[i].path_in_zip = file->path + tree->path_size;
				i++;
			}
		}
		qsort(sorter, num_files, sizeof(*sorter), sort_cmp);
	}
	return sorter;
}

//==========================================================================
//
// write_zip
//
//==========================================================================

void write_zip(const char *zipname, dir_tree_t *trees, int update)
{
#ifdef _WIN32
	char tempname[_MAX_PATH];
#else
	char tempname[PATH_MAX];
#endif
	EndOfCentralDirectory dirend;
	int i, num_files;
	file_sorted_t *sorted;
	FILE *zip, *ozip = NULL;
	void *central_dir = NULL;

	num_files = count_files(trees);
	sorted = sort_files(trees, num_files);
	if (sorted == NULL)
	{
		no_mem = 1;
		return;
	}
	if (update)
	{
		sprintf(tempname, "%s.temp", zipname);
		ozip = fopen(zipname, "rb");
		if (ozip == NULL)
		{
			fprintf(stderr, "Could not open %s for updating: %s\n", zipname, strerror(errno));
			update = 0;
		}
		else
		{
			central_dir = find_central_dir(ozip);
			if (central_dir == NULL)
			{
				fprintf(stderr, "Could not read central directory from %s. (Is it a zipfile?)\n", zipname);
				fclose(ozip);
				ozip = NULL;
				update = 0;
			}
		}
		if (!update)
		{
			fprintf(stderr, "Will proceed as if -u had not been specified.\n");
		}
	}
	if (update)
	{
		zip = fopen(tempname, "wb");
	}
	else
	{
		zip = fopen(zipname, "wb");
	}
	if (zip == NULL)
	{
		fprintf(stderr, "Could not open %s: %s\n", zipname, strerror(errno));
	}
	else
	{
		// Write each file.
		for (i = 0; i < num_files; ++i)
		{
			if (append_to_zip(zip, sorted + i, ozip, central_dir))
			{
				break;
			}
		}
		if (i == num_files)
		{
			// Write central directory.
			dirend.DirectoryOffset = LittleLong(ftell(zip));
			for (i = 0; i < num_files; ++i)
			{
				write_central_dir(zip, sorted + i);
			}
			// Write the directory terminator.
			dirend.Magic = ZIP_ENDOFDIR;
			dirend.DiskNumber = 0;
			dirend.FirstDisk = 0;
			dirend.NumEntriesOnAllDisks = dirend.NumEntries = LittleShort(i);
			// In this case LittleLong(dirend.DirectoryOffset) is undoing the transformation done above.
			dirend.DirectorySize = LittleLong(ftell(zip) - LittleLong(dirend.DirectoryOffset));
			dirend.ZipCommentLength = 0;
			if (fwrite(&dirend, sizeof(dirend), 1, zip) != 1)
			{
				fprintf(stderr, "Failed writing zip directory terminator: %s\n", strerror(errno));
			}
			printf("%s contains %d files (updated %d)\n", zipname, num_files, UpdateCount);
			fclose(zip);

			if (ozip != NULL)
			{
				// Delete original, and rename temp to take its place
				fclose(ozip);
				ozip = NULL;
				if (remove(zipname))
				{
					fprintf(stderr, "Could not delete old zip: %s\nUpdated zip can be found at %s\n",
						strerror(errno), tempname);
				}
				else if (rename(tempname, zipname))
				{
					fprintf(stderr, "Could not rename %s to %s: %s\n",
						tempname, zipname, strerror(errno));
				}
			}
		}
	}
	free(sorted);
	if (ozip != NULL)
	{
		fclose(ozip);
	}
	if (central_dir != NULL)
	{
		free(central_dir);
	}
}

//==========================================================================
//
// append_to_zip
//
// Write a given file to the zipFile.
// 
// zipfile: zip object to be written to
//    file: file to read data from
// 
// returns: 0 = success, 1 = error
//
//==========================================================================

int append_to_zip(FILE *zip_file, file_sorted_t *filep, FILE *ozip, BYTE *odir)
{
	LocalFileHeader local;
	uLong crc;
	file_entry_t *file;
	Byte *readbuf;
	Byte *compbuf[2];
	unsigned int comp_len[2];
	int offset[2];
	int method[2];
	int best;
	int slot;
	FILE *lumpfile;
	unsigned int readlen;
	unsigned int len;
	int i;
	struct tm *ltime;

	file = filep->file;

	// try to determine local time
	ltime = localtime(&file->time_write);
	time_to_dos(ltime, &file->date, &file->time);

	// lumpfile = source file
	lumpfile = fopen(file->path, "rb");
	if (lumpfile == NULL)
	{
		fprintf(stderr, "Could not open %s: %s\n", file->path, strerror(errno));
		return 1;
	}
	// len = source size
	fseek (lumpfile, 0, SEEK_END);
	len = ftell(lumpfile);
	fseek (lumpfile, 0, SEEK_SET);

	// allocate a buffer for the whole source file
	readbuf = malloc(len);
	if (readbuf == NULL)
	{
		fclose(lumpfile);
		fprintf(stderr, "Could not allocate %u bytes\n", (int)len);
		return 1;
	}
	// read the whole source file into buffer
	readlen = (unsigned int)fread(readbuf, 1, len, lumpfile);
	fclose(lumpfile);

	// if read less bytes than expected, 
	if (readlen != len)
	{
		// diagnose and return error
		free(readbuf);
		fprintf(stderr, "Unable to read %s\n", file->path);
		return 1;
	}
	// file loaded

	file->uncompressed_size = len;
	file->compressed_size = len;
	file->method = METHOD_STORED;

	// Calculate CRC32 for file.
	crc = crc32(0, NULL, 0);
	crc = crc32(crc, readbuf, (uInt)len);
	file->crc32 = LittleLong(crc);

	// Can we save time and just copy the file from the old zip?
	if (odir != NULL && ozip != NULL)
	{
		CentralDirectoryEntry *dirent;

		dirent = find_file_in_zip(odir, filep->path_in_zip, len, crc, file->date, file->time);
		if (dirent != NULL)
		{
			i = copy_zip_file(zip_file, file, ozip, dirent);
			if (i > 0)
			{
				free(readbuf);
				return 0;
			}
			if (i < 0)
			{
				free(readbuf);
				fprintf(stderr, "Unable to write %s to zip\n", file->path);
				return 1;
			}
		}
	}

	if (!Quiet)
	{
		if (ozip != NULL)
		{
			printf("Updating %-40s", filep->path_in_zip);
		}
		else
		{
			printf("Adding %-40s", filep->path_in_zip);
		}
	}
	UpdateCount++;

	// Allocate a buffer for compression, one byte less than the source buffer.
	// If it doesn't fit in that space, then skip compression and store it as-is.
	compbuf[0] = malloc(len - 1);
	compbuf[1] = malloc(len - 1);
	best = -1;	// best slot
	slot = 0;	// slot we are compressing to now

	// Find best compression method. We have two output buffers. One to hold the
	// best compression method, and the other to hold the compression we are trying
	// now.
	for (i = 0; Compressors[i].compress != NULL; ++i)
	{
		if (DeflateOnly && Compressors[i].method != METHOD_DEFLATE)
		{
			continue;
		}
		comp_len[slot] = len - 1;
		method[slot] = Compressors[i].method;
		offset[slot] = Compressors[i].compress(compbuf[slot], &comp_len[slot], readbuf, len);
		if (offset[slot] >= 0)
		{
			if (best < 0 || comp_len[slot] <= comp_len[best])
			{
				best = slot;
				slot ^= 1;
			}
		}
	}

	if (best >= 0)
	{
		file->method = method[best];
		file->compressed_size = comp_len[best];
	}
//	printf("%s -> method %d -> slot %d\n", filep->path_in_zip, file->method, best);

	// Fill in local directory header.
	local.Magic = ZIP_LOCALFILE;
	local.VersionToExtract[0] = method_to_version(file->method);
	local.VersionToExtract[1] = 0;
	local.Flags = file->method == METHOD_DEFLATE ? LittleShort(2) : 0;
	local.Method = LittleShort(file->method);
	local.ModTime = file->time;
	local.ModDate = file->date;
	local.CRC32 = file->crc32;
	local.UncompressedSize = LittleLong(file->uncompressed_size);
	local.CompressedSize = LittleLong(file->compressed_size);
	local.NameLength = LittleShort((unsigned short)strlen(filep->path_in_zip));
	local.ExtraLength = 0;

	file->zip_offset = ftell(zip_file);

	// Write out the header, file name, and file data.
	if (fwrite(&local, sizeof(local), 1, zip_file) != 1 ||
		fwrite(filep->path_in_zip, strlen(filep->path_in_zip), 1, zip_file) != 1 ||
		(file->method ? fwrite(compbuf[best] + offset[best], 1, comp_len[best], zip_file) != comp_len[best] :
						fwrite(readbuf, 1, len, zip_file) != len))
	{
		if (!Quiet)
		{
			printf("\n");
		}
		fprintf(stderr, "Unable to write %s to zip\n", file->path);
		free(readbuf);
		if (compbuf[0] != NULL)
		{
			free(compbuf[0]);
		}
		if (compbuf[1] != NULL)
		{
			free(compbuf[1]);
		}
		return 1;
	}

	// all done
	free(readbuf);
	if (compbuf[0] != NULL)
	{
		free(compbuf[0]);
	}
	if (compbuf[1] != NULL)
	{
		free(compbuf[1]);
	}
	if (!Quiet)
	{
		printf("%5.1f%% [%6u/%6u] %s\n", 100.0 - 100.0 * file->compressed_size / file->uncompressed_size,
			file->compressed_size, file->uncompressed_size, method_name(file->method));
	}
	return 0;
}

//==========================================================================
//
// write_central_dir
//
// Writes the central directory entry for a file.
//
//==========================================================================

int write_central_dir(FILE *zip, file_sorted_t *filep)
{
	CentralDirectoryEntry dir;
	file_entry_t *file;

	file = filep->file;
	dir.Magic = ZIP_CENTRALFILE;
	dir.VersionMadeBy[0] = 20;
	dir.VersionMadeBy[1] = 0;
	dir.VersionToExtract[0] = method_to_version(file->method);
	dir.VersionToExtract[1] = 0;
	dir.Flags = file->method == METHOD_DEFLATE ? LittleShort(2) : 0;
	dir.Method = LittleShort(file->method);
	dir.ModTime = file->time;
	dir.ModDate = file->date;
	dir.CRC32 = file->crc32;
	dir.CompressedSize = LittleLong(file->compressed_size);
	dir.UncompressedSize = LittleLong(file->uncompressed_size);
	dir.NameLength = LittleShort((unsigned short)strlen(filep->path_in_zip));
	dir.ExtraLength = 0;
	dir.CommentLength = 0;
	dir.StartingDiskNumber = 0;
	dir.InternalAttributes = 0;
	dir.ExternalAttributes = 0;
	dir.LocalHeaderOffset = LittleLong(file->zip_offset);

	if (fwrite(&dir, sizeof(dir), 1, zip) != 1 ||
		fwrite(filep->path_in_zip, strlen(filep->path_in_zip), 1, zip) != 1)
	{
		fprintf(stderr, "Error writing central directory header for %s: %s\n", file->path, strerror(errno));
		return 1;
	}
	return 0;
}

//==========================================================================
//
// time_to_dos
//
// Converts time from struct tm to the DOS format used by zip files.
//
//==========================================================================

void time_to_dos(struct tm *time, short *dosdate, short *dostime)
{
	if (time == NULL || time->tm_year < 80)
	{
		*dosdate = *dostime = 0;
	}
	else
	{
		*dosdate = LittleShort((time->tm_year - 80) * 512 + (time->tm_mon + 1) * 32 + time->tm_mday);
		*dostime = LittleShort(time->tm_hour * 2048 + time->tm_min * 32 + time->tm_sec / 2);
	}
}

//==========================================================================
//
// method_to_version
//
// Given a compression method, returns the version of the ZIP appnote
// required to decompress it, for filling in the directory information.
//
//==========================================================================

int method_to_version(int method)
{
	// Apparently, real-world programs get confused by setting the version
	// to extract field to something other than 2.0.
#if 0
	if (method == METHOD_LZMA || method == METHOD_PPMD)
		return 63;
	if (method == METHOD_BZIP2)
		return 46;
#endif
	// Default anything else to PKZIP 2.0.
	return 20;
}

//==========================================================================
//
// method_name
//
// Returns the name of the compression method. If the method is unknown,
// this will point to a static buffer.
//
//==========================================================================

const char *method_name(int method)
{
	static char unkn[16];

	if (method == METHOD_STORED)
	{
		return "Stored";
	}
	if (method == METHOD_DEFLATE)
	{
		return "Deflate";
	}
	if (method == METHOD_LZMA)
	{
		return "LZMA";
	}
	if (method == METHOD_PPMD)
	{
		return "PPMd";
	}
	if (method == METHOD_BZIP2)
	{
		return "BZip2";
	}
	sprintf(unkn, "Unk:%03d", method);
	return unkn;
}

//==========================================================================
//
// compress_lzma
//
// Returns non-negative offset to start of data stream on success. Barring
// any strange changes to the LZMA library in the future, success should
// always return 0.
//
//==========================================================================

int compress_lzma(Byte *out, unsigned int *outlen, const Byte *in, unsigned int inlen)
{
	CLzmaEncProps lzma_props;
	size_t props_size;
	size_t comp_len;
	int offset;

	if (*outlen < 1 + 4 + LZMA_PROPS_SIZE)
	{
		// Not enough room for LZMA properties header + compressed data.
		return -1;
	}
	if (out == NULL || in == NULL || inlen == 0)
	{
		return -1;
	}

	LzmaEncProps_Init(&lzma_props);
//	lzma_props.level = 9;
	props_size = LZMA_PROPS_SIZE;
	comp_len = *outlen - 4 - LZMA_PROPS_SIZE;

	if (SZ_OK != LzmaEncode(out + 4 + LZMA_PROPS_SIZE, &comp_len, in, inlen, &lzma_props,
		out + 4, &props_size, 0, NULL, &Alloc, &Alloc))
	{
		return -1;
	}
	// Fill in LZMA properties header
	offset = 0;
	if (props_size != LZMA_PROPS_SIZE)
	{
		// Move LZMA properties to be adjacent to the compressed data, because for
		// some reaseon the library didn't use all the space provided.
		int i;

		offset = (int)(LZMA_PROPS_SIZE - props_size);
		for (i = 4 + LZMA_PROPS_SIZE - 1; i > 4 + offset; --i)
		{
			out[i] = out[i - offset];
		}
	}
	out[offset] = MY_VER_MAJOR;
	out[offset+1] = MY_VER_MINOR;
	out[offset+2] = (Byte)props_size;
	out[offset+3] = 0;
	// Add header length to outlen
	*outlen = (unsigned int)(comp_len + 4 + props_size);
	return offset;
}

//==========================================================================
//
// compress_bzip2
//
// Returns 0 on success, negative on failure.
//
//==========================================================================

int compress_bzip2(Byte *out, unsigned int *outlen, const Byte *in, unsigned int inlen)
{
	if (BZ_OK == BZ2_bzBuffToBuffCompress((char *)out, outlen, (char *)in, inlen, 9, 0, 0))
	{
		return 0;
	}
	return -1;
}

#ifdef PPMD
//==========================================================================
//
// compress_ppmd
//
// Returns 0 on success, negative on failure.
//
// Big problem here: The zip format only allows for PPMd I rev. 1. This
// version of the code is incompatible with 64-bit processors. PPMd J rev. 1
// corrects this and also compresses slightly better, but it also changes
// the data format and is incompatible with I rev. 1. The PPMd source code
// is a tangled mass that I cannot comprehend, so fixing I rev. 1 to work
// on 64-bit processors is well beyond my means. Hence, I cannot currently
// support PPMd in zips. If the zip spec gets updated to allow J rev. 1,
// then I can, but not any sooner.
//
//==========================================================================

int compress_ppmd(Byte *out, unsigned int *outlen, const Byte *in, unsigned int inlen)
{
	int maxorder = 8;
	int sasize = 8;
	int cutoff = 0;

	_PPMD_FILE ppsin = { (char *)in, inlen, 0 };
	_PPMD_FILE ppsout = { out + 2, *outlen - 2, 0};

	if (!PPMd_StartSubAllocator(sasize))
	{
		return -1;
	}
	PPMd_EncodeFile(&ppsout, &ppsin, maxorder, cutoff);
	PPMd_StopSubAllocator();
	if (ppsout.eof)
	{
		return -1;
	}
	if (!ppsin.eof)
	{
		return -1;
	}

	const short outval = LittleShort((maxorder - 1) + ((sasize - 1) << 4) + (cutoff << 12));
	memcpy(out, (const Byte *)&outval, sizeof(short));
	*outlen = *outlen - ppsout.buffersize;
	return 0;
}
#endif

//==========================================================================
//
// compress_deflate
//
// Returns 0 on success, negative on failure.
//
//==========================================================================

int compress_deflate(Byte *out, unsigned int *outlen, const Byte *in, unsigned int inlen)
{
    z_stream stream;
    int err;

    stream.next_in = (Bytef *)in;
    stream.avail_in = inlen;
    stream.next_out = out;
    stream.avail_out = *outlen;
    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;

    err = deflateInit2(&stream, 9, Z_DEFLATED, -15, 9, Z_DEFAULT_STRATEGY);
    if (err != Z_OK) return -1;

    err = deflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        deflateEnd(&stream);
        return -1;
    }
    *outlen = stream.total_out;

    err = deflateEnd(&stream);
	return err == Z_OK ? 0 : -1;
}

//==========================================================================
//
// find_central_dir
//
// Finds and loads the central directory records in the file.
// Taken from Quake3 source and modified.
//
//==========================================================================

BYTE *find_central_dir(FILE *fin)
{
	unsigned char buf[BUFREADCOMMENT + 4];
	EndOfCentralDirectory eod;
	BYTE *dir;
	long file_size;
	long back_read;
	long max_back;		// maximum size of global comment
	long pos_found = 0;

	fseek(fin, 0, SEEK_END);

	file_size = ftell(fin);
	max_back = 0xffff > file_size ? file_size : 0xffff;

	back_read = 4;
	while (back_read < max_back)
	{
		UINT32 read_size, read_pos;
		int i;
		if (back_read + BUFREADCOMMENT > max_back) 
			back_read = max_back;
		else
			back_read += BUFREADCOMMENT;
		read_pos = file_size - back_read;

		read_size = (BUFREADCOMMENT + 4) < (file_size - read_pos) ?
					(BUFREADCOMMENT + 4) : (file_size - read_pos);

		if (fseek(fin, read_pos, SEEK_SET) != 0)
			return NULL;

		if (fread(buf, 1, read_size, fin) != read_size)
			return NULL;

		for (i = (int)read_size - 3; (i--) > 0;)
		{
			if (buf[i] == 'P' && buf[i+1] == 'K' && buf[i+2] == 5 && buf[i+3] == 6)
			{
				pos_found = read_pos + i;
				break;
			}
		}

		if (pos_found != 0)
			break;
	}
	if (pos_found == 0 ||
		fseek(fin, pos_found, SEEK_SET) != 0 ||
		fread(&eod, sizeof(eod), 1, fin) != 1 ||
		fseek(fin, LittleLong(eod.DirectoryOffset), SEEK_SET) != 0)
	{
		return NULL;
	}
	dir = malloc(LittleLong(eod.DirectorySize) + 4);
	if (dir == NULL)
	{
		no_mem = 1;
		return NULL;
	}
	if (fread(dir, 1, LittleLong(eod.DirectorySize), fin) != LittleLong(eod.DirectorySize))
	{
		free(dir);
		return NULL;
	}
	if (memcmp(dir, (const BYTE *)&centralfile, sizeof(UINT32)) != 0)
	{
		free(dir);
		return NULL;
	}
	memcpy(dir + LittleLong(eod.DirectorySize), (const BYTE *)&endofdir, sizeof(UINT32));
	return dir;
}

//==========================================================================
//
// find_file_in_zip
//
// Returns a pointer to a central directory entry to a file, if it was
// found in the zip's directory. Data endianness is in zip order.
//
//==========================================================================

CentralDirectoryEntry *find_file_in_zip(BYTE *dir, const char *path, unsigned int len, unsigned int crc, short date, short time)
{
	int pathlen = (int)strlen(path);
	CentralDirectoryEntry *ent;
	int flags;

	while (memcmp(dir, (const BYTE *)&centralfile, sizeof(UINT32)) == 0)
	{
		ent = (CentralDirectoryEntry *)dir;
		if (pathlen == LittleShort(ent->NameLength) &&
			strncmp((char *)(ent + 1), path, pathlen) == 0)
		{
			// Found something that matches by name.
			break;
		}
		dir += sizeof(*ent) + LittleShort(ent->NameLength) + LittleShort(ent->ExtraLength) + LittleShort(ent->CommentLength);
	}
	if (memcmp(dir, (const BYTE *)&centralfile, sizeof(UINT32)) != 0)
	{
		return NULL;
	}
	if (crc != LittleLong(ent->CRC32))
	{
		return NULL;
	}
	if (len != LittleLong(ent->UncompressedSize))
	{
		return NULL;
	}
	// Should I check modification date and time here?
	flags = LittleShort(ent->Flags);
	if (flags & 1)
	{ // Don't want to deal with encryption.
		return NULL;
	}
	if (ent->ExtraLength != 0)
	{ // Don't want to deal with extra data.
		return NULL;
	}
	// Okay, looks good.
	return ent;
}

//==========================================================================
//
// copy_zip_file
//
// Copies one file from ozip to zip. Returns positive on success, zero if
// the file could not be found, and negative if it failed while writing the
// file.
//
//==========================================================================

int copy_zip_file(FILE *zip, file_entry_t *file, FILE *ozip, CentralDirectoryEntry *ent)
{
	LocalFileHeader lfh;
	BYTE *buf;
	UINT32 buf_size;

	if (fseek(ozip, LittleLong(ent->LocalHeaderOffset), SEEK_SET) != 0)
	{
		return 0;
	}
	if (fread(&lfh, sizeof(lfh), 1, ozip) != 1)
	{
		return 0;
	}
	// Check to make sure the local header matches the central directory.
	if (lfh.Flags != ent->Flags || lfh.Method != ent->Method ||
		lfh.CRC32 != ent->CRC32 || lfh.CompressedSize != ent->CompressedSize ||
		lfh.UncompressedSize != ent->UncompressedSize ||
		lfh.NameLength != ent->NameLength || lfh.ExtraLength != ent->ExtraLength)
	{
		return 0;
	}
	buf_size = LittleShort(lfh.NameLength) + LittleLong(lfh.CompressedSize);
	buf = malloc(buf_size);
	if (buf == NULL)
	{
		return 0;
	}
	if (fread(buf, 1, buf_size, ozip) != buf_size)
	{
		free(buf);
		return 0;
	}
	// Check to be sure name matches.
	if (strncmp((char *)buf, (char *)(ent + 1), LittleShort(lfh.NameLength)) != 0)
	{
		free(buf);
		return 0;
	}
	// Looks good. Let's write it in.
	file->zip_offset = ftell(zip);
	if (fwrite(&lfh, sizeof(lfh), 1, zip) != 1 ||
		fwrite(buf, 1, buf_size, zip) != buf_size)
	{
		free(buf);
		return -1;
	}
	free(buf);
	file->date = lfh.ModDate;
	file->time = lfh.ModTime;
	file->uncompressed_size = LittleLong(lfh.UncompressedSize);
	file->compressed_size = LittleLong(lfh.CompressedSize);
	file->method = LittleShort(lfh.Method);
	file->crc32 = lfh.CRC32;
	return 1;
}

//==========================================================================
//
// main
//
//==========================================================================

int main (int argc, char **argv)
{
	dir_tree_t *tree, *trees;
	file_entry_t *file;
	struct stat zipstat;
	int needwrite;
	int i, j, k;
	int force = 0;
	int update = 0;

	// Find options. Options are removed from the array.
	for (i = k = 1; i < argc; ++i)
	{
		if (argv[i][0] == '-')
		{
			if (argv[i][1] == '-')
			{
				if (argv[i][2] == '\0')
				{ // -- terminates option handling for the rest of the command line
					break;
				}
			}
			for (j = 1; argv[i][j] != '\0'; ++j)
			{
				if (argv[i][j] == 'f')
				{
					force = 1;
				}
				else if (argv[i][j] == 'd')
				{
					DeflateOnly = 1;
				}
				else if (argv[i][j] == 'u')
				{
					update = 1;
				}
				else if (argv[i][j] == 'q')
				{
					Quiet = 1;
				}
				else
				{
					fprintf(stderr, "Unknown option '%c'\n", argv[i][j]);
					print_usage(argv[0]);
					return 1;
				}
			}
		}
		else
		{
			argv[k++] = argv[i];
		}
	}
	for (; i <= argc; ++i)
	{
		argv[k++] = argv[i];
	}
	argc -= i - k;

	if (argc < 3)
	{
		print_usage(argv[0]);
		return 1;
	}

	trees = add_dirs(&argv[2]);
	if (no_mem)
	{
		free_dir_trees(trees);
		fprintf(stderr, "Out of memory.\n");
		return 1;
	}

	needwrite = force;
	if (stat(argv[1], &zipstat) != 0)
	{
		if (errno == ENOENT)
		{
			needwrite = 1;
			update = 0;		// Can't update what's not there.
		}
		else
		{
			fprintf(stderr, "Could not stat %s: %s\n", argv[1], strerror(errno));
		}
	}
	else if (!needwrite)
	{
		// Check the files in each tree. If any one of them was modified more
		// recently than the zip, then it needs to be recreated.
		for (tree = trees; tree != NULL; tree = tree->next)
		{
			for (file = tree->files; file != NULL; file = file->next)
			{
				if (file->time_write > zipstat.st_mtime)
				{
					needwrite = 1;
					break;
				}
			}
		}
	}
	if (force || needwrite)
	{
		write_zip(argv[1], trees, update);
	}
	free_dir_trees(trees);
	if (no_mem)
	{
		fprintf(stderr, "Out of memory.\n");
		return 1;
	}
	return 0;
}

//==========================================================================
//
// bz_internal_error
//
// libbzip2 wants this, since we build it with BZ_NO_STDIO set.
//
//==========================================================================

void bz_internal_error (int errcode)
{
	fprintf(stderr, "libbzip2: internal error number %d\n", errcode);
	exit(3);
}
