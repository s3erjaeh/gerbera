
/// \file script.h

#ifndef __PY_SCRIPTING_SCRIPT_H__
#define __PY_SCRIPTING_SCRIPT_H__

#ifdef HAVE_PY

#include <mutex>
#include <filesystem>

#include "common.h"
#include "context.h"
#include "py_runtime.h"

// forward declaration
class CdsObject;
class ContentManager;
class PyScriptingRuntime;
class StringConverter;


class PyImportScript {
public:
    PyImportScript(std::shared_ptr<ContentManager> content);
    ~PyImportScript() = default;
    void processCdsObject( const std::shared_ptr<CdsObject>& obj, const std::string& scriptpath);
    int addCdsObject(const std::shared_ptr<CdsObject>& orgObj,CdsObject& obj, const std::vector<std::string>&, std::string containerclass);

    std::shared_ptr<Config> getConfig() const { return config; }
    std::shared_ptr<ContentManager> getContent() const { return content; }

protected:
    std::unique_ptr<PyRuntime> runtime;

    std::shared_ptr<Config> config;
    std::shared_ptr<ContentManager> content;

private:
    static PyImportScript* context;
};
#endif
#endif // __PY_SCRIPTING_SCRIPT_H__
