# ParallelProfiler

#### 介绍
Perf profiler for parallel process

#### 软件架构
软件架构说明


#### 安装教程

1.  xxxx
2.  xxxx
3.  xxxx

#### 使用说明

1.  xxxx
2.  xxxx
3.  xxxx

[Usage]
    sudo ./ParallelProfile
        --config                            Optional    file path, such as conf/example.json, default is empty
        --output                            Optional    file path, default is stdout
        --log                               Optional    file path, default is stderr
        --cpu                               Optional    such as 1,2~4, default is empty
        --plan                              Repeated    such as id or "{key:value[,key:value]}", at least one plan

[Key]
        id                                  Required    such as "myplan"
        task                                Required    such as "./task"
        type                                Required    choose "DAEMON" or "COUNT" or "SAMPLE_ALL" or "SAMPLE_PHASE"
        rt                                  Optional    choose true or false, default is false
        pincpu                              Optional    choose true or false, default is false
        phase                               Optional    such as [start,end], default is [0,0]
        perf-leader                         Optional    such as "INSTURCTIONS", default is empty
        sample-period                       Optional    default is 0
        perf-member                         Optional    such as [MEMBER1, MEMBER2], default is empty


#### 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request


#### 特技

1.  使用 Readme\_XXX.md 来支持不同的语言，例如 Readme\_en.md, Readme\_zh.md
2.  Gitee 官方博客 [blog.gitee.com](https://blog.gitee.com)
3.  你可以 [https://gitee.com/explore](https://gitee.com/explore) 这个地址来了解 Gitee 上的优秀开源项目
4.  [GVP](https://gitee.com/gvp) 全称是 Gitee 最有价值开源项目，是综合评定出的优秀开源项目
5.  Gitee 官方提供的使用手册 [https://gitee.com/help](https://gitee.com/help)
6.  Gitee 封面人物是一档用来展示 Gitee 会员风采的栏目 [https://gitee.com/gitee-stars/](https://gitee.com/gitee-stars/)
