/*MT*
    
    MediaTomb - http://www.mediatomb.cc/
    
    script.cc - this file is part of MediaTomb.
    
    Copyright (C) 2005 Gena Batyan <bgeradz@mediatomb.cc>,
                       Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>
    
    Copyright (C) 2006-2010 Gena Batyan <bgeradz@mediatomb.cc>,
                            Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>,
                            Leonhard Wimmer <leo@mediatomb.cc>
    
    MediaTomb is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.
    
    MediaTomb is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    version 2 along with MediaTomb; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
    
    $Id$
*/

/// \file script.cc

#ifdef HAVE_JS
#include "script.h" // API

#include <fmt/chrono.h>

#include "cds_objects.h"
#include "config/config_definition.h"
#include "config/config_setup.h"
#include "content/content_manager.h"
#include "database/database.h"
#include "js_functions.h"
#include "metadata/metadata_handler.h"
#include "script_names.h"
#include "scripting_runtime.h"
#include "util/string_converter.h"
#include "util/tools.h"

#ifdef ONLINE_SERVICES
#include "content/onlineservice/online_service.h"
#endif

#ifdef ATRAILERS
#include "content/onlineservice/atrailers_content_handler.h"
#endif

static constexpr std::array<duk_function_list_entry, 9> js_global_functions = { {
    { "print", js_print, DUK_VARARGS },
    { "addCdsObject", js_addCdsObject, 3 },
    { "addContainerTree", js_addContainerTree, 1 },
    { "copyObject", js_copyObject, 1 },
    { "f2i", js_f2i, 1 },
    { "m2i", js_m2i, 1 },
    { "p2i", js_p2i, 1 },
    { "j2i", js_j2i, 1 },
    { nullptr, nullptr, 0 },
} };

std::string Script::getProperty(const std::string& name)
{
    std::string ret;
    if (!duk_is_object_coercible(ctx, -1))
        return "";
    duk_get_prop_string(ctx, -1, name.c_str());
    if (duk_is_null_or_undefined(ctx, -1) || !duk_to_string(ctx, -1)) {
        duk_pop(ctx);
        return "";
    }
    ret = duk_get_string(ctx, -1);
    duk_pop(ctx);
    return ret;
}

int Script::getBoolProperty(const std::string& name)
{
    int ret;
    if (!duk_is_object_coercible(ctx, -1))
        return -1;
    duk_get_prop_string(ctx, -1, name.c_str());
    if (duk_is_null_or_undefined(ctx, -1)) {
        duk_pop(ctx);
        return -1;
    }
    ret = duk_to_boolean(ctx, -1);
    duk_pop(ctx);
    return ret;
}

int Script::getIntProperty(const std::string& name, int def)
{
    int ret;
    if (!duk_is_object_coercible(ctx, -1))
        return def;
    duk_get_prop_string(ctx, -1, name.c_str());
    if (duk_is_null_or_undefined(ctx, -1)) {
        duk_pop(ctx);
        return def;
    }
    ret = duk_to_int32(ctx, -1);
    duk_pop(ctx);
    return ret;
}

std::vector<std::string> Script::getPropertyNames()
{
    std::vector<std::string> keys;
    duk_enum(ctx, -1, 0);
    while (duk_next(ctx, -1 /*enum_idx*/, 0 /*get_value*/)) {
        /* [ ... enum key ] */
        auto sym = duk_get_string(ctx, -1);
        keys.emplace_back(sym);
        duk_pop(ctx); /* pop_key */
    }
    duk_pop(ctx); // duk_enum
    return keys;
}

void Script::setProperty(const std::string& name, const std::string& value)
{
    duk_push_string(ctx, value.c_str());
    duk_put_prop_string(ctx, -2, name.c_str());
}

void Script::setIntProperty(const std::string& name, int value)
{
    duk_push_int(ctx, value);
    duk_put_prop_string(ctx, -2, name.c_str());
}

/* **************** */

Script::Script(std::shared_ptr<ContentManager> content,
    const std::shared_ptr<ScriptingRuntime>& runtime, const std::string& name)
    : gc_counter(0)
    , config(content->getContext()->getConfig())
    , database(content->getContext()->getDatabase())
    , content(std::move(content))
    , runtime(runtime)
    , name(name)
{
    /* create a context and associate it with the JS run time */
    ScriptingRuntime::AutoLock lock(runtime->getMutex());
    ctx = runtime->createContext(name);
    if (!ctx)
        throw_std_runtime_error("Scripting: could not initialize js context");

    _p2i = StringConverter::p2i(config);
    _j2i = StringConverter::j2i(config);
    _m2i = StringConverter::m2i(CFG_IMPORT_METADATA_CHARSET, "", config);
    _f2i = StringConverter::f2i(config);
    _i2i = StringConverter::i2i(config);

    duk_push_thread_stash(ctx, ctx);
    duk_push_pointer(ctx, this);
    duk_put_prop_string(ctx, -2, "this");
    duk_pop(ctx);

    /* initialize contstants */
    for (auto&& [field, sym] : ot_names) {
        duk_push_int(ctx, field);
        duk_put_global_string(ctx, sym);
    }
#ifdef ONLINE_SERVICES
    duk_push_int(ctx, int(OS_None));
    duk_put_global_string(ctx, "ONLINE_SERVICE_NONE");
    duk_push_int(ctx, -1);
    duk_put_global_string(ctx, "ONLINE_SERVICE_YOUTUBE");

#ifdef ATRAILERS
    duk_push_int(ctx, int(OS_ATrailers));
    duk_put_global_string(ctx, "ONLINE_SERVICE_APPLE_TRAILERS");
    duk_push_string(ctx, ATRAILERS_AUXDATA_POST_DATE);
    duk_put_global_string(ctx, "APPLE_TRAILERS_AUXDATA_POST_DATE");
#else
    duk_push_int(ctx, -1);
    duk_put_global_string(ctx, "ONLINE_SERVICE_APPLE_TRAILERS");
#endif //ATRAILERS

#ifdef SOPCAST
    duk_push_int(ctx, int(OS_SopCast));
    duk_put_global_string(ctx, "ONLINE_SERVICE_SOPCAST");
#else
    duk_push_int(ctx, -1);
    duk_put_global_string(ctx, "ONLINE_SERVICE_SOPCAST");
#endif //SOPCAST

#else // ONLINE SERVICES
    duk_push_int(ctx, 0);
    duk_put_global_string(ctx, "ONLINE_SERVICE_NONE");
    duk_push_int(ctx, -1);
    duk_put_global_string(ctx, "ONLINE_SERVICE_YOUTUBE");
    duk_push_int(ctx, -1);
    duk_put_global_string(ctx, "ONLINE_SERVICE_APPLE_TRAILERS");
    duk_push_int(ctx, -1);
    duk_put_global_string(ctx, "ONLINE_SERVICE_SOPCAST");
#endif //ONLINE_SERVICES

    for (auto&& [field, sym] : mt_keys) {
        duk_push_string(ctx, sym);
        for (auto&& [f, s] : mt_names) {
            if (f == field) {
                duk_put_global_string(ctx, s);
            }
        }
    }

    for (auto&& [field, sym] : res_keys) {
        duk_push_string(ctx, sym);
        for (auto&& [f, s] : res_names) {
            if (f == field) {
                duk_put_global_string(ctx, s);
            }
        }
    }

    for (auto&& [field, sym] : upnp_classes) {
        duk_push_string(ctx, field);
        duk_put_global_string(ctx, sym);
    }

    duk_push_object(ctx); // config
    for (int i = 0; i < int(CFG_MAX); i++) {
        auto scs = ConfigDefinition::findConfigSetup(config_option_t(i), true);
        if (scs == nullptr)
            continue;
        auto value = scs->getCurrentValue();
        if (!value.empty()) {
            setProperty(scs->getItemPath(-1), value);
        }
    }

    for (auto&& dcs : ConfigDefinition::getConfigSetupList<ConfigDictionarySetup>()) {
        duk_push_object(ctx);
        auto dictionary = dcs->getValue()->getDictionaryOption(true);
        for (auto&& [key, val] : dictionary) {
            setProperty(key.substr(5), val);
        }
        duk_put_prop_string(ctx, -2, dcs->getItemPath(-1).c_str());
    }

    for (auto&& acs : ConfigDefinition::getConfigSetupList<ConfigArraySetup>()) {
        auto array = acs->getValue()->getArrayOption(true);
        auto duk_array = duk_push_array(ctx);
        for (size_t i = 0; i < array.size(); i++) {
            auto entry = array[i];
            duk_push_string(ctx, entry.c_str());
            duk_put_prop_index(ctx, duk_array, i);
        }
        duk_put_prop_string(ctx, -2, acs->getItemPath(-1).c_str());
    }

    duk_push_object(ctx); // autoscan
    std::string autoscanItemPath;
    for (auto&& ascs : ConfigDefinition::getConfigSetupList<ConfigAutoscanSetup>()) {
        auto autoscan = ascs->getValue()->getAutoscanListOption();
        for (size_t i = 0; i < autoscan->size(); i++) {
            auto&& entry = autoscan->get(i);
            auto&& adir = this->content->getAutoscanDirectory(entry->getLocation());

            duk_push_object(ctx);
            setProperty(ConfigDefinition::removeAttribute(ATTR_AUTOSCAN_DIRECTORY_LOCATION), adir->getLocation());
            setProperty(ConfigDefinition::removeAttribute(ATTR_AUTOSCAN_DIRECTORY_MODE), AutoscanDirectory::mapScanmode(adir->getScanMode()).data());
            setIntProperty(ConfigDefinition::removeAttribute(ATTR_AUTOSCAN_DIRECTORY_INTERVAL), adir->getInterval().count());
            setIntProperty(ConfigDefinition::removeAttribute(ATTR_AUTOSCAN_DIRECTORY_RECURSIVE), adir->getRecursive());
            setIntProperty(ConfigDefinition::removeAttribute(ATTR_AUTOSCAN_DIRECTORY_HIDDENFILES), adir->getHidden());
            setIntProperty(ConfigDefinition::removeAttribute(ATTR_AUTOSCAN_DIRECTORY_SCANCOUNT), adir->getActiveScanCount());
            setIntProperty(ConfigDefinition::removeAttribute(ATTR_AUTOSCAN_DIRECTORY_TASKCOUNT), adir->getTaskCount());
            setProperty(ConfigDefinition::removeAttribute(ATTR_AUTOSCAN_DIRECTORY_LMT), fmt::format("{:%Y-%m-%d %H:%M:%S}", fmt::localtime(adir->getPreviousLMT().count())));

            duk_put_prop_string(ctx, -2, fmt::format("{}", adir->getScanID()).c_str());
        }
        autoscanItemPath = ascs->getItemPath(-2);
    }
    duk_put_prop_string(ctx, -2, autoscanItemPath.c_str()); // autoscan

    duk_put_global_string(ctx, "config");

    defineFunctions(js_global_functions.data());

    std::string common_scr_path = config->getOption(CFG_IMPORT_SCRIPTING_COMMON_SCRIPT);

    if (common_scr_path.empty()) {
        log_js("Common script disabled in configuration");
    } else {
        try {
            _load(common_scr_path);
            _execute();
        } catch (const std::runtime_error& e) {
            log_js("Unable to load {}: {}", common_scr_path.c_str(),
                e.what());
        }
    }
    std::string custom_scr_path = config->getOption(CFG_IMPORT_SCRIPTING_CUSTOM_SCRIPT);
    if (!custom_scr_path.empty()) {
        try {
            _load(custom_scr_path);
            _execute();
        } catch (const std::runtime_error& e) {
            log_js("Unable to load {}: {}", custom_scr_path.c_str(),
                e.what());
        }
    }
}

Script::~Script()
{
    runtime->destroyContext(name);
}

Script* Script::getContextScript(duk_context* ctx)
{
    duk_push_thread_stash(ctx, ctx);
    duk_get_prop_string(ctx, -1, "this");
    auto self = static_cast<Script*>(duk_get_pointer(ctx, -1));
    duk_pop_2(ctx);
    if (self == nullptr) {
        log_debug("Could not retrieve class instance from global object");
        duk_error(ctx, DUK_ERR_ERROR, "Could not retrieve current script from stash");
    }
    return self;
}

void Script::defineFunction(const std::string& name, duk_c_function function, uint32_t numParams)
{
    duk_push_c_function(ctx, function, numParams);
    duk_put_global_string(ctx, name.c_str());
}

void Script::defineFunctions(const duk_function_list_entry* functions)
{
    duk_push_global_object(ctx);
    duk_put_function_list(ctx, -1, functions);
    duk_pop(ctx);
}

void Script::_load(const std::string& scriptPath)
{
    std::string scriptText = readTextFile(scriptPath);

    if (scriptText.empty())
        throw_std_runtime_error("empty script");

    auto j2i = StringConverter::j2i(config);
    try {
        scriptText = j2i->convert(scriptText, true);
    } catch (const std::runtime_error& e) {
        throw_std_runtime_error("Failed to convert import script: {}", e.what());
    }

    duk_push_string(ctx, scriptPath.c_str());
    if (duk_pcompile_lstring_filename(ctx, 0, scriptText.c_str(), scriptText.length()) != 0) {
        log_error("Failed to load script: {}", duk_safe_to_string(ctx, -1));
        throw_std_runtime_error("Scripting: failed to compile {}", scriptPath.c_str());
    }
}

void Script::load(const std::string& scriptPath)
{
    ScriptingRuntime::AutoLock lock(runtime->getMutex());
    duk_push_thread_stash(ctx, ctx);
    _load(scriptPath);
    duk_put_prop_string(ctx, -2, "script");
    duk_pop(ctx);
}

void Script::_execute()
{
    if (duk_pcall(ctx, 0) != DUK_EXEC_SUCCESS) {
        log_error("Failed to execute script: {}", duk_safe_to_string(ctx, -1));
        throw_std_runtime_error("Script: failed to execute script");
    }
    duk_pop(ctx);
}

void Script::execute()
{
    ScriptingRuntime::AutoLock lock(runtime->getMutex());
    duk_push_thread_stash(ctx, ctx);
    duk_get_prop_string(ctx, -1, "script");
    duk_remove(ctx, -2);
    _execute();
}

std::shared_ptr<CdsObject> Script::dukObject2cdsObject(const std::shared_ptr<CdsObject>& pcd)
{
    std::string val;
    int b;
    int i;
    std::unique_ptr<StringConverter> sc;

    if (this->whoami() == S_PLAYLIST) {
        sc = StringConverter::p2i(config);
    } else
        sc = StringConverter::i2i(config);

    int objType = getIntProperty("objectType", -1);
    if (objType == -1) {
        log_error("missing objectType property");
        return nullptr;
    }

    auto obj = CdsObject::createObject(objType);

    // CdsObject
    obj->setVirtual(true); // JS creates only virtual objects

    i = getIntProperty("id", INVALID_OBJECT_ID);
    if (i != INVALID_OBJECT_ID)
        obj->setID(i);
    i = getIntProperty("refID", INVALID_OBJECT_ID);
    if (i != INVALID_OBJECT_ID)
        obj->setRefID(i);
    i = getIntProperty("parentID", INVALID_OBJECT_ID);
    if (i != INVALID_OBJECT_ID) {
        obj->setParentID(i);
    }

    i = getIntProperty("mtime", 0);
    if (i > 0)
        obj->setMTime(std::chrono::seconds(i));

    duk_get_prop_string(ctx, -1, "parent");
    if (duk_is_object(ctx, -1)) {
        duk_to_object(ctx, -1);
        auto parent = dukObject2cdsObject(nullptr);
        if (parent != nullptr) {
            obj->setParent(parent);
            log_debug("dukObject2cdsObject: Parent {}", parent->getClass());
        }
    }
    duk_pop(ctx);

    val = getProperty("title");
    if (!val.empty()) {
        val = sc->convert(val);
        obj->setTitle(val);
    } else {
        if (pcd != nullptr)
            obj->setTitle(pcd->getTitle());
    }

    val = getProperty("upnpclass");
    if (!val.empty()) {
        val = sc->convert(val);
        obj->setClass(val);
    } else {
        if (pcd != nullptr)
            obj->setClass(pcd->getClass());
    }

    b = getBoolProperty("restricted");
    if (b >= 0)
        obj->setRestricted(b);

    duk_get_prop_string(ctx, -1, "meta");
    if (!duk_is_null_or_undefined(ctx, -1) && duk_is_object(ctx, -1)) {
        duk_to_object(ctx, -1);
        // only metadata enumerated in mt_keys is allowed
        for (auto&& [sym, upnp] : mt_keys) {
            val = getProperty(upnp);
            if (!val.empty()) {
                if (sym == M_TRACKNUMBER) {
                    int j = stoiString(val, 0);
                    if (j > 0) {
                        obj->setMetadata(sym, val);
                        std::static_pointer_cast<CdsItem>(obj)->setTrackNumber(j);
                    } else
                        std::static_pointer_cast<CdsItem>(obj)->setTrackNumber(0);
                } else if (sym == M_PARTNUMBER) {
                    int j = stoiString(val, 0);
                    if (j > 0) {
                        obj->setMetadata(sym, val);
                        std::static_pointer_cast<CdsItem>(obj)->setPartNumber(j);
                    } else
                        std::static_pointer_cast<CdsItem>(obj)->setPartNumber(0);
                } else {
                    val = sc->convert(val);
                    obj->setMetadata(sym, val);
                }
            }
        }
    }
    duk_pop(ctx);

    // stuff that has not been exported to js
    if (pcd != nullptr) {
        obj->setFlags(pcd->getFlags());
        obj->setResources(pcd->getResources());
        obj->setAuxData(pcd->getAuxData());
    } else {
        i = getIntProperty("flags", -1);
        if (i >= 0)
            obj->setFlags(i);

        duk_get_prop_string(ctx, -1, "res");
        if (!duk_is_null_or_undefined(ctx, -1) && duk_is_object(ctx, -1)) {
            duk_to_object(ctx, -1);
            auto keys = getPropertyNames();

            int resCount = 0;
            for (auto&& sym : keys) {
                if (sym.find("handlerType") != std::string::npos) {
                    i = getIntProperty(sym, -1);
                    if (i >= 0) {
                        obj->addResource(std::make_shared<CdsResource>(i));
                    }
                }
            }
            resCount = 0;
            for (auto&& res : obj->getResources()) {
                // only attribute enumerated in res_keys is allowed
                for (auto&& [key, upnp] : res_keys) {
                    val = getProperty(resCount == 0 ? upnp : fmt::format("{}-{}", resCount, upnp));
                    if (!val.empty()) {
                        val = sc->convert(val);
                        res->addAttribute(key, val);
                    }
                }
                auto head = fmt::format("{}#", resCount);
                for (auto&& sym : keys) {
                    if (sym.find(head) != std::string::npos) {
                        auto key = sym.substr(head.size());
                        val = getProperty(sym);
                        res->addParameter(key, val);
                    }
                }
                head = fmt::format("{}%", resCount);
                for (auto&& sym : keys) {
                    if (sym.find(head) != std::string::npos) {
                        auto key = sym.substr(head.size());
                        val = getProperty(sym);
                        res->addOption(key, val);
                    }
                }
                resCount++;
            }
        }
        duk_pop(ctx); // res

        duk_get_prop_string(ctx, -1, "aux");
        if (!duk_is_null_or_undefined(ctx, -1) && duk_is_object(ctx, -1)) {
            duk_to_object(ctx, -1);
            auto keys = getPropertyNames();
            for (auto&& sym : keys) {
                val = getProperty(sym);
                if (!val.empty()) {
                    val = sc->convert(val);
                    obj->setAuxData(sym, val);
                }
            }
        }
        duk_pop(ctx); // aux
    }

    // CdsItem
    if (obj->isItem()) {
        auto item = std::static_pointer_cast<CdsItem>(obj);
        std::shared_ptr<CdsItem> pcd_item;

        if (pcd != nullptr)
            pcd_item = std::static_pointer_cast<CdsItem>(pcd);

        val = getProperty("mimetype");
        if (!val.empty()) {
            val = sc->convert(val);
            item->setMimeType(val);
        } else {
            if (pcd != nullptr)
                item->setMimeType(pcd_item->getMimeType());
        }

        val = getProperty("serviceID");
        if (!val.empty()) {
            val = sc->convert(val);
            item->setServiceID(val);
        }

        /// \todo check what this is doing here, wasn't it already handled
        /// in the mt_keys loop?
        val = getProperty("description");
        if (!val.empty()) {
            val = sc->convert(val);
            item->setMetadata(M_DESCRIPTION, val);
        } else {
            if (pcd != nullptr)
                item->setMetadata(M_DESCRIPTION, pcd_item->getMetadata(M_DESCRIPTION));
        }
        if (this->whoami() == S_PLAYLIST) {
            item->setTrackNumber(getIntProperty("playlistOrder", 0));
        }

        // location must not be touched by character conversion!
        fs::path location = getProperty("location");
        if (!location.empty())
            obj->setLocation(location);
        else {
            if (pcd != nullptr)
                obj->setLocation(pcd->getLocation());
        }

        if (obj->isExternalItem()) {
            std::string protocolInfo;

            obj->setRestricted(true);
            val = getProperty("protocol");
            if (!val.empty()) {
                val = sc->convert(val);
                protocolInfo = renderProtocolInfo(item->getMimeType(), val);
            } else {
                protocolInfo = renderProtocolInfo(item->getMimeType(), PROTOCOL);
            }

            if (item->getResourceCount() == 0) {
                auto resource = std::make_shared<CdsResource>(CH_DEFAULT);
                resource->addAttribute(R_PROTOCOLINFO, protocolInfo);

                item->addResource(resource);
            }
        }
    }

    // CdsDirectory
    if (obj->isContainer()) {
        auto cont = std::static_pointer_cast<CdsContainer>(obj);
        i = getIntProperty("updateID", -1);
        if (i >= 0)
            cont->setUpdateID(i);

        b = getBoolProperty("searchable");
        if (b >= 0)
            cont->setSearchable(b);
    }

    return obj;
}

void Script::cdsObject2dukObject(const std::shared_ptr<CdsObject>& obj)
{
    std::string val;
    int i;

    duk_push_object(ctx);

    // CdsObject
    setIntProperty("objectType", obj->getObjectType());

    i = obj->getID();

    if (i != INVALID_OBJECT_ID)
        setIntProperty("id", i);

    i = obj->getParentID();
    if (i != INVALID_OBJECT_ID)
        setIntProperty("parentID", i);

    val = obj->getTitle();
    if (!val.empty())
        setProperty("title", val);

    val = obj->getClass();
    if (!val.empty())
        setProperty("upnpclass", val);

    val = obj->getLocation();
    if (!val.empty())
        setProperty("location", val);

    setIntProperty("mtime", int(obj->getMTime().count()));
    setIntProperty("sizeOnDisk", int(obj->getSizeOnDisk()));
    setIntProperty("flags", obj->getFlags());

    // TODO: boolean type
    i = obj->isRestricted();
    setIntProperty("restricted", i);

    if (obj->getFlag(OBJECT_FLAG_OGG_THEORA))
        setIntProperty("theora", 1);
    else
        setIntProperty("theora", 0);

#ifdef ONLINE_SERVICES
    if (obj->getFlag(OBJECT_FLAG_ONLINE_SERVICE)) {
        auto service = service_type_t(std::stoi(obj->getAuxData(ONLINE_SERVICE_AUX_ID)));
        setIntProperty("onlineservice", int(service));
    } else
#endif
        setIntProperty("onlineservice", 0);

    // setting metadata
    {
        duk_push_object(ctx);
        // stack: js meta_js

        auto meta = obj->getMetadata();
        for (auto&& [key, val] : meta) {
            setProperty(key, val);
        }

        if (std::static_pointer_cast<CdsItem>(obj)->getTrackNumber() > 0)
            setProperty(MetadataHandler::getMetaFieldName(M_TRACKNUMBER), fmt::to_string(std::static_pointer_cast<CdsItem>(obj)->getTrackNumber()));

        duk_put_prop_string(ctx, -2, "meta");
        // stack: js
    }

    // setting auxdata
    {
        duk_push_object(ctx);
        // stack: js aux_js

        auto aux = obj->getAuxData();

#ifdef HAVE_ATRAILERS
        auto tmp = obj->getAuxData(ATRAILERS_AUXDATA_POST_DATE);
        if (!tmp.empty())
            aux[ATRAILERS_AUXDATA_POST_DATE] = tmp;
#endif

        for (auto&& [key, val] : aux) {
            setProperty(key, val);
        }

        duk_put_prop_string(ctx, -2, "aux");
        // stack: js
    }

    // setting resource
    {
        duk_push_object(ctx);
        // stack: js res_js

        if (obj->getResourceCount() > 0) {
            int resCount = 0;
            for (auto&& res : obj->getResources()) {
                setProperty(fmt::format("{}:handlerType", resCount), fmt::format("{}", res->getHandlerType()));
                auto attributes = res->getAttributes();
                for (auto&& [key, val] : attributes) {
                    setProperty(resCount == 0 ? key : fmt::format("{}-{}", resCount, key), val);
                }
                auto parameters = res->getParameters();
                for (auto&& [key, val] : parameters) {
                    setProperty(fmt::format("{}#{}", resCount, key), val);
                }
                auto options = res->getOptions();
                for (auto&& [key, val] : options) {
                    setProperty(fmt::format("{}%{}", resCount, key), val);
                }
                resCount++;
            }
        }

        duk_put_prop_string(ctx, -2, "res");
        // stack: js
    }

    // CdsItem
    if (obj->isItem()) {
        auto item = std::static_pointer_cast<CdsItem>(obj);
        val = item->getMimeType();
        if (!val.empty())
            setProperty("mimetype", val);

        val = item->getServiceID();
        if (!val.empty())
            setProperty("serviceID", val);
    }

    // CdsDirectory
    if (obj->isContainer()) {
        auto cont = std::static_pointer_cast<CdsContainer>(obj);
        // TODO: boolean type, hide updateID
        i = cont->getUpdateID();
        setIntProperty("updateID", i);

        i = cont->isSearchable();
        setIntProperty("searchable", i);
    }
}

std::string Script::convertToCharset(const std::string& str, charset_convert_t chr)
{
    switch (chr) {
    case P2I:
        return _p2i->convert(str);
    case M2I:
        return _m2i->convert(str);
    case F2I:
        return _f2i->convert(str);
    case J2I:
        return _j2i->convert(str);
    case I2I:
        return _i2i->convert(str);
    }
    throw_std_runtime_error("Illegal charset given to convertToCharset(): {}", chr);
}

std::shared_ptr<CdsObject> Script::getProcessedObject()
{
    return processed;
}

#endif // HAVE_JS
