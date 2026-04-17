//
//  test_platform.h
//  czim tests
//
//  Cross-platform compatibility shim
//

#ifndef test_platform_h
#define test_platform_h

#ifdef _WIN32
    #include <io.h>
    #define unlink _unlink
    #define PATH_SEP '\\'
#else
    #include <unistd.h>
    #define PATH_SEP '/'
#endif

#endif /* test_platform_h */
