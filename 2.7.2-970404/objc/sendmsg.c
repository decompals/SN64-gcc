/* GNU Objective C Runtime message lookup 
   Copyright (C) 1993, 1995, 1996, 1997 Free Software Foundation, Inc.
   Contributed by Kresten Krab Thorup

This file is part of GNU CC.

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

/* As a special exception, if you link this library with files compiled with
   GCC to produce an executable, this does not cause the resulting executable
   to be covered by the GNU General Public License. This exception does not
   however invalidate any other reasons why the executable file might be
   covered by the GNU General Public License.  */

#include "../tconfig.h"
#include "runtime.h"
#include "sarray.h"
#include "encoding.h"

/* this is how we hack STRUCT_VALUE to be 1 or 0 */
#define gen_rtx(args...) 1
#define rtx int

#if !defined(STRUCT_VALUE) || STRUCT_VALUE == 0
#define INVISIBLE_STRUCT_RETURN 1
#else
#define INVISIBLE_STRUCT_RETURN 0
#endif

/* The uninstalled dispatch table */
struct sarray* __objc_uninstalled_dtable = 0;   /* !T:MUTEX */

/* Send +initialize to class */
static void __objc_send_initialize(Class);

static void __objc_install_dispatch_table_for_class (Class);

/* Forward declare some functions */
static void __objc_init_install_dtable(id, SEL);

/* Various forwarding functions that are used based upon the
   return type for the selector.
   __objc_block_forward for structures.
   __objc_double_forward for floats/doubles.
   __objc_word_forward for pointers or types that fit in registers.
   */
static double __objc_double_forward(id, SEL, ...);
static id __objc_word_forward(id, SEL, ...);
typedef struct { id many[8]; } __big;
#if INVISIBLE_STRUCT_RETURN 
static __big 
#else
static id
#endif
__objc_block_forward(id, SEL, ...);
static Method_t search_for_method_in_hierarchy (Class class, SEL sel);
static Method_t search_for_method_in_list(MethodList_t list, SEL op);
id nil_method(id, SEL, ...);

/* Given a selector, return the proper forwarding implementation. */
__inline__
IMP
__objc_get_forward_imp (SEL sel)
{
  const char *t = sel->sel_types;

  if (t && (*t == '[' || *t == '(' || *t == '{'))
    return (IMP)__objc_block_forward;
  else if (t && (*t == 'f' || *t == 'd'))
    return (IMP)__objc_double_forward;
  else
    return (IMP)__objc_word_forward;
}

/* Given a class and selector, return the selector's implementation.  */
__inline__
IMP
get_imp (Class class, SEL sel)
{
  IMP impl;
  void* res = sarray_get (class->dtable, (size_t) sel->sel_id);
  if (res == 0)
    {
      /* Not a valid method */
      if(class->dtable == __objc_uninstalled_dtable)
	{
	  /* The dispatch table needs to be installed. */
	  objc_mutex_lock(__objc_runtime_mutex);
	  __objc_install_dispatch_table_for_class (class);
	  objc_mutex_unlock(__objc_runtime_mutex);
	  /* Call ourselves with the installed dispatch table
	     and get the real method */
	  res = get_imp(class, sel);
	}
      else
	{
	  /* The dispatch table has been installed so the
	     method just doesn't exist for the class.
	     Return the forwarding implementation. */
	  res = __objc_get_forward_imp(sel);
	}
    }
  return res;
}

/* Query if an object can respond to a selector, returns YES if the
object implements the selector otherwise NO.  Does not check if the
method can be forwarded. */
__inline__
BOOL
__objc_responds_to (id object, SEL sel)
{
  void* res;

  /* Install dispatch table if need be */
  if (object->class_pointer->dtable == __objc_uninstalled_dtable)
    {
      objc_mutex_lock(__objc_runtime_mutex);
      __objc_install_dispatch_table_for_class (object->class_pointer);
      objc_mutex_unlock(__objc_runtime_mutex);
    }

  /* Get the method from the dispatch table */
  res = sarray_get (object->class_pointer->dtable, (size_t) sel->sel_id);
  return (res != 0);
}

/* This is the lookup function.  All entries in the table are either a 
   valid method *or* zero.  If zero then either the dispatch table
   needs to be installed or it doesn't exist and forwarding is attempted. */
__inline__
IMP
objc_msg_lookup(id receiver, SEL op)
{
  IMP result;
  if(receiver)
    {
      result = sarray_get(receiver->class_pointer->dtable, (sidx)op->sel_id);
      if (result == 0)
	{
	  /* Not a valid method */
	  if(receiver->class_pointer->dtable == __objc_uninstalled_dtable)
	    {
	      /* The dispatch table needs to be installed.
		 This happens on the very first method call to the class. */
	      __objc_init_install_dtable(receiver, op);

	      /* Get real method for this in newly installed dtable */
	      result = get_imp(receiver->class_pointer, op);
	    }
	  else
	    {
	      /* The dispatch table has been installed so the
		 method just doesn't exist for the class.
		 Attempt to forward the method. */
	      result = __objc_get_forward_imp(op);
	    }
	}
      return result;
    }
  else
    return nil_method;
}

IMP
objc_msg_lookup_super (Super_t super, SEL sel)
{
  if (super->self)
    return get_imp (super->class, sel);
  else
    return nil_method;
}

int method_get_sizeof_arguments (Method*);

retval_t
objc_msg_sendv(id object, SEL op, arglist_t arg_frame)
{
  Method* m = class_get_instance_method(object->class_pointer, op);
  const char *type;
  *((id*)method_get_first_argument (m, arg_frame, &type)) = object;
  *((SEL*)method_get_next_argument (arg_frame, &type)) = op;
  return __builtin_apply((apply_t)m->method_imp, 
			 arg_frame,
			 method_get_sizeof_arguments (m));
}

void
__objc_init_dispatch_tables()
{
  __objc_uninstalled_dtable
    = sarray_new(200, 0);
}

/* This function is called by objc_msg_lookup when the
   dispatch table needs to be installed; thus it is called once
   for each class, namely when the very first message is sent to it. */
static void
__objc_init_install_dtable(id receiver, SEL op)
{
  /* This may happen, if the programmer has taken the address of a 
     method before the dtable was initialized... too bad for him! */
  if(receiver->class_pointer->dtable != __objc_uninstalled_dtable)
    return;

  objc_mutex_lock(__objc_runtime_mutex);

  if(CLS_ISCLASS(receiver->class_pointer))
    {
      /* receiver is an ordinary object */
      assert(CLS_ISCLASS(receiver->class_pointer));

      /* install instance methods table */
      __objc_install_dispatch_table_for_class (receiver->class_pointer);

      /* call +initialize -- this will in turn install the factory 
	 dispatch table if not already done :-) */
      __objc_send_initialize(receiver->class_pointer);
    }
  else
    {
      /* receiver is a class object */
      assert(CLS_ISCLASS((Class)receiver));
      assert(CLS_ISMETA(receiver->class_pointer));

      /* Install real dtable for factory methods */
      __objc_install_dispatch_table_for_class (receiver->class_pointer);

      if (strcmp (sel_get_name (op), "initialize"))
	__objc_send_initialize((Class)receiver);
      else
	CLS_SETINITIALIZED((Class)receiver);
    }
  objc_mutex_unlock(__objc_runtime_mutex);
}

/* Install dummy table for class which causes the first message to
   that class (or instances hereof) to be initialized properly */
void
__objc_install_premature_dtable(Class class)
{
  assert(__objc_uninstalled_dtable);
  class->dtable = __objc_uninstalled_dtable;
}   

/* Send +initialize to class if not already done */
static void
__objc_send_initialize(Class class)
{
  /* This *must* be a class object */
  assert(CLS_ISCLASS(class));
  assert(!CLS_ISMETA(class));

  if (!CLS_ISINITIALIZED(class))
    {
      CLS_SETINITIALIZED(class);
      CLS_SETINITIALIZED(class->class_pointer);
      
      if(class->super_class)
	__objc_send_initialize(class->super_class);

      {
	SEL 	op = sel_register_name ("initialize");
	Class	tmpclass = class;
	IMP	imp = 0;

	while (!imp && tmpclass) {
	  MethodList_t method_list = tmpclass->class_pointer->methods;

	  while(!imp && method_list) {
	    int i;
	    Method_t method;

	    for (i=0;i<method_list->method_count;i++) {
	      method = &(method_list->method_list[i]);
	      if (method->method_name
		  && method->method_name->sel_id == op->sel_id) {
	        imp = method->method_imp;
	        break;
	      }
	    }

	    method_list = method_list->method_next;

	  }

	  tmpclass = tmpclass->super_class;
	}
	if (imp)
	    (*imp)((id)class, op);
		
      }
    }
}

/* Walk on the methods list of class and install the methods in the reverse
   order of the lists. Since methods added by categories are before the methods
   of class in the methods list, this allows categories to substitute methods
   declared in class. However if more than one category replace the same method
   nothing is guarranteed about what method will be used.
   Assumes that __objc_runtime_mutex is locked down. */
static void
__objc_install_methods_in_dtable (Class class, MethodList_t method_list)
{
  int i;

  if (!method_list)
    return;

  if (method_list->method_next)
    __objc_install_methods_in_dtable (class, method_list->method_next);

  for (i = 0; i < method_list->method_count; i++)
    {
      Method_t method = &(method_list->method_list[i]);
      sarray_at_put_safe (class->dtable,
			  (sidx) method->method_name->sel_id,
			  method->method_imp);
    }
}

/* Assumes that __objc_runtime_mutex is locked down. */
static void
__objc_install_dispatch_table_for_class (Class class)
{
  Class super;
  int counter;

  /* If the class has not yet had it's class links resolved, we must 
     re-compute all class links */
  if(!CLS_ISRESOLV(class))
    __objc_resolve_class_links();

  super = class->super_class;

  if (super != 0 && (super->dtable == __objc_uninstalled_dtable))
    __objc_install_dispatch_table_for_class (super);

  /* Allocate dtable if necessary */
  if (super == 0)
    {
      objc_mutex_lock(__objc_runtime_mutex);
      class->dtable = sarray_new (__objc_selector_max_index, 0);
      objc_mutex_unlock(__objc_runtime_mutex);
    }
  else
    class->dtable = sarray_lazy_copy (super->dtable);

  __objc_install_methods_in_dtable (class, class->methods);
}

void
__objc_update_dispatch_table_for_class (Class class)
{
  Class next;
  struct sarray *arr;

  /* not yet installed -- skip it */
  if (class->dtable == __objc_uninstalled_dtable) 
    return;

  objc_mutex_lock(__objc_runtime_mutex);

  arr = class->dtable;
  __objc_install_premature_dtable (class); /* someone might require it... */
  sarray_free (arr);			   /* release memory */

  /* could have been lazy... */
  __objc_install_dispatch_table_for_class (class); 

  if (class->subclass_list)	/* Traverse subclasses */
    for (next = class->subclass_list; next; next = next->sibling_class)
      __objc_update_dispatch_table_for_class (next);

  objc_mutex_unlock(__objc_runtime_mutex);
}


/* This function adds a method list to a class.  This function is
   typically called by another function specific to the run-time.  As
   such this function does not worry about thread safe issues.

   This one is only called for categories. Class objects have their
   methods installed right away, and their selectors are made into
   SEL's by the function __objc_register_selectors_from_class. */ 
void
class_add_method_list (Class class, MethodList_t list)
{
  int i;

  /* Passing of a linked list is not allowed.  Do multiple calls.  */
  assert (!list->method_next);

  /* Check for duplicates.  */
  for (i = 0; i < list->method_count; ++i)
    {
      Method_t method = &list->method_list[i];

      if (method->method_name)  /* Sometimes these are NULL */
	{
	  /* This is where selector names are transmogrified to SEL's */
	  method->method_name = 
	    sel_register_typed_name ((const char*)method->method_name,
				     method->method_types);
	}
    }

  /* Add the methods to the class's method list.  */
  list->method_next = class->methods;
  class->methods = list;

  /* Update the dispatch table of class */
  __objc_update_dispatch_table_for_class (class);
}

Method_t
class_get_instance_method(Class class, SEL op)
{
  return search_for_method_in_hierarchy(class, op);
}

Method_t
class_get_class_method(MetaClass class, SEL op)
{
  return search_for_method_in_hierarchy(class, op);
}


/* Search for a method starting from the current class up its hierarchy.
   Return a pointer to the method's method structure if found.  NULL
   otherwise. */   

static Method_t
search_for_method_in_hierarchy (Class cls, SEL sel)
{
  Method_t method = NULL;
  Class class;

  if (! sel_is_mapped (sel))
    return NULL;

  /* Scan the method list of the class.  If the method isn't found in the
     list then step to its super class. */
  for (class = cls; ((! method) && class); class = class->super_class)
    method = search_for_method_in_list (class->methods, sel);

  return method;
}



/* Given a linked list of method and a method's name.  Search for the named
   method's method structure.  Return a pointer to the method's method
   structure if found.  NULL otherwise. */  
static Method_t
search_for_method_in_list (MethodList_t list, SEL op)
{
  MethodList_t method_list = list;

  if (! sel_is_mapped (op))
    return NULL;

  /* If not found then we'll search the list.  */
  while (method_list)
    {
      int i;

      /* Search the method list.  */
      for (i = 0; i < method_list->method_count; ++i)
        {
          Method_t method = &method_list->method_list[i];

          if (method->method_name)
            if (method->method_name->sel_id == op->sel_id)
              return method;
        }

      /* The method wasn't found.  Follow the link to the next list of
         methods.  */
      method_list = method_list->method_next;
    }

  return NULL;
}

static retval_t __objc_forward (id object, SEL sel, arglist_t args);

/* Forwarding pointers/integers through the normal registers */
static id
__objc_word_forward (id rcv, SEL op, ...)
{
  void *args, *res;

  args = __builtin_apply_args ();
  res = __objc_forward (rcv, op, args);
  if (res)
    __builtin_return (res);
  else
    return res;
}

/* Specific routine for forwarding floats/double because of
   architectural differences on some processors.  i386s for
   example which uses a floating point stack versus general
   registers for floating point numbers.  This forward routine 
   makes sure that GCC restores the proper return values */
static double
__objc_double_forward (id rcv, SEL op, ...)
{
  void *args, *res;

  args = __builtin_apply_args ();
  res = __objc_forward (rcv, op, args);
  __builtin_return (res);
}

#if INVISIBLE_STRUCT_RETURN
static __big
#else
static id
#endif
__objc_block_forward (id rcv, SEL op, ...)
{
  void *args, *res;

  args = __builtin_apply_args ();
  res = __objc_forward (rcv, op, args);
  if (res)
    __builtin_return (res);
}


/* This function is installed in the dispatch table for all methods which are
   not implemented.  Thus, it is called when a selector is not recognized. */
static retval_t
__objc_forward (id object, SEL sel, arglist_t args)
{
  IMP imp;
  static SEL frwd_sel = 0;                      /* !T:SAFE2 */
  SEL err_sel;

  /* first try if the object understands forward:: */
  if (!frwd_sel)
    frwd_sel = sel_get_any_uid("forward::");

  if (__objc_responds_to (object, frwd_sel))
    {
      imp = get_imp(object->class_pointer, frwd_sel);
      return (*imp)(object, frwd_sel, sel, args);
    }

  /* If the object recognizes the doesNotRecognize: method then we're going
     to send it. */
  err_sel = sel_get_any_uid ("doesNotRecognize:");
  if (__objc_responds_to (object, err_sel))
    {
      imp = get_imp (object->class_pointer, err_sel);
      return (*imp) (object, err_sel, sel);
    }
  
  /* The object doesn't recognize the method.  Check for responding to
     error:.  If it does then sent it. */
  {
    size_t strlen (const char*);
    char msg[256 + strlen ((const char*)sel_get_name (sel))
             + strlen ((const char*)object->class_pointer->name)];

    sprintf (msg, "(%s) %s does not recognize %s",
	     (CLS_ISMETA(object->class_pointer)
	      ? "class"
	      : "instance" ),
             object->class_pointer->name, sel_get_name (sel));

    err_sel = sel_get_any_uid ("error:");
    if (__objc_responds_to (object, err_sel))
      {
	imp = get_imp (object->class_pointer, err_sel);
	return (*imp) (object, sel_get_any_uid ("error:"), msg);
      }

    /* The object doesn't respond to doesNotRecognize: or error:;  Therefore,
       a default action is taken. */
    objc_error (object, OBJC_ERR_UNIMPLEMENTED, "%s\n", msg);
  }
}

void
__objc_print_dtable_stats()
{
  int total = 0;

  objc_mutex_lock(__objc_runtime_mutex);

  printf("memory usage: (%s)\n",
#ifdef OBJC_SPARSE2
	 "2-level sparse arrays"
#else
	 "3-level sparse arrays"
#endif
	 );

  printf("arrays: %d = %ld bytes\n", narrays, 
	 (int)narrays*sizeof(struct sarray));
  total += narrays*sizeof(struct sarray);
  printf("buckets: %d = %ld bytes\n", nbuckets, 
	 (int)nbuckets*sizeof(struct sbucket));
  total += nbuckets*sizeof(struct sbucket);

  printf("idxtables: %d = %ld bytes\n", idxsize, (int)idxsize*sizeof(void*));
  total += idxsize*sizeof(void*);
  printf("-----------------------------------\n");
  printf("total: %d bytes\n", total);
  printf("===================================\n");

  objc_mutex_unlock(__objc_runtime_mutex);
}

/* Returns the uninstalled dispatch table indicator.
 If a class' dispatch table points to __objc_uninstalled_dtable
 then that means it needs its dispatch table to be installed. */
__inline__
struct sarray* 
objc_get_uninstalled_dtable()
{
  return __objc_uninstalled_dtable;
}
