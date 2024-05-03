#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <sys/fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <psp2kern/kernel/syscon.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>

#include "../quickjs/quickjs.h"
#include "../quickjs/cutils.h"
#include "../env.h"
#include "../system.h"
#include "../memory.h"
#include "../taskman.h"

#define MAX_DIR_FILES 1024

static char modulePath[256];

static char boot_path[255];

static JSValue vitajs_getCurrentDirectory(JSContext *ctx)
{
	char path[256];
	getcwd(path, 256);
	return JS_NewString(ctx, path);
}

static JSValue vitajs_setCurrentDirectory(JSContext *ctx, JSValueConst *argv)
{
	static char temp_path[256];
	const char *path = JS_ToCString(ctx, argv[0]);
	if (!path)
		return JS_ThrowSyntaxError(ctx, "Argument error: System.currentDirectory(file) takes a filename as string as argument.");

	vitajs_getCurrentDirectory(ctx);

	// let's do some normalization.
	// if absolute path (contains [drive]:path/)
	if (strchr(path, ':'))
	{
		strcpy(temp_path, path);
	}
	else // relative path
	{
		// remove last directory ?
		if (!strncmp(path, "..", 2))
		{
			getcwd(temp_path, 256);
			if ((temp_path[strlen(temp_path) - 1] != ':'))
			{
				int idx = strlen(temp_path) - 1;
				do
				{
					idx--;
				} while (temp_path[idx] != '/');
				temp_path[idx] = '\0';
			}
		}
		else
		{
			getcwd(temp_path, 256);
			strcat(temp_path, "/");
			strcat(temp_path, path);
		}
	}

	printf("changing directory to %s\n", temp_path);
	chdir((temp_path));

	return JS_UNDEFINED;
}

static JSValue vitajs_curdir(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc == 0)
		return vitajs_getCurrentDirectory(ctx);
	if (argc == 1)
		return vitajs_setCurrentDirectory(ctx, argv);
	return JS_ThrowSyntaxError(ctx, "Argument error: System.currentDirectory([file]) takes zero or one argument.");
}

static JSValue vitajs_dir(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{

	if (argc != 0 && argc != 1)
		return JS_ThrowSyntaxError(ctx, "Argument error: System.listDir([path]) takes zero or one argument.");

	JSValue arr = JS_NewArray(ctx);

	const char *temp_path = "";
	char path[255];

	getcwd((char *)path, 256);
	printf("current dir %s\n", (char *)path);

	if (argc != 0)
	{
		temp_path = JS_ToCString(ctx, argv[0]);
		// append the given path to the boot_path

		strcpy((char *)path, boot_path);

		if (strchr(temp_path, ':'))
			// workaround in case of temp_path is containing
			// a device name again
			strcpy((char *)path, temp_path);
		else
			strcat((char *)path, temp_path);
	}

	// strcpy(path, normalize_path(path));
	printf("\nchecking path : %s\n", path);

	int i = 0;

	DIR *d;
	struct dirent *dir;

	d = opendir(path);

	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{

			JSValue obj = JS_NewObject(ctx);

			JS_DefinePropertyValueStr(ctx, obj, "name", JS_NewString(ctx, dir->d_name), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "size", JS_NewUint32(ctx, dir->d_stat.st_size), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "dir", JS_NewBool(ctx, S_ISDIR(dir->d_stat.st_mode)), JS_PROP_C_W_E);

			JS_DefinePropertyValueUint32(ctx, arr, i++, obj, JS_PROP_C_W_E);
		}
		closedir(d);
	}

	return arr;
}

static JSValue vitajs_createDir(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *path = JS_ToCString(ctx, argv[0]);
	if (!path)
		return JS_ThrowSyntaxError(ctx, "Argument error: System.createDirectory(directory) takes a directory name as string as argument.");
	mkdir(path, 0777);

	return JS_UNDEFINED;
}

static JSValue vitajs_removeDir(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *path = JS_ToCString(ctx, argv[0]);
	if (!path)
		return JS_ThrowSyntaxError(ctx, "Argument error: System.removeDirectory(directory) takes a directory name as string as argument.");
	rmdir(path);

	return JS_UNDEFINED;
}

static JSValue vitajs_movefile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *path = JS_ToCString(ctx, argv[0]);

	if (!path)
		return JS_ThrowSyntaxError(ctx, "Argument error: System.removeFile(filename) takes a filename as string as argument.");

	const char *oldName = JS_ToCString(ctx, argv[0]);
	const char *newName = JS_ToCString(ctx, argv[1]);

	if (!oldName || !newName)
		return JS_ThrowSyntaxError(ctx, "Argument error: System.rename(source, destination) takes two filenames as strings as arguments.");

	char buf[BUFSIZ];
	size_t size;

	int source = sceIoOpen(oldName, O_RDONLY, 0);
	int dest = sceIoOpen(newName, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	while ((size = sceIoRead(source, buf, BUFSIZ)) > 0)
	{
		sceIoWrite(dest, buf, size);
	}

	sceIoClose(source);
	sceIoClose(dest);

	sceIoRemove(oldName);

	return JS_UNDEFINED;
}

static JSValue vitajs_removeFile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *path = JS_ToCString(ctx, argv[0]);
	if (!path)
		return JS_ThrowSyntaxError(ctx, "Argument error: System.removeFile(filename) takes a filename as string as argument.");
	remove(path);

	return JS_UNDEFINED;
}

static JSValue vitajs_rename(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *oldName = JS_ToCString(ctx, argv[0]);
	const char *newName = JS_ToCString(ctx, argv[1]);

	if (!oldName || !newName)
		return JS_ThrowSyntaxError(ctx, "Argument error: System.rename(source, destination) takes two filenames as strings as arguments.");

	if (sceIoRename(oldName, newName) < 0)
		return JS_ThrowInternalError(ctx, "Error: Cannot change filename (sceIoRename).");

	return JS_UNDEFINED;
}

static JSValue vitajs_copyfile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *ogfile = JS_ToCString(ctx, argv[0]);
	const char *newfile = JS_ToCString(ctx, argv[1]);

	if (!ogfile || !newfile)
		return JS_ThrowSyntaxError(ctx, "Argument error: System.copyFile(source, destination) takes two filenames as strings as arguments.");

	char buf[BUFSIZ];
	size_t size;

	int source = sceIoOpen(ogfile, O_RDONLY, 0);
	int dest = sceIoOpen(newfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	while ((size = sceIoRead(source, buf, BUFSIZ)) > 0)
	{
		sceIoWrite(dest, buf, size);
	}

	sceIoClose(source);
	sceIoClose(dest);

	return JS_UNDEFINED;
}

static void setModulePath()
{
	getcwd(modulePath, 256);
}

static JSValue vitajs_delay(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "Error: milliseconds expected.");

	int sec;
	JS_ToInt32(ctx, &sec, argv[0]);

	delayMiliseconds(sec);

	return JS_UNDEFINED;
}

static JSValue vitajs_getFreeMemory(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 0)
		return JS_ThrowSyntaxError(ctx, "Syntax Error: no arguments expected.");

	size_t result = GetMemorySize();

	return JS_NewUint32(ctx, (uint32_t)(result));
}

static JSValue vitajs_exit(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	return JS_ThrowInternalError(ctx, "System.exit");
}

void recursive_mkdir(char *dir)
{
	char *p = dir;
	while (p)
	{
		char *p2 = strstr(p, "/");
		if (p2)
		{
			p2[0] = 0;
			mkdir(dir, 0777);
			p = p2 + 1;
			p2[0] = '/';
		}
		else
			break;
	}
}

static JSValue vitajs_openfile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 2)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	const char *file_tbo = JS_ToCString(ctx, argv[0]);
	int type;
	JS_ToInt32(ctx, &type, argv[1]);

	int fd = open(file_tbo, type, 0777);
	if (fd < 0)
		return JS_ThrowSyntaxError(ctx, "cannot open requested file.");

	return JS_NewInt32(ctx, fd);
}

static JSValue vitajs_readfile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 2)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected two (file: , s)");

	int file;
	uint32_t size;

	JS_ToInt32(ctx, &file, argv[0]);
	JS_ToUint32(ctx, &size, argv[1]);
	uint8_t *buffer = (uint8_t *)malloc(size + 1);
	int len = read(file, buffer, size);
	buffer[len] = 0;
	return JS_NewStringLen(ctx, (const char *)buffer, len);
}

static JSValue vitajs_writefile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	int fd;
	uint32_t size, len;

	JS_ToInt32(ctx, &fd, argv[0]);
	unsigned char *data = JS_GetArrayBuffer(ctx, &len, argv[1]);
	JS_ToUint32(ctx, &size, argv[2]);
	write(fd, data, size);
	return JS_UNDEFINED;
}

static JSValue vitajs_closefile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	int fd;
	JS_ToInt32(ctx, &fd, argv[0]);
	close(fd);
	return JS_UNDEFINED;
}

static JSValue vitajs_seekfile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 3)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	int fd;
	int pos;
	uint32_t type;

	JS_ToInt32(ctx, &fd, argv[0]);
	JS_ToInt32(ctx, &pos, argv[1]);
	JS_ToUint32(ctx, &type, argv[2]);

	lseek(fd, pos, type);
	return JS_UNDEFINED;
}

static JSValue vitajs_sizefile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	int fd;
	JS_ToInt32(ctx, &fd, argv[0]);
	uint32_t cur_off = lseek(fd, 0, SEEK_CUR);
	uint32_t size = lseek(fd, 0, SEEK_END);
	lseek(fd, cur_off, SEEK_SET);
	return JS_NewUint32(ctx, size);
}

static JSValue vitajs_checkexist(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	const char *file_tbo = JS_ToCString(ctx, argv[0]);
	int fd = open(file_tbo, O_RDONLY, 0777);
	if (fd < 0)
		return JS_NewBool(ctx, false);
	close(fd);
	return JS_NewBool(ctx, true);
}

static JSValue vitajs_loadELF(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	JSValue val;
	int n = 0;
	char **args = NULL;
	const char *path = JS_ToCString(ctx, argv[0]);

	if (argc == 2)
	{
		if (!JS_IsArray(ctx, argv[1]))
		{
			return JS_ThrowSyntaxError(ctx, "Type error, you should use a string array.");
		}

		/*
		val = JS_GetPropertyStr(ctx, argv[1], "length");
		JS_ToInt32(ctx, &n, val);
		JS_FreeValue(ctx, val);
		args = malloc(n * sizeof(char *));

		for (int i = 0; i < n; i++)
		{
			val = JS_GetPropertyUint32(ctx, argv[1], i);
			*(args + i) = (char *)JS_ToCString(ctx, val);
			JS_FreeValue(ctx, val);
		}
		*/
	}

	// LoadELFFromFile(path, n, args);
	JS_ThrowInternalError(ctx, "LoadELFFromFile not implemented yet.");

	return JS_UNDEFINED;
}

extern void *_gp;

static volatile off_t progress, max_progress;

struct pathMap
{
	const char *in;
	const char *out;
};

/*
static int copyThread(void *data)
{
	struct pathMap *paths = (struct pathMap *)data;

	char buffer[BUFSIZ];
	int in = open(paths->in, O_RDONLY, 0);
	int out = open(paths->out, O_WRONLY | O_CREAT | O_TRUNC, 644);

	// Get the input file size
	uint32_t size = lseek(in, 0, SEEK_END);
	lseek(in, 0, SEEK_SET);

	progress = 0;
	max_progress = size;

	ssize_t bytes_read;
	while ((bytes_read = read(in, buffer, BUFSIZ)) > 0)
	{
		write(out, buffer, bytes_read);
		progress += bytes_read;
	}

	// copy is done, or an error occurred
	close(in);
	close(out);
	free(paths);
	exitkill_task();
	return JS_UNDEFINED;
}
*/

/*
static JSValue vitajs_copyasync(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 2)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	struct pathMap *copypaths = (struct pathMap *)malloc(sizeof(struct pathMap));

	copypaths->in = JS_ToCString(ctx, argv[0]);
	copypaths->out = JS_ToCString(ctx, argv[1]);
	int task = create_task("FileSystem: Copy", (void *)copyThread, 8192 + 1024, 100);
	init_task(task, (void *)copypaths);
	return JS_UNDEFINED;
}
*/

static JSValue vitajs_getfileprogress(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 0)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	JSValue obj = JS_NewObject(ctx);

	JS_DefinePropertyValueStr(ctx, obj, "current", JS_NewInt32(ctx, (int)progress), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "final", JS_NewInt32(ctx, (int)max_progress), JS_PROP_C_W_E);

	return obj;
}

static JSValue vitajs_gettemps(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	int result; // usar ksceSysconGetTemperatureLog(&result) ?

	return JS_NewFloat32(ctx, (float)(int)result);

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry system_funcs[] = {
	JS_CFUNC_DEF("openFile", 2, vitajs_openfile),
	JS_CFUNC_DEF("readFile", 2, vitajs_readfile),
	JS_CFUNC_DEF("writeFile", 3, vitajs_writefile),
	JS_CFUNC_DEF("closeFile", 1, vitajs_closefile),
	JS_CFUNC_DEF("seekFile", 3, vitajs_seekfile),
	JS_CFUNC_DEF("sizeFile", 1, vitajs_sizefile),
	JS_CFUNC_DEF("doesFileExist", 1, vitajs_checkexist),
	JS_CFUNC_DEF("currentDir", 1, vitajs_curdir),
	JS_CFUNC_DEF("listDir", 1, vitajs_dir),
	JS_CFUNC_DEF("createDirectory", 1, vitajs_createDir),
	JS_CFUNC_DEF("removeDirectory", 1, vitajs_removeDir),
	JS_CFUNC_DEF("moveFile", 2, vitajs_movefile),
	JS_CFUNC_DEF("copyFile", 2, vitajs_copyfile),
	// JS_CFUNC_DEF("threadCopyFile", 2, vitajs_copyasync),
	JS_CFUNC_DEF("getFileProgress", 0, vitajs_getfileprogress),
	JS_CFUNC_DEF("removeFile", 2, vitajs_removeFile),
	JS_CFUNC_DEF("rename", 2, vitajs_rename),
	JS_CFUNC_DEF("delay", 1, vitajs_delay),
	JS_CFUNC_DEF("getFreeMemory", 0, vitajs_getFreeMemory),
	JS_CFUNC_DEF("exit", 0, vitajs_exit),
	JS_CFUNC_DEF("loadELF", 3, vitajs_loadELF),
	// JS_CFUNC_DEF("getCPUInfo", 0, vitajs_getcpuinfo),
	// JS_CFUNC_DEF("getGPUInfo", 0, vitajs_getgpuinfo),
	JS_CFUNC_DEF("getTemperature", 0, vitajs_gettemps),
	JS_PROP_STRING_DEF("boot_path", boot_path, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("FREAD", O_RDONLY, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("FWRITE", O_WRONLY, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("FCREATE", O_CREAT | O_WRONLY, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("FRDWR", O_RDWR, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("SET", SEEK_SET, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("END", SEEK_END, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CUR", SEEK_CUR, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("READ_ONLY", 1, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("SELECT", 2, JS_PROP_CONFIGURABLE),
};

static const JSCFunctionListEntry sif_funcs[] = {
	// JS_PROP_INT32_DEF("network", NETWORK_MODULE, JS_PROP_CONFIGURABLE),
	// JS_PROP_INT32_DEF("pads", PADS_MODULE, JS_PROP_CONFIGURABLE),
	// JS_PROP_INT32_DEF("audio", AUDIO_MODULE, JS_PROP_CONFIGURABLE),
	// JS_PROP_INT32_DEF("camera", CAMERA_MODULE, JS_PROP_CONFIGURABLE),

};

static int system_init(JSContext *ctx, JSModuleDef *m)
{
	return JS_SetModuleExportList(ctx, m, system_funcs, countof(system_funcs));
}

static int sif_init(JSContext *ctx, JSModuleDef *m)
{
	return JS_SetModuleExportList(ctx, m, sif_funcs, countof(sif_funcs));
}

JSModuleDef *vitajs_system_init(JSContext *ctx)
{
	setModulePath();
	return vitajs_push_module(ctx, system_init, system_funcs, countof(system_funcs), "System");
}
