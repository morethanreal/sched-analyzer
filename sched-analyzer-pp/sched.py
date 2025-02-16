#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 Qais Yousef

import pandas as pd


def init_states(trace):

    query = "select ts, cpu, state, dur, tid, name \
            from thread_state left join thread using(utid)"

    global trace_states
    trace_states = trace.query(query)

    global df_states
    df_states = trace_states.as_pandas_dataframe()
    if df_states.empty:
        return
    df_states.ts = df_states.ts - df_states.ts[0]
    df_states.ts = df_states.ts / 1000000000
    df_states.dur = df_states.dur.astype(float) / 1000000
    df_states.set_index('ts', inplace=True)

def init(trace):

    pd.set_option('display.max_columns', None)
    pd.set_option('display.max_rows', None)
    pd.set_option('display.width', 100)

    init_states(trace)

def states_summary(plt, threads=[]):

    if df_states.empty:
        return

    for thread in threads:
        df = df_states[df_states.name.str.contains(thread).fillna(False)]

        for thread in sorted(df.name.unique()):
            df_thread = df[df.name == thread]

            print()
            print("--::", thread, "::--")
            print("+"*100)

            for tid in df_thread.tid.unique():
                df_tid = df_thread[df_thread.tid == tid]
                df_tid_running = df_tid[df_tid.state == 'Running']

                print()
                print("--" + thread + "-"*(100-len(thread)-2))
                states = sorted(df_tid.state.unique())
                if 'S' in states:
                    states.remove('S')
                data = []
                for state in states:
                    data.append([df_tid[df_tid.state == state].dur.sum().round(2)])

                plt.clf()
                plt.cld()
                plt.simple_stacked_bar([tid], data, width=100, labels=states)
                plt.show()

                print()
                cpus = sorted(df_tid_running.cpu.unique())
                labels = ['CPU{}'.format(cpu) for cpu in cpus]
                data = []
                for cpu in cpus:
                    df_cpu = df_tid_running[df_tid_running.cpu == cpu]
                    data.append([df_cpu.dur.sum().round(2)])

                plt.clf()
                plt.cld()
                plt.simple_stacked_bar([tid], data, width=100, labels=labels)
                plt.show()

                print()
                print("Time in State (ms):")
                print("-"*100)
                print(df_tid.groupby(['state']) \
                        .dur.describe(percentiles=[.75, .90, .95, .99]).round(2))

                print()
                print("Time Running on CPU (ms):")
                print("-"*100)
                print(df_tid_running.groupby(['cpu']) \
                        .dur.describe(percentiles=[.75, .90, .95, .99]).round(2))

                df_tid_sum = pd.DataFrame(df_tid_running.groupby(['cpu']).dur.sum().round(2))
                df_tid_sum['%'] = (df_tid_sum*100/df_tid_sum.sum()).round(2)
                df_tid_sum.rename(columns={'dur' : 'sum'}, inplace=True)
                print()
                print("Sum Time Running on CPU (ms):")
                print("-"*100)
                print(df_tid_sum)

def states_save_csv(prefix):

    df_states.to_csv(prefix + '_sched_states.csv')

def sched_report(plt):

    nr_top = 100

    if df_states.empty:
        return

    print()
    print("States Summary (ms):")
    print("-"*100)
    print(df_states.groupby('state').dur.describe(percentiles=[.75, .90, .95, .99]).round(2))

    df_runnable = df_states[(df_states.state == 'R') | (df_states.state == 'R+')]
    df_running = df_states[df_states.state == 'Running']
    df_usleep = df_states[df_states.state == 'D']

    if not df_runnable.empty:
        print()
        print("Top {} Runnable Tasks (ms) - sorted-by max:".format(nr_top))
        print("-"*100)
        print(df_runnable.sort_values(['dur'], ascending=False) \
                .groupby(['name', 'tid'])                       \
                .dur.describe(percentiles=[.75, .90, .95, .99]) \
                .round(2).sort_values(['max'], ascending=False) \
                .head(nr_top))
        print()
        print("Top {} Runnable Tasks (ms) - sorted-by 90%:".format(nr_top))
        print("-"*100)
        print(df_runnable.sort_values(['dur'], ascending=False) \
                .groupby(['name', 'tid'])                       \
                .dur.describe(percentiles=[.75, .90, .95, .99]) \
                .round(2).sort_values(['90%'], ascending=False) \
                .head(nr_top))

    if not df_running.empty:
        print()
        print("Top {} Running Tasks (ms) - sorted-by max:".format(nr_top))
        print("-"*100)
        print(df_running.sort_values(['dur'], ascending=False)  \
                .groupby(['name', 'tid'])                       \
                .dur.describe(percentiles=[.75, .90, .95, .99]) \
                .round(2).sort_values(['max'], ascending=False) \
                .head(nr_top))
        print()
        print("Top {} Running Tasks (ms) - sorted-by 90%:".format(nr_top))
        print("-"*100)
        print(df_running.sort_values(['dur'], ascending=False)  \
                .groupby(['name', 'tid'])                       \
                .dur.describe(percentiles=[.75, .90, .95, .99]) \
                .round(2).sort_values(['90%'], ascending=False) \
                .head(nr_top))

    if not df_usleep.empty:
        print()
        print("Top {} Uninterruptible Sleep Tasks (ms) - sorted-by max:".format(nr_top))
        print("-"*100)
        print(df_usleep.sort_values(['dur'], ascending=False)   \
                .groupby(['name', 'tid'])                       \
                .dur.describe(percentiles=[.75, .90, .95, .99]) \
                .round(2).sort_values(['max'], ascending=False) \
                .head(nr_top))
        print()
        print("Top {} Uninterruptible Sleep Tasks (ms) - sorted-by 90%:".format(nr_top))
        print("-"*100)
        print(df_usleep.sort_values(['dur'], ascending=False)   \
                .groupby(['name', 'tid'])                       \
                .dur.describe(percentiles=[.75, .90, .95, .99]) \
                .round(2).sort_values(['90%'], ascending=False) \
                .head(nr_top))
