@ stub _Exit
@ cdecl -arch=i386 __control87_2(long long ptr ptr) ucrtbase.__control87_2
@ cdecl __doserrno() ucrtbase.__doserrno
@ stub __fpe_flt_rounds
@ cdecl __fpecode() ucrtbase.__fpecode
@ cdecl __p___argc() ucrtbase.__p___argc
@ cdecl __p___argv() ucrtbase.__p___argv
@ cdecl __p___wargv() ucrtbase.__p___wargv
@ cdecl __p__acmdln() ucrtbase.__p__acmdln
@ cdecl __p__pgmptr() ucrtbase.__p__pgmptr
@ cdecl __p__wcmdln() ucrtbase.__p__wcmdln
@ cdecl __p__wpgmptr() ucrtbase.__p__wpgmptr
@ cdecl __pxcptinfoptrs() ucrtbase.__pxcptinfoptrs
@ cdecl __sys_errlist() ucrtbase.__sys_errlist
@ cdecl __sys_nerr() ucrtbase.__sys_nerr
@ cdecl __threadhandle() ucrtbase.__threadhandle
@ cdecl __threadid() ucrtbase.__threadid
@ cdecl __wcserror(wstr) ucrtbase.__wcserror
@ cdecl __wcserror_s(ptr long wstr) ucrtbase.__wcserror_s
@ cdecl _assert(str str long) ucrtbase._assert
@ cdecl _beginthread(ptr long ptr) ucrtbase._beginthread
@ cdecl _beginthreadex(ptr long ptr ptr long ptr) ucrtbase._beginthreadex
@ cdecl _c_exit() ucrtbase._c_exit
@ cdecl _cexit() ucrtbase._cexit
@ cdecl _clearfp() ucrtbase._clearfp
@ cdecl _configure_narrow_argv(long) ucrtbase._configure_narrow_argv
@ stub _configure_wide_argv
@ cdecl _control87(long long) ucrtbase._control87
@ cdecl _controlfp(long long) ucrtbase._controlfp
@ cdecl _controlfp_s(ptr long long) ucrtbase._controlfp_s
@ stub _crt_at_quick_exit
@ cdecl _crt_atexit(ptr) ucrtbase._crt_atexit
@ stub _crt_debugger_hook
@ cdecl _endthread() ucrtbase._endthread
@ cdecl _endthreadex(long) ucrtbase._endthreadex
@ cdecl _errno() ucrtbase._errno
@ stub _execute_onexit_table
@ cdecl _exit(long) ucrtbase._exit
@ cdecl _fpieee_flt(long ptr ptr) ucrtbase._fpieee_flt
@ cdecl _fpreset() ucrtbase._fpreset
@ cdecl _get_doserrno(ptr) ucrtbase._get_doserrno
@ cdecl _get_errno(ptr) ucrtbase._get_errno
@ cdecl _get_initial_narrow_environment() ucrtbase._get_initial_narrow_environment
@ stub _get_initial_wide_environment
@ cdecl _get_invalid_parameter_handler() ucrtbase._get_invalid_parameter_handler
@ stub _get_narrow_winmain_command_line
@ cdecl _get_pgmptr(ptr) ucrtbase._get_pgmptr
@ cdecl _get_terminate() ucrtbase._get_terminate
@ stub _get_thread_local_invalid_parameter_handler
@ stub _get_wide_winmain_command_line
@ cdecl _get_wpgmptr(ptr) ucrtbase._get_wpgmptr
@ cdecl _getdllprocaddr(long str long) ucrtbase._getdllprocaddr
@ cdecl _getpid() ucrtbase._getpid
@ cdecl _initialize_narrow_environment() ucrtbase._initialize_narrow_environment
@ stub _initialize_onexit_table
@ stub _initialize_wide_environment
@ cdecl _initterm(ptr ptr) ucrtbase._initterm
@ cdecl _initterm_e(ptr ptr) ucrtbase._initterm_e
@ cdecl _invalid_parameter_noinfo() ucrtbase._invalid_parameter_noinfo
@ stub _invalid_parameter_noinfo_noreturn
@ stub _invoke_watson
@ stub _query_app_type
@ stub _register_onexit_function
@ stub _register_thread_local_exe_atexit_callback
@ cdecl _resetstkoflw() ucrtbase._resetstkoflw
@ stub _seh_filter_dll
@ stub _seh_filter_exe
@ cdecl _set_abort_behavior(long long) ucrtbase._set_abort_behavior
@ cdecl _set_app_type(long) ucrtbase._set_app_type
@ cdecl _set_controlfp(long long) ucrtbase._set_controlfp
@ cdecl _set_doserrno(long) ucrtbase._set_doserrno
@ cdecl _set_errno(long) ucrtbase._set_errno
@ cdecl _set_error_mode(long) ucrtbase._set_error_mode
@ cdecl _set_invalid_parameter_handler(ptr) ucrtbase._set_invalid_parameter_handler
@ cdecl _set_new_handler(ptr) ucrtbase._set_new_handler
@ stub _set_thread_local_invalid_parameter_handler
@ cdecl _seterrormode(long) ucrtbase._seterrormode
@ cdecl _sleep(long) ucrtbase._sleep
@ cdecl _statusfp() ucrtbase._statusfp
@ cdecl -arch=i386 _statusfp2(ptr ptr) ucrtbase._statusfp2
@ cdecl _strerror(long) ucrtbase._strerror
@ stub _strerror_s
@ cdecl _wassert(wstr wstr long) ucrtbase._wassert
@ cdecl _wcserror(long) ucrtbase._wcserror
@ cdecl _wcserror_s(ptr long long) ucrtbase._wcserror_s
@ cdecl _wperror(wstr) ucrtbase._wperror
@ cdecl _wsystem(wstr) ucrtbase._wsystem
@ cdecl abort() ucrtbase.abort
@ cdecl exit(long) ucrtbase.exit
@ stub feclearexcept
@ stub fegetenv
@ stub fegetexceptflag
@ stub fegetround
@ stub feholdexcept
@ stub fesetenv
@ stub fesetexceptflag
@ stub fesetround
@ stub fetestexcept
@ cdecl perror(str) ucrtbase.perror
@ stub quick_exit
@ cdecl raise(long) ucrtbase.raise
@ stub set_terminate
@ cdecl signal(long long) ucrtbase.signal
@ cdecl strerror(long) ucrtbase.strerror
@ cdecl strerror_s(ptr long long) ucrtbase.strerror_s
@ cdecl system(str) ucrtbase.system
@ stub terminate
