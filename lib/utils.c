/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2016 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @brief Set of utility functions to operate on Platform QoS (pqos) data
 *        structures.
 *
 * These functions need no synchronization mechanisms.
 *
 */
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <alloca.h>       /* alloca() */
#endif /* __linux__ */

#include "pqos.h"
#include "types.h"

#define TOPO_OBJ_SOCKET     0
#define TOPO_OBJ_L2_CLUSTER 2
#define TOPO_OBJ_L3_CLUSTER 3

/**
 * @brief Calculates number of CPU topology objects
 *
 * @param [in] cpu CPU topology
 * @param [in] type CPU topology object type to be counted
 *             TOPO_OBJ_SOCKET - sockets
 *             TOPO_OBJ_L2_CLUSTER - L2 cache clusters
 *             TOPO_OBJ_L3_CLUSTER - L3 cache clusters
 *
 * @return Number of CPU topology objects
 * @retval 0 on error or if no object found
 */
static unsigned
__get_num_of_topology_objs(const struct pqos_cpuinfo *cpu, const int type)
{
        unsigned count = 0, i = 0;

        ASSERT(cpu != NULL);
        if (cpu == NULL)
                return 0;
        if (type != TOPO_OBJ_SOCKET && type != TOPO_OBJ_L2_CLUSTER &&
            type != TOPO_OBJ_L3_CLUSTER)
                return 0;

        ASSERT(cpu->num_cores > 0);
        if (cpu->num_cores == 0)
                return 0;

        for (i = 1, count = 1; i < cpu->num_cores; i++) {
                /**
                 * Check if topology object was already counted in
                 */
                unsigned j = 0;

                for (j = 0; j < i; j++) {
                        if (type == TOPO_OBJ_SOCKET &&
                            cpu->cores[i].socket == cpu->cores[j].socket)
                                break;
                        if (type == TOPO_OBJ_L2_CLUSTER &&
                            cpu->cores[i].l2_id == cpu->cores[j].l2_id)
                                break;
                        if (type == TOPO_OBJ_L3_CLUSTER &&
                            cpu->cores[i].l3_id == cpu->cores[j].l3_id)
                                break;
                }

                if (j >= i)
                        count++; /**< object not reported yet */
        }

        return count;
}

int
pqos_cpu_get_num_sockets(const struct pqos_cpuinfo *cpu,
			 unsigned *count)
{
        ASSERT(cpu != NULL);
        ASSERT(count != NULL);

        if (cpu == NULL || count == NULL)
		return PQOS_RETVAL_PARAM;
	*count = __get_num_of_topology_objs(cpu, TOPO_OBJ_SOCKET);
	if (*count == 0)
		return PQOS_RETVAL_ERROR;
	return PQOS_RETVAL_OK;
}

int
pqos_cpu_get_sockets(const struct pqos_cpuinfo *cpu,
                     const unsigned max_count,
                     unsigned *count,
                     unsigned *sockets)
{
        unsigned scount = 0, i = 0;

        ASSERT(cpu != NULL);
        ASSERT(count != NULL);
        ASSERT(sockets != NULL);
        ASSERT(max_count > 0);
        if (cpu == NULL || count == NULL ||
            sockets == NULL || max_count == 0)
                return PQOS_RETVAL_PARAM;

        for (i = 1; i < cpu->num_cores; i++) {
                unsigned j = 0;

                /**
                 * Check if this socket id is already on the \a sockets list
                 */
                for (j = 0; j < scount && scount > 0; j++)
                        if (cpu->cores[i].socket == sockets[j])
                                break;

                if (j >= scount || scount == 0) {
                        /**
                         * This socket wasn't reported before
                         */
                        if (scount >= max_count)
                                return PQOS_RETVAL_ERROR;
                        sockets[scount] = cpu->cores[i].socket;
                        scount++;
                }
        }

        *count = scount;
        return PQOS_RETVAL_OK;
}

/**
 * @brief Creates list of cores belonging to given topology object
 *
 * @param [in] cpu CPU topology
 * @param [in] type CPU topology object type to search cores for
 *             TOPO_OBJ_SOCKET - sockets
 *             TOPO_OBJ_L2_CLUSTER - L2 cache clusters
 *             TOPO_OBJ_L3_CLUSTER - L3 cache clusters
 * @param [in] id CPU topology object ID to search cores for
 * @param [out] count place to put number of objects found
 *
 * @return Pointer to list of cores for given topology object
 * @retval NULL on error or if no core found
 */
static unsigned *
__get_cores_per_topology_obj(const struct pqos_cpuinfo *cpu,
                             const int type,
                             const unsigned id,
                             unsigned *count)
{
        unsigned num = 0, i = 0;
        unsigned *core_list = NULL;

        ASSERT(cpu != NULL);
        ASSERT(count != NULL);
        if (cpu == NULL || count == NULL)
                return NULL;

        core_list = (unsigned *) malloc(cpu->num_cores * sizeof(core_list[0]));
        if (core_list == NULL)
                return NULL;

        for (i = 0; i < cpu->num_cores; i++)
                if ((type == TOPO_OBJ_L3_CLUSTER &&
                     id == cpu->cores[i].l3_id) ||
                    (type == TOPO_OBJ_L2_CLUSTER &&
                     id == cpu->cores[i].l2_id) ||
                    (type == TOPO_OBJ_SOCKET && id == cpu->cores[i].socket))
                        core_list[num++] = cpu->cores[i].lcore;

        if (num == 0) {
                free(core_list);
                return NULL;
        }

        *count = num;
        return core_list;
}

unsigned *
pqos_cpu_get_cores_l3id(const struct pqos_cpuinfo *cpu, const unsigned l3_id,
                        unsigned *count)
{
        return __get_cores_per_topology_obj(cpu, TOPO_OBJ_L3_CLUSTER, l3_id,
                                            count);
}

int
pqos_cpu_get_cores(const struct pqos_cpuinfo *cpu,
                   const unsigned socket,
                   const unsigned max_count,
                   unsigned *count,
                   unsigned *cores)
{
        unsigned i = 0, cnt = 0;

        ASSERT(cpu != NULL);
        ASSERT(count != NULL);
        ASSERT(cores != NULL);
        ASSERT(max_count > 0);

        if (cpu == NULL || count == NULL ||
            cores == NULL || max_count == 0)
                return PQOS_RETVAL_PARAM;

        for (i = 0; i < cpu->num_cores; i++)
                if (cpu->cores[i].socket == socket) {
                        if (max_count == 1) {
                                /**
                                 * Special case when app wants to get
                                 * just one core for the socket
                                 */
                                *cores = cpu->cores[i].lcore;
                                *count = 1;
                                return PQOS_RETVAL_OK;
                        }

                        /**
                         * Is there is more cores than \a cores can accommodate?
                         */
                        if (cnt >= max_count)
                                return PQOS_RETVAL_ERROR;
                        cores[cnt] = cpu->cores[i].lcore;
                        cnt++;
                }

        if (!cnt)
                return PQOS_RETVAL_ERROR;               /**< no core found */

        *count = cnt;
        return PQOS_RETVAL_OK;
}

int
pqos_cpu_check_core(const struct pqos_cpuinfo *cpu,
                    const unsigned lcore)
{
        unsigned i = 0;

        ASSERT(cpu != NULL);
        if (cpu == NULL)
                return PQOS_RETVAL_PARAM;

        for (i = 0; i < cpu->num_cores; i++)
                if (cpu->cores[i].lcore == lcore)
                        return PQOS_RETVAL_OK;

        return PQOS_RETVAL_ERROR;
}

int
pqos_cpu_get_socketid(const struct pqos_cpuinfo *cpu,
                      const unsigned lcore,
                      unsigned *socket)
{
        unsigned i = 0;

        if (cpu == NULL || socket == NULL)
                return PQOS_RETVAL_PARAM;

        for (i = 0; i < cpu->num_cores; i++)
                if (cpu->cores[i].lcore == lcore) {
                        *socket = cpu->cores[i].socket;
                        return PQOS_RETVAL_OK;
                }

        return PQOS_RETVAL_ERROR;
}

int
pqos_cpu_get_clusterid(const struct pqos_cpuinfo *cpu,
                       const unsigned lcore,
                       unsigned *cluster)
{
        unsigned i = 0;

        if (cpu == NULL || cluster == NULL)
                return PQOS_RETVAL_PARAM;

        for (i = 0; i < cpu->num_cores; i++)
                if (cpu->cores[i].lcore == lcore) {
                        *cluster = cpu->cores[i].l3_id;
                        return PQOS_RETVAL_OK;
                }

        return PQOS_RETVAL_ERROR;
}

int
pqos_cap_get_type(const struct pqos_cap *cap,
                  const enum pqos_cap_type type,
                  const struct pqos_capability **cap_item)
{
        int ret = PQOS_RETVAL_RESOURCE;
        unsigned i;

        ASSERT(cap != NULL && cap_item != NULL);
        if (cap == NULL || cap_item == NULL)
                return PQOS_RETVAL_PARAM;

        ASSERT(type < PQOS_CAP_TYPE_NUMOF);
        if (type >= PQOS_CAP_TYPE_NUMOF)
                return PQOS_RETVAL_PARAM;

        for (i = 0; i < cap->num_cap; i++) {
                if (cap->capabilities[i].type != type)
                        continue;
                *cap_item = &cap->capabilities[i];
                ret = PQOS_RETVAL_OK;
                break;
        }

        return ret;
}

int
pqos_cap_get_event(const struct pqos_cap *cap,
                   const enum pqos_mon_event event,
                   const struct pqos_monitor **p_mon)
{
        const struct pqos_capability *cap_item = NULL;
        const struct pqos_cap_mon *mon = NULL;
        int ret = PQOS_RETVAL_OK;
        unsigned i;

        if (cap == NULL || p_mon == NULL)
                return PQOS_RETVAL_PARAM;

        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MON, &cap_item);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ASSERT(cap_item != NULL);
        mon = cap_item->u.mon;

        ret = PQOS_RETVAL_ERROR;

        for (i = 0; i < mon->num_events; i++) {
                if (mon->events[i].type == event) {
                        *p_mon = &mon->events[i];
                        ret = PQOS_RETVAL_OK;
                        break;
                }
        }

        return ret;
}

int
pqos_l3ca_get_cos_num(const struct pqos_cap *cap,
                       unsigned *cos_num)
{
        const struct pqos_capability *item = NULL;
        int ret = PQOS_RETVAL_OK;

        ASSERT(cap != NULL && cos_num != NULL);
        if (cap == NULL || cos_num == NULL)
                return PQOS_RETVAL_PARAM;

        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_L3CA, &item);
        if (ret != PQOS_RETVAL_OK)
                return ret;                           /**< no L3CA capability */

        ASSERT(item != NULL);
        *cos_num = item->u.l3ca->num_classes;
        return ret;
}

int
pqos_l2ca_get_cos_num(const struct pqos_cap *cap,
                       unsigned *cos_num)
{
        const struct pqos_capability *item = NULL;
        int ret = PQOS_RETVAL_OK;

        ASSERT(cap != NULL && cos_num != NULL);
        if (cap == NULL || cos_num == NULL)
                return PQOS_RETVAL_PARAM;

        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_L2CA, &item);
        if (ret != PQOS_RETVAL_OK)
                return ret;                           /**< no L2CA capability */

        ASSERT(item != NULL);
        *cos_num = item->u.l2ca->num_classes;
        return ret;
}

int
pqos_l3ca_cdp_enabled(const struct pqos_cap *cap,
                      int *cdp_supported,
                      int *cdp_enabled)
{
        const struct pqos_capability *item = NULL;
        int ret = PQOS_RETVAL_OK;

        ASSERT(cap != NULL && (cdp_enabled != NULL || cdp_supported != NULL));
        if (cap == NULL || (cdp_enabled == NULL && cdp_supported == NULL))
                return PQOS_RETVAL_PARAM;

        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_L3CA, &item);
        if (ret != PQOS_RETVAL_OK)
                return ret;                           /**< no L3CA capability */

        ASSERT(item != NULL);
        if (cdp_supported != NULL)
                *cdp_supported = item->u.l3ca->cdp;
        if (cdp_enabled != NULL)
                *cdp_enabled = item->u.l3ca->cdp_on;
        return ret;
}
