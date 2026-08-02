//
// Created by gcaptari on 03/08/2026.
//

#ifndef FT_PING_TYPES_H
#define FT_PING_TYPES_H

/**
 * @file types.h
 *
 * Forward typedefs shared by the whole project.
 *
 * A typedef may only be repeated from C11 on, so declaring the same alias in
 * two headers makes any file including both ill-formed in C99. Every alias
 * lives here once, and the headers include this file instead of copying it.
 *
 * Only the aliases live here, never the struct bodies: a forward typedef is
 * enough to declare a pointer or a function parameter, but a field held by
 * value, a sizeof or a member access still needs the header that defines the
 * struct.
 */

typedef struct s_binary_stream t_binary_stream;

typedef struct s_header_icmp t_header_icmp;

typedef struct s_echo_request t_echo_request;

typedef struct s_echo_reply t_echo_reply;

typedef struct s_packet_processor t_packet_processor;

#endif //FT_PING_TYPES_H
