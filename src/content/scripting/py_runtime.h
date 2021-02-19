
/// \file script.h

#ifndef __PY_RUNTIME_H__
#define __PY_RUNTIME_H__

#ifdef HAVE_PY

#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <mutex>
#include <filesystem>

#include "common.h"
#include "cds_objects.h"

namespace py = pybind11;
using t_addCdsObject = std::function<int(CdsObject&,const std::vector<std::string>&, std::string)>;

class PyRuntime
{
public:
    PyRuntime(std::string scriptpath);
    ~PyRuntime() = default;

protected:
    py::scoped_interpreter guard {};
    py::module importModule;

private:
    static t_addCdsObject callback;

public:    
    void processCdsObject(const std::shared_ptr<CdsObject>& obj, const std::string& scriptpath, t_addCdsObject callback);
    static int addCdsObject(CdsObject& obj, const std::vector<std::string>& chain, std::string containerclass);
};
#endif
#endif // __PY_RUNTIME_H__
