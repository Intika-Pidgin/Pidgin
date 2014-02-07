/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _PURPLE_SIGNALS_H_
#define _PURPLE_SIGNALS_H_
/**
 * SECTION:signals
 * @section_id: libpurple-signals
 * @short_description: <filename>signals.h</filename>
 * @title: Purple-signals API
 * @see_also: <link linkend="chapter-tut-signals">Signals tutorial</link>
 */

#include <glib.h>
#include <glib-object.h>

#define PURPLE_CALLBACK(func) ((PurpleCallback)func)

typedef void (*PurpleCallback)(void);
typedef void (*PurpleSignalMarshalFunc)(PurpleCallback cb, va_list args,
									  void *data, void **return_val);

G_BEGIN_DECLS

/**************************************************************************/
/* Signal API                                                             */
/**************************************************************************/

/**
 * PURPLE_SIGNAL_PRIORITY_DEFAULT:
 *
 * The priority of a signal connected using purple_signal_connect().
 *
 * See purple_signal_connect_priority()
 */
#define PURPLE_SIGNAL_PRIORITY_DEFAULT     0

/**
 * PURPLE_SIGNAL_PRIORITY_HIGHEST:
 *
 * The largest signal priority; signals with this priority will be called
 * <emphasis>last</emphasis>.  (This is highest as in numerical value, not as in
 * order of importance.)
 *
 * See purple_signal_connect_priority().
 */
#define PURPLE_SIGNAL_PRIORITY_HIGHEST  9999

/**
 * PURPLE_SIGNAL_PRIORITY_LOWEST:
 *
 * The smallest signal priority; signals with this priority will be called
 * <emphasis>first</emphasis>.  (This is lowest as in numerical value, not as in
 * order of importance.)
 *
 * See purple_signal_connect_priority().
 */
#define PURPLE_SIGNAL_PRIORITY_LOWEST  -9999

/**
 * purple_signal_register:
 * @instance:   The instance to register the signal for.
 * @signal:     The signal name.
 * @marshal:    The marshal function.
 * @ret_type:   The return type, or G_TYPE_NONE for no return type.
 * @num_values: The number of values to be passed to the callbacks.
 * @...:        The types of the parameters for the callbacks.
 *
 * Registers a signal in an instance.
 *
 * Returns: The signal ID local to that instance, or 0 if the signal
 *          couldn't be registered.
 */
gulong purple_signal_register(void *instance, const char *signal,
							PurpleSignalMarshalFunc marshal,
							GType ret_type, int num_values, ...);

/**
 * purple_signal_unregister:
 * @instance: The instance to unregister the signal for.
 * @signal:   The signal name.
 *
 * Unregisters a signal in an instance.
 */
void purple_signal_unregister(void *instance, const char *signal);

/**
 * purple_signals_unregister_by_instance:
 * @instance: The instance to unregister the signal for.
 *
 * Unregisters all signals in an instance.
 */
void purple_signals_unregister_by_instance(void *instance);

/**
 * purple_signal_get_types:
 * @instance:           The instance the signal is registered to.
 * @signal:             The signal.
 * @ret_type: (out):    The return type.
 * @num_values: (out):  The returned number of parameters.
 * @param_types: (out): The returned list of parameter types.
 *
 * Outputs a list of value types used for a signal through the @ret_type,
 * @num_values and @param_types out parameters.
 */
void purple_signal_get_types(void *instance, const char *signal,
							GType *ret_type, int *num_values,
							GType **param_types);

/**
 * purple_signal_connect_priority:
 * @instance: The instance to connect to.
 * @signal:   The name of the signal to connect.
 * @handle:   The handle of the receiver.
 * @func:     The callback function.
 * @data:     The data to pass to the callback function.
 * @priority: The priority with which the handler should be called. Signal
 *                 handlers are called in ascending numerical order of
 *                 @priority from #PURPLE_SIGNAL_PRIORITY_LOWEST to
 *                 #PURPLE_SIGNAL_PRIORITY_HIGHEST.
 *
 * Connects a signal handler to a signal for a particular object.
 *
 * Take care not to register a handler function twice. Purple will
 * not correct any mistakes for you in this area.
 *
 * See purple_signal_disconnect()
 *
 * Returns: The signal handler ID.
 */
gulong purple_signal_connect_priority(void *instance, const char *signal,
	void *handle, PurpleCallback func, void *data, int priority);

/**
 * purple_signal_connect:
 * @instance: The instance to connect to.
 * @signal:   The name of the signal to connect.
 * @handle:   The handle of the receiver.
 * @func:     The callback function.
 * @data:     The data to pass to the callback function.
 *
 * Connects a signal handler to a signal for a particular object.
 * (Its priority defaults to 0, aka #PURPLE_SIGNAL_PRIORITY_DEFAULT.)
 *
 * Take care not to register a handler function twice. Purple will
 * not correct any mistakes for you in this area.
 *
 * See purple_signal_disconnect()
 *
 * Returns: The signal handler ID.
 */
gulong purple_signal_connect(void *instance, const char *signal,
	void *handle, PurpleCallback func, void *data);

/**
 * purple_signal_connect_priority_vargs:
 * @instance: The instance to connect to.
 * @signal:   The name of the signal to connect.
 * @handle:   The handle of the receiver.
 * @func:     The callback function.
 * @data:     The data to pass to the callback function.
 * @priority: The priority with which the handler should be called. Signal
 *                 handlers are called in ascending numerical order of
 *                 @priority from #PURPLE_SIGNAL_PRIORITY_LOWEST to
 *                 #PURPLE_SIGNAL_PRIORITY_HIGHEST.
 *
 * Connects a signal handler to a signal for a particular object.
 *
 * The signal handler will take a va_args of arguments, instead of
 * individual arguments.
 *
 * Take care not to register a handler function twice. Purple will
 * not correct any mistakes for you in this area.
 *
 * See purple_signal_disconnect()
 *
 * Returns: The signal handler ID.
 */
gulong purple_signal_connect_priority_vargs(void *instance, const char *signal,
	void *handle, PurpleCallback func, void *data, int priority);

/**
 * purple_signal_connect_vargs:
 * @instance: The instance to connect to.
 * @signal:   The name of the signal to connect.
 * @handle:   The handle of the receiver.
 * @func:     The callback function.
 * @data:     The data to pass to the callback function.
 *
 * Connects a signal handler to a signal for a particular object.
 * (Its priority defaults to 0, aka #PURPLE_SIGNAL_PRIORITY_DEFAULT.)
 *
 * The signal handler will take a va_args of arguments, instead of
 * individual arguments.
 *
 * Take care not to register a handler function twice. Purple will
 * not correct any mistakes for you in this area.
 *
 * See purple_signal_disconnect()
 *
 * Returns: The signal handler ID.
 */
gulong purple_signal_connect_vargs(void *instance, const char *signal,
	void *handle, PurpleCallback func, void *data);

/**
 * purple_signal_disconnect:
 * @instance: The instance to disconnect from.
 * @signal:   The name of the signal to disconnect.
 * @handle:   The handle of the receiver.
 * @func:     The registered function to disconnect.
 *
 * Disconnects a signal handler from a signal on an object.
 *
 * See purple_signal_connect()
 */
void purple_signal_disconnect(void *instance, const char *signal,
							void *handle, PurpleCallback func);

/**
 * purple_signals_disconnect_by_handle:
 * @handle: The receiver handle.
 *
 * Removes all callbacks associated with a receiver handle.
 */
void purple_signals_disconnect_by_handle(void *handle);

/**
 * purple_signal_emit:
 * @instance: The instance emitting the signal.
 * @signal:   The signal being emitted.
 *
 * Emits a signal.
 *
 * See purple_signal_connect(), purple_signal_disconnect()
 */
void purple_signal_emit(void *instance, const char *signal, ...);

/**
 * purple_signal_emit_vargs:
 * @instance: The instance emitting the signal.
 * @signal:   The signal being emitted.
 * @args:     The arguments list.
 *
 * Emits a signal, using a va_list of arguments.
 *
 * See purple_signal_connect(), purple_signal_disconnect()
 */
void purple_signal_emit_vargs(void *instance, const char *signal, va_list args);

/**
 * purple_signal_emit_return_1:
 * @instance: The instance emitting the signal.
 * @signal:   The signal being emitted.
 *
 * Emits a signal and returns the first non-NULL return value.
 *
 * Further signal handlers are NOT called after a handler returns
 * something other than NULL.
 *
 * Returns: The first non-NULL return value
 */
void *purple_signal_emit_return_1(void *instance, const char *signal, ...);

/**
 * purple_signal_emit_vargs_return_1:
 * @instance: The instance emitting the signal.
 * @signal:   The signal being emitted.
 * @args:     The arguments list.
 *
 * Emits a signal and returns the first non-NULL return value.
 *
 * Further signal handlers are NOT called after a handler returns
 * something other than NULL.
 *
 * Returns: The first non-NULL return value
 */
void *purple_signal_emit_vargs_return_1(void *instance, const char *signal,
									  va_list args);

/**
 * purple_signals_init:
 *
 * Initializes the signals subsystem.
 */
void purple_signals_init(void);

/**
 * purple_signals_uninit:
 *
 * Uninitializes the signals subsystem.
 */
void purple_signals_uninit(void);

/**************************************************************************/
/* Marshal Functions                                                      */
/**************************************************************************/

void purple_marshal_VOID(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_VOID__INT(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_VOID__INT_INT(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_VOID__POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_VOID__POINTER_UINT(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_VOID__POINTER_INT_INT(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_VOID__POINTER_INT_POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_VOID__POINTER_POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_VOID__POINTER_POINTER_UINT(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_VOID__POINTER_POINTER_UINT_UINT(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_VOID__POINTER_UINT_UINT(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_VOID__POINTER_POINTER_POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_VOID__POINTER_POINTER_POINTER_UINT(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_VOID__POINTER_POINTER_POINTER_UINT_UINT(
		PurpleCallback cb, va_list args, void *data, void **return_val);

void purple_marshal_INT__INT(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_INT__INT_INT(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_INT__POINTER_POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_INT__POINTER_POINTER_POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_INT__POINTER_POINTER_POINTER_POINTER_POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);

void purple_marshal_BOOLEAN__POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_BOOLEAN__POINTER_POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_BOOLEAN__POINTER_BOOLEAN(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_BOOLEAN__POINTER_POINTER_POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_BOOLEAN__POINTER_POINTER_UINT(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_UINT(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_UINT(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER_POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);

void purple_marshal_BOOLEAN__INT_POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);

void purple_marshal_POINTER__POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_POINTER__POINTER_INT(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_POINTER__POINTER_INT64(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_POINTER__POINTER_INT_BOOLEAN(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_POINTER__POINTER_INT64_BOOLEAN(
		PurpleCallback cb, va_list args, void *data, void **return_val);
void purple_marshal_POINTER__POINTER_POINTER(
		PurpleCallback cb, va_list args, void *data, void **return_val);

G_END_DECLS

#endif /* _PURPLE_SIGNALS_H_ */

