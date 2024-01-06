/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Qais Yousef */
#include <condition_variable>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <perfetto.h>

#include "parse_argp.h"

PERFETTO_DEFINE_CATEGORIES(
	perfetto::Category("pelt-cpu").SetDescription("Track PELT at CPU level"),
	perfetto::Category("pelt-task").SetDescription("Track PELT at task level"),
	perfetto::Category("nr-running-cpu").SetDescription("Track number of tasks running on each CPU")
);

PERFETTO_TRACK_EVENT_STATIC_STORAGE();


extern "C" void init_perfetto(void)
{
	const char *android_traced_prodcuer = "/dev/socket/traced_producer";
	const char *android_traced_consumer = "/dev/socket/traced_consumer";

	if (!access(android_traced_prodcuer, F_OK))
		setenv("PERFETTO_PRODUCER_SOCK_NAME", "/dev/socket/traced_producer", 0);
	if (!access(android_traced_consumer, F_OK))
		setenv("PERFETTO_CONSUMER_SOCK_NAME", "/dev/socket/traced_consumer", 0);

	perfetto::TracingInitArgs args;

	// The backends determine where trace events are recorded. You may select one
	// or more of:

	// 1) The in-process backend only records within the app itself.
	if (sa_opts.app)
		args.backends |= perfetto::kInProcessBackend;

	// 2) The system backend writes events into a system Perfetto daemon,
	//    allowing merging app and system events (e.g., ftrace) on the same
	//    timeline. Requires the Perfetto `traced` daemon to be running (e.g.,
	//    on Android Pie and newer).
	if (sa_opts.system)
		args.backends |= perfetto::kSystemBackend;

	perfetto::Tracing::Initialize(args);
	perfetto::TrackEvent::Register();
}

extern "C" void flush_perfetto(void)
{
	perfetto::TrackEvent::Flush();
}

static std::unique_ptr<perfetto::TracingSession> tracing_session;
static int fd;

extern "C" void start_perfetto_trace(void)
{
	char buffer[256];

	perfetto::TraceConfig cfg;
	cfg.add_buffers()->set_size_kb(1024*100);  // Record up to 100 MiB.
	cfg.add_buffers()->set_size_kb(1024*100);  // Record up to 100 MiB.
	cfg.set_duration_ms(3600000);
	cfg.set_max_file_size_bytes(sa_opts.max_size);
	cfg.set_unique_session_name("sched-analyzer");

	/* Track Events Data Source */
	perfetto::protos::gen::TrackEventConfig track_event_cfg;
	track_event_cfg.add_enabled_categories("sched-analyzer");

	auto *te_ds_cfg = cfg.add_data_sources()->mutable_config();
	te_ds_cfg->set_name("track_event");
	te_ds_cfg->set_track_event_config_raw(track_event_cfg.SerializeAsString());

	/* Ftrace Data Source */
	perfetto::protos::gen::FtraceConfig ftrace_cfg;
	ftrace_cfg.add_ftrace_events("sched/sched_switch");
	ftrace_cfg.add_ftrace_events("sched/sched_process_exit");
	ftrace_cfg.add_ftrace_events("sched/sched_process_free");
	ftrace_cfg.add_ftrace_events("power/suspend_resume");
	ftrace_cfg.add_ftrace_events("power/cpu_frequency");
	ftrace_cfg.add_ftrace_events("power/cpu_idle");
	ftrace_cfg.add_ftrace_events("task/task_newtask");
	ftrace_cfg.add_ftrace_events("task/task_rename");
	ftrace_cfg.add_ftrace_events("ftrace/print");

	auto *ft_ds_cfg = cfg.add_data_sources()->mutable_config();
	ft_ds_cfg->set_name("linux.ftrace");
	ft_ds_cfg->set_ftrace_config_raw(ftrace_cfg.SerializeAsString());

	/* Process Stats Data Source */
	perfetto::protos::gen::ProcessStatsConfig ps_cfg;
	ps_cfg.set_proc_stats_poll_ms(100);
	ps_cfg.set_record_thread_names(true);

	auto *ps_ds_cfg = cfg.add_data_sources()->mutable_config();
	ps_ds_cfg->set_name("linux.process_stats");
	ps_ds_cfg->set_process_stats_config_raw(ps_cfg.SerializeAsString());

	/* On Android traces can be saved on specific path only */
	const char *android_traces_path = "/data/misc/perfetto-traces";
	DIR *dir = opendir(android_traces_path);
	if (!sa_opts.output_path) {
		sa_opts.output_path = ".";
		if (dir) {
			sa_opts.output_path = android_traces_path;
			closedir(dir);
		}
	}

	snprintf(buffer, 256, "%s/%s", sa_opts.output_path, sa_opts.output);
	fd = open(buffer, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		snprintf(buffer, 256, "Failed to create %s/%s", sa_opts.output_path, sa_opts.output);
		perror(buffer);
		return;
	}

	tracing_session = perfetto::Tracing::NewTrace();
	tracing_session->Setup(cfg, fd);
	tracing_session->StartBlocking();
}

extern "C" void stop_perfetto_trace(void)
{
	if (fd < 0)
		return;

	tracing_session->StopBlocking();
	close(fd);
}

extern "C" void trace_cpu_util_avg(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d util_avg", cpu);

	TRACE_COUNTER("pelt-cpu", track_name, ts, value);
}

extern "C" void trace_cpu_util_est_enqueued(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d util_est.enqueued", cpu);

	TRACE_COUNTER("pelt-cpu", track_name, ts, value);
}

extern "C" void trace_cpu_util_avg_rt(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d util_avg_rt", cpu);

	TRACE_COUNTER("pelt-cpu", track_name, ts, value);
}

extern "C" void trace_cpu_util_avg_dl(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d util_avg_rt", cpu);

	TRACE_COUNTER("pelt-cpu", track_name, ts, value);
}

extern "C" void trace_cpu_util_avg_irq(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d util_avg_rt", cpu);

	TRACE_COUNTER("pelt-cpu", track_name, ts, value);
}

extern "C" void trace_cpu_util_avg_thermal(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d util_avg_thermal", cpu);

	TRACE_COUNTER("pelt-cpu", track_name, ts, value);
}

extern "C" void trace_task_util_avg(uint64_t ts, const char *name, int pid, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "%s-%d util_avg", name, pid);

	TRACE_COUNTER("pelt-task", track_name, ts, value);
}

extern "C" void trace_task_util_est_enqueued(uint64_t ts, const char *name, int pid, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "%s-%d util_est.enqueued", name, pid);

	TRACE_COUNTER("pelt-task", track_name, ts, value);
}

extern "C" void trace_task_util_est_ewma(uint64_t ts, const char *name, int pid, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "%s-%d util_est.ewma", name, pid);

	TRACE_COUNTER("pelt-task", track_name, ts, value);
}

extern "C" void trace_cpu_nr_running(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d nr_running", cpu);

	TRACE_COUNTER("nr-running-cpu", track_name, ts, value);
}

#if 0
extern "C" int main(int argc, char **argv)
{
	init_perfetto();
	/* wait_for_perfetto(); */

	start_perfetto_trace();

	for (int i = 0; i < 2000; i++) {
		trace_cpu_pelt(0, i % 300);
		usleep(1000);
	}

	stop_perfetto_trace();

	/* flush_perfetto(); */

	return 0;
}
#endif
