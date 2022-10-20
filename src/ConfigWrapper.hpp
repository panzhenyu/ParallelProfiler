#pragma once

#include "Profiler/Config.hpp"

/**
 * SystemConfig
 * collect system config such as cpu num
 */
class SystemConfig: public IValidConf {
public:
    virtual bool valid() const override;

};

/**
 * TaskConfig
 * Parse task config file, provide task object
 */
class TaskConfig: public IValidConf {
public:
    virtual bool valid() const override;
};

/**
 * PlanConfig
 * Parse plan config file, provide plan object
 */
class PlanConfig: public IValidConf {
public:
    virtual bool valid() const override;
};