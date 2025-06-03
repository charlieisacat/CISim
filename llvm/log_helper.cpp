#include <stdio.h>
#include <cstdint>
#include <string>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#define BUFFER_SIZE 1024 * 1024 * 1024 // 1G

#include <set>
#include <string>

using namespace std;

extern "C"
{
#if 0
    void print_address(uint64_t address)
    {
        printf("address=%lx\n", address);
    }

    void print_br_cond(uint8_t cond)
    {
        printf("branch cond=%d\n", cond);
    }

    int open_file()
    {
        int fd = open("example.txt", O_WRONLY | O_CREAT, 0644);
        return fd;
    }

    void close_file(int fd)
    {
        close(fd);
    }

    void dump_address(int fd, uint64_t address)
    {
        char hex_str[17];
        sprintf(hex_str, "%lx\n", address);
        write(fd, hex_str, strlen(hex_str));
    }

    void dump_br_cond(int fd, uint8_t cond)
    {
        char hex_str[3];
        sprintf(hex_str, "%d\n", cond);
        write(fd, hex_str, strlen(hex_str));
    }

    void dump_switch_cond(int fd, uint64_t cond)
    {
        char hex_str[100];
        sprintf(hex_str, "%d\n", cond);
        write(fd, hex_str, strlen(hex_str));
    }

    void dump_cycle(int fd, uint64_t cycle)
    {
        char hex_str[17];
        sprintf(hex_str, "%lx\n", cycle);
        write(fd, hex_str, strlen(hex_str));
    }
#endif

#if 1
    uint64_t *data_buffer = NULL;
    int buffer_index = 0;
    FILE *file = nullptr;
    FILE *fname_file = nullptr;
    set<string> *fname_set = nullptr;
    FILE *start_bb_file = nullptr;

    void log_helper_init()
    {
        file = fopen("example.txt", "w");
        fname_file = fopen("funcname.txt", "w");
        start_bb_file = fopen("start_bb.txt", "w");

        fname_set = new set<string>();
        // fname_set->insert("main");
    }

    void log_helper_flush_buffer()
    {
        for (int i = 0; i < buffer_index; i++)
        {
            fprintf(file, "%lx\n", data_buffer[i]);
        }
        buffer_index = 0;
    }

    void log_helper_finish()
    {
        // printf("funsize=%d\n", (*fname_set).size());
        for (string name : (*fname_set))
        {
            fprintf(fname_file, "%s\n", name.c_str());
        }

        log_helper_flush_buffer();
        fclose(file);
        fclose(fname_file);
        fclose(start_bb_file);
    }

    void log_helper_dump_data(uint64_t data, uint8_t do_dump = false)
    {
        if (do_dump)
        {
            if (!data_buffer)
            {
                data_buffer = (uint64_t *)malloc(BUFFER_SIZE * sizeof(uint64_t));
            }

            data_buffer[buffer_index++] = data;

            if (buffer_index >= BUFFER_SIZE)
            {
                log_helper_flush_buffer();
            }
        }
    }

    void log_helper_dump_func_name(char *name, uint8_t do_dump = false)
    {
        if (do_dump)
        {
            (*fname_set).insert(string(name));
        }
    }

    void update_insncount_interval(uint64_t *insncount, uint64_t *interval, uint64_t threshold, uint64_t inc, int *name)
    {
        *insncount += inc;

        if (*insncount >= threshold)
        {
            *insncount = 0;
            *interval += 1;
        }
    }

    void update_dump_status(uint64_t *interval, uint64_t threshold, uint8_t *flag, int *name, char *func_name)
    {
        if (*flag)
        {
            // printf("BB=%d\n", *name);
            log_helper_dump_data(*name, true);
            log_helper_dump_func_name(func_name, true);
        }

        if (*interval == threshold)
        {
            if (*flag == 0)
                fprintf(start_bb_file, "S=%d\n", *name);
            *flag = 1;
        }
        else
        {
            *flag = 0;
        }
    }
#endif
}
