# ---------------------------------------------------------------------------
#                   Copyright Joe Drago 2010 - 2011.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http:#www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

set(frisk_src
    friskContext.c
    friskContext.h
)

add_library(frisk
    ${frisk_src}
)
