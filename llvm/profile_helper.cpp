#include <stdio.h>
#include <cstdint>
#include <string>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <map>
#include <vector>
#include <iostream>

using namespace std;

extern "C"
{
    FILE *file = nullptr;
    uint64_t *insn_count;
    int *interval;
    map<int, uint64_t> *bb_count_map;
    map<int, uint64_t> *bb_cycle_map;
    bool *flag;
    FILE *bb_cycle_file = nullptr;
    FILE *start_bb_file = nullptr;

    void profiler_init()
    {
        file = fopen("example.txt", "w");
        bb_cycle_file = fopen("cycles.txt", "w");
        start_bb_file = fopen("start_bbs.txt", "w");

        insn_count = new uint64_t(0);
        interval = new int(0);
        flag = new bool(0);

        bb_count_map = new map<int, uint64_t>();
        bb_cycle_map = new map<int, uint64_t>();
    }

    void profiler_finish()
    {
        fprintf(file, "T");
        // printf("T:%d", *interval);
        int i = 0;
        for (auto iter = (*bb_count_map).begin(); iter != (*bb_count_map).end(); iter++)
        {
            int name = iter->first + 1;
            uint64_t count = iter->second;
            // printf(" :%d:%ld", name, count);
            if (i == 0)
            {
                fprintf(file, ":%d:%ld", name, count);
                i++;
            }
            else
            {
                fprintf(file, " :%d:%ld", name, count);
            }
        }
        fprintf(file, "\n");
        // printf("\n");

        fclose(file);

        fprintf(bb_cycle_file, "T");
        // printf("T:%d", *interval);
        i = 0;
        for (auto iter = (*bb_cycle_map).begin(); iter != (*bb_cycle_map).end(); iter++)
        {
            int name = iter->first + 1;
            uint64_t cycle = iter->second;
            if (i == 0)
            {
                fprintf(bb_cycle_file, ":%d:%ld", name, cycle);
                i++;
            }
            else
            {
                fprintf(bb_cycle_file, " :%d:%ld", name, cycle);
            }
        }
        fprintf(bb_cycle_file, "\n");
        // printf("\n");

        fclose(bb_cycle_file);

        fclose(start_bb_file);
    }

    void dump_T(uint64_t T)
    {
        fprintf(file, "T:%ld", T);
    }

    void dump_bb_count(int name, uint64_t count)
    {
        if (count != 0)
            fprintf(file, " :%d:%ld", name, count);
    }

    void dump_newline()
    {
        fprintf(file, "\n");
    }

    void update_flag(int bb)
    {
        if (*flag == 0)
        {
            // cout << "S=" << bb << "\n";
            // fprintf(file, "S=%d\n", bb);
            *flag = 1;
        }
    }

    uint64_t x86_time()
    {
        uint64_t a, d;
        __asm__ volatile("rdtscp" : "=a"(a), "=d"(d));
        return (a | (d << 32));
    }

    void profile_interface(int bb, uint64_t inc, uint64_t threshold, uint64_t cycle)
    {

        // cout << "bb=" << bb << " inc=" << inc << " threshold=" << threshold << "\n";

        if (*insn_count == 0)
        {
            fprintf(start_bb_file, "%d\n", bb);
        }

        *insn_count += inc;
        if (!(*bb_count_map).count(bb))
        {
            (*bb_count_map)[bb] = 0;
        }
        (*bb_count_map)[bb] += 1;

        if (!(*bb_cycle_map).count(bb))
        {
            (*bb_cycle_map)[bb] = 0;
        }
        (*bb_cycle_map)[bb] += cycle;

        if (*insn_count >= threshold)
        {
            *insn_count = 0;

            fprintf(file, "T");
            int i = 0;
            // printf("T:%d", *interval);
            for (auto iter = (*bb_count_map).begin(); iter != (*bb_count_map).end(); iter++)
            {
                // start from 1
                int name = iter->first + 1;
                uint64_t count = iter->second;
                // printf(" :%d:%ld", name, count);
                if (i == 0)
                {
                    fprintf(file, ":%d:%ld", name, count);
                    i++;
                }
                else
                {
                    fprintf(file, " :%d:%ld", name, count);
                }
            }
            fprintf(file, "\n");
            // printf("\n");

            fprintf(bb_cycle_file, "T");
            i = 0;
            // printf("T:%d", *interval);
            for (auto iter = (*bb_cycle_map).begin(); iter != (*bb_cycle_map).end(); iter++)
            {
                // start from 1
                int name = iter->first + 1;
                uint64_t cycle = iter->second;
                // printf(" :%d:%ld", name, count);
                if (i == 0)
                {
                    fprintf(bb_cycle_file, ":%d:%ld", name, cycle);
                    i++;
                }
                else
                {
                    fprintf(bb_cycle_file, " :%d:%ld", name, cycle);
                }
            }
            fprintf(bb_cycle_file, "\n");

            (*interval)++;
            (*bb_count_map).clear();
            (*bb_cycle_map).clear();

            *flag = 0;
        }
    }

    // void check_insncount(uint8_t *flag, int bb)
    // {
    //     if (*flag == 0)
    //     {
    //         // fprintf(file, "S=%d\n", bb);
    //         names.push_back(bb);
    //         *flag = 1;
    //     }
    // }
}