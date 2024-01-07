/**
 * (C) 2007-22 - ntop.org and contributors
 * Copyright (C) 2023 Hamish Coleman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not see see <http://www.gnu.org/licenses/>
 *
 */


#include <stdbool.h>
#include <stdlib.h>      // for exit
#include "n2n.h"         // for n2n_sn_t, open_socket, run_sn_loop, sn_init

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>  // for INADDR_ANY, INADDR_LOOPBACK
#endif


static bool keep_running = true;

int main () {

    n2n_sn_t sss_node;
    int rc;
    struct sockaddr_in local_address;

#ifdef _WIN32
    initWin32();
#endif

    sn_init_defaults(&sss_node);
    sss_node.daemon = false;   // Whether to daemonize
    sss_node.lport = 1234; // Main UDP listen port

    memset(&local_address, 0, sizeof(local_address));
    local_address.sin_family = AF_INET;
    local_address.sin_port = htons(sss_node.lport);
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);

    sss_node.sock = open_socket(
        (struct sockaddr *)&local_address,
        sizeof(local_address),
        0 /* UDP */
        );
    if(-1 == sss_node.sock) {
        exit(-2);
    }

    memset(&local_address, 0, sizeof(local_address));
    local_address.sin_family = AF_INET;
    local_address.sin_port = htons(5645);
    local_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    sss_node.mgmt_sock = open_socket(
        (struct sockaddr *)&local_address,
        sizeof(local_address),
        0 /* UDP */
        ); // Main UDP management port
    if(-1 == sss_node.mgmt_sock) {
        exit(-2);
    }

    sn_init(&sss_node);

    sss_node.keep_running = &keep_running;
    rc = run_sn_loop(&sss_node);

    sn_term(&sss_node);

    return rc;
}
