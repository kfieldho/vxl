#ifndef vcl_queue_h_
#define vcl_queue_h_

#include "vcl_compiler.h"

#include "iso/vcl_queue.h"

#define VCL_QUEUE_INSTANTIATE(T) extern "you must #include vcl_queue.txx"
#define VCL_PRIORITY_QUEUE_INSTANTIATE(T) extern "you must #include vcl_queue.txx"

#include "vcl_queue.txx"

#endif // vcl_queue_h_
