/*
 * Copyright (C) 2023 Hamish Coleman
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Handlers for configuration files
 */

#include <n3n/conffile.h>
#include <stdbool.h>            // for true, false
#include <stdint.h>             // for uint32_t
#include <stdio.h>              // for printf
#include <stdlib.h>             // for malloc
#include <string.h>             // for strcmp

#include <auth.h>               // for generate_private_key
#include <n2n.h>                // for edge_conf_add_supernode
#include <n2n_define.h>         // for HEADER_ENCRYPTION_UNKNOWN...

static struct n3n_conf_section *registered_sections;

void n3n_config_register_section (char *name, struct n3n_conf_option options[]) {
    struct n3n_conf_section *section;
    section = malloc(sizeof(*section));
    if(!section) {
        return;
    }

    // TODO: should confirm that we register each section name only once

    section->next = registered_sections;
    section->name = name;
    section->options = options;
    registered_sections = section;
}

static struct n3n_conf_option *lookup_section (char *section) {
    struct n3n_conf_section *p = registered_sections;
    while(p) {
        if(0==strcmp(p->name, section)) {
            return p->options;
        }
        p = p->next;
    }
    return NULL;
}

static struct n3n_conf_option *lookup_option (char *section, char *option) {
    struct n3n_conf_option *p = lookup_section(section);
    if(!p) {
        return NULL;
    }

    while(p->name) {
        if(0==strcmp(p->name, option)) {
            return p;
        }
        p++;
    }
    return NULL;
}

int n3n_config_set_option (void *conf, char *section, char *option, char *value) {
    struct n3n_conf_option *p = lookup_option(section, option);
    if(!p) {
        return -1;
    }

    switch(p->type) {
        case n3n_conf_strncpy: {
            char *dst = (char *)conf + p->offset;

            strncpy(dst, value, p->length);
            dst[p->length -1] = 0;
            return 0;
        }
        case n3n_conf_bool: {
            bool *dst = (bool *)((char *)conf + p->offset);

            if(0==strcmp("true", value)) {
                *dst = true;
            } else if(0==strcmp("false", value)) {
                *dst = false;
            } else {
                return -1;
            }
            return 0;
        }
        case n3n_conf_uint32: {
            uint32_t *dst = (uint32_t *)((char *)conf + p->offset);
            char *endptr;
            uint32_t i = strtoull(value, &endptr, 10);

            if(*value && !*endptr) {
                // "the entire string is valid"
                *dst = i;
                return 0;
            }
            return -1;
        }
        case n3n_conf_strdup: {
            char **dst = (char **)((char *)conf + p->offset);
            if(*dst) {
                free(*dst);
            }
            *dst = strdup(value);
            if(*dst) {
                return 0;
            }
            return -1;
        }
        case n3n_conf_transform: {
            uint8_t *dst = ((uint8_t *)conf + p->offset);
            // TODO: in the future, we should lookup against a struct of
            // registered transforms and prefer to use strings instead of
            // numbers.
            // For now, manually keep the max ids in sync with n2n_transform_t

            char *endptr;
            uint32_t i = strtoull(value, &endptr, 10);

            if(*value && !*endptr) {
                // "the entire string is valid"

                if(i>0 && i<6) {
                    // N2N_TRANSFORM_ID_NULL = 1
                    // ...
                    // N2N_TRANSFORM_ID_SPECK = 5
                    *dst = i;
                    return 0;
                }
            }
            return -1;
        }
        case n3n_conf_headerenc: {
            uint8_t *dst = ((uint8_t *)conf + p->offset);
            // TODO: this is a bit of an odd one out, since it is a tristate boolean

            if(0==strcmp("true", value)) {
                *dst = HEADER_ENCRYPTION_ENABLED;
            } else if(0==strcmp("false", value)) {
                *dst = HEADER_ENCRYPTION_NONE;
            } else if(0==strcmp("unknown", value)) {
                *dst = HEADER_ENCRYPTION_UNKNOWN;
            } else {
                return -1;
            }
            return 0;
        }
        case n3n_conf_compression: {
            uint8_t *dst = ((uint8_t *)conf + p->offset);
            // TODO: in the future, we should lookup against a struct of
            // registered compressions and prefer to use strings instead of
            // numbers.
            // For now, manually keep the max ids in sync with n2n_transform_t

            char *endptr;
            uint32_t i = strtoull(value, &endptr, 10);

            if(*value && !*endptr) {
                // "the entire string is valid"

                switch(i) {
                    case 0:
                        *dst = N2N_COMPRESSION_ID_NONE;
                        return 0;
                    case 1:
                        *dst = N2N_COMPRESSION_ID_LZO;
                        return 0;
#ifdef HAVE_ZSTD
// FIXME: codebase has these defs wrong, they should be HAVE_LIBZSTD
                    case 2:
                        *dst = N2N_COMPRESSION_ID_ZSTD;
                        return 0;
#endif
                }
            }
            return -1;
        }
        case n3n_conf_supernode: {
            return edge_conf_add_supernode(conf, value);
        }
        case n3n_conf_privatekey: {
            n2n_private_public_key_t **dst = (n2n_private_public_key_t **)((char *)conf + p->offset);
            if(*dst) {
                free(*dst);
            }
            *dst = malloc(sizeof(**dst));
            if(!*dst) {
                return -1;
            }
            generate_private_key(**dst, value);
            return 0;
        }
    }
    return -1;
}
