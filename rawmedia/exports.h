#ifndef RAWMEDIA_EXPORTS_H_
#define RAWMEDIA_EXPORTS_H_

#ifdef rawmedia_EXPORTS
    #if defined _WIN32 || defined __CYGWIN__
        #define RAWMEDIA_IMPORT __declspec(dllimport)
        #define RAWMEDIA_EXPORT __declspec(dllexport)
        #define RAWMEDIA_LOCAL
    #else
        #if __GNUC__ >= 4
            #define RAWMEDIA_IMPORT __attribute__ ((visibility ("default")))
            #define RAWMEDIA_EXPORT __attribute__ ((visibility ("default")))
            #define RAWMEDIA_LOCAL  __attribute__ ((visibility ("hidden")))
        #else
            #define RAWMEDIA_IMPORT
            #define RAWMEDIA_EXPORT
            #define RAWMEDIA_LOCAL
        #endif
    #endif
#else
    #define RAWMEDIA_IMPORT
    #define RAWMEDIA_EXPORT
    #define RAWMEDIA_LOCAL
#endif

#endif
