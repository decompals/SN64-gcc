/* Thread and mutex controls for Objective C.
   Copyright (C) 1996, 1997 Free Software Foundation, Inc.
   Contributed by Galen C. Hunt (gchunt@cs.rochester.edu)

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

GNU CC is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2, or (at your option) any later version.

GNU CC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
details.

You should have received a copy of the GNU General Public License along with
GNU CC; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* As a special exception, if you link this library with files
   compiled with GCC to produce an executable, this does not cause
   the resulting executable to be covered by the GNU General Public License.
   This exception does not however invalidate any other reasons why
   the executable file might be covered by the GNU General Public License.  */


#ifndef __thread_INCLUDE_GNU
#define __thread_INCLUDE_GNU

#include "objc/objc.h"

/*************************************************************************
 *  Universal static variables:
 */
extern int __objc_thread_exit_status;      /* Global exit status.   */

/********
 *  Thread safe implementation types and functions.  
 */

#define OBJC_THREAD_INTERACTIVE_PRIORITY        2
#define OBJC_THREAD_BACKGROUND_PRIORITY         1
#define OBJC_THREAD_LOW_PRIORITY                0

typedef void * objc_thread_t;
typedef struct objc_mutex *objc_mutex_t;
typedef struct objc_condition *objc_condition_t;

objc_mutex_t objc_mutex_allocate(void);
int     objc_mutex_deallocate(objc_mutex_t mutex);
int     objc_mutex_lock(objc_mutex_t mutex);
int     objc_mutex_unlock(objc_mutex_t mutex);
int     objc_mutex_trylock(objc_mutex_t mutex);

objc_condition_t objc_condition_allocate(void);
int     objc_condition_deallocate(objc_condition_t condition);
int     objc_condition_wait(objc_condition_t condition, objc_mutex_t mutex);
int     objc_condition_signal(objc_condition_t condition);
int     objc_condition_broadcast(objc_condition_t condition);

objc_thread_t objc_thread_create(void (*func)(void *arg), void *arg);
void    objc_thread_yield(void);
int     objc_thread_exit(void);
int     objc_thread_set_priority(int priority);
int     objc_thread_get_priority(void);
void *  objc_thread_get_data(void);
int     objc_thread_set_data(void *value);
objc_thread_t objc_thread_id(void);

objc_thread_t objc_thread_detach(SEL selector, id object, id argument);
int     objc_mutex_lock_x(objc_mutex_t mutex, const char *f, int l);
int     objc_mutex_unlock_x(objc_mutex_t mutex, const char *f, int l);

/*
  Use this to set the hook function that will be called when the 
  runtime initially becomes multi threaded.
  The hook function is only called once, meaning only when the 
  2nd thread is spawned, not for each and every thread.

  It returns the previous hook function or NULL if there is none.

  A program outside of the runtime could set this to some function so
  it can be informed; for example, the GNUstep Base Library sets it 
  so it can implement the NSBecomingMultiThreaded notification.
  */
typedef void (*objc_thread_callback)();
objc_thread_callback objc_set_thread_callback(objc_thread_callback func);

/* For debugging of locks, uncomment these two macros: */
/* #define objc_mutex_lock(x)      objc_mutex_lock_x(x, __FILE__, __LINE__) */
/* #define objc_mutex_unlock(x)    objc_mutex_unlock_x(x, __FILE__, __LINE__)*/

#endif /* not __thread_INCLUDE_GNU */
