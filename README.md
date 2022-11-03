# ParallelProfiler

#### 介绍
一个性能收集工具，可收集多个任务并行执行时的性能参数。

#### 目录结构
+ conf/下包含了本项目所依赖的配置文件示例。

+ src/下包含了项目的核心代码。

+ test/下包含了项目部分模块的测试代码。

#### 功能描述
+ 本项目实现了一个可以收集多个任务并行执行性能数据的工具。

+ 每个任务被视作一个计划(Plan)，它包含了任务所需执行的命令与待测的性能事件。这些任务可以是完整的程序，也可以是完整程序中的某一段(phase)。phase被定义为一个四元组(event, period, begin, end)：event为性能事件；period为性能事件触发的阈值；begin用于定义phase的起始位置(phase begin)，它表示event在阈值period下触发begin次(详见man perf_event_open: sample event)时，该程序达到phase begin；end的意义同begin。每个Plan在运行时实例化为进程，进行性能测绘(profile)。工具通过同步控制手段让所有进程保持同时运行/暂停，并在暂停时收集各个Plan的性能数据。

+ 工具的整个执行流程可以分为prepare, ready, init, profile四个阶段。prepare阶段负责一些参数、权限的合法性检查，并在检查完成后为每一个plan生成进程；ready阶段负责检查所有plan对应的进程是否被成功创建；init阶段负责将带有所有phase要求的plan运行至phase begin；profile阶段负责所有plan的性能数据收集，当任意进程满足退出条件(正常退出或达到phase end)后，该过程结束并保存结果。需要注意的时，本工具仅在profile阶段触发退出条件时保存数据。

+ 本项目共支持四类Plan来适配不同的并行任务执行需求，分别为DAEMON、COUNT、SAMPLE_ALL、SAMPLE_PHASE。四类Plan均会启动一个进程执行预设的命令，区别在于不同类型的计划对性能事件的响应行为不同。DAEMON不响应性能事件；COUNT会生成计数事件，并保证收集到各性能事件的总计数值；SAMPLE_ALL会生成以leader事件为group leader的采样事件，并在leader达到阈值(period)时暂停所有计划，收集所有Plan的性能事件计数；SAMPLE_PHASE与SAMPLE_ALL相似，区别在于在profile开始之前，工具会通过相应的同步控制手段让该Plan运行至phase begin，在profile过程中，若该plan达到phase end，则会结束整个profile过程。

+ 本项目中，每个Plan下的所有性能事件都会以event group的方式存在。

#### 安装教程

+ 安装依赖库：libboost, libpfm4。

+ 进入项目根目录(后用ROOT代替),执行如下命令。
    ```
    mkdir build && cd build && cmake .. && make
    ```

#### 使用说明

+ 在${ROOT}/build/src目录下执行如下命令，显示使用说明。
	```
	./profile --help

	[Usage]                                                                                                         
		sudo ./profile                                                                                              
			--config        Optional        file path, such as conf/example.json, default is empty                  
			--output        Optional        file path, default is stdout                                            
			--log           Optional        file path, default is stderr                                            
			--cpu           Optional        such as 1,2~4, default is empty                                         
			--plan          Repeated        such as id or "{key:value[,key:value]}", at least one plan            
	[Supported Key]                                                                                                 
			id              Required        such as "myplan"                                                      
			task            Required        such as "./task"                                                      
			type            Required        choose "DAEMON" or "COUNT" or "SAMPLE_ALL" or "SAMPLE_PHASE"    
			rt              Optional        choose true or false, default is false                                  
			pincpu          Optional        choose true or false, default is false                                  
			phase           Optional        such as [start,end], default is [0,0]                                   
			leader          Optional        such as "INSTURCTIONS", default is empty                              
			period          Optional        default is 0                                                            
			member          Optional        such as [MEMBER1, MEMBER2], default is empty
	```
	其中，plan后跟参数可分为两类，一类为plan id，一类为左花括号打头的json字符串。当参数为plan id时，plan id需要对应配置文件中"Plan"列表下的某一个id，不允许出现重复的plan id；若为json串，则至少需要提供id、task、type字段，并根据type字段值对phase、leader、period字段做进一步约束要求(同配置文件说明的附加约束)。

+ 配置文件说明
	1. 配置文件示例见${ROOT}/conf/example.json，它包含Task与Plan两个列表，基本结构如下：
		```
		{
			"Task": [
				{
					"id": "your unique task id",
					"dir": "working dir for task, default is "." ",
					"cmd": "command to exec"
				}
			],
			"Plan": [
				{
					"id": "your unique plan id"
					"type": "DAEMON / COUNT / SAMPLE_ALL / SAMPLE_PHASE",
					"task": {
						"id": "your task id",
						"param": ["param1", "param2"],
						"rt": build a rt task?,
						"pincpu": build a task with cpu affinity?,
						"phase": [phase begin, phase end]
					},
					"perf": {
						"leader": "event leader",
						"period": a integer, describe the sample period of leader
						"member": [
							"child event 1", "child event 2"
						]
					}
				}
			]
		}
		```
	2. Task、Plan各自列表内object的id字段需各不相同，列表间可以重复。即允许id为1的Task与Plan同时存在，但不允许存在两个id为1的Task或Plan同时存在。
	3. Task列表存储任务的静态描述。每个列表项支持id(Task的唯一标识符)、dir(执行该任务时的当前路径)、cmd三个字段(执行该任务所需的命令)，其中id、cmd为必选字段，dir默认值为"."，为可选字段，三个字段的值均为字符串类型。
	4. Plan列表存储任务的动态描述。每个列表项支持id(Plan的唯一标识符)、type(Plan类型)、task(任务运行时属性的描述)、perf(任务运行时的性能事件描述)。其中id、type、task:id(表示task字段下的id字段)为必选字段，perf为可选字段。
	5. Plan将根据type字段值约束task与perf中的部分字段：DAEMON将忽略task:phase字段与perf字段；COUNT将忽略task:phase字段与perf:period字段，且perf:leader字段将被约束为必选字段；SAMPLE_ALL将忽略task:phase字段，且perf:leader、perf:period被约束为必选字段，其中perf:period必须为正整数；SAMPLE_PHASE约束task:phase、perf:leader、perf:period为必选字段，并要求task:phase的phase end > phase begin，perf:period为正整数。
	6. Plan:task:id需与Task中的任意一项id匹配。本工具允许在Task:cmd字段中使用$NUM(NUM为正整数)占位符创建一个变参cmd，在实际执行时，每一个$NUM都会被Plan:task:param中的第NUM个(从1开始计数)参数替换。
	7. Plan:task:rt的值为bool类型，用于指示是否将该plan创建为FIFO进程。
	8. Plan:task:pincpu的值为bool类型，用于指示是否为该plan对应的进程设置cpu亲和度，仅支持将亲和度设置为单个cpu。

+ 示例
	```
	sudo ./profile --config=../../conf/example-spec.json --output=output.txt --log=log.txt --cpu=1,2 --plan=456.hmmer --plan=403.gcc
	
	sudo ./profile --plan='{"id": "ls-daemon", "task": "/bin/ls", "type": "DAEMON"}'

	sudo ./profile --plan='{"id": "ls-count", "task": "/bin/ls", "type": "COUNT", "leader": "INSTRUCTIONS"}'

	sudo ./profile --plan='{"id": "ls-sample", "task": "/bin/ls", "type": "SAMPLE_ALL", "leader": "INSTRUCTIONS", "period": 10000}'
	
	sudo ./profile --plan='{"id": "ls-phase", "task": "/bin/ls", "type": "SAMPLE_PHASE", "leader": "INSTRUCTIONS", "period": 10000, "phase": [0, 5]}'
	```

+ 结果格式说明
	1. 第一行为方括号包裹的plan id列表，不同plan id间用逗号分隔。

	2. 余下k行(假设plan id列表长度为k)，每一行格式如下：
		```
		${plan id}\t\tleader:${leader}[\t\tchild event:${child event}]
		```
		其中```${leader}```表示leader事件计数值，```${child event}```表示各成员事件计数值，方括号包裹的```${child event}```表示该项的长度由child event个数(member字段的长度)决定。

	3. 以example-spec.json中的plan为例：
		```
		{
            "id": "456.hmmer",
            "type": "SAMPLE_PHASE",
            "task": {
                "id": "456.hmmer",
                "param": [],
                "rt": true,
                "pincpu": true,
                "phase": [0, 5]
            },
            "perf": {
                "leader": "INSTRUCTIONS",
                "period": 100000000,
                "member": ["CYCLES", "LLC_MISSES", "LLC_REFERENCES"]
            }
        }
		```

		执行命令：
		```
		sudo ./profile --config=../../conf/example-spec.json --cpu=1 --plan=456.hmmer
		```

		结果格式如下：
		```
		[456.hmmer]
		456.hmmer               CYCLES:188205902                INSTRUCTIONS:500000038          LLC_MISSES:41876                LLC_REFERENCES:768389
		```

#### 参与贡献

1. Fork 本仓库
2. 新建 Feat_xxx 分支
3. 提交代码
4. 新建 Pull Request
