
typedef struct
{
	int type;
	char name[16];
	int value;
} DiscType;

extern char *normalize_path(char *path_name);
extern void load_elf(const char *elf_path);
extern size_t GetFreeSize(void);