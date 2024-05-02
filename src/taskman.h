typedef struct
{
    int id;
    int status;
    size_t stack_size;
    const char *title;
} Task;

// OK, HERE I HAVE NO IDEA
typedef struct t_ee_thread
{
    int status;           // 0x00
    void *func;           // 0x04
    void *stack;          // 0x08
    int stack_size;       // 0x0C
    void *gp_reg;         // 0x10
    int initial_priority; // 0x14
    int current_priority; // 0x18
    uint32_t attr;        // 0x1C
    uint32_t option;      // 0x20 Do not use - officially documented to not work.

} ee_thread_t;

typedef struct t_ee_thread_status
{
    int status;           // 0x00
    void *func;           // 0x04
    void *stack;          // 0x08
    int stack_size;       // 0x0C
    void *gp_reg;         // 0x10
    int initial_priority; // 0x14
    int current_priority; // 0x18
    uint32_t attr;        // 0x1C
    uint32_t option;      // 0x20
    uint32_t waitType;    // 0x24
    uint32_t waitId;      // 0x28
    uint32_t wakeupCount; // 0x2C
} ee_thread_status_t;

#define MAX_THREADS 256

bool is_invalid_task(Task *task);
void new_task(int id, size_t stack_size, const char *title);
void del_task(int id);
void init_taskman();
int create_task(const char *title, void *func, int stack_size, int priority);
void init_task(int id, void *args);
void kill_task(int id);
void exitkill_task();
Task *get_tasks();
