
#ifndef CNOID_PYTHONSIMSCRIPTPLUGIN_EXPORTDECL_H_INCLUDED
# define CNOID_PYTHONSIMSCRIPTPLUGIN_EXPORTDECL_H_INCLUDED

# if defined _WIN32 || defined __CYGWIN__
#  define CNOID_PYTHONSIMSCRIPTPLUGIN_DLLIMPORT __declspec(dllimport)
#  define CNOID_PYTHONSIMSCRIPTPLUGIN_DLLEXPORT __declspec(dllexport)
#  define CNOID_PYTHONSIMSCRIPTPLUGIN_DLLLOCAL
# else
#  if __GNUC__ >= 4
#   define CNOID_PYTHONSIMSCRIPTPLUGIN_DLLIMPORT __attribute__ ((visibility("default")))
#   define CNOID_PYTHONSIMSCRIPTPLUGIN_DLLEXPORT __attribute__ ((visibility("default")))
#   define CNOID_PYTHONSIMSCRIPTPLUGIN_DLLLOCAL  __attribute__ ((visibility("hidden")))
#  else
#   define CNOID_PYTHONSIMSCRIPTPLUGIN_DLLIMPORT
#   define CNOID_PYTHONSIMSCRIPTPLUGIN_DLLEXPORT
#   define CNOID_PYTHONSIMSCRIPTPLUGIN_DLLLOCAL
#  endif
# endif

# ifdef CNOID_PYTHONSIMSCRIPTPLUGIN_STATIC
#  define CNOID_PYTHONSIMSCRIPTPLUGIN_DLLAPI
#  define CNOID_PYTHONSIMSCRIPTPLUGIN_LOCAL
# else
#  ifdef CnoidPythonSimScriptPlugin_EXPORTS
#   define CNOID_PYTHONSIMSCRIPTPLUGIN_DLLAPI CNOID_PYTHONSIMSCRIPTPLUGIN_DLLEXPORT
#  else
#   define CNOID_PYTHONSIMSCRIPTPLUGIN_DLLAPI CNOID_PYTHONSIMSCRIPTPLUGIN_DLLIMPORT
#  endif
#  define CNOID_PYTHONSIMSCRIPTPLUGIN_LOCAL CNOID_PYTHONSIMSCRIPTPLUGIN_DLLLOCAL
# endif

#endif

#ifdef CNOID_EXPORT
# undef CNOID_EXPORT
#endif
#define CNOID_EXPORT CNOID_PYTHONSIMSCRIPTPLUGIN_DLLAPI