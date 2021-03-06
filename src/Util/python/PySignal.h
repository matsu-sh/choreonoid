/*!
  @author Shin'ichiro Nakaoka
*/

#ifndef CNOID_UTIL_PYSIGNAL_H
#define CNOID_UTIL_PYSIGNAL_H

#include "../Signal.h"
#include "PyUtil.h"
#include <cnoid/PythonUtil>

namespace cnoid {

namespace signal_private {

template<typename T> struct python_function_caller0 {
    boost::python::object func;
    python_function_caller0(boost::python::object func) : func(func) { }
    T operator()() {
        PyGILock lock;
        T result;
        try {
            result = func();
        } catch(boost::python::error_already_set const& ex) {
            cnoid::handlePythonException();
        }
        return result;
    }
};

template<> struct python_function_caller0<void> {
    boost::python::object func;
    python_function_caller0(boost::python::object func) : func(func) { }
    void operator()() {
        PyGILock lock;
        try {
            func();
        } catch(boost::python::error_already_set const& ex) {
            cnoid::handlePythonException();
        }
    }
};

template<typename T, typename ARG1> struct python_function_caller1 {
    boost::python::object func;
    python_function_caller1(boost::python::object func) : func(func) { }
    T operator()(ARG1 arg1) {
        PyGILock lock;
        T result;
        try {
            result = func(arg1);
        } catch(boost::python::error_already_set const& ex) {
            handlePythonException();
        }
        return result;
    }
};

template<typename ARG1> struct python_function_caller1<void, ARG1> {
    boost::python::object func;
    python_function_caller1(boost::python::object func) : func(func) { }
    void operator()(ARG1 arg1) {
        PyGILock lock;
        try {
            func(arg1);
        } catch(boost::python::error_already_set const& ex) {
            handlePythonException();
        }
    }
};

template<typename T, typename ARG1, typename ARG2> struct python_function_caller2 {
    boost::python::object func;
    python_function_caller2(boost::python::object func) : func(func) { }
    T operator()(ARG1 arg1, ARG2 arg2) {
        PyGILock lock;
        T result;
        try {
            result = func(arg1, arg2);
        } catch(boost::python::error_already_set const& ex) {
            handlePythonException();
        }
        return result;
    }
};

template<typename ARG1, typename ARG2> struct python_function_caller2<void, ARG1, ARG2> {
    boost::python::object func;
    python_function_caller2(boost::python::object func) : func(func) { }
    void operator()(ARG1 arg1, ARG2 arg2) {
        PyGILock lock;
        try {
            func(arg1, arg2);
        } catch(boost::python::error_already_set const& ex) {
            handlePythonException();
        }
    }
};


template<int Arity, typename Signature, typename Combiner>
class py_signal_proxy_impl;

template<typename Signature, typename Combiner>
class py_signal_proxy_impl<0, Signature, Combiner>
{
    typedef boost::function_traits<Signature> traits;
public:
    typedef python_function_caller0<typename traits::result_type> caller;
};

template<typename Signature, typename Combiner>
class py_signal_proxy_impl<1, Signature, Combiner>
{
    typedef boost::function_traits<Signature> traits;
public:
    typedef python_function_caller1<typename traits::result_type,
                                    typename traits::arg1_type> caller;
};

template<typename Signature, typename Combiner>
class py_signal_proxy_impl<2, Signature, Combiner>
{
    typedef boost::function_traits<Signature> traits;
public:
    typedef python_function_caller2<typename traits::result_type,
                                    typename traits::arg1_type,
                                    typename traits::arg2_type> caller;
};

} // namespace signal_private

template<
    typename Signature, 
    typename Combiner = signal_private::last_value<typename boost::function_traits<Signature>::result_type>
    >
class PySignalProxy : public signal_private::py_signal_proxy_impl<
    (boost::function_traits<Signature>::arity), Signature, Combiner>
{
    typedef signal_private::py_signal_proxy_impl<(boost::function_traits<Signature>::arity), Signature, Combiner> base_type;
    
    static Connection connect(SignalProxy<Signature, Combiner>& self, boost::python::object func){
        return self.connect(typename base_type::caller(func));
    }
public:
    PySignalProxy(const char* name) {
        boost::python::class_< SignalProxy<Signature, Combiner> >(name)
            .def("connect", &PySignalProxy::connect);
    }
};

}

#endif
