#pragma once


#ifndef ELEM_BLOCK
  #define ELEM_BLOCK(x) do { x } while (false)
#endif

#ifndef ELEM_ASSERT
  #include <cassert>
  #define ELEM_ASSERT(x) ELEM_BLOCK(assert(x);)
#endif

#ifndef ELEM_ASSERT_FALSE
  #define ELEM_ASSERT_FALSE ELEM_ASSERT(false)
#endif

#ifndef ELEM_RETURN_IF
  #define ELEM_RETURN_IF(cond, val) ELEM_BLOCK( if (cond) { return (val); } )
#endif
