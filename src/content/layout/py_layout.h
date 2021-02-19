
/// \file js_layout.h

#ifndef __PY_LAYOUT_H__
#define __PY_LAYOUT_H__

#include <memory>

#include "layout.h"

// forward declaration
class ContentManager;
class PyImportScript;
class PyScriptingRuntime;

class PyLayout : public Layout {
protected:
    std::shared_ptr<PyScriptingRuntime> runtime;
    std::unique_ptr<PyImportScript> import_script;

public:
    PyLayout(const std::shared_ptr<ContentManager>& content);
    ~PyLayout() override;

    void processCdsObject(std::shared_ptr<CdsObject> obj, fs::path rootpath) override;
};

#endif // __JS_LAYOUT_H__
