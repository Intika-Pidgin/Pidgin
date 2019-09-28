/*
 * Header file for common error description library.
 *
 * Copyright 1988, Student Information Processing Board of the
 * Massachusetts Institute of Technology.
 *
 * For copyright and distribution info, see the documentation supplied
 * with this package.
 */

#ifndef PURPLE_ZEPHYR_COM_ERR_H
#define PURPLE_ZEPHYR_COM_ERR_H

#define COM_ERR_BUF_LEN 25

#include <stdarg.h>

typedef void (*error_handler_t)(const char *, long, const char *, va_list);
extern error_handler_t com_err_hook;
void com_err(const char *, long, const char *, ...);
const char *error_message(long);
const char *error_message_r(long, char *);
error_handler_t set_com_err_hook(error_handler_t);
error_handler_t reset_com_err_hook(void);

#endif /* PURPLE_ZEPHYR_COM_ERR_H */
