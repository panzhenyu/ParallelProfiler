#### 模块说明
+ 见src目录，分为性能收集器(Profiler)，三方库(ThirdParty)，其他工具(Utils)三部分。
+ Profiler
	1. PerfProfiler提供性能事件收集功能。
	2. ParallelProfiler用于进行并行任务的同步控制与性能收集。
	3. Config.hpp中的各个结构组合构成Plan，用于描述任务。
+ Utils
	1. PosixUtil提供Posix接口的封装。
	2. PerfEventWrapper提供perf接口封装。