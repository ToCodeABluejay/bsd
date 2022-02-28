/* Public domain. */

#ifndef _LINUX_PM_QOS_H
#define _LINUX_PM_QOS_H

struct pm_qos_request {
};

#define PM_QOS_DEFAULT_VALUE	-1

#define cpu_latency_qos_update_request(a, b)
#define cpu_latency_qos_add_request(a, b)
#define cpu_latency_qos_remove_request(a)
#define cpu_latency_qos_request_active(a)	false

#endif
